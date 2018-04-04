#include <pthread.h>
#include <libavformat/avformat.h>
#include "tsserver.h"
#include "mongoose.h"
#include "key_list.h"
#include "log.h"

#define IO_BUFFER_SIZE      65536
#define HEADER_FORMAT "Cache-Control: no-cache\r\nContent-Type: %s\r\nContent-Length: %d\r\n"
#define TS_HEADER_FORMAT   "Cache-Control: no-cache\r\nTransfer-Encoding: chunked\r\nContent-Type: %s\r\n"
#define HTML_INDEX    "<html><head><title>Welcome to %s!</title><style>body{width:35em;margin:0 auto;font-family:Tahoma,Verdana,Arial,sans-serif;}</style></head><body><h1>Welcome to %s!</h1><p>If you see this page, the server is successfully installed and working.</p><p><em>Thank you for using.Contact me QQ:26597925.</em></p></body></html>"
#define APPLE "Apple"
#define SPLIT_URI "/"

typedef struct tsserver {
	char *server_name;
	
	char *port;
	
	struct mg_mgr mgr;
	
	pthread_t pthread_id;
	
	int is_stop;
	
	int queue_index;
	
	key_list_t 	*queue_list;
	
	int sequence;
	
	int flags;
	
	char *m3u8;
	
} tsserver_t;

typedef struct queue_node {

   long long index;

   queue_t *queue;
   
} queue_node_t;

static tsserver_t tsserver;

static void cache_releaser(value_t value)
{
	//AKLOGE("call %s %s %d \n",__FILE__,__FUNCTION__,__LINE__);
    queue_node_t* queue_node = (queue_node_t*)value.value;
	free_queue(queue_node->queue);
    free(queue_node);
	//AKLOGE("call %s %s %d \n",__FILE__,__FUNCTION__,__LINE__);
}

static long long get_uri_index(char *uri)
{
	char *temp =  malloc(strlen(uri));
	strncpy(temp, uri, strlen(uri));
	char *s = strtok(temp, "/");
    long long index = atoll(s);
	free(s);
	free(temp);
	return index;
}

static queue_t *get_queue_node(long long index)
{
	key_list_foreach(tsserver.queue_list, cache_node) 
	{
		queue_node_t *queue_node = (queue_node_t*)cache_node->value.value;
		if(queue_node->index == index)
		{
			return queue_node->queue;
		}
	}
	return NULL;
}

static void hls_window()
{
	int i;
	int duration = 2;
	char *write_buf = (char *)av_malloc(sizeof(char) * 1024);
	
	if (!write_buf) 
	{
		return -1;
	}
	
	sprintf(write_buf, "#EXTM3U\n");
    sprintf(write_buf, "%s#EXT-X-TARGETDURATION:%d\n", write_buf, duration);
    sprintf(write_buf, "%s#EXT-X-MEDIA-SEQUENCE:%d\n", write_buf, tsserver.sequence);
	
	for (i = 0; i < 3; i++) {
        sprintf(write_buf, "%s#EXTINF:%d,\n", write_buf, duration);

        sprintf(write_buf, "%s%d.ts\n", write_buf, tsserver.sequence + i);
    }
	
	//printf("call %s %s %d %p\n",__FILE__,__FUNCTION__,__LINE__, write_buf);
	if(tsserver.m3u8 != NULL)
	{
		free(tsserver.m3u8);
	}
	tsserver.sequence++;
	tsserver.m3u8 = write_buf;
	//printf("call %s %s %d %s\n",__FILE__,__FUNCTION__,__LINE__, tsserver.m3u8);
}

static void clearCache()
{
	key_list_foreach(tsserver.queue_list, cache_node) 
	{
		if(tsserver.queue_index == cache_node->key)
			continue;
		
		queue_node_t *queue_node = (queue_node_t*)cache_node->value.value;
		AKLOGE("call %s %s %d %d\n",__FILE__,__FUNCTION__,__LINE__, queue_node->queue->is_use);
		//key_list_delete(tsserver.queue_list, cache_node->key);
	}
}

