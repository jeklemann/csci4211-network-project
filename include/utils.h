#include <stddef.h>
#include <stdint.h>

#ifndef __MQTTD_UTILS_H
#define __MQTTD_UTILS_H

/* TODO: ARRAY_SIZE, store cmd buffer size (1024), hash table sizes (16 usually),
 * generally nuke magic numbers */

/* Allocated as one contiuous block which is freed in one operation.
 * delim must be null terminated
 * Returns 0 if memory fails to allocate. Otherwise, data is set to out. */
size_t split_string(char *str, size_t str_len, char *delim, char ***out);

uint64_t get_current_time(void);

#endif /* __MQTTD_UTILS_H */
