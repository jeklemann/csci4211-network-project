#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <netinet/ip.h>
#include <unistd.h>
#include <pthread.h>

#include "hash.h"
#include "server.h"

static struct list connections = { &connections, &connections };

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

static void parse_command(char *cmd, size_t len)
{
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

    for (int i = 0; i < num_toks; i++)
        printf("Token: %s\n", toks[i]);

    free(toks);
}

static void *handle_connection(void *data)
{
    struct connection *conn = (struct connection *)data;
    static char buf[64];
    size_t len;

    list_add_head(&connections, &conn->entry);

    len = recv(conn->sock, buf, sizeof(buf), 0);
    parse_command(buf, len);

    send(conn->sock, buf, len, 0);

    close_connection(conn);

    return NULL;
}

void start_server(unsigned short port)
{
    int sock, conn_sock, thread_ret;
    struct sockaddr_in addr;
    struct connection *conn;
    unsigned int addr_len;

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
        if ((thread_ret = pthread_create(&conn->thread, NULL, handle_connection, conn)))
        {
            free(conn);
            fprintf(stderr, "pthread_create: %d\n", thread_ret);
        }
    }

    close(sock);

    printf("Exiting\n");
}
