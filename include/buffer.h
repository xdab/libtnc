#ifndef BUFFER_H
#define BUFFER_H

#include "common.h"
#include <stdbool.h>

typedef struct buffer
{
    unsigned char *data;
    int capacity;
    int size;
} buffer_t;

static inline bool buf_has_capacity_ge(const buffer_t *buf, int than)
{
    if (buf == NULL || buf->data == NULL)
        return 0;
    return buf->capacity >= than;
}

static inline bool buf_has_size_ge(const buffer_t *buf, int than)
{
    if (buf == NULL || buf->data == NULL)
        return 0;
    return buf->size >= than;
}

typedef struct float_buffer
{
    float *data;
    int capacity;
    int size;
} float_buffer_t;

static inline bool fbuf_has_capacity_ge(const float_buffer_t *buf, int than)
{
    if (buf == NULL || buf->data == NULL)
        return 0;
    return buf->capacity >= than;
}

static inline bool fbuf_has_size_ge(const float_buffer_t *buf, int than)
{
    if (buf == NULL || buf->data == NULL)
        return 0;
    return buf->size >= than;
}

#define assert_buffer_valid(buf)                                             \
    {                                                                        \
        nonnull(buf, "buf");                                                 \
        nonnull((buf)->data, "buf.data");                                    \
        _assert((buf)->size <= (buf)->capacity, "buf.size <= buf.capacity"); \
    }

#endif
