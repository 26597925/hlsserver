#include "tsproxy.h"
#include "thread.h"
#include "key_list.h"
#include "hlsenc.h"
#include "tsserver.h"
#include "log.h"

#define URL_PRIFEX_FORMAT "http://127.0.0.1:%s"
#define OUTPUT_FORMAT "/%lld"
#define URL_FORMAT "%s%s/playlist.m3u8"

//http://www.linuxidc.com/Linux/2012-06/62549p2.htm

typedef struct tsproxy {
	
	char url_prifex[32];
	
	char url[64];
	
	thread_t *thread;
	
	key_list_t 	*hls_list;
	
	int hls_index;
	
	pthread_mutex_t mutex;
	
	pthread_cond_t is_wait;
	
	HLSContext *hls;
	
	int flags;
	
} tsproxy_t;

static tsproxy_t tsproxy;

typedef struct hls_buffer {
	
	long long index;
	
	char *input_url;
	
	char *output_url;
	
} hls_buffer_t;
	
static void hls_releaser(value_t value)
{
	hls_buffer_t *hls_buffer = (hls_buffer_t*)value.value;
	free(hls_buffer->input_url);
	free(hls_buffer->output_url);
	free(hls_buffer);
}

static void clear_hls_list()
{
	key_list_foreach(tsproxy.hls_list, hls_node) 
	{
		key_list_delete(tsproxy.hls_list, hls_node->key);
	}
}

static void *task_loop(void *arg) 
{	
	while(!thread_is_interrupted(tsproxy.thread))
	{
		if(tsproxy.hls != NULL)
		{
			pthread_mutex_lock(&(tsproxy.mutex));
			stop_slice(tsproxy.hls);
			tsproxy.hls = NULL;
			pthread_mutex_unlock(&(tsproxy.mutex));
		}
		
		if(key_list_find_key(tsproxy.hls_list, tsproxy.hls_index))
		{
			value_t value;
			if(key_list_get(tsproxy.hls_list, tsproxy.hls_index, &value) == 0) 
			{
				
				pthread_mutex_lock(&(tsproxy.mutex));
				hls_buffer_t *hls_buffer = (hls_buffer_t*)value.value;
				HLSContext *hls = init_hlscontex(hls_buffer->input_url);
				//AKLOGE("call %s %s %d %p\n",__FILE__, __FUNCTION__, __LINE__, hls);
				if(hls != NULL)
				{
					sprintf(tsproxy.url, URL_FORMAT, tsproxy.url_prifex, hls_buffer->output_url);
					put_queuelist(hls_buffer->index, hls->queue);
					tsproxy.hls = hls;
					start_slice(hls);
				//	AKLOGE("call %s %s %d\n",__FILE__, __FUNCTION__, __LINE__);
				}
				key_list_delete(tsproxy.hls_list, tsproxy.hls_index);
				pthread_mutex_unlock(&(tsproxy.mutex));
				
			}
		}
		
		pthread_mutex_lock(&(tsproxy.mutex));
		pthread_cond_wait(&(tsproxy.is_wait), &(tsproxy.mutex));
		pthread_mutex_unlock(&(tsproxy.mutex));
	}
	
	key_list_destroy(tsproxy.hls_list);
	
	//AKLOGE("call %s %s %d\n",__FILE__, __FUNCTION__, __LINE__);
	return 0;
}

int tsproxy_init(char *port)
{
	av_register_all();
	avformat_network_init();
	
	sprintf(tsproxy.url_prifex, URL_PRIFEX_FORMAT, port);
	
	start_tsserver(port);
		
	tsproxy.hls_list = key_list_create(hls_releaser);
	tsproxy.hls_index = -1;
	tsproxy.hls = NULL;
	
	pthread_mutex_init(&(tsproxy.mutex), NULL);
	tsproxy.thread = thread_create();
	thread_start(tsproxy.thread, task_loop, NULL);
	
}

void tsplay_video(char *url)
{	
	reset_tsserver();
	clear_hls_list();
	
	char *output = (char *) malloc(64);

	struct timeval tv;
    gettimeofday(&tv,NULL);
	long long index = ((long long)tv.tv_sec) * 1000 + tv.tv_usec / 1000;	
	sprintf(output, OUTPUT_FORMAT, index);
	
	hls_buffer_t *hls_buffer = (hls_buffer_t *)malloc(sizeof(hls_buffer_t));
	hls_buffer->index = index;
	hls_buffer->input_url = url;
	hls_buffer->output_url = output;
	
	value_t value;
	value.value = hls_buffer;
	
	tsproxy.hls_index++;	
	key_list_add(tsproxy.hls_list, tsproxy.hls_index, value);
	
	pthread_cond_broadcast(&(tsproxy.is_wait));
}

char *tsget_url(void)
{
	if( has_cache() > 0)
	{
		return tsproxy.url;
	}
	
	return NULL;
}

void stop_tsproxy(void)
{
	clear_tscache();
}

int tsproxy_uninit(void)
{		
	thread_stop(tsproxy.thread, NULL);
    thread_destroy(tsproxy.thread);
	pthread_mutex_destroy(&(tsproxy.mutex));
	
	stop_tsserver();
}