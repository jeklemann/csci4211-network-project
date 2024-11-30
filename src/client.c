#include <assert.h>
#include <errno.h>
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

/* TODO: Handle receiving published data */

static struct client client;

static void prompt_name(char *buf, size_t buf_len)
{
    char *s;

    printf("Enter client name: ");
    s = fgets(buf, buf_len, stdin);
    while (s == NULL || strchr(buf, ','))
    {
        printf("Do not use commas in the name!\nEnter client name: ");
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

static int send_data(int sock, char *msg, size_t msg_len, char *response, size_t *resp_len)
{
    int res;

    res = send(sock, msg, msg_len, 0);
    if (res == -1 && (errno == ENOTCONN || errno == ECONNRESET))
    {
        perror("send");
        return SEND_FAIL;
    }

    res = recv(sock, response, *resp_len, 0);
    if (res > 0)
    {
        *resp_len = res;
        return SEND_OK;
    }
    else if (!res)
    {
        *resp_len = 0;
        return SEND_FAIL;
    }
    else if (errno == EAGAIN || errno == EWOULDBLOCK)
    {
        *resp_len = 0;
        return SEND_TIMEOUT;
    }

    perror("recv");
    *resp_len = 0;
    return SEND_FAIL;
}

static void select_name(struct client *client)
{
    static char *CONN_ACK = "<CONN_ACK>";
    char client_name[128], req_buf[1024], resp_buf[1024];
    size_t resp_buf_size, resp_len;
    int res = SEND_TIMEOUT;

    resp_len = resp_buf_size = sizeof(resp_buf) / sizeof(*resp_buf);

    while (res == SEND_TIMEOUT)
    {
        prompt_name(client_name, sizeof(client_name) / sizeof(*client_name));
        gen_conn_cmd(client_name, req_buf, sizeof(req_buf) / sizeof(*req_buf));
        res = send_data(client->sock, req_buf, strlen(req_buf), resp_buf, &resp_len);
        if (res == SEND_OK && !strncmp(resp_buf, CONN_ACK, strlen(CONN_ACK)))
            client->client_name = strdup(client_name);
        else if (res != SEND_TIMEOUT && res != SEND_OK)
            client->closing = 1;
        else
            printf("This name cannot be used. Pick another\n");
    }
}

static void handle_sub(struct client *client, char **toks, size_t num_toks)
{
    static char *SUB_ACK = "<SUB_ACK>";
    char req_buf[1024], resp_buf[1024];
    size_t resp_len;
    int res;

    if (num_toks < 2)
    {
        printf("Insufficient arguments. Usage: SUB <TOPIC>\n");
        return;
    }

    resp_len = sizeof(resp_buf) / sizeof(*resp_buf);

    gen_sub_cmd(client->client_name, toks[1], req_buf, sizeof(req_buf) / sizeof(*req_buf));
    res = send_data(client->sock, req_buf, strlen(req_buf), resp_buf, &resp_len);
    if (res == SEND_OK && !strncmp(resp_buf, SUB_ACK, strlen(SUB_ACK)))
    {
        printf("Subscription successful\n");
    }
    else if (res == SEND_OK)
    {
        printf("Subscription failed: ");
        fwrite(resp_buf, resp_len, 1, stdout);
        printf("\n");
    }
    else if (res != SEND_TIMEOUT && res != SEND_OK)
    {
        printf("Connection failed\n");
        client->closing = 1;
    }
    else
    {
        printf("Subscription failed\n");
    }
}

static void handle_pub(struct client *client, char **toks, size_t num_toks)
{
    if (num_toks < 3)
    {
        printf("Insufficient arguments. Usage: PUB <TOPIC> <MSG>\n");
        return;
    }

}

static void handle_disc(struct client *client, char **toks, size_t num_toks)
{
    char req_buf[1024], resp_buf[1024];
    size_t resp_len;

    resp_len = sizeof(resp_buf) / sizeof(*resp_buf);

    gen_disc_cmd(req_buf, sizeof(req_buf) / sizeof(*req_buf));
    send_data(client->sock, req_buf, strlen(req_buf), resp_buf, &resp_len);
    client->closing = 1;
}

void start_client(struct addrinfo *addr)
{
    static char *SUB = "SUB", *PUB = "PUB", *DISC = "DISC", *DELIM = " ";
    char client_name[128], cmd[1024], msg[760], resp_buf[1024];
    struct timeval timeout = {.tv_sec = 5, .tv_usec = 0};
    size_t resp_len, resp_buf_size, num_toks;
    struct addrinfo *aptr;
    char *s, **toks;
    int sock;

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
    setsockopt(client.sock, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));

    select_name(&client);

    printf("Connected as %s!\nCommands:\nSUB <TOPIC>\nPUB <TOPIC> <MESSAGE>\nDISC\n\n", client.client_name);
    while (!client.closing)
    {
        s = fgets(cmd, sizeof(cmd) / sizeof(*cmd), stdin);
        if (strchr(cmd, ','))
        {
            printf("Do not use commas in your message\n");
            continue;
        }

        s = strchr(s, '\n');
        if (s)
            *s = '\0';

        num_toks = split_string(cmd, strlen(cmd), DELIM, &toks);
        if (!num_toks)
        {
            printf("Unable to parse command.\n");
            continue;
        }

        if (!strcmp(toks[0], SUB))
            handle_sub(&client, toks, num_toks);
        else if (!strcmp(toks[0], DISC))
            handle_disc(&client, toks, num_toks);

        free(toks);
    }

    printf("Quitting\n");
    close(client.sock);
    free(client.client_name);
}
