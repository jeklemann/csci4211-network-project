#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <netinet/ip.h>
#include <unistd.h>
#include <pthread.h>

#include "hash.h"
#include "server.h"

/* Thread and socket information only */
static struct list connections = { &connections, &connections };

/* Connected clients */
static struct hash_table *connected_clients;

static void close_connection(struct connection *conn)
{
    close(conn->sock);
    list_remove(&conn->entry);
    free(conn);
}

/* Allocated as one contiuous block which is freed in one operation.
 * Returns 0 if memory fails to allocate. Otherwise, data is set to out. */
static size_t split_string(char *str, char delim, size_t len, char ***out)
{
    size_t i, num_tok = 0;
    char *ptr, **ret;

    ptr = str;
    do
    {
        while(*ptr == delim && len > ptr - str)
            ptr++;

        if (len <= ptr - str)
            break;

        num_tok++;
        ptr = memchr(ptr, delim, len - (ptr - str));
    }
    while (ptr++); /* Advance the pointer 1 step past delim */

    if (!num_tok)
        return 0;

    /* Put the string after the pointers so it can be freed in one go */
    ret = malloc(num_tok * sizeof(*ret) + (len + 1) * sizeof(**ret));
    if (!ret)
        return 0;

    /* Copy string to after pointers and null terminate */
    ptr = (char *)&ret[num_tok];
    memcpy(ptr, str, len);
    ptr[len] = '\0';

    /* Null terminate other tokens */
    for (i = 0; i < num_tok; i++)
    {
        while(*ptr == delim)
            ptr++;

        ret[i] = ptr;
        ptr = strchr(ptr, delim);
        if (ptr)
            *ptr++ = '\0';
    }

    *out = ret;
    return num_tok;
}

static void connect_command(struct connection *conn, char **cmd_toks, size_t num_toks)
{
    printf("Hello\n");

    if (num_toks < 2)
        return; /* Specification does not demand we respond */
}

static void publish_command(struct connection *conn, char **cmd_toks, size_t num_toks)
{
    printf("Pub\n");

    if (num_toks < 4)
        return; /* Specification does not demand we respond */
}

static void subscribe_command(struct connection *conn, char **cmd_toks, size_t num_toks)
{
    printf("Sub\n");

    if (num_toks < 3)
        return; /* Specification does not demand we respond */
}

static void disconnect_command(struct connection *conn, char **cmd_toks, size_t num_toks)
{
    printf("Bye\n");

    return;
}

static void parse_command(struct connection *conn, char *cmd, size_t len)
{
    static char *NOT_CONNECTED = "NOT_CONNECTED";
    size_t num_toks;
    char **toks;

    printf("Received command: ");
    fwrite(cmd, len, 1, stdout);
    printf("\n");

    num_toks = split_string(cmd, ' ', len, &toks);
    if (!num_toks)
    {
        fprintf(stderr, "Unable to parse command, dropping\n");
        return;
    }

    if (!strcmp(toks[0], "CONNECT"))
    {
        connect_command(conn, toks, num_toks);
        goto out;
    }

    if (!conn->name)
    {
        send(conn->sock, NOT_CONNECTED, strlen(NOT_CONNECTED), 0);
        goto out;
    }

    if (!strcmp(toks[0], "PUBLISH"))
        publish_command(conn, toks, num_toks);
    else if (!strcmp(toks[0], "SUBSCRIBE"))
        subscribe_command(conn, toks, num_toks);
    else if (!strcmp(toks[0], "DISCONNECT"))
        disconnect_command(conn, toks, num_toks);

out:
    free(toks);
}

static void *handle_connection(void *data)
{
    struct connection *conn = (struct connection *)data;
    static char buf[1024];
    size_t len;

    /* TODO: Handle connection dropping */
    len = recv(conn->sock, buf, sizeof(buf), 0);
    parse_command(conn, buf, len);

    close_connection(conn);

    return NULL;
}

void start_server(unsigned short port)
{
    int sock, conn_sock, thread_ret;
    struct sockaddr_in addr;
    struct connection *conn;
    unsigned int addr_len;

    connected_clients = hash_init(16);
    if (!connected_clients)
    {
        perror("hash_init");
        exit(EXIT_FAILURE);
    }

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
