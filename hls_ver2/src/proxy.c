#include <semaphore.h>
#include <android/log.h>
#include <unistd.h>
#include <errno.h>
#include "thread.h"
#include "key_list.h"
#include "server.h"
#include "proxy.h"

#define LOG_TAG "HLS"
#define AKLOGE(fmt, ...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, fmt, ##__VA_ARGS__)
#define AKLOGI(fmt, ...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, fmt, ##__VA_ARGS__)

typedef struct hls_proxy {
	char *port;
	
	thread_t *thread;
	
	pthread_mutex_t mutex;
	
	pthread_cond_t is_wait;
	
	key_list_t 	*arg_list;
	
	int arg_index;
	
	int flags;
	
	char *url;
	
} hls_proxy_t;

static hls_proxy_t hls_proxy;

typedef struct urls {
   char *url;
} urls_t;

static void urls_releaser(value_t value)
{
    urls_t* urls = (urls_t*)value.value;
	free(urls->url);
    free(urls);
}

static void clear_arg_list()
{
	key_list_foreach(hls_proxy.arg_list, arg_node) 
	{
		key_list_delete(hls_proxy.arg_list, arg_node->key);
	}
}

static void *task_loop(void *arg) 
{
	while(!thread_is_interrupted(hls_proxy.thread))
	{	
		if(key_list_find_key(hls_proxy.arg_list, hls_proxy.arg_index)){
			value_t value;
		
			if(key_list_get(hls_proxy.arg_list, hls_proxy.arg_index, &value) == 0) 
			{
				urls_t *urls = (urls_t*)value.value;
				hls_proxy.url = strdup(urls->url);
				//AKLOGE("%s, %s, %d , %s \n", __FILE__,__FUNCTION__,__LINE__, hls_proxy.url);
				hls_proxy.flags = 1;
				key_list_delete(hls_proxy.arg_list, hls_proxy.arg_index);
			}
			
		}
		
		pthread_mutex_lock(&(hls_proxy.mutex));
		pthread_cond_wait(&(hls_proxy.is_wait), &(hls_proxy.mutex));
		pthread_mutex_unlock(&(hls_proxy.mutex));
	
	}
	return 0;
}

int proxy_init(char *port)
{	
	hls_proxy.thread = thread_create();
	pthread_mutex_init(&(hls_proxy.mutex), NULL);
	pthread_cond_init(&(hls_proxy.is_wait), NULL);
	thread_start(hls_proxy.thread, task_loop, NULL);
	
	hls_proxy.port = port;
	hls_proxy.arg_list = key_list_create(urls_releaser);
	hls_proxy.arg_index = -1;
	hls_proxy.flags = 0;
	
	start_server(port);
	
	return 0;
}

void play_video(char *url)
{	
	clear_arg_list();

	char *output = (char *) malloc(1024);
	
	if(!strncmp(url, "http://", strlen("http://"))
		&& (strstr(url, ".m3u8") || strstr(url, ".m3u")))
	{
		
		struct timeval tv;
		gettimeofday(&tv,NULL);
		long long seconds = ((long long)tv.tv_sec) * 1000 + tv.tv_usec / 1000;
	
		char desurl[1024];
		mg_base64_encode(url, strlen(url), desurl);
		sprintf(output, "http://127.0.0.1:%s/%lld/playlist.m3u8?url=%s", hls_proxy.port, seconds, desurl);
		
	}else
	{
		output = url;
	}
	
	urls_t* urls = (urls_t*)malloc(sizeof(urls_t));
	urls->url = output;
	
	value_t value;
	value.value = urls;
	
	hls_proxy.arg_index++;
	key_list_add(hls_proxy.arg_list, hls_proxy.arg_index, value);

	pthread_cond_broadcast(&(hls_proxy.is_wait));
}

char *get_url(void)
{
	if(hls_proxy.flags > 0)
	{
		hls_proxy.flags = 0;
		//AKLOGE("%s, %s, %d , %s \n", __FILE__,__FUNCTION__,__LINE__, hls_proxy.url);
		return hls_proxy.url;
	}
	
	return NULL;
}

void stop_play(void)
{
	clear_server();
}

int proxy_uninit(void)
{	
	stop_server();
	
	thread_stop(hls_proxy.thread, NULL);
    thread_destroy(hls_proxy.thread);
	
	pthread_mutex_destroy(&(hls_proxy.mutex));
    pthread_cond_destroy(&(hls_proxy.is_wait));
	
	key_list_destroy(hls_proxy.arg_list);
	return 0;
}

