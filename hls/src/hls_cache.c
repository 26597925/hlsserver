#include "mongoose.h"
#include "hls_cache.h"
#include <android/log.h>

#define CACHE_SIZE 4*1024*1024

#define LOG_TAG "HLS"
#define AKLOGE(fmt, ...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, fmt, ##__VA_ARGS__)
#define AKLOGI(fmt, ...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, fmt, ##__VA_ARGS__)

typedef struct buffer {
    char buffer[1024];
} buffer_t;

static void buffer_releaser(value_t value)
{
    buffer_t* buffer = (buffer_t*)value.value;
    free(buffer);
}

static void clear_m3u8_list(m3u8_file_t *m3u8_file)
{
	key_list_foreach(m3u8_file->m3u8_list, m3u8_node) 
	{
		int key = m3u8_file->m3u8_refresh_key - m3u8_file->m3u8_cache_key;
		if(key > m3u8_node->key && key_list_find_key(m3u8_file->m3u8_list, m3u8_node->key))
		{
			key_list_delete(m3u8_file->m3u8_list, m3u8_node->key);
		}
	}
}

static void delete_m3u8_list(m3u8_file_t *m3u8_file)
{
	key_list_foreach(m3u8_file->m3u8_list, m3u8_node) 
	{
		key_list_delete(m3u8_file->m3u8_list, m3u8_node->key);
	}
}

static void clear_ts_list(m3u8_file_t *m3u8_file, ts_file_t *ts_file)
{
	int key_number = (m3u8_file->m3u8_request_key - m3u8_file->m3u8_refresh_key)*ts_file->slice + ts_file->slice;//m3u8_file.m3u8_cache_key*ts_file.slice;
	int key_count = key_list_count(ts_file->ts_list);
	
	if(key_count < key_number)
		return;
	
	int key = ts_file->ts_request_key - key_number;
	key_list_foreach(ts_file->ts_list, ts_node) 
	{
		if(key > ts_node->key && key_list_find_key(ts_file->ts_list, ts_node->key))
		{
			value_t value;
			key_list_get(ts_file->ts_list, ts_node->key, &value);
			buffer_t *buffer = (buffer_t*)value.value;
			ts_buffer_t *ts_buffer;
			if(!mcache_kv_get(ts_file->ts_cache, buffer->buffer, &ts_buffer))
			{
				free(ts_buffer->p);
				free(ts_buffer);
			}
			
			mcache_kv_delete(ts_file->ts_cache, buffer->buffer);
			key_list_delete(ts_file->ts_list, ts_node->key);
		}
	}
}

static void delete_ts_list(ts_file_t *ts_file)
{
	key_list_foreach(ts_file->ts_list, ts_node) 
	{
		if(key_list_find_key(ts_file->ts_list, ts_node->key))
		{
			value_t value;
			key_list_get(ts_file->ts_list, ts_node->key, &value);
			buffer_t *buffer = (buffer_t*)value.value;
			ts_buffer_t *ts_buffer;
			if(!mcache_kv_get(ts_file->ts_cache, buffer->buffer, &ts_buffer))
			{
				free(ts_buffer->p);
				free(ts_buffer);
			}
			
			mcache_kv_delete(ts_file->ts_cache, buffer->buffer);
			key_list_delete(ts_file->ts_list, ts_node->key);
		}
	}
}

hls_cache_t *init_hls_cache()
{
	hls_cache_t *hls_cache = (hls_cache_t *)malloc(sizeof(hls_cache_t));
	m3u8_file_t *m3u8_file = (m3u8_file_t *)malloc(sizeof(m3u8_file_t));
	m3u8_file->m3u8_list = key_list_create(buffer_releaser);
	m3u8_file->m3u8_refresh_key = -1;
	m3u8_file->m3u8_request_key = -1;
	m3u8_file->m3u8_cache_key   = 0;
	hls_cache->m3u8_file		= m3u8_file;
	
	char err_buf[1024];
	ts_file_t *ts_file = (ts_file_t *)malloc(sizeof(ts_file_t));
	ts_file->ts_cache 		= mcache_kv_init(CACHE_SIZE, err_buf, sizeof(err_buf));
	ts_file->ts_list  		= key_list_create(buffer_releaser);
	ts_file->ts_request_key = -1;
	ts_file->slice			= 3;
	hls_cache->ts_file 		= ts_file;
	
	return hls_cache;
}

