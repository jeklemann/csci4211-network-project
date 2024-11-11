#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
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

/* TODO: Queue messages, Phase 1 does not include this */

/* Connected clients */
static struct hash_table *online_clients;
pthread_mutex_t online_lock = PTHREAD_MUTEX_INITIALIZER;

static struct hash_table *topics;

static void close_connection(struct connection *conn)
{
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

    cur = bucket->next;
    while (cur != bucket)
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

    hash = hash_bytes(name, strlen(name) + 1) % topics->size;
    bucket = &topics->buckets[hash];

    cur = bucket->next;
    while (cur != bucket)
    {
        conn = LIST_ENTRY(cur, struct connection, entry);
        if (!strcmp(conn->name, name))
            return conn;
    }

    return NULL;
}

/* Must lock topic->subs_lock */
static int exists_sub_by_name(struct topic *topic, char *name)
{
    struct list *bucket, *cur;
    struct subscription *sub;
    size_t hash;

    hash = hash_bytes(name, strlen(name) + 1) % topics->size;
    bucket = &topics->buckets[hash];

    cur = bucket->next;
    while (cur != bucket)
    {
        sub = LIST_ENTRY(cur, struct subscription, entry);
        if (!strcmp(sub->client_name, name))
            return 1;
    }

    return 0;
}

static void reply_conn(struct connection *conn, char *msg, size_t msg_len)
{
    int res;

    res = send(conn->sock, msg, msg_len, 0);
    if (res == -1 && (errno == ENOTCONN || errno == ECONNRESET))
    {
        perror("send");
        conn->closing = 1;
    }
    else if (!res)
    {
        conn->closing = 1;
    }

    return;
}

static void connect_command(struct connection *conn, char **cmd_toks, size_t num_toks)
{
    static char *CONN_ACK = "CONN_ACK";
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

    if (get_client_by_name(name))
    {
        /* This name is taken, we have no defined error in the standard though */
        pthread_mutex_unlock(&online_lock);
        return;
    }

    list_remove(&conn->entry); /* In case of resending the CONN command */
    conn->name = name;
    hash_insert(online_clients, conn->name, strlen(conn->name) + 1, &conn->entry);

    pthread_mutex_unlock(&online_lock);

    reply_conn(conn, CONN_ACK, strlen(CONN_ACK));

    return;
}

static void publish_command(struct connection *conn, char **cmd_toks, size_t num_toks)
{
    printf("Pub\n");

    if (num_toks < 4)
        return; /* Specification does not demand we respond */
}

static void subscribe_command(struct connection *conn, char **cmd_toks, size_t num_toks)
{
    static char *NOT_FOUND = "<ERROR: Subscription Failed - Subject Not Found>";
    static char *SUB_ACK = "<SUB_ACK>";
    struct subscription *sub;
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

    pthread_mutex_lock(&topic->subs_lock);

    if (exists_sub_by_name(topic, name))
    {
        /* Already subscribed, just ACK */
        reply_conn(conn, SUB_ACK, strlen(SUB_ACK));
        pthread_mutex_unlock(&topic->subs_lock);
        return;
    }

    sub = malloc(sizeof(*sub));
    if (!sub)
    {
        perror("malloc");
        pthread_mutex_unlock(&topic->subs_lock);
        return;
    }

    list_init(&sub->entry);
    sub->client_name = strdup(name);
    if (!sub->client_name)
    {
        perror("malloc");
        free(sub);
        pthread_mutex_unlock(&topic->subs_lock);
        return;
    }

    hash_insert(topic->subs, sub->client_name, strlen(sub->client_name) + 1, &sub->entry);

    pthread_mutex_unlock(&topic->subs_lock);

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

    num_toks = split_string(cmd + 1, len - 2, DELIM, strlen(DELIM), &toks);
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
