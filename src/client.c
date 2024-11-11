#include <assert.h>
#include <stdio.h>
#include <sys/socket.h>
#include <netinet/ip.h>
#include <unistd.h>

#include "client.h"
#include "hash.h"
#include "utils.h"

static char *DEFAULT_TOPIC_NAMES[] = {
    "WEATHER",
    "NEWS",
};

void start_client(void)
{
    printf("Starting mqttc\n");
}
