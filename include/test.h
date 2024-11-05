#include <stdio.h>
#include <stdarg.h>

#ifndef __MQTTD_TEST_H
#define __MQTTD_TEST_H

static int __test_failures = 0;

#define END_TEST() return __test_failures

static void run_test(int condition, char *msg, ...)
{
    va_list valist;
    va_start(valist, msg);
    if (!condition)
    {
        vfprintf(stderr, msg, valist);
        __test_failures++;
    }
    va_end(valist);
}

#endif /* __MQTTD_TEST_H */
