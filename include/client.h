#include <netdb.h>
#include <pthread.h>

#include "hash.h"

#define BUF_SIZE 1024

struct client
{
    char *client_name;
    int sock;
    int closing;
    pthread_mutex_t lock;
};

struct cmd_listener
{
    size_t arg_pos;
    char cmd_name[16]; /* Max command length is 15, plus null terminator */
    uint64_t expire;
    int ready;

    char **toks;
    size_t num_toks;

    struct list entry;

    pthread_mutex_t lock;
    pthread_cond_t ready_cond;
};

enum
{
    SEND_OK,
    SEND_TIMEOUT,
    SEND_FAIL,
};

void start_client(struct addrinfo *addr);
