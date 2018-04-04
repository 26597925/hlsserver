#include <android/log.h>
#include "mongoose.h"
#include "hls_cache.h"

#define CACHE_SIZE 30*1024*1024

#define LOG_TAG "HLS"
#define AKLOGE(fmt, ...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, fmt, ##__VA_ARGS__)
#define AKLOGI(fmt, ...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, fmt, ##__VA_ARGS__)

typedef struct uri_buffer {
	char *uri;
} uri_buffer_t;

static void m3u8_releaser(value_t value)
{
	m3u8_buffer_t* m3u8_buffer = (m3u8_buffer_t*)value.value;
	free(m3u8_buffer->m3u8);
    free(m3u8_buffer);
}

static void uri_releaser(value_t value)
{
	uri_buffer_t* uri_buffer = (uri_buffer_t*)value.value;
	free(uri_buffer->uri);
    free(uri_buffer);
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

static void clear_ts_list(m3u8_file_t *m3u8_file, ts_file_t *ts_file)
{
	int key = m3u8_file->m3u8_refresh_key - (m3u8_file->m3u8_cache_key+2)*ts_file->slice;
	
	if(key < m3u8_file->m3u8_cache_key)
		return;
	
	key_list_foreach(ts_file->ts_list, ts_node) 
	{
		if(key > ts_node->key )
		{
			uri_buffer_t *uri_buffer = (uri_buffer_t*)ts_node->value.value;
			
			if(!mcache_kv_find(ts_file->ts_cache, uri_buffer->uri))
			{
				ts_buffer_t *ts_buffer;
				if(!mcache_kv_get(ts_file->ts_cache, uri_buffer->uri, &ts_buffer))
				{
					free(ts_buffer->p);
					free(ts_buffer);
				}
				mcache_kv_delete(ts_file->ts_cache, uri_buffer->uri);
			}
			
			key_list_delete(ts_file->ts_list, ts_node->key);
		}
	}
}

static void refresh_m3u8_list(hls_cache_t *hls_cache)
{	
	if(hls_cache == NULL)
		return;
	
	m3u8_file_t  *m3u8_file = hls_cache->m3u8_file;
	
	if(m3u8_file == NULL)
		return;
	
	if(m3u8_file->m3u8_request_key > m3u8_file->m3u8_refresh_key)
	{
		if(key_list_find_key(m3u8_file->m3u8_list, m3u8_file->m3u8_refresh_key))
		{
			value_t value;
			if(key_list_get(m3u8_file->m3u8_list, m3u8_file->m3u8_refresh_key, &value) == 0) 
			{
				m3u8_file->m3u8_buffer = (m3u8_buffer_t*)value.value;
				
				clear_m3u8_list(m3u8_file);
				ts_file_t *ts_file = hls_cache->ts_file;
				clear_ts_list(m3u8_file, ts_file);
				
				m3u8_file->m3u8_refresh_key++;
			}
		}
	}
}

static void delete_ts_list(ts_file_t *ts_file)
{
	key_list_foreach(ts_file->ts_list, ts_node) 
	{
		if(key_list_find_key(ts_file->ts_list, ts_node->key))
		{
			uri_buffer_t *uri_buffer = (uri_buffer_t*)ts_node->value.value;
			
			if(!mcache_kv_find(ts_file->ts_cache, uri_buffer->uri))
			{
				ts_buffer_t *ts_buffer;
				if(!mcache_kv_get(ts_file->ts_cache, uri_buffer->uri, &ts_buffer))
				{
					free(ts_buffer->p);
					free(ts_buffer);
				}
				mcache_kv_delete(ts_file->ts_cache, uri_buffer->uri);
			}
			
			key_list_delete(ts_file->ts_list, ts_node->key);
		}
	}
}

static void clear_hls_cache(hls_cache_t *hls_cache)
{
	m3u8_file_t *m3u8_file = hls_cache->m3u8_file;
	key_list_destroy(m3u8_file->m3u8_list);
	free(m3u8_file);
	
	ts_file_t *ts_file = hls_cache->ts_file;
	delete_ts_list(ts_file);
	mcache_kv_free(ts_file->ts_cache);
	key_list_destroy(ts_file->ts_list);
	free(ts_file);
	
	free(hls_cache);

}

//线程刷新
static void *task_loop(void *arg) 
{	
	
	hls_cache_t *hls_cache = (hls_cache_t *)arg;
	
	while(!thread_is_interrupted(hls_cache->thread))
	{
		/*struct timeval tv;
		tv.tv_sec = 1;
		tv.tv_usec = 0;
		select(0, NULL, NULL, NULL, &tv);*/
		refresh_m3u8_list(hls_cache);
	}
	
	clear_hls_cache(hls_cache);
	
}

hls_cache_t *init_hls_cache()
{
	
	hls_cache_t *hls_cache = (hls_cache_t *)malloc(sizeof(hls_cache_t));
	hls_cache->thread = thread_create();
	
	m3u8_file_t *m3u8_file = (m3u8_file_t *)malloc(sizeof(m3u8_file_t));
	m3u8_file->m3u8_list = key_list_create(m3u8_releaser);
	m3u8_file->m3u8_request_key = -1;
	m3u8_file->m3u8_refresh_key = 0;
	m3u8_file->m3u8_cache_key   = 0;
	hls_cache->m3u8_file		= m3u8_file;
	
	char err_buf[1024];
	ts_file_t *ts_file = (ts_file_t *)malloc(sizeof(ts_file_t));
	ts_file->ts_cache 		= mcache_kv_init(CACHE_SIZE, err_buf, sizeof(err_buf));
	ts_file->ts_list  		= key_list_create(uri_releaser);
	ts_file->ts_request_key = -1;
	ts_file->slice			= 3;
	hls_cache->ts_file 		= ts_file;
	
	thread_start(hls_cache->thread, task_loop, hls_cache);
	
	return hls_cache;
}

void add_m3u8_list(hls_cache_t *hls_cache, struct http_message *hm)
{
	if(hls_cache == NULL)
		return;
	
	m3u8_file_t  *m3u8_file = hls_cache->m3u8_file;
	
	m3u8_buffer_t* m3u8_buffer = (m3u8_buffer_t*)malloc(sizeof(m3u8_buffer_t));
	m3u8_buffer->m3u8 = malloc(hm->body.len);
	memcpy(m3u8_buffer->m3u8, hm->body.p, hm->body.len);
	m3u8_buffer->len = hm->body.len;
	
	value_t value;
	value.value = m3u8_buffer;
	
	m3u8_file->m3u8_request_key++;
	
	key_list_add(m3u8_file->m3u8_list, m3u8_file->m3u8_request_key, value);

}

void add_ts_list(hls_cache_t *hls_cache, struct http_message *hm)
{
	if(hls_cache == NULL)
		return;
	
	ts_file_t *ts_file = hls_cache->ts_file;
	char uri[100] = {0};
	snprintf(uri, sizeof(uri), "%.*s", (int) hm->uri.len, hm->uri.p);
	
	if(!mcache_kv_find(ts_file->ts_cache, uri))
	{
		ts_buffer_t *get_buffer;
		mcache_kv_get(ts_file->ts_cache, uri, &get_buffer);
		free(get_buffer->p);
		free(get_buffer);
		mcache_kv_delete(ts_file->ts_cache, uri);
	}
	
	ts_buffer_t* ts_buffer = (ts_buffer_t*)malloc(sizeof(ts_buffer_t*));
	ts_buffer->p = malloc(hm->body.len);
	memcpy(ts_buffer->p, hm->body.p, hm->body.len);
	ts_buffer->len = hm->body.len;
	mcache_kv_set(ts_file->ts_cache , uri, ts_buffer);
	
	uri_buffer_t* uri_buffer = (uri_buffer_t*)malloc(sizeof(uri_buffer_t));
	uri_buffer->uri = malloc(sizeof(uri));
	memcpy(uri_buffer->uri, uri, sizeof(uri));
	value_t value;
	value.value = uri_buffer;
	ts_file->ts_request_key++;
	key_list_add(ts_file->ts_list, ts_file->ts_request_key, value);

}

ts_buffer_t *get_ts_cache(hls_cache_t *hls_cache, struct http_message *hm)
{
	if(hls_cache == NULL)
		return NULL;
	
	ts_file_t *ts_file = hls_cache->ts_file;
	if(ts_file == NULL)
		return NULL;
	
	char uri[100] = {0};
	snprintf(uri, sizeof(uri), "%.*s", (int) hm->uri.len, hm->uri.p);
	
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
	if(hls_cache == NULL)
		return 0;
	
	m3u8_file_t  *m3u8_file = hls_cache->m3u8_file;
	
	if(m3u8_file->m3u8_refresh_key > m3u8_file->m3u8_cache_key){
		return 1;
	}
	return 0;
}

m3u8_buffer_t *get_m3u8_cache(hls_cache_t *hls_cache)
{
	if(hls_cache == NULL)
		return NULL;
	
	return hls_cache->m3u8_file->m3u8_buffer;
}

void destory_hls_cache(hls_cache_t *hls_cache)
{
	if(hls_cache == NULL)
		return;
	
	thread_stop(hls_cache->thread, NULL);
    thread_destroy(hls_cache->thread);
}
