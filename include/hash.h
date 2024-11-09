#include <stddef.h>
#include <stdlib.h>

#ifndef __MQTTD_HASH_H
#define __MQTTD_HASH_H

#define LIST_ENTRY(elem, type, field) \
    ((type *)((char *)(elem) - offsetof(type, field)))

struct list
{
    struct list *prev;
    struct list *next;
};

void list_init(struct list *list);
void list_add_head(struct list *list, struct list *elem);
void list_add_tail(struct list *list, struct list *elem);
void list_remove(struct list *list);

enum {
    HASH_OK,
    HASH_ERR_NO_MEM,
    HASH_ERR_KEY_EXISTS,
    HASH_ERR_KEY_DOES_NOT_EXIST,
};
    
struct hash_table
{
    size_t size;
    struct list *buckets;
};

struct hash_entry
{
    struct list entry;
    void *key;
    size_t key_len;
    void *value;
    size_t value_len;
};

struct hash_table *hash_init(size_t size);
/* Freeing a non-empty table will result in memory leaks */
void hash_free(struct hash_table *table);
/* Key is copied into new memory, value is stored as is. */
int hash_insert(struct hash_table *table, void *key, size_t key_len, void *value, size_t value_len);
/* Value is written to value and its length to value_len */
int hash_get(struct hash_table *table, void *key, size_t key_len, void **value, size_t *value_len);
/* Key inside is freed */
int hash_delete(struct hash_table *table, void *key, size_t key_len);

#endif /* __MQTTD_HASH_H */
