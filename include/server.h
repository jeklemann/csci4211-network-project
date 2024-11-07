#include <pthread.h>

#include "hash.h"

struct connection
{
    struct list entry;
    pthread_t thread;
    int sock;
};

void start_server(unsigned short port);
