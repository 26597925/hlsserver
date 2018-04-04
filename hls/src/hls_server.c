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
#define AKLOGE(fmt, ...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, fmt, ##__VA_ARGS__)
#define AKLOGI(fmt, ...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, fmt, ##__VA_ARGS__)

typedef struct hls_server {
	
	char *server_name;
	
	int is_stop;
	
	struct mg_mgr mgr;
	
	struct mg_serve_http_opts s_http_server_opts;
	
	pthread_t pthread_id;
	
	key_list_t	*task_list;
	
	char *cur_url;
	
	hls_cache_t *prev_cache;
	
	hls_cache_t *cur_cache;
	
	hls_cache_t *next_cache;
} hls_server_t;

const char *html_index    = "<html><head><title>Welcome to %s!</title><style>body{width:35em;margin:0 auto;font-family:Tahoma,Verdana,Arial,sans-serif;}</style></head><body><h1>Welcome to %s!</h1><p>If you see this page, the server is successfully installed and working.</p><p><em>Thank you for using.Contact me QQ:26597925.</em></p></body></html>";
const char *header_format = "Cache-Control: no-cache\r\nContent-Type: %s\r\nContent-Length: %d\r\n";
static hls_server_t hls_server;

static void start_refresh()
{
   struct itimerval itv;
   itv.it_interval.tv_sec = REFRESH_TIME;
   itv.it_interval.tv_usec = 0;
   itv.it_value.tv_sec = 1;
   itv.it_value.tv_usec = 0;
   setitimer(ITIMER_REAL,&itv,NULL);
}

static void end_refresh()
{
	struct itimerval value;  
	value.it_value.tv_sec = 0;  
	value.it_value.tv_usec = 0;  
	value.it_interval = value.it_value;  
	setitimer(ITIMER_REAL, &value, NULL);  
}

static void refresh_handle(int sig)
{
	if(hls_server.cur_cache != NULL)
	{
		refresh_m3u8_list(hls_server.cur_cache);
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
				
				char header[512];
				char *m3u8 = get_m3u8_cache(hls_server.cur_cache);
				sprintf(header,  header_format, "application/x-mpegURL", strlen(m3u8));
				
				mg_send_response_line(nc, 200, header);
				mg_send(nc, m3u8, strlen(m3u8));
				
			}
			else if(lastIndexOf(uri, ".ts") > -1)
			{
				AKLOGE("call %s %s %d %s",__FILE__, __FUNCTION__, __LINE__, uri);
				ts_buffer_t *ts_buffer = get_ts_cache(hls_server.cur_cache, uri);
				if(ts_buffer != NULL)
				{
					char header[512];
					sprintf(header, header_format, "video/MP2T", ts_buffer->len);
					
					mg_send_response_line(nc, 200, header);
					mg_send(nc, ts_buffer->p, ts_buffer->len);
					
				}else
				{
					mg_http_send_error(nc, 404, NULL);
				}
				
			}else{
				
				char temp[1024];
				sprintf(temp, html_index, hls_server.server_name, hls_server.server_name); 
				mg_send_response_line(nc, 200, "Content-Type: text/html;charset=UTF-8\r\n");
				mg_send(nc, temp, strlen(temp));
				
			}
		}
		else if(!strcmp(method, "POST"))
		{
			AKLOGE("call %s %s %d %s",__FILE__, __FUNCTION__, __LINE__, uri);
			if(hls_server.next_cache == NULL){
				hls_server.next_cache = init_hls_cache();
			}
			
			if (strstr(uri, "/playlist.m3u8"))
			{
				add_m3u8_list(hls_server.next_cache, hm);
			}
			else if(lastIndexOf(uri, ".ts") > -1)
			{
				add_ts_list(hls_server.next_cache, uri, hm);
			}
			
			hls_server.cur_cache = hls_server.next_cache;
			
		}
		nc->flags |= MG_F_SEND_AND_CLOSE;
	}
}

static void *serve(void *mgr) 
{
  while (!hls_server.is_stop) 
  {
	mg_mgr_poll(mgr, 1000);
  }
  return NULL;
}

int start_hls_server(char *port)
{
	struct mg_connection *nc;
	
	mg_mgr_init(&hls_server.mgr, NULL);
	nc = mg_bind(&hls_server.mgr, port, ev_handler);
	mg_set_protocol_http_websocket(nc);

	start_refresh();
	signal(SIGALRM, refresh_handle);
	
	hls_server.server_name = "Hls Server";
	hls_server.is_stop = 0;
	hls_server.pthread_id = mg_start_thread(serve, &hls_server.mgr);
	hls_server.cur_url = NULL;
	hls_server.cur_cache = NULL;
	hls_server.next_cache = NULL;
	hls_server.prev_cache = NULL;
    //AKLOGE("call %s %s %d start_hls_server\n",__FILE__,__FUNCTION__,__LINE__);
	return 0;
}

int stop_hls_server(void)
{
	end_refresh();
	
	hls_server.is_stop = 1;
	pthread_join(hls_server.pthread_id, NULL);
	mg_mgr_free(&hls_server.mgr);
	destory_hls_cache(hls_server.cur_cache);
	AKLOGE("call %s %s %d stop_hls_server\n",__FILE__,__FUNCTION__,__LINE__);
	return 0;
}

int has_cache()
{
	if(hls_server.next_cache != NULL)
	{
		return has_m3u8_key(hls_server.next_cache);
	}
	return 0;
}

void clear_hls_cache(void)
{
	destory_hls_cache(hls_server.prev_cache);
	hls_server.prev_cache = hls_server.next_cache;
	hls_server.next_cache = NULL;
}