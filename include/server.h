#include <stdint.h>
#include <pthread.h>

#include "hash.h"

struct connection
{
    struct list entry;
    pthread_t thread;
    int sock;
    int closing; /* 1 for closing */
    char *name;
    struct list subbed_topics;
};

struct offline_client
{
    struct list entry;
    uint64_t disc_time;
    char *name;
    struct list subs;
};

struct topic
{
    struct list entry;
    char *name;
    struct hash_table *subs;
    pthread_mutex_t subs_lock;
};

struct subscriber
{
    struct list entry;
    char *client_name;
};

struct subscription
{
    struct list entry;
    char *topic_name;
};

void start_server(unsigned short port);
