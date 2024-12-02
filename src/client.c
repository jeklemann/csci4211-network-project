#include <assert.h>
#include <errno.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <netdb.h>
#include <unistd.h>

#include "client.h"
#include "hash.h"
#include "utils.h"

static struct client client;

static struct list recv_listeners;
pthread_mutex_t recv_listeners_lock = PTHREAD_MUTEX_INITIALIZER;

static size_t wait_for_cmd(struct cmd_listener *listener, char ***tokens)
{
    pthread_mutex_lock(&listener->lock);
    while (!listener->ready)
        pthread_cond_wait(&listener->ready_cond, &listener->lock);
    pthread_mutex_unlock(&listener->lock);

    if (tokens)
        *tokens = listener->toks;
    return listener->num_toks;
}

static struct cmd_listener *add_cmd_listener(char *cmd, size_t arg_pos)
{
    struct cmd_listener *listener;

    listener = calloc(sizeof(*listener), 1);
    if (!listener)
    {
        perror("calloc");
        return NULL;
    }

    listener->arg_pos = arg_pos;
    strcpy(listener->cmd_name, cmd);
    listener->expire = get_current_time() + 5;
    listener->ready = 0;
    listener->toks = NULL;

    list_init(&listener->entry);
    pthread_mutex_init(&listener->lock, NULL);
    pthread_cond_init(&listener->ready_cond, NULL);

    pthread_mutex_lock(&recv_listeners_lock);
    list_add_tail(&recv_listeners, &listener->entry);
    pthread_mutex_unlock(&recv_listeners_lock);

    return listener;
}

static void remove_cmd_listener(struct cmd_listener *listener)
{
    pthread_mutex_lock(&recv_listeners_lock);
    list_remove(&listener->entry);
    free(listener);
    pthread_mutex_unlock(&recv_listeners_lock);
}

static void remove_stale_listeners(void)
{
    uint64_t cur_time = get_current_time();
    struct list *entry, *entry_next;
    struct cmd_listener *listener;

    pthread_mutex_lock(&recv_listeners_lock);
    for (entry = recv_listeners.next, entry_next = entry->next;
         entry != &recv_listeners;
         entry = entry_next, entry_next = entry_next->next)
    {
        listener = LIST_ENTRY(entry, struct cmd_listener, entry);
        if (listener->expire > cur_time)
            break;

        list_remove(entry);
        pthread_mutex_lock(&listener->lock);
        listener->num_toks = 0;
        listener->ready = 1;
        pthread_cond_signal(&listener->ready_cond);
        pthread_mutex_unlock(&listener->lock);
    }
    pthread_mutex_unlock(&recv_listeners_lock);
}

/* Returns 1 if sent to a listener, 0 if not */
static int send_to_listener(char **tokens, size_t num_toks)
{
    struct list *entry, *entry_next;
    struct cmd_listener *listener;

    pthread_mutex_lock(&recv_listeners_lock);
    for (entry = recv_listeners.next, entry_next = entry->next;
         entry != &recv_listeners;
         entry = entry_next, entry_next = entry_next->next)
    {
        listener = LIST_ENTRY(entry, struct cmd_listener, entry);
        if (listener->arg_pos >= num_toks)
            continue;

        if (strcmp(tokens[listener->arg_pos], listener->cmd_name))
            continue;

        list_remove(entry);
        pthread_mutex_lock(&listener->lock);
        listener->toks = tokens;
        listener->num_toks = num_toks;
        listener->ready = 1;
        pthread_cond_signal(&listener->ready_cond);
        pthread_mutex_unlock(&listener->lock);

        pthread_mutex_unlock(&recv_listeners_lock);
        return 1;
    }

    pthread_mutex_unlock(&recv_listeners_lock);
    return 0;
}

static uint64_t calc_net_timeout(void)
{
    struct cmd_listener *listener;
    struct list *entry = recv_listeners.next;

    /* If list is empty, default to 5 seconds */
    if (entry == &recv_listeners)
        return 5;

    listener = LIST_ENTRY(entry, struct cmd_listener, entry);

    return listener->expire - get_current_time() + 1;
}

