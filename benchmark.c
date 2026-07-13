#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <sys/time.h>
#include <math.h>

#include "buffer.h"
#include "monitor.h"
#include "fair_buffer.h"

typedef struct {
    const char *label;
    int n_p;
    int n_c;
    int buf_size;
    int items_per_p;
} cfg_t;

typedef struct {
    double time_ms;
    int total_consumed;
    double fairness_stddev;
} result_t;

/* ============ Semaphore variant ============ */

typedef struct { int id; int n; buffer_t *b; } sp_arg;
typedef struct { int id; int eaten; buffer_t *b; } sc_arg;

static void *sem_prod(void *a) {
    sp_arg *p = a;
    for (int i = 0; i < p->n; i++) {
        item_t it = { .id = i, .producer_id = p->id, .timestamp_us = 0 };
        buffer_put(p->b, it);
    }
    return NULL;
}

static void *sem_cons(void *a) {
    sc_arg *c = a;
    while (1) {
        item_t it = buffer_get(c->b);
        if (it.id == POISON_PILL) break;
        c->eaten++;
    }
    return NULL;
}

/* ============ Monitor variant ============ */

typedef struct { int id; int n; mon_buffer_t *b; } mp_arg;
typedef struct { int id; int eaten; mon_buffer_t *b; } mc_arg;

static void *mon_prod(void *a) {
    mp_arg *p = a;
    for (int i = 0; i < p->n; i++) {
        item_t it = { .id = i, .producer_id = p->id, .timestamp_us = 0 };
        mon_put(p->b, it);
    }
    return NULL;
}

static void *mon_cons(void *a) {
    mc_arg *c = a;
    while (1) {
        item_t it = mon_get(c->b);
        if (it.id == POISON_PILL) break;
        c->eaten++;
    }
    return NULL;
}

/* ============ Fair variant ============ */

typedef struct { int id; int n; fair_buffer_t *b; } fp_arg;
typedef struct { int id; int eaten; fair_buffer_t *b; } fc_arg;

static void *fair_prod(void *a) {
    fp_arg *p = a;
    for (int i = 0; i < p->n; i++) {
        item_t it = { .id = i, .producer_id = p->id, .timestamp_us = 0 };
        fair_put(p->b, it);
    }
    return NULL;
}

static void *fair_cons(void *a) {
    fc_arg *c = a;
    while (1) {
        item_t it = fair_get(c->b);
        if (it.id == POISON_PILL) break;
        c->eaten++;
    }
    return NULL;
}

/* ============ Helpers ============ */

static double elapsed_ms(struct timeval t0, struct timeval t1) {
    return (t1.tv_sec - t0.tv_sec) * 1000.0
         + (t1.tv_usec - t0.tv_usec) / 1000.0;
}

static double stddev(int *vals, int n) {
    if (n <= 0) return 0;
    double sum = 0;
    for (int i = 0; i < n; i++) sum += vals[i];
    double mean = sum / n;
    double var = 0;
    for (int i = 0; i < n; i++) {
        double d = vals[i] - mean;
        var += d * d;
    }
    return sqrt(var / n);
}

/* ============ Runners ============ */

static result_t run_sem(cfg_t cfg) {
    buffer_t b;
    buffer_init(&b, cfg.buf_size);

    pthread_t *pt = malloc(cfg.n_p * sizeof(pthread_t));
    pthread_t *ct = malloc(cfg.n_c * sizeof(pthread_t));
    sp_arg *pa = calloc(cfg.n_p, sizeof(sp_arg));
    sc_arg *ca = calloc(cfg.n_c, sizeof(sc_arg));

    struct timeval t0, t1;
    gettimeofday(&t0, NULL);

    for (int i = 0; i < cfg.n_c; i++) {
        ca[i].id = i; ca[i].b = &b;
        pthread_create(&ct[i], NULL, sem_cons, &ca[i]);
    }
    for (int i = 0; i < cfg.n_p; i++) {
        pa[i].id = i; pa[i].n = cfg.items_per_p; pa[i].b = &b;
        pthread_create(&pt[i], NULL, sem_prod, &pa[i]);
    }

    for (int i = 0; i < cfg.n_p; i++) pthread_join(pt[i], NULL);
    for (int i = 0; i < cfg.n_c; i++) {
        item_t pill = { .id = POISON_PILL, .producer_id = -1, .timestamp_us = 0 };
        buffer_put(&b, pill);
    }
    for (int i = 0; i < cfg.n_c; i++) pthread_join(ct[i], NULL);

    gettimeofday(&t1, NULL);

    int *counts = malloc(cfg.n_c * sizeof(int));
    int total = 0;
    for (int i = 0; i < cfg.n_c; i++) { counts[i] = ca[i].eaten; total += counts[i]; }

    result_t r = { elapsed_ms(t0, t1), total, stddev(counts, cfg.n_c) };

    free(pt); free(ct); free(pa); free(ca); free(counts);
    buffer_destroy(&b);
    return r;
}

