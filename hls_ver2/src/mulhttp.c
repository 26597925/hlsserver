/*
 *
 * Copyright (c) wenford.li
 * wenford.li <26597925@qq.com>
 *
 */
#include <stdio.h>
#include "mulhttp.h"
#include "config.h"
 
#include <android/log.h>
#define LOG_TAG "HLS"
#define AKLOGE(fmt, ...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, fmt, ##__VA_ARGS__)
#define AKLOGI(fmt, ...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, fmt, ##__VA_ARGS__)

struct http_header {
  char 	 *memory;
  size_t size;
};
 
typedef struct threadinfo {
	
	long start_pos;  //下载起始位置
	long end_pos;	   //下载结束位置
	long block_size; //本线程负责下载的数据大小
	long recv_size; //本线程已经接收到的数据大小
	
	char *memory;
	CURL *curl;
	struct curl_slist *headers;
	
	mulhttp_t *mulhttp; 
	
} threadinfo_t;

static size_t header_callback(void *contents, size_t size, size_t nmemb, void *userp)
{
	
	size_t realsize = size * nmemb;
	struct http_header *mem = (struct http_header *)userp;

	mem->memory = realloc(mem->memory, mem->size + realsize + 1);
	memcpy(&(mem->memory[mem->size]), contents, realsize);
	mem->size += realsize;
	mem->memory[mem->size] = 0;

	return realsize;
}

static size_t write_callback(void *contents, size_t size, size_t nmemb, void *userp)
{
	
	size_t realsize = size * nmemb;
	threadinfo_t *threadinfo = (threadinfo_t *) userp;
	threadinfo->memory = realloc(threadinfo->memory, threadinfo->recv_size + realsize + 1);
	memcpy(&(threadinfo->memory[threadinfo->recv_size]), contents, realsize);
	threadinfo->recv_size += realsize;
	threadinfo->memory[threadinfo->recv_size] = 0;
	
	return realsize;
}

static void *task_loop(void *arg) 
{
	threadinfo_t *threadinfo = (threadinfo_t *) arg;
	
	mulhttp_t *mulhttp = threadinfo->mulhttp;
	
	pthread_mutex_lock(&mulhttp->lock);
	
	CURLcode res;
	res = curl_easy_perform(threadinfo->curl);
	
	long retcode = 0;
	res = curl_easy_getinfo(threadinfo->curl, CURLINFO_RESPONSE_CODE , &retcode);
	
	AKLOGE("%s, %s, %d,  %ld\n", __FILE__, __FUNCTION__, __LINE__, retcode);
	
	curl_slist_free_all(threadinfo->headers);
	curl_easy_cleanup(threadinfo->curl);
	
	mulhttp->flags++;
	
	AKLOGE("%s, %s, %d,  %d, %p\n", __FILE__, __FUNCTION__, __LINE__, threadinfo->recv_size, threadinfo->memory);
	mulhttp->memory = realloc(mulhttp->memory, threadinfo->recv_size + threadinfo->start_pos);
	memcpy(&(mulhttp->memory[threadinfo->start_pos]), threadinfo->memory, threadinfo->recv_size);
	
	mulhttp->mem_size += threadinfo->recv_size;
	AKLOGE("%s, %s, %d,  %ld\n", __FILE__, __FUNCTION__, __LINE__, mulhttp->mem_size);

	free(threadinfo->memory);
	free(threadinfo);
	
	if(mulhttp->flags == mulhttp->thread_num)
	{
		long content_size = mulhttp->content_size;
		if(mulhttp->mem_size != content_size)
		{
			free(mulhttp->memory);
			mulhttp->memory = NULL;
			mulhttp->mem_size = 0;
		}
		sem_post(&mulhttp->sem);	
	}
	
	pthread_mutex_unlock(&mulhttp->lock);
	
	return 0;
}

static struct curl_slist *parse_header(mulhttp_t *mulhttp)
{
	if(mulhttp->headers == NULL)
		return NULL;
	
