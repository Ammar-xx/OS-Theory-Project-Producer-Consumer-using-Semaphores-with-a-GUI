#include "monitor.h"
#include <stdlib.h>

int mon_init(mon_buffer_t *b, int size) {
    if (size <= 0) return -1;
    b->data = calloc(size, sizeof(item_t));
    if (!b->data) return -1;

    b->size = size;
    b->in = b->out = b->count = 0;

    pthread_mutex_init(&b->lock, NULL);
    pthread_cond_init(&b->not_empty, NULL);
    pthread_cond_init(&b->not_full, NULL);
    return 0;
}

void mon_destroy(mon_buffer_t *b) {
    pthread_mutex_destroy(&b->lock);
    pthread_cond_destroy(&b->not_empty);
    pthread_cond_destroy(&b->not_full);
    free(b->data);
}

void mon_put(mon_buffer_t *b, item_t item) {
    pthread_mutex_lock(&b->lock);

    /* Mesa semantics -- must recheck condition after wake, hence while not if */
    while (b->count == b->size)
        pthread_cond_wait(&b->not_full, &b->lock);

    b->data[b->in] = item;
    b->in = (b->in + 1) % b->size;
    b->count++;

    pthread_cond_signal(&b->not_empty);
    pthread_mutex_unlock(&b->lock);
}

item_t mon_get(mon_buffer_t *b) {
    pthread_mutex_lock(&b->lock);

    while (b->count == 0)
        pthread_cond_wait(&b->not_empty, &b->lock);

    item_t item = b->data[b->out];
    b->out = (b->out + 1) % b->size;
    b->count--;

    pthread_cond_signal(&b->not_full);
    pthread_mutex_unlock(&b->lock);
    return item;
}
