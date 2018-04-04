/*
 *
 * Copyright (c) wenford.li
 * wenford.li <26597925@qq.com>
 *
 */
 
#include "thread.h"
#include "threadpool.h"
#include "key_list.h"

#ifndef _SCACHE_H_
#define _SCACHE_H_
 
typedef struct scache {

	char *url;
	
	int add_key;
	
	thread_t *thread;
	
	pthread_mutex_t mutex;
	
	pthread_cond_t queue_not_empty;
	
	key_list_t *ts_list;
	
} scache_t;
 
scache_t *init_scache(char *url);

char *request_url(scache_t *scache);

struct memory_stru get_cache(scache_t *scache, char* tsid);

void destroy_cache(scache_t *scache);

#endif