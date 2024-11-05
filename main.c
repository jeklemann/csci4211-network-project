#include <stdio.h>
#include <string.h>

#include "hash.h"

void test_hash(void)
{
    struct hash_table *table = hash_init(16);
    char *key1 = "foo", *val1 = "bar";
    char *key2 = "foobar", *val2 = "quux";
    char buf[256];
    int ret;

    ret = hash_insert(table, key1, strlen(key1) + 1, val1, strlen(val1) + 1);
    printf("expected: %d, got: %d\n", HASH_OK, ret);
    ret = hash_insert(table, key1, strlen(key1) + 1, val1, strlen(val1) + 1);
    printf("expected: %d, got: %d\n", HASH_ERR_KEY_EXISTS, ret);
    ret = hash_insert(table, key2, strlen(key2) + 1, val2, strlen(val2) + 1);
    printf("expected: %d, got: %d\n", HASH_OK, ret);

    ret = hash_get(table, key1, strlen(key1) + 1, buf, strlen(val1));
    printf("expected: %d, got: %d\n", HASH_ERR_BUFFER_TOO_SMALL, ret);
    ret = hash_get(table, key1, strlen(key1) + 1, buf, sizeof(buf));
    printf("expected: %d, got: %d\n", HASH_OK, ret);
    printf("expected value: %s, got value: %s\n", val1, buf);
    ret = hash_get(table, key2, strlen(key2) + 1, buf, sizeof(buf));
    printf("expected: %d, got: %d\n", HASH_OK, ret);
    printf("expected value: %s, got value: %s\n", val2, buf);
    ret = hash_get(table, val1, strlen(val1) + 1, buf, sizeof(buf));
    printf("expected: %d, got: %d\n", HASH_ERR_KEY_DOES_NOT_EXIST, ret);

    ret = hash_delete(table, key1, strlen(key1) + 1);
    printf("expected: %d, got: %d\n", HASH_OK, ret);
    ret = hash_delete(table, key1, strlen(key1) + 1);
    printf("expected: %d, got: %d\n", HASH_ERR_KEY_DOES_NOT_EXIST, ret);
    ret = hash_delete(table, key2, strlen(key2) + 1);
    printf("expected: %d, got: %d\n", HASH_OK, ret);

    hash_free(table);
}

int main(int argc, char **argv)
{
    test_hash();
    printf("Starting mqttd\n");
    return 0;
}
