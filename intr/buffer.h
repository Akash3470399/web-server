#ifndef BUFFER_H
#define BUFFER_H

#include "dtypes.h"

    typedef struct Buffer Buffer;

    typedef enum{
        Start_P,
        Current_P,
        End_P
    }RefPos;

    Buffer *buffer_new(uint bufsize);

    int buffer_copy(Buffer *dst, Buffer *src, uint offset, uint size);

    int buffer_append(Buffer *buf, uchar *data, uint size);

    int buffer_read(Buffer *buf, uchar *data, uint size);

    int buffer_read_till_delim(Buffer *buf, uchar **data, uchar delim);

    int buffer_seek(Buffer *buf, int byte_loc, RefPos pos);

    void buffer_destroy(Buffer *buf);

    uint buffer_datasize(Buffer *buf);

    void buffer_reset(Buffer *buf);

#endif