#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <sys/time.h>
#include <android/log.h>

#include "util.h"
#include "hls_cache.h"
#include "mongoose.h"
#include "hls_server.h"

#define LOG_TAG "HLS"
#define REFRESH_TIME 1
#define HTML_INDEX    "<html><head><title>Welcome to %s!</title><style>body{width:35em;margin:0 auto;font-family:Tahoma,Verdana,Arial,sans-serif;}</style></head><body><h1>Welcome to %s!</h1><p>If you see this page, the server is successfully installed and working.</p><p><em>Thank you for using.Contact me QQ:26597925.</em></p></body></html>"
#define HEADER_FORMAT "Cache-Control: no-cache\r\nContent-Type: %s\r\nContent-Length: %d\r\n"
#define AKLOGE(fmt, ...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, fmt, ##__VA_ARGS__)
#define AKLOGI(fmt, ...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, fmt, ##__VA_ARGS__)

typedef struct hls_server {
	
	char *server_name;
	
	char *port;
	
	int is_stop;
	
	struct mg_mgr mgr;
	
	struct mg_serve_http_opts s_http_server_opts;
	
	pthread_t pthread_id;
	
	int cache_index;
	
	key_list_t 	*cache_list;
	
	int flags;
	
} hls_server_t;

typedef struct buffer {

   long long index;

   hls_cache_t *cache;
   
} buffer_t;

static hls_server_t hls_server;

static void buffer_releaser(value_t value)
{
    buffer_t* buffer = (buffer_t*)value.value;
	destory_hls_cache(buffer->cache);
    free(buffer);
}

static long long get_uri_index(char *uri)
{
	char *temp =  malloc(strlen(uri));
	strncpy(temp, uri, strlen(uri));
	char **urls = split_str(temp, "/");
	long long index = atoll(urls[0]);
	free(temp);
	free(urls);
	return index;
}

static hls_cache_t *get_hls_cache(long long index)
{
	key_list_foreach(hls_server.cache_list, cache_node) 
	{
		buffer_t *buffer = (buffer_t*)cache_node->value.value;
		if(buffer->index == index)
		{
			return buffer->cache;
		}
	}
	return NULL;
}

static void clearCache(long long index){
	key_list_foreach(hls_server.cache_list, cache_node) 
	{
		buffer_t *buffer = (buffer_t*)cache_node->value.value;
		if(buffer->index == index 
			|| hls_server.cache_index == cache_node->key)
			continue;
		
		key_list_delete(hls_server.cache_list, cache_node->key);
	}
}

