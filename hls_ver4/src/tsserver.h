#ifndef _TS_SERVER_H_
#define _TS_SERVER_H_
#include "queue.h"

void start_tsserver(char *port);

void put_queuelist(long long index,  queue_t *queue);

void stop_tsserver(void);

void reset_tsserver(void);

int has_cache(void);

void clear_tscache(void);

#endif