#include <stdio.h>
#include <string.h>

#include "hash.h"

void test_hash(void)
{
    char *key1 = "penis";
    char *val1 = "cum";
    char *key2 = "bruh";
    char *val2 = "real";
    char buf[256];

    struct hash_table *table = hash_init(16);
    printf("%d\n", hash_insert(table, key1, strlen(key1) + 1, val1, strlen(val1) + 1));
    printf("%d\n", hash_insert(table, key2, strlen(key2) + 1, val2, strlen(val2) + 1));
    printf("%d\n", hash_get(table, key1, strlen(key1) + 1, buf, strlen(val1)));
    printf("%d\n", hash_get(table, key1, strlen(key1) + 1, buf, 256));
    printf("%s\n", buf);
    printf("%d\n", hash_get(table, key2, strlen(key2) + 1, buf, 256));
    printf("%s\n", buf);

    printf("%d\n", hash_delete(table, key1, strlen(key1) + 1));
    printf("%d\n", hash_delete(table, key2, strlen(key2) + 1));
    hash_free(table);
}

int main(int argc, char **argv)
{
    printf("Starting mqttd\n");
    return 0;
}
