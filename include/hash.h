#include <stddef.h>
#include <stdint.h>
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

struct hash_table
{
    size_t size;
    struct list *buckets;
};

struct hash_table *hash_init(size_t size);
/* Freeing a non-empty table will result in memory leaks */
void hash_free(struct hash_table *table);
void hash_insert(struct hash_table *table, void *key, size_t key_len, struct list *elem);
uint64_t hash_bytes(void *key, size_t len);

#endif /* __MQTTD_HASH_H */
