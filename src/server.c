#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <unistd.h>
#include <pthread.h>

#include "hash.h"
#include "server.h"
#include "utils.h"

static char *DEFAULT_TOPIC_NAMES[] = {
    "WEATHER",
    "NEWS",
};

/* Connected clients */
static struct hash_table *online_clients;
pthread_mutex_t online_lock = PTHREAD_MUTEX_INITIALIZER;

/* These clients are always added in order of disconnect time */
static struct hash_table *offline_clients;
pthread_mutex_t offline_lock = PTHREAD_MUTEX_INITIALIZER;

static struct list msg_queue = LIST_INIT(msg_queue);
pthread_mutex_t msg_queue_lock = PTHREAD_MUTEX_INITIALIZER;

static struct hash_table *topics;

static void reply_conn(struct connection *conn, char *msg, size_t msg_len)
{
    int res;

    res = send(conn->sock, msg, msg_len, 0);
    if (res == -1 && errno != EAGAIN && errno != EWOULDBLOCK)
    {
        perror("send");
        conn->closing = 1;
    }

    return;
}

static uint64_t get_oldest_offline_client_time(void)
{
    struct offline_client *client;
    uint64_t oldest_time = ~0u;
    size_t i;

    pthread_mutex_lock(&offline_lock);

    if (hash_empty(offline_clients))
    {
        pthread_mutex_unlock(&offline_lock);
        return ~0u; /* Removes everything */
    }

    for (i = 0; i < offline_clients->size; i++)
    {
        if (list_empty(&offline_clients->buckets[i]))
            continue;
        
        /* prev/tail is the oldest element, elements are added head first */
        client = LIST_ENTRY(offline_clients->buckets[i].prev,
                            struct offline_client,
                            entry);

        if (client->disc_time < oldest_time)
            oldest_time = client->disc_time;
    }

    pthread_mutex_unlock(&offline_lock);

    return oldest_time;
}

static void remove_stale_messages(void)
{
    uint64_t oldest_time = get_oldest_offline_client_time();
    struct queued_msg *msg;
    struct list *cur;

    pthread_mutex_lock(&msg_queue_lock);

    for (cur = msg_queue.next; cur != &msg_queue; cur = cur->next)
    {
        msg = LIST_ENTRY(cur, struct queued_msg, entry);
        if (msg->time < oldest_time)
        {
            list_remove(cur);
            free(msg->message);
            free(msg->topic);
            free(msg->sender);
            free(msg);
        }
    }

    pthread_mutex_unlock(&msg_queue_lock);
}

static int is_offline_client_subscribed(struct offline_client *client, char *topic)
{
    struct subscription *sub;
    struct list *cur;

    for (cur = client->subs.next; cur != &client->subs; cur = cur->next)
    {
        sub = LIST_ENTRY(cur, struct subscription, entry);
        if (!strcmp(sub->topic_name, topic))
            return 1;
    }

    return 0;
}

static void add_offline_client(struct connection *conn)
{
    struct offline_client *off_client; 

    off_client = calloc(sizeof(*off_client), 1);
    if (!off_client)
    {
        perror("calloc");
        return;
    }

    off_client->name = strdup(conn->name);
    if (!off_client->name)
    {
        perror("stdup");
        free(off_client);
        return;
    }

    list_init(&off_client->entry);
    list_init(&off_client->subs);
    off_client->disc_time = get_current_time();

    /* Move from one list to another */
    list_add_head(&off_client->subs, &conn->subbed_topics);
    list_remove(&conn->subbed_topics);
    list_init(&conn->subbed_topics);

    pthread_mutex_lock(&offline_lock);

    hash_insert(offline_clients,
                off_client->name,
                strlen(off_client->name) + 1,
                &off_client->entry);

    pthread_mutex_unlock(&offline_lock);
    
    return;
}

