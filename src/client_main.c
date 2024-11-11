#include <limits.h>
#include <stdio.h>

#include "client.h"

enum
{
    DEFAULT_PORT = 1883,
};

void usage()
{
    printf("Usage: mqttd [address] [port]\n");
    exit(EXIT_FAILURE);
}

int main(int argc, char **argv)
{
    int p;

    if (argc > 3)
        usage();

    if (argc > 1)
        printf("FIXME: Implement getaddrinfo call\n");

    if (argc == 3)
        p = atoi(argv[1]);
    else
        p = DEFAULT_PORT;

    if (!p)
    {
        printf("invalid port\n");
        usage();
    }

    if (p > USHRT_MAX)
    {
        printf("port is too high\n");
        usage();
    }

    if (p < 1024)
    {
        printf("port should not be below 1024\n");
        usage();
    }

    start_client();
    return 0;
}
