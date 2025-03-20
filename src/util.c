#include <stdio.h>
#include "util.h"

uint round_up(uint n, int multiple)
{
    uint rem = n % multiple;
    if (rem == 0)
        return n + multiple;
    return (n + multiple - rem);
}

int strip_bytes(uchar *buf, int n, uchar *remove_set, int sn)
{
    if (n <= 0)
        return -1;

    uchar *start = buf, *end = (buf + n - 1);
    uchar map[256] = {0};

    for (int i = 0; i < sn; i++)
        map[remove_set[i]] = 1;

    while ((start < end) && (map[*start] == 1))
        start++;

    while ((start <= end) && map[*end] == 1)
        end--;

    n = end - start + 1;
    while (start <= end)
        *buf++ = *start++;

    return n;
}

int getword(uint *data, int datalen, uchar *word)
{
    int word_len = 0;
    for (int i = 0; i < datalen; i++)
    {
        if (*data == ' ' || *data == '\t' || *data == '\n' || *data == '\r' || *data == '\0')
        {
            word[word_len] = '\0';
            return word_len;
        }
        else
        {
            *word = *data;
            word += 1;
            word_len += 1;
        }
    }
    return word_len;
}