/* Removes offline client and moves data to connection. Frees offline_client */
static void reconnect_offline_client(struct offline_client *offline, struct connection *conn)
{
    struct queued_msg *msg;
    struct list *cur;
    char msg_buf[1024];

    pthread_mutex_lock(&msg_queue_lock);

    for (cur = msg_queue.next; cur != &msg_queue; cur = cur->next)
    {
        msg = LIST_ENTRY(cur, struct queued_msg, entry);

        if (offline->disc_time < msg->time)
            break;

        if (!is_offline_client_subscribed(offline, msg->topic))
            continue;

        snprintf(msg_buf, 1024, "<%s, PUB, %s, %s>", msg->sender, msg->topic, msg->message);

        reply_conn(conn, msg_buf, 1024);
    }

    pthread_mutex_unlock(&msg_queue_lock);

    /* Move from one list to another */
    list_add_head(&conn->subbed_topics, &offline->subs);
    list_remove(&offline->subs);

    list_remove(&offline->entry);
    free(offline->name);
    free(offline);

    remove_stale_messages();

    return;
}

static void close_connection(struct connection *conn)
{
    add_offline_client(conn);

    pthread_mutex_lock(&online_lock);
    list_remove(&conn->entry);
    pthread_mutex_unlock(&online_lock);

    close(conn->sock);
    free(conn->name);
    free(conn);
}

static struct topic *get_topic(char *name)
{
    struct list *bucket, *cur;
    struct topic *topic;
    size_t hash;

    hash = hash_bytes(name, strlen(name) + 1) % topics->size;
    bucket = &topics->buckets[hash];

    for (cur = bucket->next; cur != bucket; cur = cur->next)
    {
        topic = LIST_ENTRY(cur, struct topic, entry);
        if (!strcmp(topic->name, name))
            return topic;
    }

    return NULL;
}

/* Must lock online_lock when calling */
static struct connection *get_client_by_name(char *name)
{
    struct list *bucket, *cur;
    struct connection *conn;
    size_t hash;

    hash = hash_bytes(name, strlen(name) + 1) % online_clients->size;
    bucket = &online_clients->buckets[hash];

    for (cur = bucket->next; cur != bucket; cur = cur->next)
    {
        conn = LIST_ENTRY(cur, struct connection, entry);
        if (!strcmp(conn->name, name))
            return conn;
    }

    return NULL;
}

/* Must lock offline_lock when calling */
static struct offline_client *get_offline_client_by_name(char *name)
{
    struct list *bucket, *cur;
    struct offline_client *client;
    size_t hash;

    hash = hash_bytes(name, strlen(name) + 1) % offline_clients->size;
    bucket = &offline_clients->buckets[hash];

    for (cur = bucket->next; cur != bucket; cur = cur->next)
    {
        client = LIST_ENTRY(cur, struct offline_client, entry);
        if (!strcmp(client->name, name))
            return client;
    }

    return NULL;
}

/* Must lock topic->subs_lock */
static int exists_sub_by_name(struct topic *topic, char *name)
{
    struct list *bucket, *cur;
    struct subscriber *sub;
    size_t hash;

    hash = hash_bytes(name, strlen(name) + 1) % topic->subs->size;
    bucket = &topic->subs->buckets[hash];

    for (cur = bucket->next; cur != bucket; cur = cur->next)
    {
        sub = LIST_ENTRY(cur, struct subscriber, entry);
        if (!strcmp(sub->client_name, name))
            return 1;
    }

    return 0;
}

static void send_to_client_by_name(char *client_name, char *msg, size_t msg_len)
{
    struct connection *conn;

    pthread_mutex_lock(&online_lock);

    conn = get_client_by_name(client_name);
    if (!conn)
    {
        pthread_mutex_unlock(&online_lock);
        return;
    }

    reply_conn(conn, msg, msg_len);

    pthread_mutex_unlock(&online_lock);

    return;
}