	char *header = strdup(mulhttp->headers);
	struct curl_slist *headers = NULL;
	char *line = strtok(header, SPLIT_RN);  
	while (line != NULL) 
	{
		headers = curl_slist_append(headers, line);
		line = strtok(NULL, SPLIT_RN);
	}
	
	return headers;
}

static void start_http(mulhttp_t *mulhttp, threadinfo_t *threadinfo)
{
	CURL *curl = curl_easy_init();
	if (curl == NULL)  
    {
        curl_global_cleanup();   
        return NULL;  
    }
	
	threadinfo->curl = curl;
	
	char range[64] = { 0 };
    snprintf (range, sizeof (range), "%ld-%ld", threadinfo->start_pos, threadinfo->end_pos);
	
	//printf("%s, %s, %d,  %s\n", __FILE__, __FUNCTION__, __LINE__, range);
	
	curl_easy_setopt(threadinfo->curl, CURLOPT_URL, mulhttp->url);
	
	struct curl_slist *headers = parse_header(mulhttp);
	curl_easy_setopt(threadinfo->curl, CURLOPT_HTTPHEADER, headers);
	
	threadinfo->headers = headers;	
	curl_easy_setopt(threadinfo->curl, CURLOPT_BUFFERSIZE, DUF_SIZE);
	curl_easy_setopt(threadinfo->curl, CURLOPT_HEADER, 0);//设置非0则，输出包含头信息
	curl_easy_setopt(threadinfo->curl, CURLOPT_NOSIGNAL, 1L);
	curl_easy_setopt(threadinfo->curl, CURLOPT_CONNECTTIMEOUT, mulhttp->timeout);
	curl_easy_setopt(threadinfo->curl, CURLOPT_TIMEOUT, mulhttp->timeout);
	curl_easy_setopt(threadinfo->curl, CURLOPT_WRITEFUNCTION, write_callback);
	curl_easy_setopt(threadinfo->curl, CURLOPT_WRITEDATA, (void *)threadinfo);
	curl_easy_setopt(threadinfo->curl, CURLOPT_FOLLOWLOCATION, mulhttp->location);
	curl_easy_setopt(threadinfo->curl, CURLOPT_MAXREDIRS, 5);
	curl_easy_setopt(threadinfo->curl, CURLOPT_RANGE, range);
	curl_easy_setopt(threadinfo->curl, CURLOPT_IPRESOLVE, CURL_IPRESOLVE_V4);
	
	threadpool_add(mulhttp->thread__pool, task_loop, threadinfo, 0);
}

static void get_download_info(mulhttp_t *mulhttp)
{
	struct http_header header;
	header.memory = NULL;
	header.size = 0;
	
	CURL *handle = curl_easy_init();
	curl_easy_setopt(handle, CURLOPT_URL, mulhttp->url);
	
	struct curl_slist *headers = parse_header(mulhttp);
	curl_easy_setopt(handle, CURLOPT_HTTPHEADER, headers);
	
	curl_easy_setopt(handle, CURLOPT_HEADER, 1);
	curl_easy_setopt(handle, CURLOPT_FOLLOWLOCATION, 1);
	curl_easy_setopt(handle, CURLOPT_NOBODY, 1);
	curl_easy_setopt(handle, CURLOPT_NOSIGNAL, 1);
	curl_easy_setopt(handle, CURLOPT_MAXREDIRS, 5);
	curl_easy_setopt(handle, CURLOPT_HEADERFUNCTION, header_callback);
	curl_easy_setopt(handle, CURLOPT_HEADERDATA, &header);
	curl_easy_setopt(handle, CURLOPT_RANGE, "0-");
	curl_easy_setopt(handle, CURLOPT_IPRESOLVE, CURL_IPRESOLVE_V4);
	CURLcode res = curl_easy_perform(handle);
	
	long retcode = 0;
	res = curl_easy_getinfo(handle, CURLINFO_RESPONSE_CODE , &retcode);
	
	if (res == CURLE_OK) {

		curl_easy_getinfo(handle, CURLINFO_CONTENT_LENGTH_DOWNLOAD, &mulhttp->content_size);
		
		AKLOGE("%s, %s, %d, %s\n", __FILE__,__FUNCTION__,__LINE__, header.memory);
		if(header.memory != NULL){
			
			if(retcode == 206 || strstr(header.memory, "Content-Range: bytes"))
			{
				mulhttp->is_range = 1;
			}
			
		}
	}
	
	curl_slist_free_all(headers);
	curl_easy_cleanup(handle);
}

