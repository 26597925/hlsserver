/*
 *
 * Copyright (c) wenford.li
 * wenford.li <26597925@qq.com>
 *
 */

#ifndef _HLS_H_
#define _HLS_H_

#include <semaphore.h>
#include "thread.h"

typedef struct transcode_task {
	
	thread_t *thread;
	
	char *task_name;
	
	sem_t *sem;
	
	int running;
	
	char **arguments;
	
} transcode_task_t;

transcode_task_t *start_transcode_hls(int port, char *url, sem_t *sem);

void stop_transcode_hls(transcode_task_t *transcode_task);

void destroy_transcode_hls(transcode_task_t *transcode_task);

#endif