#include <stdio.h>

#include "server.h"

int main(int argc, char **argv)
{
    printf("Starting mqttd\n");
    start_server(8024);
    return 0;
}