static result_t run_mon(cfg_t cfg) {
    mon_buffer_t b;
    mon_init(&b, cfg.buf_size);

    pthread_t *pt = malloc(cfg.n_p * sizeof(pthread_t));
    pthread_t *ct = malloc(cfg.n_c * sizeof(pthread_t));
    mp_arg *pa = calloc(cfg.n_p, sizeof(mp_arg));
    mc_arg *ca = calloc(cfg.n_c, sizeof(mc_arg));

    struct timeval t0, t1;
    gettimeofday(&t0, NULL);

    for (int i = 0; i < cfg.n_c; i++) {
        ca[i].id = i; ca[i].b = &b;
        pthread_create(&ct[i], NULL, mon_cons, &ca[i]);
    }
    for (int i = 0; i < cfg.n_p; i++) {
        pa[i].id = i; pa[i].n = cfg.items_per_p; pa[i].b = &b;
        pthread_create(&pt[i], NULL, mon_prod, &pa[i]);
    }

    for (int i = 0; i < cfg.n_p; i++) pthread_join(pt[i], NULL);
    for (int i = 0; i < cfg.n_c; i++) {
        item_t pill = { .id = POISON_PILL, .producer_id = -1, .timestamp_us = 0 };
        mon_put(&b, pill);
    }
    for (int i = 0; i < cfg.n_c; i++) pthread_join(ct[i], NULL);

    gettimeofday(&t1, NULL);

    int *counts = malloc(cfg.n_c * sizeof(int));
    int total = 0;
    for (int i = 0; i < cfg.n_c; i++) { counts[i] = ca[i].eaten; total += counts[i]; }

    result_t r = { elapsed_ms(t0, t1), total, stddev(counts, cfg.n_c) };

    free(pt); free(ct); free(pa); free(ca); free(counts);
    mon_destroy(&b);
    return r;
}

static result_t run_fair(cfg_t cfg) {
    fair_buffer_t b;
    fair_init(&b, cfg.buf_size);

    pthread_t *pt = malloc(cfg.n_p * sizeof(pthread_t));
    pthread_t *ct = malloc(cfg.n_c * sizeof(pthread_t));
    fp_arg *pa = calloc(cfg.n_p, sizeof(fp_arg));
    fc_arg *ca = calloc(cfg.n_c, sizeof(fc_arg));

    struct timeval t0, t1;
    gettimeofday(&t0, NULL);

    for (int i = 0; i < cfg.n_c; i++) {
        ca[i].id = i; ca[i].b = &b;
        pthread_create(&ct[i], NULL, fair_cons, &ca[i]);
    }
    for (int i = 0; i < cfg.n_p; i++) {
        pa[i].id = i; pa[i].n = cfg.items_per_p; pa[i].b = &b;
        pthread_create(&pt[i], NULL, fair_prod, &pa[i]);
    }

    for (int i = 0; i < cfg.n_p; i++) pthread_join(pt[i], NULL);
    for (int i = 0; i < cfg.n_c; i++) {
        item_t pill = { .id = POISON_PILL, .producer_id = -1, .timestamp_us = 0 };
        fair_put(&b, pill);
    }
    for (int i = 0; i < cfg.n_c; i++) pthread_join(ct[i], NULL);

    gettimeofday(&t1, NULL);

    int *counts = malloc(cfg.n_c * sizeof(int));
    int total = 0;
    for (int i = 0; i < cfg.n_c; i++) { counts[i] = ca[i].eaten; total += counts[i]; }

    result_t r = { elapsed_ms(t0, t1), total, stddev(counts, cfg.n_c) };

    free(pt); free(ct); free(pa); free(ca); free(counts);
    fair_destroy(&b);
    return r;
}

/* ============ Main ============ */

int main(void) {
    cfg_t configs[] = {
        { "balanced_small",  2, 2,  5, 100 },
        { "balanced_med",    4, 4, 10, 100 },
        { "balanced_large",  8, 8, 20, 100 },
        { "tiny_buffer",     4, 4,  1, 100 },
        { "big_buffer",      4, 4, 50, 100 },
        { "consumer_heavy",  2, 8, 10, 100 },
        { "producer_heavy",  8, 2, 10, 100 },
    };
    int n_cfg = sizeof(configs) / sizeof(configs[0]);

    /* CSV header */
    printf("config,producers,consumers,buffer_size,items_per_producer,"
           "implementation,time_ms,total_consumed,fairness_stddev\n");

    /* Print human-readable progress to stderr so it doesn't pollute the CSV */
    fprintf(stderr, "Running %d configs x 3 implementations = %d benchmarks\n\n",
            n_cfg, n_cfg * 3);

    for (int i = 0; i < n_cfg; i++) {
        cfg_t c = configs[i];
        fprintf(stderr, "[%d/%d] %s (P=%d C=%d buf=%d items=%d)\n",
                i + 1, n_cfg, c.label, c.n_p, c.n_c, c.buf_size, c.items_per_p);

        result_t r;

        r = run_sem(c);
        printf("%s,%d,%d,%d,%d,semaphore,%.2f,%d,%.2f\n",
               c.label, c.n_p, c.n_c, c.buf_size, c.items_per_p,
               r.time_ms, r.total_consumed, r.fairness_stddev);
        fprintf(stderr, "    semaphore: %.2f ms, stddev=%.2f\n",
                r.time_ms, r.fairness_stddev);

        r = run_mon(c);
        printf("%s,%d,%d,%d,%d,monitor,%.2f,%d,%.2f\n",
               c.label, c.n_p, c.n_c, c.buf_size, c.items_per_p,
               r.time_ms, r.total_consumed, r.fairness_stddev);
        fprintf(stderr, "    monitor:   %.2f ms, stddev=%.2f\n",
                r.time_ms, r.fairness_stddev);

        r = run_fair(c);
        printf("%s,%d,%d,%d,%d,fair,%.2f,%d,%.2f\n",
               c.label, c.n_p, c.n_c, c.buf_size, c.items_per_p,
               r.time_ms, r.total_consumed, r.fairness_stddev);
        fprintf(stderr, "    fair:      %.2f ms, stddev=%.2f\n\n",
                r.time_ms, r.fairness_stddev);
    }

    fprintf(stderr, "Done. CSV written to stdout.\n");
    return 0;
}