mulhttp_t *init_mulhttp(char *url)
{
	//printf("%s, %s, %d,  %s\n", __FILE__, __FUNCTION__, __LINE__, url);
	mulhttp_t *mulhttp = (mulhttp_t *)malloc(sizeof(mulhttp_t));
	mulhttp->headers = DEUA;
	if(strstr(url, "gxtv.cn") > 0)
	{
		mulhttp->headers = GXTV;
	}else if(strstr(url, "ysten.com") > 0 
		|| strstr(url, "ysten-business") > 0
	){
		mulhttp->headers = YSTEN;
	}
	mulhttp->url = strdup(url);
	//printf("%s, %s, %d,  %s\n", __FILE__, __FUNCTION__, __LINE__, mulhttp->url);
	mulhttp->is_range = 0;
	mulhttp->content_size = 0;
	mulhttp->timeout = 10;
	mulhttp->location = 1;
	mulhttp->flags = 0;
	mulhttp->mem_size = 0;
	mulhttp->memory = NULL;
	mulhttp->thread_num = 6;
	mulhttp->thread__pool = threadpool_create(mulhttp->thread_num, 60, 0);
	pthread_mutex_init(&mulhttp->lock, NULL);
	sem_init(&mulhttp->sem, 0, 0);
	
	return mulhttp;
}

void add_mul_header(mulhttp_t *mulhttp, char *header)
{
	mulhttp->headers = header;
}

void set_mul_time_out(mulhttp_t *mulhttp, int tout)
{
	mulhttp->timeout = tout;
}

void set_mul_follow_location(mulhttp_t *mulhttp, int flocation)
{
	mulhttp->location = flocation;
}

int start_download(mulhttp_t *mulhttp)
{
	int ret = -1;
	
	get_download_info(mulhttp);
	
	if(mulhttp->content_size == 0 
		|| mulhttp->is_range == 0)
	{
		return ret;
	}
	
	long part_size =  mulhttp->content_size/mulhttp->thread_num;
	
	int i;
	for (i = 0; i < mulhttp->thread_num; i++)  
    {  
		threadinfo_t *threadinfo = (threadinfo_t *)malloc(sizeof(threadinfo_t));
		threadinfo->mulhttp = mulhttp;
		threadinfo->recv_size = 0;
		threadinfo->memory = NULL;
		
		if (i < (mulhttp->thread_num - 1))  
        {  
            threadinfo->start_pos = i * part_size;  
            threadinfo->end_pos = (i + 1) * part_size - 1;
        }  
        else  
        {   
            threadinfo->start_pos = i * part_size;  
            threadinfo->end_pos = mulhttp->content_size - 1;  
        }
		
		threadinfo->block_size = threadinfo->end_pos - threadinfo->start_pos + 1;
		
		start_http(mulhttp, threadinfo);
	}
	
	ret = 1;
	
	return ret;
}

void stop_download(mulhttp_t *mulhttp)
{
	
	threadpool_destroy(mulhttp->thread__pool, 0);
	pthread_mutex_destroy(&(mulhttp->lock));
	
	sem_destroy(&mulhttp->sem);
	
	free(mulhttp->url);
	free(mulhttp);
	
}

void wait_content(mulhttp_t *mulhttp)
{
	struct timespec ts;
	ts.tv_sec = time(NULL) + mulhttp->timeout;
	ts.tv_nsec = 0;
    sem_timedwait(&mulhttp->sem, &ts);
	//sem_wait(&mulhttp->sem);
}

void write_file(mulhttp_t *mulhttp, char *path)
{
	FILE *stream;
	stream = fopen(path, "w");
	fwrite(mulhttp->memory, 1, mulhttp->content_size, stream);
	fclose(stream);
}
