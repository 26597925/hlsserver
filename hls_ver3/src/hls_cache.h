/*
 *
 * Copyright (c) wenford.li
 * wenford.li <26597925@qq.com>
 *
 */

#ifndef _HLS_CACHE_H_
#define _HLS_CACHE_H_

#include "key_list.h"
#include "mcache.h"
#include "thread.h"
 
typedef struct m3u8_buffer {
	char 	*m3u8;
	size_t 	len;
} m3u8_buffer_t;

typedef struct ts_buffer{
	const char *p;
	size_t len;
} ts_buffer_t;

typedef struct m3u8_file {
	key_list_t		*m3u8_list;
	int 			m3u8_refresh_key;
	int 			m3u8_request_key;
	int 	m3u8_cache_key;
	m3u8_buffer_t   *m3u8_buffer;
} m3u8_file_t;

typedef struct ts_file {
	mcache_kv_t		*ts_cache;
	key_list_t 		*ts_list;
	int 			ts_request_key;
	int				slice;
} ts_file_t;

typedef struct hls_cache {
	thread_t *thread;
	m3u8_file_t  	*m3u8_file;
	ts_file_t    	*ts_file;
} hls_cache_t;

hls_cache_t *init_hls_cache(void);

void add_m3u8_list(hls_cache_t *hls_cache, struct http_message *hm);

void add_ts_list(hls_cache_t *hls_cache, struct http_message *hm);

ts_buffer_t *get_ts_cache(hls_cache_t *hls_cache, struct http_message *hm);

int has_m3u8_key(hls_cache_t *hls_cache);

m3u8_buffer_t *get_m3u8_cache(hls_cache_t *hls_cache);

void destory_hls_cache(hls_cache_t *hls_cache);

#endif