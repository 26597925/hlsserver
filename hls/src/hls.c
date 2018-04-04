#include <semaphore.h>
#include <android/log.h>
#include <sys/time.h>
#include "ffmpeg.h"
#include "cmdutils.h"
#include "hls.h"

#define LOG_TAG "HLS"
#define AKLOGE(fmt, ...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, fmt, ##__VA_ARGS__)
#define AKLOGI(fmt, ...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, fmt, ##__VA_ARGS__)

#define ARGS_NUMBER 16 
#define DEUA "User-Agent: Mozilla/4.0 (compatible; MS IE 6.0; (ziva))\r\n"
#define GXTV "User-Agent: Mozilla/4.0 (compatible; MS IE 6.0; (ziva))\r\nReferer: http://www.gxtv.cn/swf/new_videoplayer.swf\r\n"


static char** build_arguments(char *url, char *header, char *output)
{
	char** arguments = calloc(ARGS_NUMBER, sizeof(char*));
	arguments[0] = "ffmpeg";
	arguments[1] = "-i";
	arguments[2] = url;
	arguments[3] = "-headers";
	arguments[4] = header;
	arguments[5] = "-c:a";
	arguments[6] = "copy";
	arguments[7] = "-c:v";
	arguments[8] = "copy";
	arguments[9] = "-hls_time";
	arguments[10] = "5";
	arguments[11] = "-hls_list_size";
	arguments[12] = "3";
	arguments[13] = "-f";
	arguments[14] = "hls";
	arguments[15] = output;
	return arguments;
}

static void *task_loop(void *arg) 
{
    transcode_task_t *transcode_task = (transcode_task_t *)arg;
	while(!thread_is_interrupted(transcode_task->thread))
	{
		start_transcode(ARGS_NUMBER, transcode_task->arguments, transcode_task->sem);
	}
	AKLOGE("call %s %s %d",__FILE__,__FUNCTION__,__LINE__);
	return 0;
}

transcode_task_t *start_transcode_hls(int port, char *url, sem_t *sem)
{
	AKLOGE("call %s %s %d %s",__FILE__,__FUNCTION__,__LINE__, url);
	char *ua = DEUA;
	
	if(strstr(url, "gxtv.cn") > 0)
	{
		ua = GXTV;
	}
	
	char *output = (char *) malloc(64);
	int seconds  = time((time_t*)NULL);
	sprintf(output, "http://127.0.0.1:%s/%d/playlist.m3u8", port, seconds);
	
	transcode_task_t *transcode_task = (transcode_task_t *)malloc(sizeof(transcode_task_t));
	transcode_task->running = 1;
	transcode_task->task_name = "TRANSCODETASK";
	transcode_task->sem = sem;
	transcode_task->arguments = build_arguments(url, ua, output);
    transcode_task->thread = thread_create();
	thread_start(transcode_task->thread, task_loop, transcode_task);
	
	return transcode_task;
}

void stop_transcode_hls(transcode_task_t *transcode_task)
{
	if(transcode_task->running)
	{
		AKLOGE("call %s %s %d",__FILE__,__FUNCTION__,__LINE__);
		transcode_task->running = 0;
		stop_transcode();
		AKLOGE("call %s %s %d",__FILE__,__FUNCTION__,__LINE__);
	}
}

void destroy_transcode_hls(transcode_task_t *transcode_task)
{
	free(transcode_task->task_name);
	free(transcode_task->sem);
	free(transcode_task->arguments);
	thread_destroy(transcode_task->thread);
}