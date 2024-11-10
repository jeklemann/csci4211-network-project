#include <stdint.h>
#include <string.h>
#include <assert.h>

#include "hash.h"

void list_init(struct list *list)
{
    list->prev = list->next = list; 
}

void list_add_head(struct list *list, struct list *elem)
{
    elem->next = list->next;
    elem->prev = list;
    
    list->next->prev = elem;
    list->next = elem;
}

void list_add_tail(struct list *list, struct list *elem)
{
    elem->next = list;
    elem->prev = list->prev;
    
    list->prev->next = elem;
    list->prev = elem;
}

void list_remove(struct list *elem)
{
    elem->prev->next = elem->next;
    elem->next->prev = elem->prev;
}

struct hash_table *hash_init(size_t size)
{
    struct hash_table *ret;
    size_t i;

    assert(size);

    ret = malloc(sizeof(*ret));
    if (!ret)
        return NULL;

    ret->size = size;
    ret->buckets = malloc(size * sizeof(*ret->buckets));
    for (i = 0; i < size; i++)
        list_init(&ret->buckets[i]);

    return ret;
}

void hash_free(struct hash_table *table)
{
    free(table->buckets);
    free(table);
}

uint64_t hash_bytes(void *key, size_t len)
{
    static const uint64_t fnv_prime = 0x100000001b3;
    uint64_t hash = 0xcbf29ce484222325;
    uint8_t *bytes = key;
    size_t i;

    for (i = 0; i < len; i++)
        hash = (hash ^ bytes[i]) * fnv_prime;

    return hash;
}

int hash_insert(struct hash_table *table, void *key, size_t key_len, struct list *elem)
{
    size_t hash;
 
    assert(table);
    assert(key);
    assert(key_len);
    assert(elem);

    hash = hash_bytes(key, key_len) % table->size; 

    list_add_head(&table->buckets[hash], elem);
    return HASH_OK;
}

