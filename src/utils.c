#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "utils.h"

char *strnstr(char *haystack, char *needle, size_t haystack_len, size_t needle_len)
{
    assert(haystack_len);
    assert(needle_len);

    size_t h = haystack_len - needle_len + 1;
    while (h--)
    {
        if (!strncmp(haystack, needle, needle_len))
            return haystack;

        haystack++;
    }

    return NULL;
}

size_t split_string(char *str, size_t str_len, char *delim, size_t delim_len, char ***out)
{
    size_t i, num_tok = 0;
    char *start, *ptr, **ret;

    ptr = str;
    do
    {
        while(str_len > ptr - str + delim_len - 1 && !strncmp(ptr, delim, delim_len))
            ptr += delim_len;

        if (str_len <= ptr - str)
            break;

        num_tok++;
        ptr = strnstr(ptr, delim, str_len - (ptr - str), delim_len);
    }
    while (ptr);

    if (!num_tok)
        return 0;

    /* Put the string after the pointers so it can be freed in one go */
    ret = malloc(num_tok * sizeof(*ret) + (str_len + 1) * sizeof(**ret));
    if (!ret)
        return 0;

    /* Copy string to after pointers and null terminate */
    start = ptr = (char *)&ret[num_tok];
    memcpy(ptr, str, str_len);
    ptr[str_len] = '\0';

    /* Null terminate other tokens */
    for (i = 0; i < num_tok; i++)
    {
        while(!strncmp(ptr, delim, delim_len))
            ptr += delim_len;

        ret[i] = ptr;
        ptr = strnstr(ptr, delim, str_len - (ptr - start), delim_len);
        if (ptr)
        {
            *ptr = '\0';
            ptr += delim_len;
        }
    }

    *out = ret;
    return num_tok;
}

