#include <semaphore.h>
#include <android/log.h>
#include "thread.h"
#include "hls.h"
#include "probe.h"
#include "hls_server.h"
#include "hls_proxy.h"


#define LOG_TAG "HLS"
#define DEFAULT_PORT 9798
#define AKLOGE(fmt, ...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, fmt, ##__VA_ARGS__)
#define AKLOGI(fmt, ...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, fmt, ##__VA_ARGS__)

typedef struct hls_proxy {
	char *urls;
	char *port;
	
	thread_t *thread;
	
	sem_t sem;
	
	transcode_task_t *transcode_task;
	
} hls_proxy_t;

static hls_proxy_t hls_proxy;

static void play_video(void)
{	
	AKLOGE("call %s %s %d \n",__FILE__,__FUNCTION__,__LINE__);
	//hls_proxy.url = probe_url(urls);//curl = "http://fms.cntv.lxdns.com/live/flv/channel50.flv";
	hls_proxy.transcode_task = start_transcode_hls(hls_proxy.port, hls_proxy.urls, &hls_proxy.sem);
	AKLOGE("call %s %s %d \n",__FILE__,__FUNCTION__,__LINE__);
	
}

static void *task_loop(void *arg) 
{
	while(!thread_is_interrupted(hls_proxy.thread))
	{
		sem_wait(&hls_proxy.sem);
		if(hls_proxy.transcode_task != NULL)
		{
			destroy_transcode_hls(hls_proxy.transcode_task);
		}
		clear_hls_cache();
		play_video();
	}
	return 0;
}

int hls_proxy_init(char *port)
{
	AKLOGE("hls_proxy_init port:%s", port);
	hls_proxy.port = port;
	hls_proxy.transcode_task = NULL;
	hls_proxy.thread = thread_create();
	thread_start(hls_proxy.thread, task_loop, NULL);
	
	start_hls_server(port);
	
	sem_init(&hls_proxy.sem, 0, 0);
	
	return 0;
}

void hls_play_video(char *urls)
{
	hls_proxy.urls = urls;
	
	if(hls_proxy.transcode_task != NULL 
		&& hls_proxy.transcode_task->running)
	{
		AKLOGE("call %s %s %d \n",__FILE__,__FUNCTION__,__LINE__);
		hls_stop_play();
	}else
	{
		AKLOGE("call %s %s %d \n",__FILE__,__FUNCTION__,__LINE__);
		if(hls_proxy.transcode_task != NULL){
			return;
		}
		sem_post(&hls_proxy.sem);
	}
}

char *hls_get_url(void)
{

	if(hls_proxy.transcode_task != NULL
		&& hls_proxy.transcode_task->running
		&& has_cache()
		)
	{
		return hls_proxy.transcode_task->arguments[15];
	}
	
	return NULL;
}

void hls_stop_play(void)
{
	stop_transcode_hls(hls_proxy.transcode_task);
	AKLOGE("call %s %s %d \n",__FILE__,__FUNCTION__,__LINE__);
}

int hls_proxy_uninit(void)
{
	AKLOGE("hls_proxy_uninit");
	stop_hls_server();
	sem_destroy(&hls_proxy.sem);
	AKLOGE("call %s %s %d \n",__FILE__,__FUNCTION__,__LINE__);
	return 0;
}

