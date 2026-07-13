#ifndef FAIR_BUFFER_H
#define FAIR_BUFFER_H

#include <pthread.h>
#include "buffer.h"

typedef struct {
    item_t *data;
    int size;
    int in;
    int out;
    int count;

    pthread_mutex_t lock;
    pthread_cond_t  cond;

    /* Ticket counters: each producer/consumer takes a number on entry
     * and waits until it is being served. Guarantees FIFO order. */
    int next_p_ticket;
    int serving_p;
    int next_c_ticket;
    int serving_c;
} fair_buffer_t;

int    fair_init(fair_buffer_t *b, int size);
void   fair_destroy(fair_buffer_t *b);
void   fair_put(fair_buffer_t *b, item_t item);
item_t fair_get(fair_buffer_t *b);

#endif
