/*
 *
 * Copyright (c) wenford.li
 * wenford.li <26597925@qq.com>
 *
 */

#include <curl/curl.h>
#include <semaphore.h>
#include <pthread.h>
#include "threadpool.h"

typedef struct mulhttp {
	
	const char *url;

	char *headers;

	int timeout;

	int location;
	
	int is_range;
	
	double content_size;
	
	int flags;
	
	sem_t sem;
	
	long mem_size;
	
	char *memory;
	
	int thread_num;
	
	threadpool_t *thread__pool;
	
	pthread_mutex_t lock;
	
} mulhttp_t;

mulhttp_t *init_mulhttp(char *url);

void add_mul_header(mulhttp_t *mulhttp, char *header);

void set_mul_time_out(mulhttp_t *mulhttp, int tout);

void set_mul_follow_location(mulhttp_t *mulhttp, int flocation);

int start_download(mulhttp_t *mulhttp);

void stop_download(mulhttp_t *mulhttp);

void wait_content(mulhttp_t *mulhttp);

void write_file(mulhttp_t *mulhttp, char *path);
