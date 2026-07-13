#ifndef PRODUCER_H
#define PRODUCER_H

#include "buffer.h"

typedef struct {
    int producer_id;
    int items_to_produce;
    buffer_t *buf;
} producer_arg_t;

void *producer_thread(void *arg);

#endif
