#ifndef BUFFER_H
#define BUFFER_H

#include <semaphore.h>
#include <pthread.h>

typedef struct {
    int id;
    int producer_id;
    long timestamp_us;
} item_t;

#define POISON_PILL (-1)

typedef struct {
    item_t *data;
    int size;
    int in;
    int out;

    sem_t empty;
    sem_t full;
    sem_t mutex;
} buffer_t;

int    buffer_init(buffer_t *buf, int size);
void   buffer_destroy(buffer_t *buf);
void   buffer_put(buffer_t *buf, item_t item);
item_t buffer_get(buffer_t *buf);

#endif
