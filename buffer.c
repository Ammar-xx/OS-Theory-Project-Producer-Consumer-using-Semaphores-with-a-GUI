#include "buffer.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int buffer_init(buffer_t *buf, int size) {
    if (size <= 0) {
        fprintf(stderr, "buffer_init: size must be > 0\n");
        return -1;
    }

    buf->data = calloc(size, sizeof(item_t));
    if (!buf->data) {
        perror("calloc");
        return -1;
    }

    buf->size = size;
    buf->in   = 0;
    buf->out  = 0;

    if (sem_init(&buf->empty, 0, size) != 0) { perror("sem_init empty"); return -1; }
    if (sem_init(&buf->full,  0, 0)    != 0) { perror("sem_init full");  return -1; }
    if (sem_init(&buf->mutex, 0, 1)    != 0) { perror("sem_init mutex"); return -1; }

    return 0;
}

void buffer_destroy(buffer_t *buf) {
    sem_destroy(&buf->empty);
    sem_destroy(&buf->full);
    sem_destroy(&buf->mutex);
    free(buf->data);
    buf->data = NULL;
}

/* Counting semaphore (empty/full) MUST be acquired before the binary mutex
 * to avoid deadlock. */
void buffer_put(buffer_t *buf, item_t item) {
    sem_wait(&buf->empty);
    sem_wait(&buf->mutex);

    buf->data[buf->in] = item;
    buf->in = (buf->in + 1) % buf->size;

    sem_post(&buf->mutex);
    sem_post(&buf->full);
}

item_t buffer_get(buffer_t *buf) {
    item_t item;

    sem_wait(&buf->full);
    sem_wait(&buf->mutex);

    item = buf->data[buf->out];
    buf->out = (buf->out + 1) % buf->size;

    sem_post(&buf->mutex);
    sem_post(&buf->empty);

    return item;
}
