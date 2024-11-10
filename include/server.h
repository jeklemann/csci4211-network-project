#include <pthread.h>

#include "hash.h"

struct connection
{
    struct list entry;
    pthread_t thread;
    int sock;

    char *name;
};

void start_server(unsigned short port);
