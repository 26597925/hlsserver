#include <semaphore.h>
#include <android/log.h>
#include <unistd.h>
#include <errno.h>
#include "thread.h"
#include "hls_server.h"
#include "hls_proxy.h"
#include "key_list.h"

#ifndef ARRAY_SIZE
#define ARRAY_SIZE(array) (sizeof(array) / sizeof(array[0]))
#endif

#define RESTY_NUMBER 2
#define HTTP_ARRAY_LEN  18
#define OTHER_ARRAY_LEN 15
#define SO_PATH "/data/data/com.mylove.galaxy/lib/libxcode.so"

#define LOG_TAG "HLS"
#define AKLOGE(fmt, ...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, fmt, ##__VA_ARGS__)
#define AKLOGI(fmt, ...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, fmt, ##__VA_ARGS__)

#define DEUA  "User-Agent: Mozilla/4.0 (compatible; MS IE 6.0; (ziva))\r\n"
#define GXTV  "User-Agent: Mozilla/4.0 (compatible; MS IE 6.0; (ziva))\r\nReferer: http://www.gxtv.cn/swf/new_videoplayer.swf\r\n"
#define IJNTV "User-Agent: Mozilla/5.0 (Windows NT 5.1) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/50.0.2661.102 Safari/537.36\r\n"
#define YSTEN "User-Agent: 015173001962594#Ystenplatform_viper5_APK_20150505001#5167#5.1.6.7#Sep  9 2015,16:34:34\r\nYID: 015173001962594#Ystenplatform_viper5_APK_20150505001#5167#5.1.6.7#Sep  9 2015,16:34:34\r\n"

typedef struct hls_proxy {

	int child_id;
	
	char *port;
	
	char **arguments;
	
	thread_t *thread;
	
	key_list_t 	*arg_list;
	
	int arg_index;
	
	int flags;
	
} hls_proxy_t;

static hls_proxy_t hls_proxy;
static int array_len = HTTP_ARRAY_LEN;

typedef struct buffer {
   char **arguments;
} buffer_t;

static void buffer_releaser(value_t value)
{
    buffer_t* buffer = (buffer_t*)value.value;
	free(buffer->arguments);
    free(buffer);
}

static void clear_arg_list()
{
	key_list_foreach(hls_proxy.arg_list, arg_node) 
	{
		key_list_delete(hls_proxy.arg_list, arg_node->key);
	}
}

static char** build_arguments(char *url, char *output)
{
	char** arguments;
	if(!(strncmp(url, "http://", strlen("http://")) 
		|| strncmp(url, "https://", strlen("https://")))){
		char *headers = DEUA;
		
		if(strstr(url, "gxtv.cn") > 0)
		{
			headers = GXTV;
		}else if(strstr(url, "ijntv.cn") > 0){
			headers = IJNTV;
		}else if(strstr(url, "ysten.com") > 0){
			headers = YSTEN;
		}
		array_len = HTTP_ARRAY_LEN;
		arguments = calloc(array_len, sizeof(char*));
		arguments[0] = "hls";
		arguments[1] = "-re";
		arguments[2] = "-headers";
		arguments[3] = headers;
		arguments[4] = "-i";
		arguments[5] = url;
		arguments[6] = "-c:a";
		arguments[7] = "copy";
		arguments[8] = "-c:v";
		arguments[9] = "copy";
		arguments[10] = "-hls_time";
		arguments[11] = "3";
		arguments[12] = "-hls_list_size";
		arguments[13] = "3";
		arguments[14] = "-f";
		arguments[15] = "hls";
		arguments[16] = output;
		arguments[17] = (char*)0;
	}else{
		array_len = OTHER_ARRAY_LEN;
		arguments = calloc(array_len, sizeof(char*));
		arguments[0] = "hls";
		arguments[1] = "-i";
		arguments[2] = url;
		arguments[3] = "-c:a";
		arguments[4] = "copy";
		arguments[5] = "-c:v";
		arguments[6] = "copy";
		arguments[7] = "-hls_time";
		arguments[8] = "3";
		arguments[9] = "-hls_list_size";
		arguments[10] = "3";
		arguments[11] = "-f";
		arguments[12] = "hls";
		arguments[13] = output;
		arguments[14] = (char*)0;
	}
	return arguments;
}

static int start_transcode(char** arguments)
{
	pid_t pid;
	int status;
	
	if((pid = fork())<0)
	{
	    status = -1;//fork失败，返回-1
	}else if(pid == 0)
	{
		hls_proxy.child_id = getpid();
		status = execv(SO_PATH, arguments);
	}else
	{	
		hls_proxy.child_id = pid;
		while(waitpid(pid, &status, 0) < 0)
		{
			if(errno != EINTR)
			{
				status = -1;
				break;
			}
		}
	}
	return status;
}

static void *task_loop(void *arg) 
{
	while(!thread_is_interrupted(hls_proxy.thread))
	{
		int status = 0;
		if(key_list_find_key(hls_proxy.arg_list, hls_proxy.arg_index)){
			value_t value;
		
			if(key_list_get(hls_proxy.arg_list, hls_proxy.arg_index, &value) == 0) 
			{
				buffer_t *buffer = (buffer_t*)value.value;
				hls_proxy.arguments = buffer->arguments;
				status = start_transcode(buffer->arguments);
				hls_proxy.child_id = 0;
				hls_proxy.flags++;
			}
			
		}
	}
	return 0;
}

int hls_proxy_init(char *port)
{	
	hls_proxy.child_id = 0;
	hls_proxy.port = port;
	hls_proxy.thread = thread_create();
	thread_start(hls_proxy.thread, task_loop, NULL);
	
	hls_proxy.arg_list = key_list_create(buffer_releaser);
	hls_proxy.arg_index = -1;
	hls_proxy.flags = 0;
	
	start_hls_server(port);
	
	return 0;
}

void hls_play_video(char *url)
{	
	reset_hls_server();
	clear_arg_list();
	
	if(hls_proxy.child_id > 0){
		kill(hls_proxy.child_id, SIGKILL);
	}
	
	char *output = (char *) malloc(64);

	struct timeval tv;
    gettimeofday(&tv,NULL);
	long long seconds = ((long long)tv.tv_sec) * 1000 + tv.tv_usec / 1000;
	
	sprintf(output, "http://127.0.0.1:%s/%lld/playlist.m3u8", hls_proxy.port, seconds);
	
	buffer_t* buffer = (buffer_t*)malloc(sizeof(buffer_t));
	buffer->arguments = build_arguments(url, output);
	
	value_t value;
	value.value = buffer;
	
	hls_proxy.flags = 0;
	hls_proxy.arg_index++;
	
	key_list_add(hls_proxy.arg_list, hls_proxy.arg_index, value);
	
}

char *hls_get_url(void)
{
	if( has_cache() > 0)
	{
		if(array_len == 17){
			return hls_proxy.arguments[16];
		}else{
			return hls_proxy.arguments[13];
		}
	}
	
	if(hls_proxy.flags > RESTY_NUMBER)
	{
		reset_hls_server();
		if(array_len == 17){
			return hls_proxy.arguments[5];
		}else{
			return hls_proxy.arguments[2];
		}
	}
	
	return NULL;
}

void hls_stop_play(void)
{
	clear_hls_cache();
}

int hls_proxy_uninit(void)
{
	if(hls_proxy.child_id > 0){
		kill(hls_proxy.child_id, SIGKILL);
	}
	
	thread_stop(hls_proxy.thread, NULL);
    thread_destroy(hls_proxy.thread);
	
	key_list_destroy(hls_proxy.arg_list);
	stop_hls_server();
	return 0;
}

