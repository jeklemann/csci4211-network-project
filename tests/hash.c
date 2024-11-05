#include <string.h>

#include "hash.h"
#include "test.h"

int main(void)
{
    struct hash_table *table = hash_init(16);
    char *key1 = "foo", *val1 = "bar";
    char *key2 = "foobar", *val2 = "quux";
    char buf[256];
    int ret;

    ret = hash_insert(table, key1, strlen(key1) + 1, val1, strlen(val1) + 1);
    run_test(ret == HASH_OK, "expected: %d, got: %d\n", HASH_OK, ret);
    ret = hash_insert(table, key1, strlen(key1) + 1, val1, strlen(val1) + 1);
    run_test(ret == HASH_ERR_KEY_EXISTS, "expected: %d, got: %d\n", HASH_ERR_KEY_EXISTS, ret);
    ret = hash_insert(table, key2, strlen(key2) + 1, val2, strlen(val2) + 1);
    run_test(ret == HASH_OK, "expected: %d, got: %d\n", HASH_OK, ret);

    ret = hash_get(table, key1, strlen(key1) + 1, buf, strlen(val1));
    run_test(ret == HASH_ERR_BUFFER_TOO_SMALL, "expected: %d, got: %d\n", HASH_ERR_BUFFER_TOO_SMALL, ret);
    ret = hash_get(table, key1, strlen(key1) + 1, buf, sizeof(buf));
    run_test(ret == HASH_OK, "expected: %d, got: %d\n", HASH_OK, ret);
    run_test(!strcmp(val1, buf), "expected: %s, got: %s\n", val1, buf);
    ret = hash_get(table, key2, strlen(key2) + 1, buf, sizeof(buf));
    run_test(ret == HASH_OK, "expected: %d, got: %d\n", HASH_OK, ret);
    run_test(!strcmp(val2, buf), "expected: %s, got: %s\n", val2, buf);
    ret = hash_get(table, val1, strlen(val1) + 1, buf, sizeof(buf));
    run_test(ret == HASH_ERR_KEY_DOES_NOT_EXIST, "expected: %d, got: %d\n", HASH_ERR_KEY_DOES_NOT_EXIST, ret);

    ret = hash_delete(table, key1, strlen(key1) + 1);
    run_test(ret == HASH_OK, "expected: %d, got: %d\n", HASH_OK, ret);
    ret = hash_delete(table, key1, strlen(key1) + 1);
    run_test(ret == HASH_ERR_KEY_DOES_NOT_EXIST, "expected: %d, got: %d\n", HASH_ERR_KEY_DOES_NOT_EXIST, ret);
    ret = hash_delete(table, key2, strlen(key2) + 1);
    run_test(ret == HASH_OK, "expected: %d, got: %d\n", HASH_OK, ret);

    hash_free(table);

    END_TEST();
}