static void enqueue_msg(char *msg, char *topic, char *sender)
{
    uint64_t cur_time = get_current_time();
    struct queued_msg *queued_msg;

    queued_msg = calloc(sizeof(*queued_msg), 1);
    if (!queued_msg)
    {
        perror("calloc");
        return;
    }

    list_init(&queued_msg->entry);
    queued_msg->time = cur_time;
    queued_msg->message = strdup(msg);
    if (!queued_msg->message)
    {
        perror("calloc");
        free(queued_msg);
        return;
    }
    queued_msg->topic = strdup(topic);
    if (!queued_msg->topic)
    {
        perror("calloc");
        free(queued_msg->message);
        free(queued_msg);
        return;
    }
    queued_msg->sender = strdup(sender);
    if (!queued_msg->sender)
    {
        perror("calloc");
        free(queued_msg->topic);
        free(queued_msg->message);
        free(queued_msg);
        return;
    }

    pthread_mutex_lock(&msg_queue_lock);

    list_add_tail(&msg_queue, &queued_msg->entry);

    pthread_mutex_unlock(&msg_queue_lock);

    return;
}

/* Must lock topic->subs_lock */
static void publish_msg(struct topic *topic, char **cmd, size_t num_toks)
{
    struct list *bucket, *cur;
    size_t len, i, msg_size;
    struct subscriber *sub;
    char msg[1024];

    assert(num_toks >= 4);

    msg_size = sizeof(msg) / sizeof(*msg);
    len = snprintf(msg, msg_size, "<%s, %s, %s, %s>", cmd[0], cmd[1], cmd[2], cmd[3]);
    if (len >= msg_size)
        len = msg_size - 1;

    assert(len > 1);

    for (i = 0; i < topic->subs->size; i++)
    {
        bucket = &topic->subs->buckets[i];
        for (cur = bucket->next; cur != bucket; cur = cur->next)
        {
            sub = LIST_ENTRY(cur, struct subscriber, entry);
            send_to_client_by_name(sub->client_name, msg, len);
        }
    }

    if (!hash_empty(offline_clients))
        enqueue_msg(cmd[3], cmd[2], cmd[0]);

    return;
}

static void connect_command(struct connection *conn, char **cmd_toks, size_t num_toks)
{
    static char *CONN_ACK = "<CONN_ACK>";
    struct connection *found;
    struct offline_client *offline_client;
    char *name;

    if (num_toks < 2)
        return; /* Specification does not demand we respond */

    name = strdup(cmd_toks[0]);
    if (name == NULL)
    {
        perror("strdup");
        return;
    }

    pthread_mutex_lock(&online_lock);

    found = get_client_by_name(name);
    if (found)
    {
        /* Only ACK if this is already connected */
        if (found == conn)
            reply_conn(conn, CONN_ACK, strlen(CONN_ACK));

        pthread_mutex_unlock(&online_lock);
        return;
    }

    /* Already connected, remove from list and add offline entry */
    if (conn->name)
    {
        add_offline_client(conn);
        list_remove(&conn->entry);
    }

    conn->name = name;
    hash_insert(online_clients, conn->name, strlen(conn->name) + 1, &conn->entry);

    pthread_mutex_unlock(&online_lock);

    pthread_mutex_lock(&offline_lock);

    offline_client = get_offline_client_by_name(conn->name);
    if (offline_client)
        reconnect_offline_client(offline_client, conn);

    pthread_mutex_unlock(&offline_lock);

    reply_conn(conn, CONN_ACK, strlen(CONN_ACK));

    return;
}

