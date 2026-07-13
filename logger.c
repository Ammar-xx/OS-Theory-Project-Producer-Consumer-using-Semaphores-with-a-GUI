#include "logger.h"
#include <stdio.h>
#include <stdarg.h>
#include <pthread.h>
#include <sys/time.h>

static FILE *g_log = NULL;
static pthread_mutex_t g_lock = PTHREAD_MUTEX_INITIALIZER;
static long g_start_us = 0;

static long now_us(void) {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return tv.tv_sec * 1000000L + tv.tv_usec;
}

int logger_init(const char *filename) {
    g_log = fopen(filename, "w");
    if (!g_log) return -1;
    g_start_us = now_us();
    fprintf(g_log, "# elapsed_us | event   | message\n");
    fflush(g_log);
    return 0;
}

void logger_close(void) {
    pthread_mutex_lock(&g_lock);
    if (g_log) {
        fclose(g_log);
        g_log = NULL;
    }
    pthread_mutex_unlock(&g_lock);
}

/* Safe to call even if logger not initialized -- becomes a no-op. */
void log_event(log_event_t e, const char *fmt, ...) {
    if (!g_log) return;

    pthread_mutex_lock(&g_lock);
    if (g_log) {
        long elapsed = now_us() - g_start_us;
        const char *names[] = { "INFO", "PRODUCE", "CONSUME", "SYSTEM" };
        fprintf(g_log, "%8ld | %-7s | ", elapsed, names[e]);

        va_list args;
        va_start(args, fmt);
        vfprintf(g_log, fmt, args);
        va_end(args);
        fprintf(g_log, "\n");
    }
    pthread_mutex_unlock(&g_lock);
}
