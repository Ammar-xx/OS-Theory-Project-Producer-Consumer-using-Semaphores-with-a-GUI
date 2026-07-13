#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <getopt.h>
#include <time.h>

#include "buffer.h"
#include "producer.h"
#include "consumer.h"

static void usage(const char *prog) {
    fprintf(stderr,
        "Usage: %s [-p N] [-c M] [-b B] [-i I]\n"
        "  -p N      number of producer threads     (default 2)\n"
        "  -c M      number of consumer threads     (default 2)\n"
        "  -b B      buffer capacity                (default 5)\n"
        "  -i I      items each producer makes      (default 10)\n"
        "  -h        show this help\n",
        prog);
}

int main(int argc, char *argv[]) {
    int n_producers = 2;
    int n_consumers = 2;
    int buf_size    = 5;
    int items_per_p = 10;

    int opt;
    while ((opt = getopt(argc, argv, "p:c:b:i:h")) != -1) {
        switch (opt) {
            case 'p': n_producers = atoi(optarg); break;
            case 'c': n_consumers = atoi(optarg); break;
            case 'b': buf_size    = atoi(optarg); break;
            case 'i': items_per_p = atoi(optarg); break;
            case 'h': usage(argv[0]); return 0;
            default:  usage(argv[0]); return 1;
        }
    }

    if (n_producers < 1 || n_consumers < 1 || buf_size < 1 || items_per_p < 1) {
        fprintf(stderr, "All values must be >= 1\n");
        return 1;
    }

    srand((unsigned)time(NULL));

    printf("=== Producer-Consumer (semaphore version) ===\n");
    printf("Producers: %d  Consumers: %d  Buffer: %d  Items/producer: %d\n",
           n_producers, n_consumers, buf_size, items_per_p);
    printf("Total items expected: %d\n\n", n_producers * items_per_p);

    buffer_t buf;
    if (buffer_init(&buf, buf_size) != 0) return 1;

    pthread_t      *prod_threads = malloc(n_producers * sizeof(pthread_t));
    pthread_t      *cons_threads = malloc(n_consumers * sizeof(pthread_t));
    producer_arg_t *prod_args    = malloc(n_producers * sizeof(producer_arg_t));
    consumer_arg_t *cons_args    = malloc(n_consumers * sizeof(consumer_arg_t));

    if (!prod_threads || !cons_threads || !prod_args || !cons_args) {
        perror("malloc");
        return 1;
    }

    for (int i = 0; i < n_consumers; i++) {
        cons_args[i].consumer_id    = i;
        cons_args[i].items_consumed = 0;
        cons_args[i].buf            = &buf;
        if (pthread_create(&cons_threads[i], NULL, consumer_thread, &cons_args[i]) != 0) {
            perror("pthread_create consumer");
            return 1;
        }
    }

    for (int i = 0; i < n_producers; i++) {
        prod_args[i].producer_id      = i;
        prod_args[i].items_to_produce = items_per_p;
        prod_args[i].buf              = &buf;
        if (pthread_create(&prod_threads[i], NULL, producer_thread, &prod_args[i]) != 0) {
            perror("pthread_create producer");
            return 1;
        }
    }

    for (int i = 0; i < n_producers; i++) pthread_join(prod_threads[i], NULL);

    printf("\n[main] all producers finished, sending stop signals to consumers\n\n");

    for (int i = 0; i < n_consumers; i++) {
        item_t pill = { .id = POISON_PILL, .producer_id = -1, .timestamp_us = 0 };
        buffer_put(&buf, pill);
    }

    int total_consumed = 0;
    for (int i = 0; i < n_consumers; i++) {
        pthread_join(cons_threads[i], NULL);
        total_consumed += cons_args[i].items_consumed;
    }

    int total_produced = n_producers * items_per_p;
    printf("\n=== Summary ===\n");
    printf("Total produced: %d\n", total_produced);
    printf("Total consumed: %d\n", total_consumed);
    for (int i = 0; i < n_consumers; i++)
        printf("  Consumer %d ate %d items\n", i, cons_args[i].items_consumed);

    if (total_consumed == total_produced) {
        printf("OK: all items accounted for, no items lost.\n");
    } else {
        printf("WARNING: counts don't match -- something is wrong with sync!\n");
    }

    buffer_destroy(&buf);
    free(prod_threads);
    free(cons_threads);
    free(prod_args);
    free(cons_args);

    return 0;
}