static void publish_command(struct connection *conn, char **cmd_toks, size_t num_toks)
{
    static char *NOT_FOUND = "<ERROR: Subject Not Found>";
    static char *NOT_SUBBED = "<ERROR: Not Subscribed>";
    char *name, *topic_name;
    struct topic *topic;

    if (num_toks < 4)
        return; /* Specification does not demand we respond */

    name = cmd_toks[0];
    topic_name = cmd_toks[2];
    topic = get_topic(topic_name);
    if (!topic)
    {
        reply_conn(conn, NOT_FOUND, strlen(NOT_FOUND));
        return;
    }

    pthread_mutex_lock(&topic->subs_lock);

    if (!exists_sub_by_name(topic, name))
    {
        reply_conn(conn, NOT_SUBBED, strlen(NOT_SUBBED));
        pthread_mutex_unlock(&topic->subs_lock);
        return;
    }

    publish_msg(topic, cmd_toks, num_toks);

    pthread_mutex_unlock(&topic->subs_lock);

    return;
}

static void subscribe_command(struct connection *conn, char **cmd_toks, size_t num_toks)
{
    static char *NOT_FOUND = "<ERROR: Subscription Failed - Subject Not Found>";
    static char *SUB_ACK = "<SUB_ACK>";
    struct subscription *topic_sub;
    struct subscriber *subscriber;
    char *name, *topic_name;
    struct topic *topic;

    if (num_toks < 3)
        return; /* Specification does not demand we respond */

    name = cmd_toks[0];
    topic_name = cmd_toks[2];
    topic = get_topic(topic_name);
    if (!topic)
    {
        reply_conn(conn, NOT_FOUND, strlen(NOT_FOUND));
        return;
    }

    subscriber = malloc(sizeof(*subscriber));
    if (!subscriber)
    {
        perror("malloc");
        return;
    }

    topic_sub = malloc(sizeof(*topic_sub));
    if (!topic_sub)
    {
        perror("malloc");
        free(subscriber);
        return;
    }

    list_init(&subscriber->entry);
    subscriber->client_name = strdup(name);
    if (!subscriber->client_name)
    {
        perror("malloc");
        free(subscriber);
        free(topic_sub);
        return;
    }

    list_init(&topic_sub->entry);
    topic_sub->topic_name = strdup(topic_name);
    if (!topic_sub->topic_name)
    {
        perror("malloc");
        free(subscriber->client_name);
        free(subscriber);
        free(topic_sub);
        return;
    }

    pthread_mutex_lock(&topic->subs_lock);

    if (exists_sub_by_name(topic, name))
    {
        /* Already subscribed, just ACK */
        reply_conn(conn, SUB_ACK, strlen(SUB_ACK));
        free(subscriber->client_name);
        free(subscriber);
        free(topic_sub->topic_name);
        free(topic_sub);
        pthread_mutex_unlock(&topic->subs_lock);
        return;
    }

    hash_insert(topic->subs, subscriber->client_name, strlen(subscriber->client_name) + 1, &subscriber->entry);

    pthread_mutex_unlock(&topic->subs_lock);

    list_add_tail(&conn->subbed_topics, &topic_sub->entry);

    reply_conn(conn, SUB_ACK, strlen(SUB_ACK));

    return;
}

static void disconnect_command(struct connection *conn, char **cmd_toks, size_t num_toks)
{
    static char *DISC_ACK = "<DISC_ACK>";
    int res;

    pthread_mutex_lock(&online_lock);

    list_remove(&conn->entry); /* In case of resending the CONNECT command */
    conn->closing = 1;

    pthread_mutex_unlock(&online_lock);

    res = send(conn->sock, DISC_ACK, strlen(DISC_ACK), 0);
    if (res == -1)
        perror("send"); /* Result doesn't matter, we are closing this */

    return;
}