static void ev_handler(struct mg_connection *nc, int ev, void *p)
{
	if (ev == MG_EV_HTTP_REQUEST) 
	{
		struct http_message *hm = (struct http_message *) p;
		char uri[100];
		snprintf(uri, sizeof(uri), "%.*s", (int) hm->uri.len, hm->uri.p);
		if (lastIndexOf(uri, ".m3u8") > -1)
		{
			
			char header[512];
			sprintf(header,  HEADER_FORMAT, "application/x-mpegURL", strlen(tsserver.m3u8));
		
			mg_send_response_line(nc, 200, header);
			mg_send(nc, tsserver.m3u8, strlen(tsserver.m3u8));
			nc->flags |= MG_F_SEND_AND_CLOSE;
					
		}else if(lastIndexOf(uri, ".ts") > -1)
		{
			printf("call %s %s %d %.2lf\n",__FILE__,__FUNCTION__,__LINE__, mg_time());
			printf("call %s %s %d %s\n",__FILE__,__FUNCTION__,__LINE__,uri);
			long long index = get_uri_index(uri);
			queue_t *queue = get_queue_node(index);
			
			if(queue == NULL)
			{
				mg_http_send_error(nc, 404, NULL);
			}else
			{
				if(has_queue(queue) > 0)
				{
					message_t *message = malloc(sizeof(message_t));
					int ret = get_queue(queue, message);
					if(ret > 0)
					{
						char header[512];
						sprintf(header,  HEADER_FORMAT, "video/x-mpegts", message->len);
						
						mg_send_response_line(nc, 200, header);
						printf("call %s %s %d %p\n",__FILE__,__FUNCTION__,__LINE__, message->ptr);
						mg_send(nc, message->ptr,  message->len);
						nc->flags |= MG_F_SEND_AND_CLOSE;
						free(message->ptr);
						free(message);
					}else
					{
						mg_http_send_error(nc, 404, NULL);
					}
				}else
				{
					mg_http_send_error(nc, 404, NULL);
				}
			}
		}else
		{	
			char temp[1024];
			sprintf(temp, HTML_INDEX, tsserver.server_name, tsserver.server_name); 
			mg_send_response_line(nc, 200, "Content-Type: text/html;charset=UTF-8\r\n");
			mg_send(nc, temp, strlen(temp));
			nc->flags |= MG_F_SEND_AND_CLOSE;
		}
	}else if(ev ==  MG_EV_TIMER)
	{
		double now = *(double *) p;
		double next = mg_set_timer(nc, 0) + 2;
		hls_window();
		mg_set_timer(nc, next);
	}
}

static void *serve(void *mgr) 
{
	
	struct mg_connection *nc;
	
	mg_mgr_init(&tsserver.mgr, NULL);
	nc = mg_bind(&tsserver.mgr, tsserver.port, ev_handler);
	mg_set_protocol_http_websocket(nc);
	mg_set_timer(nc, mg_time());
	 
	while (!tsserver.is_stop) 
	{
		mg_mgr_poll(&tsserver.mgr, 1000);
	}
	
	mg_mgr_free(&tsserver.mgr);
	
	clear_tscache();
	
	return 0;
}

void start_tsserver(char *port)
{
	int i;
	tsserver.server_name = "Ts Server";
	tsserver.port = port;
	tsserver.is_stop = 0;
	tsserver.flags = 1;
	tsserver.queue_index = 0;
	tsserver.queue_list = key_list_create(cache_releaser);
	tsserver.pthread_id = mg_start_thread(serve, &tsserver.mgr);
	tsserver.sequence = 1;
	tsserver.m3u8 = NULL;
}

void put_queuelist(long long index,  queue_t *queue)
{	
	clearCache();
	
	queue_node_t* queue_node = (queue_node_t*)malloc(sizeof(queue_node_t));
	queue_node->index = index;
	queue_node->queue = queue;
	value_t value;
	value.value = queue_node;
	tsserver.queue_index++;
	key_list_add(tsserver.queue_list, tsserver.queue_index, value);
	tsserver.flags = 0;
}

int has_cache(void)
{
	if(tsserver.flags > 0){
		//clearCache();
		return 0;
	}
	
	if(tsserver.queue_index > 0){
		if(key_list_find_key(tsserver.queue_list, tsserver.queue_index))
		{
			value_t value;
			key_list_get(tsserver.queue_list, tsserver.queue_index, &value);
			queue_node_t *queue_node = (queue_node_t*)value.value;
			if(queue_node == NULL)
			{
				return 0;
			}
			
			queue_t *queue = queue_node->queue;
			if(has_queue(queue) > 0)
			{
				tsserver.sequence = 0;
				tsserver.flags = 1;
				return 1;
			}
		}
	}
	return 0;
}

void reset_tsserver(void)
{
	tsserver.flags = 1;
}

void stop_tsserver(void)
{
	tsserver.is_stop = 1;
	pthread_join(tsserver.pthread_id, NULL);
}

void clear_tscache(void)
{
	key_list_foreach(tsserver.queue_list, cache_node) 
	{
		key_list_delete(tsserver.queue_list, cache_node->key);
	}
}