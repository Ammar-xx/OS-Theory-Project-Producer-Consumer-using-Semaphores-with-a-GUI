#include "consumer.h"
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

void *consumer_thread(void *arg) {
    consumer_arg_t *c = (consumer_arg_t *)arg;
    c->items_consumed = 0;

    while (1) {
        item_t item = buffer_get(c->buf);
        if (item.id == POISON_PILL) break;

        c->items_consumed++;
        printf("  [C%d] consumed item %d from P%d\n",
               c->consumer_id, item.id, item.producer_id);
        fflush(stdout);

        usleep((rand() % 150) * 1000);
    }

    printf("  [C%d] done, consumed %d items\n", c->consumer_id, c->items_consumed);
    return NULL;
}
