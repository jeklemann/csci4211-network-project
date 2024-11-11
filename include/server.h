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

void start_server(unsigned short port);
