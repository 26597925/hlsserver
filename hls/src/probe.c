#include <sys/types.h>
#include <sys/time.h>
#include <sys/resource.h>

#include "libavcodec/avcodec.h"  
#include "libavformat/avformat.h"
#include "thread.h"
#include "probe.h"

typedef struct probe_task {
	
	thread_t *thread;
	
	char *urls;
	
	char *split;
	
} probe_task_t;

static probe_task_t probe_task;

static void (*request_url)(char *url);

static double probe_time(char *url)
{	
	double ret = -1;
	time_t start_time = time(0);

	AVFormatContext *ifmt_ctx = NULL;
	AVDictionary *format_opts = NULL;
	AVDictionary **opts;
	
	av_register_all();//注册组件
    avformat_network_init();//支持网络流
    ifmt_ctx = avformat_alloc_context();

	av_dict_set(&format_opts, "timeout", "5", 0);
	
	
	if(avformat_open_input(&ifmt_ctx, url, NULL, &format_opts) < 0 )
	{
		ret = -1;
		goto back;
    }
	
	opts = setup_find_stream_info_opts(ifmt_ctx, NULL);
	
	if(avformat_find_stream_info(ifmt_ctx, opts) < 0){
		ret = -1;
		goto back;
	}
	
	if(strcmp(ifmt_ctx->iformat->name, "hls,applehttp")){
		int i;
		for ( i = 0; i < ifmt_ctx->nb_streams; i++) {
			AVStream *stream;
			AVCodecContext *codec_ctx;
			stream = ifmt_ctx->streams[i];
			codec_ctx = stream->codec;
			
			if(codec_ctx->codec_type == AVMEDIA_TYPE_AUDIO || codec_ctx->codec_type == AVMEDIA_TYPE_VIDEO){
				if(!(codec_ctx->codec_id == CODEC_ID_MPEG1VIDEO
					|| codec_ctx->codec_id == CODEC_ID_MPEG2VIDEO
					|| codec_ctx->codec_id == CODEC_ID_MPEG4
					|| codec_ctx->codec_id == CODEC_ID_H264
					|| codec_ctx->codec_id == CODEC_ID_MP2
					|| codec_ctx->codec_id == CODEC_ID_MP3
					|| codec_ctx->codec_id == CODEC_ID_AAC
					|| codec_ctx->codec_id == CODEC_ID_AC3
				)){
					ret = -1;
					goto back;
				}
			}
		}
	}
	
	time_t end_time = time(0);
	ret = difftime(end_time, start_time);
	
back:
    avformat_close_input(&ifmt_ctx);
    avformat_network_deinit();
	
	return ret;
}

static void* task_loop(void* arg) 
{
	probe_task_t *probe_task = (probe_task_t *)arg;
	char *url = strtok(probe_task->urls, probe_task->split);  
    while (url != NULL) {  
		double time = probe_time(url);
		if(time != -1){
			if (request_url){
				request_url(url);
				url = NULL;
				break;
			}
		}
        url = strtok(NULL, probe_task->split);  
    }
	
	return 0;
}

void register_probe(void (*cb)(char *url))
{
	request_url = cb;
}

void probe_url(char *urls)
{	
    probe_task.thread = thread_create();
	probe_task.urls = urls;
	probe_task.split = ",";
	
	thread_start(probe_task.thread, task_loop, &probe_task);
}

void destroy_probe()
{
	thread_stop(probe_task.thread, NULL);
    thread_destroy(probe_task.thread);
}