static void parse_command(struct connection *conn, char *cmd, size_t len)
{
    static char *DELIM = ", ";
    size_t num_toks;
    char **toks;

    if (len < 2 || cmd[0] != '<' || cmd[len - 1] != '>')
        return; /* Requests must be surrounded by <> */

    num_toks = split_string(cmd + 1, len - 2, DELIM, &toks);
    if (!num_toks)
    {
        fprintf(stderr, "Unable to parse command, dropping\n");
        return;
    }

    if (!strcmp(toks[0], "DISC"))
    {
        disconnect_command(conn, toks, num_toks);
        goto out;
    }

    /* Remaining commands have the command in the second argument */
    if (num_toks < 2)
        goto out;

    if (!strcmp(toks[1], "PUB"))
        publish_command(conn, toks, num_toks);
    else if (!strcmp(toks[1], "SUB"))
        subscribe_command(conn, toks, num_toks);
    else if (!strcmp(toks[1], "CONN"))
        connect_command(conn, toks, num_toks);

out:
    free(toks);
}

static void *handle_connection(void *data)
{
    struct connection *conn = (struct connection *)data;
    static char buf[1024];
    ssize_t len;

    while (!conn->closing)
    {
        len = recv(conn->sock, buf, sizeof(buf), 0);
        if (len == -1 && (errno == ENOTCONN || errno == ECONNRESET))
        {
            perror("recv");
            conn->closing = 1;
        }
        else if (len > 0)
        {
            parse_command(conn, buf, len);
        }
        else
        {
            conn->closing = 1;
        }
    }

    close_connection(conn);

    return NULL;
}

static void init_topics()
{
    size_t i, num_topics = sizeof(DEFAULT_TOPIC_NAMES) / sizeof(*DEFAULT_TOPIC_NAMES);
    struct topic *topic;

    topics = hash_init(8);
    if (!topics)
        exit(EXIT_FAILURE);

    for (i = 0; i < num_topics; i++)
    {
        topic = malloc(sizeof(*topic));
        if (!topic)
        {
            perror("malloc");
            free(topics);
            exit(EXIT_FAILURE);
        }

        topic->name = strdup(DEFAULT_TOPIC_NAMES[i]);
        if (!topic->name)
        {
            perror("strdup");
            free(topics);
            free(topic);
            exit(EXIT_FAILURE);
        }

        pthread_mutex_init(&topic->subs_lock, NULL);
        list_init(&topic->entry);
        topic->subs = hash_init(16);
        if (!topic->subs)
        {
            perror("strdup");
            free(topic->name);
            free(topic);
            free(topics);
            exit(EXIT_FAILURE);
        }

        hash_insert(topics, topic->name, strlen(topic->name) + 1, &topic->entry);
    }
}

void start_server(unsigned short port)
{
    int sock, conn_sock, thread_ret;
    struct sockaddr_in addr;
    struct connection *conn;
    unsigned int addr_len;
    int enable = 1;

    online_clients = hash_init(16);
    if (!online_clients)
    {
        perror("hash_init");
        exit(EXIT_FAILURE);
    }

    init_topics();

    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    addr.sin_addr.s_addr = INADDR_ANY;
    addr_len = sizeof(addr);

    sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock == -1)
    {
        perror("socket");
        exit(EXIT_FAILURE);
    }

    if (setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &enable, sizeof(enable)))
    {
        perror("setsockopt");
        exit(EXIT_FAILURE);
    }

    if (bind(sock, (struct sockaddr *)&addr, addr_len) == -1)
    {
        perror("bind");
        exit(EXIT_FAILURE);
    }

    if (listen(sock, 5) == -1)
    {
        perror("listen");
        exit(EXIT_FAILURE);
    }

    for (;;)
    {
        conn_sock = accept(sock, (struct sockaddr *)&addr, &addr_len);
        if (conn_sock == -1)
        {
            perror("accept");
            continue;
        }

        conn = malloc(sizeof(*conn));
        conn->sock = conn_sock;
        conn->name = NULL;
        conn->closing = 0;
        list_init(&conn->entry);

        if ((thread_ret = pthread_create(&conn->thread, NULL, handle_connection, conn)))
        {
            free(conn);
            fprintf(stderr, "pthread_create: %d\n", thread_ret);
        }
    }

    close(sock);

    printf("Exiting\n");
}
