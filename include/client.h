#include <netdb.h>
#include <pthread.h>

#include "hash.h"

struct client
{
    char *client_name;
    int sock;
    int closing;
};

enum
{
    SEND_OK,
    SEND_TIMEOUT,
    SEND_FAIL,
};

void start_client(struct addrinfo *addr);
