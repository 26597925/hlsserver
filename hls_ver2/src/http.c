/*
 *
 * Copyright (c) wenford.li
 * wenford.li <26597925@qq.com>
 *
 */
#include "http.h"
#include "config.h"
#include "util.h"

#include <android/log.h>
#define LOG_TAG "HLS"
#define AKLOGE(fmt, ...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, fmt, ##__VA_ARGS__)
#define AKLOGI(fmt, ...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, fmt, ##__VA_ARGS__)
 
struct http_header {
  char 	 *memory;
  size_t size;
};

static struct curl_slist *parse_header(http_t *http)
{
	if(http->headers == NULL)
		return NULL;
	
	char *header = strdup(http->headers);
	struct curl_slist *headers = NULL;
	char *line = strtok(header, SPLIT_RN);  
	while (line != NULL) 
	{
		headers = curl_slist_append(headers, line);
		line = strtok(NULL, SPLIT_RN);
	}
	
	return headers;
}

http_t *request(char *url)
{
	CURL *curl = curl_easy_init();
	if (curl == NULL)  
    {
        curl_global_cleanup();   
        return NULL;  
    }
	
	curl_easy_setopt(curl, CURLOPT_URL, url);
	
	http_t *http = (http_t *)malloc(sizeof(http_t));
	http->curl = curl;
	http->headers = DEUA;
	if(strstr(url, "gxtv.cn") > 0)
	{
		http->headers = GXTV;
	}else if(strstr(url, "ysten.com") > 0
		|| strstr(url, "ysten-business") > 0
	){
		http->headers = YSTEN;
	}
	http->timeout = 10;
	http->location = 1;
	http->redirect_url = NULL;
	
	return http;
}

void add_header(http_t *http, char *header)
{
	http->headers =  header;
}

void add_params(http_t *http, char *param)
{
	curl_easy_setopt(http->curl, CURLOPT_POSTFIELDS, param);
    curl_easy_setopt(http->curl, CURLOPT_POSTFIELDSIZE, sizeof(param));
}

void add_proxy(http_t *http, char *proxy_port, char *proxy_user_password)
{
	curl_easy_setopt(http->curl, CURLOPT_PROXY, proxy_port);
	curl_easy_setopt(http->curl, CURLOPT_PROXYUSERPWD, proxy_user_password);
}

void set_proxy_type(http_t *http, long type)
{
	curl_easy_setopt(http->curl, CURLOPT_PROXYTYPE, type);//CURLPROXY_SOCKS4,默认http
}

void add_cookie(http_t *http, char *cookie)
{
	curl_easy_setopt(http->curl, CURLOPT_COOKIE, cookie);
}

void set_refer(http_t *http, char *refer){
	curl_easy_setopt(http->curl, CURLOPT_REFERER, refer);
}

void set_user_agent(http_t *http, char *use_agent){
	curl_easy_setopt(http->curl, CURLOPT_USERAGENT, use_agent);
}

void set_time_out(http_t *http, int tout)
{
	http->timeout = tout;
}

void set_follow_location(http_t *http, int flocation)
{
	http->location = flocation;
}

static size_t header_callback(void *contents, size_t size, size_t nmemb, void *userp)
{
	
	size_t realsize = size * nmemb;
	struct http_header *mem = (struct http_header *)userp;
	
	if(!strncmp(contents, "Location: ", strlen("Location: ")))
	{
		char *location = malloc(strlen(contents)-strlen("Location: ")-2);
		substring(location, contents, strlen("Location: "), strlen(contents)-2);
		
		mem->memory = location;
		mem->size = strlen(location);
	}

	return realsize;
}

static size_t write_data(void *contents, size_t size, size_t nmemb, void *userp)
{
	size_t realsize = size * nmemb;
	struct memory_stru *mem = (struct memory_stru *)userp;

	mem->memory = realloc(mem->memory, mem->size + realsize + 1);

	memcpy(&(mem->memory[mem->size]), contents, realsize);
	mem->size += realsize;
	mem->memory[mem->size] = 0;

	return realsize;
}

struct memory_stru response(http_t *http){
	struct memory_stru result;
	result.memory = NULL;
	result.size = 0;
	
	struct http_header header;
	header.memory = NULL;
	header.size = 0;
	
	struct curl_slist *headers = parse_header(http);
	curl_easy_setopt(http->curl, CURLOPT_BUFFERSIZE, DUF_SIZE);
	curl_easy_setopt(http->curl, CURLOPT_HTTPHEADER, headers);
	curl_easy_setopt(http->curl, CURLOPT_HEADER, 0);//设置非0则，输出包含头信息
	curl_easy_setopt(http->curl, CURLOPT_NOSIGNAL, 1);
	curl_easy_setopt(http->curl, CURLOPT_CONNECTTIMEOUT, http->timeout);
	curl_easy_setopt(http->curl, CURLOPT_TIMEOUT, http->timeout);
	curl_easy_setopt(http->curl, CURLOPT_WRITEFUNCTION, write_data);
	curl_easy_setopt(http->curl, CURLOPT_WRITEDATA, (void *)&result);
	curl_easy_setopt(http->curl, CURLOPT_FOLLOWLOCATION, http->location);
	curl_easy_setopt(http->curl, CURLOPT_MAXREDIRS, 5);
	curl_easy_setopt(http->curl, CURLOPT_HEADERFUNCTION, header_callback);
	curl_easy_setopt(http->curl, CURLOPT_HEADERDATA, &header);
	curl_easy_setopt(http->curl, CURLOPT_IPRESOLVE, CURL_IPRESOLVE_V4);
	
	CURLcode res;
	res = curl_easy_perform(http->curl);
	
	long retcode = 0;
	res = curl_easy_getinfo(http->curl, CURLINFO_RESPONSE_CODE , &retcode);
	
	if(http->location == 0 && (retcode == 301 || retcode == 302)) 
	{
		char *redirect_url;
		res = curl_easy_getinfo(http->curl, CURLINFO_REDIRECT_URL , &redirect_url);
		http->redirect_url = strdup(redirect_url);
	}
	
	if(header.size != 0)
	{
		http->redirect_url = strdup(header.memory);
	}
	
	curl_easy_cleanup(http->curl);
	
	if(headers != NULL) 
		curl_slist_free_all(headers);
	
	return result;
}

void stop_http(http_t *http)
{
	if(http->redirect_url != NULL)
		free(http->redirect_url);
	free(http);
}
