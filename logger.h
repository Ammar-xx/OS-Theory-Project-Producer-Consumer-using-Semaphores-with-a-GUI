#ifndef LOGGER_H
#define LOGGER_H

typedef enum {
    LOG_INFO,
    LOG_PRODUCE,
    LOG_CONSUME,
    LOG_SYSTEM
} log_event_t;

int  logger_init(const char *filename);
void logger_close(void);
void log_event(log_event_t e, const char *fmt, ...);

#endif
