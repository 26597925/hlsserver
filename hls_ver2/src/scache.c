#include "scache.h"
#include "mulhttp.h"
#include "http.h"
#include "config.h"

#define SPLIT_N "\n"
#define SPLIT_W "?"
#define M3U8    "#EXT"
#define TSPATH  "playlist.ts?tsid="
#define CACHE_SIZE 30*1024*1024
#define URLSIZE 1024

#define LOG_TAG "HLS"
#define AKLOGE(fmt, ...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, fmt, ##__VA_ARGS__)
#define AKLOGI(fmt, ...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, fmt, ##__VA_ARGS__)

typedef struct ts_buffer{
	scache_t *scache;
	char *ts_url;
	char *des_url;
} ts_buffer_t;

static void ts_releaser(value_t value)
{
	ts_buffer_t* ts_buffer = (ts_buffer_t*)value.value;
	
	if(ts_buffer != NULL){
		free(ts_buffer->ts_url);
		free(ts_buffer->des_url);
		free(ts_buffer);
	}

}

static struct memory_stru *download_ts(ts_buffer_t* ts_buffer)
{	
	char *ts_url = (char *)malloc(URLSIZE);

	char *url = ts_buffer->scache->url;
	
	ff_make_absolute_url(ts_url, URLSIZE, url, ts_buffer->ts_url);
	
	mulhttp_t *mulhttp = init_mulhttp(ts_url);

	if(strstr(url, "gxtv.cn") > 0)
	{
		add_mul_header(mulhttp, GXTV);
	}else if(strstr(url, "ysten.com") > 0
		|| strstr(url, "ysten-business") > 0
	){
		add_mul_header(mulhttp, YSTEN);
	}else{
		add_mul_header(mulhttp, DEUA);
	}
	
	struct memory_stru *m3u8_buffer = (struct memory_stru*)malloc(sizeof(struct memory_stru));
	m3u8_buffer->memory = NULL;
	m3u8_buffer->size = 0;
	
	//AKLOGE("%s, %s, %d\n", __FILE__,__FUNCTION__,__LINE__);
	if(start_download(mulhttp) > 0)
	{
	//	AKLOGE("%s, %s, %d, %s\n", __FILE__,__FUNCTION__,__LINE__, ts_url);
		wait_content(mulhttp);
		printf("%s, %s, %d downlong success!\n", __FILE__,__FUNCTION__,__LINE__);
		long content_size = mulhttp->content_size;
		
		if(mulhttp->mem_size == content_size)
		{
			m3u8_buffer->memory = mulhttp->memory;
			m3u8_buffer->size = mulhttp->mem_size;
		}
		
	}else
	{
		//AKLOGE("%s, %s, %d, %s\n", __FILE__,__FUNCTION__,__LINE__, ts_url);
		http_t *http = request(ts_url);
	
		if(http != NULL)
		{
			struct memory_stru memstru = response(http);
			
			m3u8_buffer->size = memstru.size;
			m3u8_buffer->memory = memstru.memory;
			
			stop_http(http);
		}
	}
	
	stop_download(mulhttp);
	
	free(ts_url);
	
	if(m3u8_buffer->memory == NULL 
		|| m3u8_buffer->size == 0)
	{
		return NULL;
	}
	
	return m3u8_buffer;
}

static int check_ts_url(scache_t *scache, char *url)
{
	key_list_foreach(scache->ts_list, cache_node) 
	{
		ts_buffer_t *ts_buffer = (ts_buffer_t*)cache_node->value.value;
		if(indexOf(ts_buffer->ts_url, SPLIT_W) > -1)
		{
			char *inner_ptr = NULL;
			char *outer_ptr = NULL;
			char *tsurl = strdup(ts_buffer->ts_url);
			tsurl = strtok_r(tsurl, SPLIT_W, &inner_ptr);
			strtok_r(NULL, SPLIT_W, &inner_ptr);
			
			char *testurl = strdup(url);
			testurl = strtok_r(testurl, SPLIT_W, &outer_ptr);
			strtok_r(NULL, SPLIT_W, &outer_ptr);
			
			if(!strcmp(tsurl, testurl))
			{
				free(tsurl);
				free(testurl);
				return cache_node->key;
			}
			
			free(tsurl);
			free(testurl);
			
		}else
		{
			
			if(!strcmp(ts_buffer->ts_url, url))
			{
				return cache_node->key;
			}
			
		}
	}
	
	return -1;
}

static void upload_ts_list(scache_t *scache)
{
	key_list_foreach(scache->ts_list, ts_node) 
	{	
		int key = scache->add_key - 50;
		if(key > ts_node->key && key_list_find_key(scache->ts_list, ts_node->key))
		{
			key_list_delete(scache->ts_list, ts_node->key);
		}
	}
}

static char *get_m3u8_list(scache_t *scache, char *url)
{
	struct memory_stru memstru;
	memstru.memory = NULL;
	memstru.size = 0;
	
	http_t *http = request(url);
	if(http == NULL) 
		return NULL;

	memstru = response(http);

	AKLOGE("%s, %s, %d, %d\n", __FILE__,__FUNCTION__,__LINE__, memstru.size);
	
	if(http->redirect_url != NULL)
	{
		free(scache->url);
		scache->url = strdup(http->redirect_url);
	}
	
	stop_http(http);
	
	if(memstru.size > 0)
	{
		memstru.size = 0;
		return memstru.memory;
	}
	
	return NULL;
}

