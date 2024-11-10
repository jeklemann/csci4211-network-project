#include <stddef.h>

#ifndef __MQTTD_UTILS_H
#define __MQTTD_UTILS_H

/* Allocated as one contiuous block which is freed in one operation.
 * Returns 0 if memory fails to allocate. Otherwise, data is set to out. */
size_t split_string(char *str, char delim, size_t len, char ***out);

#endif /* __MQTTD_UTILS_H */
