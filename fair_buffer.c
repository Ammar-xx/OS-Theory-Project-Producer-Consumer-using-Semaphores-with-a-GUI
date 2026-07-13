#include "fair_buffer.h"
#include <stdlib.h>

int fair_init(fair_buffer_t *b, int size) {
    if (size <= 0) return -1;
    b->data = calloc(size, sizeof(item_t));
    if (!b->data) return -1;

    b->size = size;
    b->in = b->out = b->count = 0;
    b->next_p_ticket = b->serving_p = 0;
    b->next_c_ticket = b->serving_c = 0;

    pthread_mutex_init(&b->lock, NULL);
    pthread_cond_init(&b->cond, NULL);
    return 0;
}

void fair_destroy(fair_buffer_t *b) {
    pthread_mutex_destroy(&b->lock);
    pthread_cond_destroy(&b->cond);
    free(b->data);
}

void fair_put(fair_buffer_t *b, item_t item) {
    pthread_mutex_lock(&b->lock);
    int my = b->next_p_ticket++;

    /* wait until it's my ticket AND there's space */
    while (my != b->serving_p || b->count == b->size)
        pthread_cond_wait(&b->cond, &b->lock);

    b->data[b->in] = item;
    b->in = (b->in + 1) % b->size;
    b->count++;
    b->serving_p++;

    pthread_cond_broadcast(&b->cond);
    pthread_mutex_unlock(&b->lock);
}

item_t fair_get(fair_buffer_t *b) {
    pthread_mutex_lock(&b->lock);
    int my = b->next_c_ticket++;

    while (my != b->serving_c || b->count == 0)
        pthread_cond_wait(&b->cond, &b->lock);

    item_t item = b->data[b->out];
    b->out = (b->out + 1) % b->size;
    b->count--;
    b->serving_c++;

    pthread_cond_broadcast(&b->cond);
    pthread_mutex_unlock(&b->lock);
    return item;
}
