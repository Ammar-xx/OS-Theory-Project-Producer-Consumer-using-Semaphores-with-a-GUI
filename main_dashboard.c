#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <getopt.h>
#include <time.h>
#include <sys/time.h>

#include "buffer.h"
#include "dashboard.h"

typedef struct {
    int producer_id;
    int items_to_produce;
    buffer_t *buf;
} prod_arg_t;

typedef struct {
    int consumer_id;
    buffer_t *buf;
} cons_arg_t;

static long now_us(void) {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return tv.tv_sec * 1000000L + tv.tv_usec;
}

static void *producer_dash_thread(void *arg) {
    prod_arg_t *p = arg;
    for (int i = 0; i < p->items_to_produce; i++) {
        item_t item = { .id = i, .producer_id = p->producer_id, .timestamp_us = now_us() };
        buffer_put(p->buf, item);
        dash_log_produce(p->producer_id, i);
        usleep((rand() % 200) * 1000);   /* slower than CLI version so the UI is watchable */
    }
    return NULL;
}

static void *consumer_dash_thread(void *arg) {
    cons_arg_t *c = arg;
    while (1) {
        item_t item = buffer_get(c->buf);
        if (item.id == POISON_PILL) break;
        dash_log_consume(c->consumer_id, item.id, item.producer_id);
        usleep((rand() % 250) * 1000);
    }
    return NULL;
}

static void usage(const char *prog) {
    fprintf(stderr,
        "Usage: %s [-p N] [-c M] [-b B] [-i I]\n"
        "  -p N      number of producer threads     (default 2)\n"
        "  -c M      number of consumer threads     (default 2)\n"
        "  -b B      buffer capacity                (default 5)\n"
        "  -i I      items each producer makes      (default 10)\n",
        prog);
}

int main(int argc, char *argv[]) {
    int n_p = 2, n_c = 2, buf_size = 5, items_per_p = 10;

    int opt;
    while ((opt = getopt(argc, argv, "p:c:b:i:h")) != -1) {
        switch (opt) {
            case 'p': n_p          = atoi(optarg); break;
            case 'c': n_c          = atoi(optarg); break;
            case 'b': buf_size     = atoi(optarg); break;
            case 'i': items_per_p  = atoi(optarg); break;
            case 'h': usage(argv[0]); return 0;
            default:  usage(argv[0]); return 1;
        }
    }

    if (n_p < 1 || n_c < 1 || buf_size < 1 || items_per_p < 1) {
        fprintf(stderr, "All values must be >= 1\n");
        return 1;
    }

    srand((unsigned)time(NULL));

    buffer_t buf;
    if (buffer_init(&buf, buf_size) != 0) return 1;

    int total = n_p * items_per_p;
    if (dash_init(&buf, n_p, n_c, total) != 0) {
        buffer_destroy(&buf);
        return 1;
    }

    dash_log_system("Started");

    pthread_t  *prod_t = malloc(n_p * sizeof(pthread_t));
    pthread_t  *cons_t = malloc(n_c * sizeof(pthread_t));
    prod_arg_t *p_args = malloc(n_p * sizeof(prod_arg_t));
    cons_arg_t *c_args = malloc(n_c * sizeof(cons_arg_t));

    for (int i = 0; i < n_c; i++) {
        c_args[i].consumer_id = i; c_args[i].buf = &buf;
        pthread_create(&cons_t[i], NULL, consumer_dash_thread, &c_args[i]);
    }
    for (int i = 0; i < n_p; i++) {
        p_args[i].producer_id = i;
        p_args[i].items_to_produce = items_per_p;
        p_args[i].buf = &buf;
        pthread_create(&prod_t[i], NULL, producer_dash_thread, &p_args[i]);
    }

    for (int i = 0; i < n_p; i++) pthread_join(prod_t[i], NULL);

    dash_log_system("All producers done, sending poison pills");

    for (int i = 0; i < n_c; i++) {
        item_t pill = { .id = POISON_PILL, .producer_id = -1, .timestamp_us = 0 };
        buffer_put(&buf, pill);
    }

    for (int i = 0; i < n_c; i++) pthread_join(cons_t[i], NULL);

    dash_log_system("Demo complete -- press 'q' to exit");

    dash_wait_for_quit();
    dash_close();

    buffer_destroy(&buf);
    free(prod_t); free(cons_t); free(p_args); free(c_args);
    return 0;
}
