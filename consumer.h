#ifndef CONSUMER_H
#define CONSUMER_H

#include "buffer.h"

typedef struct {
    int consumer_id;
    int items_consumed;
    buffer_t *buf;
} consumer_arg_t;

void *consumer_thread(void *arg);

#endif
