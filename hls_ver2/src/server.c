#include "mongoose.h"
#include "scache.h"
#include "server.h"
#include "http.h"

#define URLSIZE 1024
#define HEADER_FORMAT "Content-Type: %s\r\nConnection: close"
#define HTML_INDEX    "<html><head><title>Welcome to %s!</title><style>body{width:35em;margin:0 auto;font-family:Tahoma,Verdana,Arial,sans-serif;}</style></head><body><h1>Welcome to %s!</h1><p>If you see this page, the server is successfully installed and working.</p><p><em>Thank you for using.Contact me QQ:26597925.</em></p></body></html>"

typedef struct server {
	
	char *server_name;
	
	char *port;
	
	int is_stop;
	
	struct mg_mgr mgr;
	
	struct mg_serve_http_opts s_http_server_opts;
	
	pthread_t pthread_id;
	
	int cache_index;
	
	key_list_t *cache_list;
	
} server_t;

typedef struct buffer {

   long long index;
   
   scache_t *scache;
   
} buffer_t;


static server_t s_server;

static void buffer_releaser(value_t value)
{
    buffer_t* buffer = (buffer_t*)value.value;
	destroy_cache(buffer->scache);
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

static scache_t *get_scache(long long index)
{
	key_list_foreach(s_server.cache_list, cache_node) 
	{
		buffer_t *buffer = (buffer_t*)cache_node->value.value;
		if(buffer->index == index)
		{
			return buffer->scache;
		}
	}
	return NULL;
}

static void clearCache(long long index){
	key_list_foreach(s_server.cache_list, cache_node) 
	{
		buffer_t *buffer = (buffer_t*)cache_node->value.value;
		if(buffer->index == index 
			|| s_server.cache_index == cache_node->key)
			continue;
		
		key_list_delete(s_server.cache_list, cache_node->key);
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
		if (!strcmp(method, "GET"))
		{
			if(lastIndexOf(uri, "playlist.m3u8") > -1)
			{	
				//char url[1024], desurl[1024];
				char *url = malloc(URLSIZE);
				char *desurl = malloc(URLSIZE);
				mg_get_http_var(&hm->query_string, "url", url, URLSIZE);
				mg_base64_decode(url, URLSIZE, desurl);
				
				long long index = get_uri_index(uri);
				
				scache_t *scache = get_scache(index);
				
				if(scache == NULL)
				{
					scache = init_scache(desurl);
					buffer_t* buffer = (buffer_t*)malloc(sizeof(buffer_t));
					buffer->index = index;
					buffer->scache = scache;
					value_t value;
					value.value = buffer;
					s_server.cache_index++;
					key_list_add(s_server.cache_list, s_server.cache_index, value);
				}
				
				char *result = request_url(scache);
				
				if(result != NULL)
				{
					char header[512];
					sprintf(header,  HEADER_FORMAT, "application/vnd.apple.mpegurl");
				
					AKLOGE("%s, %s, %d , %s \n", __FILE__,__FUNCTION__,__LINE__, result);
					
					mg_send_head(nc, 200, strlen(result), header);
					mg_send(nc, result, strlen(result));
					nc->flags |= MG_F_SEND_AND_CLOSE;
					
					free(result);
					
				}else
				{
					mg_http_send_error(nc, 404, "Open failed");
				}
				
				free(url);
				free(desurl);
				
			}else if(lastIndexOf(uri, "playlist.ts") > -1)
			{
				
				long long index = get_uri_index(uri);
				char tsid[10];
				mg_get_http_var(&hm->query_string, "tsid", tsid, sizeof(tsid));
				scache_t *scache = get_scache(index);
				
				if(scache != NULL)
				{
					struct memory_stru result = get_cache(scache, tsid);
					
					if(result.size == 0 || result.memory == NULL)
					{
						mg_http_send_error(nc, 404, "Open failed");
					}else
					{
						char header[512];
						sprintf(header, HEADER_FORMAT, "video/MP2T");
							
						mg_send_head(nc, 200, result.size, header);
						//mg_send_http_chunk
						mg_send(nc, result.memory, result.size);
						free(result.memory);
						result.size = 0;
						nc->flags |= MG_F_SEND_AND_CLOSE;
						//clearCache(index);
					}
				}
				else
				{
					mg_http_send_error(nc, 404, "Open failed");
				}
			}else
			{
				char temp[1024];
				sprintf(temp, HTML_INDEX, s_server.server_name, s_server.server_name); 
				mg_send_response_line(nc, 200, "Content-Type: text/html;charset=UTF-8\r\n");
				mg_send(nc, temp, strlen(temp));
				nc->flags |= MG_F_SEND_AND_CLOSE;
			}
		}
	}
}

static void *serve(void *mgr) 
{
	struct mg_connection *nc;
	
	mg_mgr_init(&s_server.mgr, NULL);
	nc = mg_bind(&s_server.mgr, s_server.port, ev_handler);
	mg_set_protocol_http_websocket(nc);
	
	while (!s_server.is_stop) 
	{
		mg_mgr_poll(&s_server.mgr, 1000);
	}
	
	mg_mgr_free(&s_server.mgr);
	
	return 0;
}

void start_server(char *port)
{
	s_server.server_name = "Speed Server";
	s_server.port = port;
	s_server.is_stop = 0;
	s_server.cache_index = 0;
	s_server.cache_list = key_list_create(buffer_releaser);
	s_server.pthread_id = mg_start_thread(serve, NULL);
}

void stop_server(void)
{
	s_server.is_stop = 1;
	pthread_join(s_server.pthread_id, NULL);
}

void clear_server(void)
{
	key_list_foreach(s_server.cache_list, cache_node) 
	{
		key_list_delete(s_server.cache_list, cache_node->key);
	}
	
	key_list_destroy(s_server.cache_list);
}