static void ev_handler(struct mg_connection *nc, int ev, void *p) 
{
	if (ev == MG_EV_HTTP_REQUEST) 
	{
		struct http_message *hm = (struct http_message *) p;
		char method[5], uri[100];
		snprintf(method, sizeof(method), "%.*s", (int) hm->method.len, hm->method.p);
		snprintf(uri, sizeof(uri), "%.*s", (int) hm->uri.len, hm->uri.p);
		if (!strcmp(method, "GET")){
			
			if(lastIndexOf(uri, ".m3u8") > -1)
			{	
				long long index = get_uri_index(uri);
				
				hls_cache_t *cache = get_hls_cache(index);
				
				if(cache == NULL)
				{
					mg_http_send_error(nc, 404, NULL);
				}
				else
				{
					m3u8_buffer_t *m3u8_buffer = get_m3u8_cache(cache);
					if(m3u8_buffer != NULL)
					{
						char header[512];
						sprintf(header,  HEADER_FORMAT, "application/x-mpegURL", m3u8_buffer->len);
					
						mg_send_response_line(nc, 200, header);
						mg_send(nc, m3u8_buffer->m3u8, m3u8_buffer->len);
						nc->flags |= MG_F_SEND_AND_CLOSE;
						
					}else
					{
						mg_http_send_error(nc, 404, NULL);
					}
				}
			}
			else if(lastIndexOf(uri, ".ts") > -1)
			{
				long long index = get_uri_index(uri);
				
				hls_cache_t *cache = get_hls_cache(index);
				
				if(cache == NULL)
				{
					mg_http_send_error(nc, 404, NULL);
				}
				else
				{
					ts_buffer_t *ts_buffer = get_ts_cache(cache, hm);
					if(ts_buffer != NULL)
					{
						char header[512];
						
						sprintf(header, HEADER_FORMAT, "video/MP2T", ts_buffer->len);
						
						mg_send_response_line(nc, 200, header);
						mg_send(nc, ts_buffer->p, ts_buffer->len);
						nc->flags |= MG_F_SEND_AND_CLOSE;
						
						clearCache(index);
					}else
					{
						mg_http_send_error(nc, 404, NULL);
					}
				}
			}else{
				
				char temp[1024];
				sprintf(temp, HTML_INDEX, hls_server.server_name, hls_server.server_name); 
				mg_send_response_line(nc, 200, "Content-Type: text/html;charset=UTF-8\r\n");
				mg_send(nc, temp, strlen(temp));
				nc->flags |= MG_F_SEND_AND_CLOSE;
				
			}
		}
		else if(!strcmp(method, "POST"))
		{
			AKLOGE("call %s %s %d %s\n",__FILE__,__FUNCTION__,__LINE__, uri);
			long long index = get_uri_index(uri);
			
			hls_cache_t *cache = get_hls_cache(index);
			
			if(cache == NULL)
			{	
				cache = init_hls_cache();
				buffer_t* buffer = (buffer_t*)malloc(sizeof(buffer_t));
				buffer->index = index;
				buffer->cache = cache;
				value_t value;
				value.value = buffer;
				hls_server.cache_index++;
				key_list_add(hls_server.cache_list, hls_server.cache_index, value);
				hls_server.flags = 0;
			}
			
			if(strstr(uri, "/playlist.m3u8"))
			{
				add_m3u8_list(cache, hm);
			}
			else if(lastIndexOf(uri, ".ts") > -1)
			{
				add_ts_list(cache, hm);
			}
			
			mg_http_send_error(nc, 200, NULL);
		}
	}
}

static void *serve(void *mgr) 
{
	
	struct mg_connection *nc;
	
	mg_mgr_init(&hls_server.mgr, NULL);
	nc = mg_bind(&hls_server.mgr, hls_server.port, ev_handler);
	mg_set_protocol_http_websocket(nc);
	//mg_enable_multithreading(nc);
	
	while (!hls_server.is_stop) 
	{
		mg_mgr_poll(&hls_server.mgr, 1000);
	}
	
	mg_mgr_free(&hls_server.mgr);
	
	return 0;
}

int start_hls_server(char *port)
{
	
	hls_server.server_name = "Hls Server";
	hls_server.port = port;
	hls_server.is_stop = 0;
	hls_server.flags = 1;
	hls_server.cache_index = 0;
	hls_server.cache_list = key_list_create(buffer_releaser);
	hls_server.pthread_id = mg_start_thread(serve, &hls_server.mgr);
    //AKLOGE("call %s %s %d start_hls_server\n",__FILE__,__FUNCTION__,__LINE__);
	
	return 0;
}

void reset_hls_server(void)
{
	hls_server.flags = 1;
}

int stop_hls_server(void)
{
	
	hls_server.is_stop = 1;
	pthread_join(hls_server.pthread_id, NULL);
	AKLOGE("call %s %s %d stop_hls_server\n",__FILE__,__FUNCTION__,__LINE__);
	
	return 0;
}

int has_cache()
{
	if(hls_server.flags > 0){
		return 0;
	}
	
	if(hls_server.cache_index > 0){
		if(key_list_find_key(hls_server.cache_list, hls_server.cache_index))
		{
			value_t value;
			key_list_get(hls_server.cache_list, hls_server.cache_index, &value);
			buffer_t *buffer = (buffer_t*)value.value;
			hls_cache_t *cache = buffer->cache;
			if(has_m3u8_key(cache) > 0)
			{
				hls_server.flags = 1;
				return 1;
			}
		}
	}
	return 0;
}

void clear_hls_cache(void)
{
	
	key_list_foreach(hls_server.cache_list, cache_node) 
	{
		key_list_delete(hls_server.cache_list, cache_node->key);
	}
	
}
