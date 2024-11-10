#include <string.h>

#include "hash.h"
#include "test.h"

struct item
{
    struct list entry;

    char *key;
    char *val;
};

static char *get_value(struct hash_table *table, void *key, size_t key_len)
{
    struct list *bucket, *cur;
    struct item *item;
    size_t hash;

    hash = hash_bytes(key, key_len) % table->size;
    bucket = &table->buckets[hash];

    cur = bucket->next;
    while (cur != bucket)
    {
        item = LIST_ENTRY(cur, struct item, entry);
        if (!strcmp(item->key, (char *)key))
            return item->val;
    }

    return NULL;
}

static struct item *del_item(struct hash_table *table, void *key, size_t key_len)
{
    struct list *bucket, *cur;
    struct item *item;
    size_t hash;

    hash = hash_bytes(key, key_len) % table->size;
    bucket = &table->buckets[hash];

    cur = bucket->next;
    while (cur != bucket)
    {
        item = LIST_ENTRY(cur, struct item, entry);
        if (!strcmp(item->key, (char *)key))
        {
            list_remove(&item->entry);
            return item;
        }
    }

    return NULL;
}

int main(void)
{
    struct hash_table *table = hash_init(16);
    struct item item1 = {
        .key = "foo",
        .val = "bar"
    };
    struct item item2 = {
        .key = "baz",
        .val = "qux"
    };
    struct item *item_ptr;
    char *buf;
    int ret;

    ret = hash_insert(table, item1.key, strlen(item1.key) + 1, &item1.entry);
    run_test(ret == HASH_OK, "expected: %d, got: %d\n", HASH_OK, ret);
    ret = hash_insert(table, item2.key, strlen(item2.key) + 1, &item2.entry);
    run_test(ret == HASH_OK, "expected: %d, got: %d\n", HASH_OK, ret);

    buf = get_value(table, item1.key, strlen(item1.key) + 1);
    run_test(buf == item1.val, "expected: %p, got: %p\n", &item1.val, buf);
    buf = get_value(table, item2.key, strlen(item2.key) + 1);
    run_test(buf == item2.val, "expected: %p, got: %p\n", &item2.val, buf);
    buf = get_value(table, item2.val, strlen(item2.val) + 1);
    run_test(!buf, "expected: %p, got: %p\n", NULL, buf);

    item_ptr = del_item(table, item1.key, strlen(item1.key) + 1);
    run_test(item_ptr == &item1, "expected: %p, got: %p\n", &item1, item_ptr);
    item_ptr = del_item(table, item1.key, strlen(item1.key) + 1);
    run_test(!item_ptr, "expected: %p, got: %p\n", NULL, item_ptr);
    item_ptr = del_item(table, item2.key, strlen(item2.key) + 1);
    run_test(item_ptr == &item2, "expected: %p, got: %p\n", &item2, item_ptr);

    hash_free(table);

    END_TEST();
}

