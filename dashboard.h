#ifndef DASHBOARD_H
#define DASHBOARD_H

#include "buffer.h"

int  dash_init(buffer_t *buf, int n_producers, int n_consumers, int total_items);
void dash_close(void);

void dash_log_produce(int p_id, int item_id);
void dash_log_consume(int c_id, int item_id, int from_p);
void dash_log_system(const char *msg);

void dash_wait_for_quit(void);

#endif