static void *net_loop(void *arg)
{
    struct timeval timeout = {.tv_sec = 5, .tv_usec = 0}; /* Default 5 second timeout */
    struct client *client = (struct client *)arg;
    char buf[BUF_SIZE], **toks;
    size_t i, num_toks;
    int res, drop;

    while (!client->closing)
    {
        drop = 0;

        remove_stale_listeners();

        timeout.tv_sec = calc_net_timeout();
        setsockopt(client->sock, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));

        /* Peek for the end of the packet */
        res = recv(client->sock, buf, sizeof(buf), MSG_PEEK);
        if (res <= 0)
        {
            if (!res || (errno != EAGAIN && errno != EWOULDBLOCK))
            {
                pthread_mutex_lock(&client->lock);
                client->closing = 1;
                pthread_mutex_unlock(&client->lock);
            }
            continue;
        }

        /* Find end of packet marked by '>' */
        i = 0;
        while (i < res && buf[i] != '>')
            i++;

        /* If not found, drop but still read the data */
        if (i == BUF_SIZE)
            drop = 1;
        else
            i++;

        if (i <= 2)
            drop = 1;

        /* Commands must start with '<' */
        if (buf[0] != '<')
            drop = 1;

        /* Actually read out the data */
        res = recv(client->sock, buf, i, 0);
        if (res <= 0)
        {
            if (!res || (errno != EAGAIN && errno != EWOULDBLOCK))
            {
                pthread_mutex_lock(&client->lock);
                client->closing = 1;
                pthread_mutex_unlock(&client->lock);
            }
            continue;
        }

        if (drop)
            continue;

        num_toks = split_string(buf + 1, i - 2, ", ", &toks);
        if (!num_toks)
            continue;

        /*
         * Never forward PUB commands,
         * these should be printed out immediately
         */
        if (num_toks == 4 && !strcmp(toks[1], "PUB"))
        {
            printf("[%s] [%s]: %s\n", toks[0], toks[2], toks[3]);
            free(toks);
            continue;
        }

        res = send_to_listener(toks, num_toks);
        if (!res)
        {
            /* Errors will be returned as one token */
            if (num_toks > 0)
                fprintf(stderr, "Server: %s\n", toks[0]);
            free(toks);
        }
    }

    return NULL;
}


static void prompt_name(char *buf, size_t buf_len)
{
    char *s;

    printf("Enter client name: ");
    s = fgets(buf, buf_len, stdin);
    while (s == NULL || strchr(buf, ','))
    {
        if (s)
            printf("Do not use commas in the name!\nEnter client name: ");
        else
            clearerr(stdin);
        s = fgets(buf, buf_len, stdin);
    }

    s = strchr(s, '\n');
    if (s)
        *s = '\0';
}

static void gen_conn_cmd(char *name, char *req_buf, size_t req_len)
{
    assert(strlen(name) < 128);

    snprintf(req_buf, req_len, "<%s, CONN>", name);
}

static void gen_sub_cmd(char *name, char *subject, char *req_buf, size_t req_len)
{
    assert(strlen(name) < 128);
    assert(strlen(subject) < 128);

    snprintf(req_buf, req_len, "<%s, SUB, %s>", name, subject);
}

static void gen_pub_cmd(char *name, char *subject, char *msg, char *req_buf, size_t req_len)
{
    assert(strlen(name) < 128);
    assert(strlen(subject) < 128);
    assert(strlen(msg) < 760);

    snprintf(req_buf, req_len, "<%s, PUB, %s, %s>", name, subject, msg);
}

static void gen_disc_cmd(char *req_buf, size_t req_len)
{
    snprintf(req_buf, req_len, "<DISC>");
}

static int send_data(int sock, char *msg, size_t msg_len)
{
    int res;

    res = send(sock, msg, msg_len, 0);
    if (res > 0)
    {
        return SEND_OK;
    }
    else if (res == -1 && errno != EAGAIN && errno != EWOULDBLOCK)
    {
        perror("send");
        return SEND_FAIL;
    }
    else if (!res)
    {
        return SEND_FAIL;
    }
    else
    {
        return SEND_TIMEOUT;
    }
}

static void select_name(struct client *client)
{
    char client_name[128], req_buf[BUF_SIZE];
    struct cmd_listener *listener;
    int res = SEND_TIMEOUT;

    while (res == SEND_TIMEOUT && !client->closing)
    {
        prompt_name(client_name, sizeof(client_name) / sizeof(*client_name));
        gen_conn_cmd(client_name, req_buf, sizeof(req_buf) / sizeof(*req_buf));
        listener = add_cmd_listener("CONN_ACK", 0);
        if (!listener)
            exit(EXIT_FAILURE);

        res = send_data(client->sock, req_buf, strlen(req_buf));

        if (res == SEND_FAIL)
        {
            remove_cmd_listener(listener);
            client->closing = 1;
            continue;
        }
        else if (res == SEND_TIMEOUT)
        {
            printf("This name cannot be used. Pick another\n");
            remove_cmd_listener(listener);
            continue;
        }

        res = wait_for_cmd(listener, NULL);
        if (res)
        {
            pthread_mutex_lock(&client->lock);
            client->client_name = strdup(client_name);
            pthread_mutex_unlock(&client->lock);
            free(listener);
            return;
        }

        free(listener);
    }
}

