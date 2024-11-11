#include <limits.h>
#include <stdio.h>

#include "server.h"

enum
{
    DEFAULT_PORT = 1883,
};

void usage()
{
    printf("Usage: mqttd [port]\n");
    exit(EXIT_FAILURE);
}

int main(int argc, char **argv)
{
    int p;

    if (argc > 2)
        usage();

    if (argc == 2)
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

    printf("Starting mqttd on port %hu\n", p);
    start_server(p);
    return 0;
}
