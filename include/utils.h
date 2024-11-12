#include <stddef.h>

#ifndef __MQTTD_UTILS_H
#define __MQTTD_UTILS_H

/* TODO: ARRAY_SIZE, store cmd buffer size (1024), hash table sizes (16 usually),
 * generally nuke magic numbers */

char *strnstr(char *haystack, char *needle, size_t haystack_len, size_t needle_len);

/* Allocated as one contiuous block which is freed in one operation.
 * Returns 0 if memory fails to allocate. Otherwise, data is set to out. */
size_t split_string(char *str, size_t str_len, char *delim, size_t delim_len, char ***out);

#endif /* __MQTTD_UTILS_H */
