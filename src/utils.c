#include <stdlib.h>
#include <string.h>

#include "utils.h"

size_t split_string(char *str, char delim, size_t len, char ***out)
{
    size_t i, num_tok = 0;
    char *ptr, **ret;

    ptr = str;
    do
    {
        while(*ptr == delim && len > ptr - str)
            ptr++;

        if (len <= ptr - str)
            break;

        num_tok++;
        ptr = memchr(ptr, delim, len - (ptr - str));
    }
    while (ptr++); /* Advance the pointer 1 step past delim */

    if (!num_tok)
        return 0;

    /* Put the string after the pointers so it can be freed in one go */
    ret = malloc(num_tok * sizeof(*ret) + (len + 1) * sizeof(**ret));
    if (!ret)
        return 0;

    /* Copy string to after pointers and null terminate */
    ptr = (char *)&ret[num_tok];
    memcpy(ptr, str, len);
    ptr[len] = '\0';

    /* Null terminate other tokens */
    for (i = 0; i < num_tok; i++)
    {
        while(*ptr == delim)
            ptr++;

        ret[i] = ptr;
        ptr = strchr(ptr, delim);
        if (ptr)
            *ptr++ = '\0';
    }

    *out = ret;
    return num_tok;
}

