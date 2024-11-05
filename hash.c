#include <stdint.h>
#include <string.h>

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
    struct hash_table *ret = malloc(sizeof(*ret));
    size_t i;

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
        hash = (hash * fnv_prime) ^ bytes[i];

    return hash;
}

static struct hash_entry *hash_lookup(struct hash_table *table, void *key, size_t key_len)
{
    size_t bucket = hash_bytes(key, key_len) % table->size;
    struct list *cur = table->buckets[bucket].next;
    struct hash_entry *node;

    while (cur != &table->buckets[bucket])
    {
        node = LIST_ENTRY(cur, struct hash_entry, entry);
        if (node->key_len == key_len && !memcmp(node->key, key, key_len))
            return node;
        cur = cur->next;
    }

    return NULL;
}

int hash_insert(struct hash_table *table, void *key, size_t key_len, void *value, size_t value_len)
{
    struct hash_entry *entry;
    size_t hash;
 
    if (hash_lookup(table, key, key_len))
        return HASH_ERR_KEY_EXISTS;

    entry = malloc(sizeof(*entry));
    if (!entry)
        return HASH_ERR_NO_MEM;

    hash = hash_bytes(key, key_len) % table->size; 

    entry->key = malloc(key_len);
    if (!entry->key)
    {
        free(entry);
        return HASH_ERR_NO_MEM;
    }

    entry->value = malloc(value_len);
    if (!entry->value)
    {
        free(entry->key);
        free(entry);
        return HASH_ERR_NO_MEM;
    }

    memcpy(entry->key, key, key_len);
    memcpy(entry->value, value, value_len);
    entry->key_len = key_len;
    entry->value_len = value_len;

    list_add_head(&table->buckets[hash], &entry->entry);
    return HASH_OK;
}

int hash_get(struct hash_table *table, void *key, size_t key_len, void *value, size_t value_len)
{
    struct hash_entry *node;

    node = hash_lookup(table, key, key_len);
    
    if (!node)
        return HASH_ERR_KEY_DOES_NOT_EXIST;

    if (value_len < node->value_len)
        return HASH_ERR_BUFFER_TOO_SMALL;

    memcpy(value, node->value, node->value_len);
    return HASH_OK;
}

int hash_delete(struct hash_table *table, void *key, size_t key_len)
{
    struct hash_entry *node;

    node = hash_lookup(table, key, key_len);
    
    if (!node)
        return HASH_ERR_KEY_DOES_NOT_EXIST;

    list_remove(&node->entry);
    free(node->value);
    free(node->key);
    free(node);

    return HASH_OK;
}
