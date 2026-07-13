#ifndef MONITOR_H
#define MONITOR_H

#include <pthread.h>
#include "buffer.h"

typedef struct {
    item_t *data;
    int size;
    int in;
    int out;
    int count;

    pthread_mutex_t lock;
    pthread_cond_t  not_empty;
    pthread_cond_t  not_full;
} mon_buffer_t;

int    mon_init(mon_buffer_t *b, int size);
void   mon_destroy(mon_buffer_t *b);
void   mon_put(mon_buffer_t *b, item_t item);
item_t mon_get(mon_buffer_t *b);

#endif
