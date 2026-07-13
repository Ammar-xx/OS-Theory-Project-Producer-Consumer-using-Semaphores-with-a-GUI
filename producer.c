#include "producer.h"
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/time.h>

static long now_us(void) {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return tv.tv_sec * 1000000L + tv.tv_usec;
}

void *producer_thread(void *arg) {
    producer_arg_t *p = (producer_arg_t *)arg;

    for (int i = 0; i < p->items_to_produce; i++) {
        item_t item;
        item.id           = i;
        item.producer_id  = p->producer_id;
        item.timestamp_us = now_us();

        buffer_put(p->buf, item);

        printf("[P%d] produced item %d\n", p->producer_id, i);
        fflush(stdout);

        usleep((rand() % 100) * 1000);
    }

    printf("[P%d] done, made %d items\n", p->producer_id, p->items_to_produce);
    return NULL;
}