void add_m3u8_list(hls_cache_t *hls_cache, struct http_message *hm)
{
	m3u8_file_t  *m3u8_file = hls_cache->m3u8_file;
	if(m3u8_file->m3u8 == NULL){
		sprintf(m3u8_file->m3u8, "%s", hm->body.p);
	}
	
	buffer_t* buffer = (buffer_t*)malloc(sizeof(buffer_t));
	sprintf(buffer->buffer, "%s", hm->body.p);
	value_t value;
	value.value = buffer;
	
	m3u8_file->m3u8_request_key++;
	
	key_list_add(m3u8_file->m3u8_list, m3u8_file->m3u8_request_key, value);
	
	clear_m3u8_list(m3u8_file);
}

void add_ts_list(hls_cache_t *hls_cache, const char *uri , struct http_message *hm)
{
	ts_file_t *ts_file = hls_cache->ts_file;
	
	buffer_t* buffer = (buffer_t*)malloc(sizeof(buffer_t));
	sprintf(buffer->buffer, "%s", uri);
	
	value_t value;
	value.value = buffer;
	
	ts_file->ts_request_key++;
	key_list_add(ts_file->ts_list, ts_file->ts_request_key, value);
	
	if(!mcache_kv_find(ts_file->ts_cache, uri))
	{
		ts_buffer_t *get_buffer;
		mcache_kv_get(ts_file->ts_cache, buffer->buffer, &get_buffer);
		free(get_buffer->p);
		free(get_buffer);
		mcache_kv_delete(ts_file->ts_cache, uri);
	}
	
	ts_buffer_t* ts_buffer = (ts_buffer_t*)malloc(sizeof(ts_buffer_t*));
	ts_buffer->p = malloc(hm->body.len);
	memcpy(ts_buffer->p, hm->body.p, hm->body.len);
	ts_buffer->len = hm->body.len;
	mcache_kv_set(ts_file->ts_cache , uri, ts_buffer);
	
	m3u8_file_t  *m3u8_file = hls_cache->m3u8_file;
	
	clear_ts_list(m3u8_file, ts_file);

}

ts_buffer_t *get_ts_cache(hls_cache_t *hls_cache, const char *uri)
{
	ts_file_t *ts_file = hls_cache->ts_file;
	
	if(!mcache_kv_find(ts_file->ts_cache, uri))
	{
		ts_buffer_t *ts_buffer;
		mcache_kv_get(ts_file->ts_cache, uri, &ts_buffer);
		return ts_buffer;
	}
	
	return NULL;
}

int has_m3u8_key(hls_cache_t *hls_cache)
{
	m3u8_file_t  *m3u8_file = hls_cache->m3u8_file;
	if(m3u8_file->m3u8_refresh_key > -1){
		return 1;
	}
	return 0;
}

void refresh_m3u8_list(hls_cache_t *hls_cache)
{	
	m3u8_file_t  *m3u8_file = hls_cache->m3u8_file;
	
	if(m3u8_file->m3u8_request_key > m3u8_file->m3u8_cache_key 
		&& m3u8_file->m3u8_request_key > m3u8_file->m3u8_refresh_key)
	{
		m3u8_file->m3u8_refresh_key++;
		value_t value;
		if(key_list_get(m3u8_file->m3u8_list, m3u8_file->m3u8_refresh_key, &value) == 0) 
		{
			sprintf(m3u8_file->m3u8, "%s", ((buffer_t*)value.value)->buffer);
		}
	}
}

char *get_m3u8_cache(hls_cache_t *hls_cache)
{
	return hls_cache->m3u8_file->m3u8;
}

void destory_hls_cache(hls_cache_t *hls_cache)
{
	if(hls_cache == NULL)
		return;
	
	m3u8_file_t *m3u8_file = hls_cache->m3u8_file;
	ts_file_t *ts_file = hls_cache->ts_file;
	
	delete_m3u8_list(m3u8_file);
	key_list_destroy(m3u8_file->m3u8_list);
	free(m3u8_file);
	
	delete_ts_list(ts_file);
	mcache_kv_free(ts_file->ts_cache);
	key_list_destroy(ts_file->ts_list);
	free(ts_file);
	free(hls_cache);
}
