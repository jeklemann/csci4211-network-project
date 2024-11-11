#include <pthread.h>

#include "hash.h"

struct client
{
    struct list entry;
    char *client_name;
};

void start_client(void);