static char* parse_ts_url(scache_t *scache, char *result)
{	
	if(indexOf(result, M3U8) < 0)
		return NULL;
	
	char *buf = "";
	char *s = strtok(result, SPLIT_N);  
	while (s != NULL) 
	{
		if(indexOf(s, M3U8) < 0)
		{
			if(indexOf(s, ".m3u8") > -1 
				|| indexOf(s, ".m3u") > -1)
			{
				del_char(s, '\r');
				
				char *ts_url = (char *) malloc(URLSIZE);
				ff_make_absolute_url(ts_url, URLSIZE, scache->url, s);
				
				AKLOGE("%s, %s, %d, %s\n", __FILE__,__FUNCTION__,__LINE__,  ts_url);
				
				char *m3u8 = get_m3u8_list(scache, ts_url);
				
				AKLOGE("%s, %s, %d, %s\n", __FILE__,__FUNCTION__,__LINE__,  m3u8);
				free(ts_url);
				free(result);
				
				if(m3u8 == NULL)
					return NULL;
				
				return parse_ts_url(scache, m3u8);
			}else
			{
				char *des_url;
				int key = check_ts_url(scache, s);
				//printf("%s, %s, %d, key:%d\n", __FILE__,__FUNCTION__,__LINE__,  key);
				//AKLOGE("%s, %s, %d, %d\n", __FILE__,__FUNCTION__,__LINE__,  key);
				if(key > -1)
				{
					value_t value;
					if(key_list_get(scache->ts_list, key, &value) == 0) 
					{
						ts_buffer_t* ts_buffer = (ts_buffer_t*)value.value;
						des_url = join_str(ts_buffer->des_url, "\n");
					}
				}else
				{
					del_char(s, '\r');
					scache->add_key++;
				
					char tsid[10];
					itoa(scache->add_key, tsid, 10);
					des_url = join_str(TSPATH, tsid);
					
					ts_buffer_t* ts_buffer = (ts_buffer_t*)malloc(sizeof(ts_buffer_t*));
					ts_buffer->scache = scache;
					ts_buffer->ts_url = strdup(s);
					ts_buffer->des_url = des_url;
					
					value_t value;
					value.value = ts_buffer;
					key_list_add(scache->ts_list, scache->add_key, value);
					des_url = join_str(des_url, "\n");
					
					pthread_cond_broadcast(&(scache->queue_not_empty));
					
				}
				buf = join_str(buf, des_url);
				free(des_url);
			}
		}else
		{
			if(!strstr(s, "#EXT-X-PROGRAM-DATE-TIME"))
			{
				buf = join_str(buf, s);
				buf = join_str(buf, "\n");
			}
		}
		s = strtok(NULL, SPLIT_N);
	}
	
	return buf;
}

static char* refresh_ts(scache_t *scache)
{	
	char *m3u8 = get_m3u8_list(scache, scache->url);
	
	if(m3u8 == NULL)
		return NULL;
	
	return parse_ts_url(scache, m3u8);
}

static void clear_cache(scache_t *scache)
{
	key_list_foreach(scache->ts_list, ts_node) 
	{
		key_list_delete(scache->ts_list, ts_node->key);
	}
	
	key_list_destroy(scache->ts_list);
	
	free(scache->url);
	free(scache);
	
	//AKLOGE("%s, %s, %d\n", __FILE__,__FUNCTION__,__LINE__);
}

static void *task_loop(void *arg) 
{	
	
	scache_t *scache = (scache_t *)arg;
	
	while(!thread_is_interrupted(scache->thread))
	{	
		pthread_mutex_lock(&(scache->mutex));
		pthread_cond_wait(&(scache->queue_not_empty), &(scache->mutex));
		pthread_mutex_unlock(&(scache->mutex));
		
		pthread_mutex_lock(&(scache->mutex));
		upload_ts_list(scache);
		pthread_mutex_unlock(&(scache->mutex));
	}
	
	pthread_mutex_lock(&(scache->mutex));
	thread_destroy(scache->thread);
	pthread_mutex_unlock(&(scache->mutex));
	
	pthread_mutex_destroy(&(scache->mutex));
    pthread_cond_destroy(&(scache->queue_not_empty));
	//AKLOGE("%s, %s, %d\n", __FILE__,__FUNCTION__,__LINE__);
	
	pthread_mutex_lock(&(scache->mutex));
	clear_cache(scache);
	pthread_mutex_unlock(&(scache->mutex));
	
	return 0;
}

scache_t *init_scache(char *url)
{
	scache_t *scache = (scache_t *)malloc(sizeof(scache_t));
	scache->add_key = -1;
	scache->url = strdup(url);
	
	scache->ts_list = key_list_create(ts_releaser);
	
	pthread_mutex_init(&(scache->mutex), NULL);
	pthread_cond_init(&(scache->queue_not_empty), NULL);
	
	scache->thread = thread_create();
	thread_start(scache->thread, task_loop, scache);
	
	return scache;
}

char *request_url(scache_t *scache)
{
	return refresh_ts(scache);
}

struct memory_stru get_cache(scache_t *scache, char* tsid)
{
	
	struct memory_stru result;
	result.memory = NULL;
	result.size = 0; 
	
	int key = atoi(tsid);
	if(key_list_find_key(scache->ts_list, key))
	{
		value_t value;
		if(key_list_get(scache->ts_list, key, &value) == 0) 
		{
			ts_buffer_t* ts_buffer = (ts_buffer_t*)value.value;
			struct memory_stru *memstru = download_ts(ts_buffer);
			if(memstru == NULL)
			{
				return result;
			}else
			{
				result.memory = memstru->memory;
				result.size = memstru->size;
				free(memstru);
			}
		}
	}
	
	return result;
}

void destroy_cache(scache_t *scache)
{
	thread_stop(scache->thread, NULL);
	pthread_cond_broadcast(&(scache->queue_not_empty));
}

