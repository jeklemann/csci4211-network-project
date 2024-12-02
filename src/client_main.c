#include <limits.h>
#include <netdb.h>
#include <stdio.h>
#include <sys/socket.h>
#include <string.h>

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
    struct addrinfo hints, *addr;
    char *node = "localhost", *service = "1883";
    int r;

    if (argc > 3)
        usage();

    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = 0;
    hints.ai_protocol = 0;
    if (argc > 1)
        node = argv[1];

    if (argc > 2)
        service = argv[2];

    r = getaddrinfo(node, service, &hints, &addr);
    if (r)
    {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(r));
        usage();
        exit(EXIT_FAILURE);
    }

    start_client(addr);
    return 0;
}
