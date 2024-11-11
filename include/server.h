#include <pthread.h>

#include "hash.h"

struct connection
{
    struct list entry;
    pthread_t thread;
    int sock;
    int closing; /* 1 for closing */
    char *name;
};

struct topic
{
    struct list entry;
    char *name;
    struct hash_table *subs;
    pthread_mutex_t subs_lock;
};

struct subscription
{
    struct list entry;
    char *client_name;
};

void start_server(unsigned short port);