static void handle_sub(struct client *client, char **toks, size_t num_toks)
{
    struct cmd_listener *listener;
    char req_buf[BUF_SIZE];
    int res;

    if (num_toks < 2)
    {
        printf("Insufficient arguments. Usage: SUB <TOPIC>\n");
        return;
    }

    gen_sub_cmd(client->client_name, toks[1], req_buf, sizeof(req_buf) / sizeof(*req_buf));
    listener = add_cmd_listener("SUB_ACK", 0);
    if (!listener)
        exit(EXIT_FAILURE);

    res = send_data(client->sock, req_buf, strlen(req_buf));
    if (res == SEND_FAIL)
    {
        remove_cmd_listener(listener);
        client->closing = 1;
        return;
    }
    else if (res == SEND_TIMEOUT)
    {
        printf("Subscription failed\n");
        remove_cmd_listener(listener);
        return;
    }

    if (wait_for_cmd(listener, NULL))
        printf("Subscription successful\n");
    else
        printf("Subscription failed\n");

    free(listener);
}

static void handle_pub(struct client *client, char **toks, size_t num_toks)
{
    char msg_buf[BUF_SIZE], req_buf[BUF_SIZE];
    size_t i;
    int res;

    if (num_toks < 3)
    {
        printf("Insufficient arguments. Usage: PUB <TOPIC> <MSG>\n");
        return;
    }

    for (i = 2; i < num_toks; i++)
    {
        strcat(msg_buf, toks[i]);
        if (i != num_toks - 1)
            strcat(msg_buf, " ");
    }

    gen_pub_cmd(client->client_name, toks[1], msg_buf, req_buf, sizeof(req_buf) / sizeof(*req_buf));
    res = send_data(client->sock, req_buf, strlen(req_buf));
    if (res == SEND_FAIL)
        client->closing = 1;
    else if (res == SEND_TIMEOUT)
        printf("Publish failed\n");
}

static void handle_disc(struct client *client, char **toks, size_t num_toks)
{
    char req_buf[BUF_SIZE];

    gen_disc_cmd(req_buf, sizeof(req_buf) / sizeof(*req_buf));
    send_data(client->sock, req_buf, strlen(req_buf));
    client->closing = 1;
}

void start_client(struct addrinfo *addr)
{
    static char *SUB = "SUB", *PUB = "PUB", *DISC = "DISC";
    char *s, **toks, cmd[BUF_SIZE];
    struct addrinfo *aptr;
    pthread_t net_thread;
    size_t num_toks;
    int sock, ret;

    for (aptr = addr; aptr != NULL; aptr = aptr->ai_next)
    {
        sock = socket(aptr->ai_family, aptr->ai_socktype, aptr->ai_protocol);

        if (sock == -1)
            continue;

        if (connect(sock, aptr->ai_addr, aptr->ai_addrlen) != -1)
            break;

        close(sock); /* Failed, close and try next */
    }

    freeaddrinfo(addr);

    if (aptr == NULL)
    {
        fprintf(stderr, "unable to connect\n");
        exit(EXIT_FAILURE);
    }

    printf("Starting mqttc\n");

    client.sock = sock;

    pthread_mutex_init(&client.lock, NULL);

    list_init(&recv_listeners);
    if ((ret = pthread_create(&net_thread, NULL, net_loop, &client)) != 0)
    {
        fprintf(stderr, "pthread: %s\n", strerror(ret));
        close(client.sock);
        free(client.client_name);
        exit(EXIT_FAILURE);
    }

    select_name(&client);

    printf("Connected as %s!\nCommands:\nSUB <TOPIC>\nPUB <TOPIC> <MESSAGE>\nDISC\n\n", client.client_name);
    while (!client.closing)
    {
        s = fgets(cmd, sizeof(cmd) / sizeof(*cmd), stdin);
        if (!s)
        {
            clearerr(stdin);
            continue;
        }

        if (strchr(cmd, ','))
        {
            printf("Do not use commas in your message\n");
            continue;
        }

        s = strchr(s, '\n');
        if (s)
            *s = '\0';

        num_toks = split_string(cmd, strlen(cmd), " ", &toks);
        if (!num_toks)
        {
            printf("Unable to parse command\n");
            continue;
        }

        if (!strcmp(toks[0], SUB))
            handle_sub(&client, toks, num_toks);
        else if (!strcmp(toks[0], PUB))
            handle_pub(&client, toks, num_toks);
        else if (!strcmp(toks[0], DISC))
            handle_disc(&client, toks, num_toks);
        else
            printf("Unknown command\n");

        free(toks);
    }

    printf("Quitting\n");
    close(client.sock);
    free(client.client_name);
    pthread_join(net_thread, NULL);

    return;
}
