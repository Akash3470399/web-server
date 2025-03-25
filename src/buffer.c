#include <stdlib.h>
#include <string.h>

#include "consts.h"
#include "buffer.h"
#include "dtypes.h"
#include "util.h"

struct Buffer
{
    uchar *data;
    uint bufsize, datasize, read_pos;
};

Buffer *buffer_new(uint size)
{
    Buffer *buf;

    if ((buf = malloc(sizeof(Buffer))) == NULL)
        return NULL;

    if ((buf->data = malloc(size)) == NULL)
    {
        free(buf);
        return NULL;
    }

    buf->bufsize = size;
    buf->datasize = 0;
    buf->read_pos = 0;
    return buf;
}

int buffer_copy(Buffer *dst, Buffer *src, uint offset, uint size)
{
    uint eff_datasize = min(size, (src->datasize - offset));

    if (offset > src->datasize)
        return -1;

    return buffer_append(dst, src->data + offset, eff_datasize);
}

int buffer_append(Buffer *buf, uchar *data, uint size)
{
    uint eff_datasize, eff_bufsize;

    if (!buf || !data)
        return -1;

    void *mem = buf->data;

    eff_datasize = buf->datasize + size;
    if (eff_datasize > buf->bufsize)
    {
        eff_bufsize = round_up(eff_datasize, BUFFER_ROUNDUP_SIZE);
        if ((mem = realloc(buf->data, eff_bufsize)) == NULL)
            return -1;
    }

    buf->data = (uchar *)mem;
    memmove((buf->data + buf->datasize), data, size);
    buf->datasize = eff_datasize;
    return size;
}

int buffer_read(Buffer *buf, uchar *data, uint size)
{
    if (!buf || !data)
        return -1;

    size = min(size, (buf->datasize - buf->read_pos));
    memmove(data, (buf->data + buf->read_pos), size);
    buf->read_pos += size;
    return size;
}

// it dont include delim in return data
// appends data with '\0'
int buffer_read_till_delim(Buffer *buf, uchar **data, uchar delim)
{
    uchar *data_start, *data_end;
    int data_len;

    if (!buf || !data)
        return -1;

    data_start = (buf->data + buf->read_pos);
    data_end = memchr(data_start, delim, (buf->datasize - buf->read_pos));

    if (!data_end)
        return -1;

    data_len = data_end - data_start;
    if ((*data = calloc(data_len + 1, sizeof(uchar))) != NULL)
        memmove(*data, data_start, data_len);
    else
        return -1;

    buf->read_pos += data_len + 1; // 1 is for delim read
    return data_len;
}

int buffer_seek(Buffer *buf, int offset, RefPos pos)
{
    int eff_loc = -1;

    if (!buf)
        return -1;

    if ((pos == Start_P) && (offset > -1))
        eff_loc = offset;

    else if (pos == Current_P)
        eff_loc = buf->read_pos + offset;

    else if ((pos == End_P) && (offset < 1))
        eff_loc = buf->datasize + offset;

    if ((0 <= eff_loc) && (eff_loc < buf->datasize))
        buf->read_pos = eff_loc;

    return eff_loc;
}

void buffer_destroy(Buffer *buf)
{
    if (buf)
    {

        free(buf->data);
        free(buf);
    }
}

uint buffer_datasize(Buffer *buf)
{
    if (buf)
        return buf->datasize;
    return 0;
}

void buffer_reset(Buffer *buf)
{
    if(buf)
    {
        for(int i = 0; i < buf->bufsize; i++)
            buf->data[i] = 0;
        buf->datasize = 0;
        buf->read_pos = 0;
    }
}
