#include <android/log.h>
#include "libavformat/avformat.h"
#include "tsmux.h"

#define LOG_TAG "HLS"
#define AKLOGE(fmt, ...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, fmt, ##__VA_ARGS__)
#define AKLOGI(fmt, ...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, fmt, ##__VA_ARGS__)
#define IO_BUFFER_SIZE 32768

int read_buffer(void *opaque, uint8_t *buf, int buf_size){  
    if(opaque != NULL){  
        int size=sizeof(opaque);
		buf = (uint8_t *) opaque;
        return size;  
    }else{  
        return -1;  
    }
}  

int build_ts(const char* inbuffer, const char* filename)
{
	int ret;
	AVFormatContext* ifmt_ctx=NULL;
	AVFormatContext* ofmt_ctx=NULL;
	AVDictionary **opts;
	
	av_register_all();
	ifmt_ctx = avformat_alloc_context();
    ret = avformat_alloc_output_context2(&ofmt_ctx, NULL, "mpegts", filename); 
	
	const char *buffer;
	int buffer_size = IO_BUFFER_SIZE;
	buffer = av_malloc(buffer_size);
	AVIOContext *avio_in = avio_alloc_context(buffer, IO_BUFFER_SIZE, 0, inbuffer, read_buffer, NULL, NULL);
	
	if(avio_in == NULL) return -1;
	ifmt_ctx->pb = avio_in;
    ifmt_ctx->flags = AVFMT_FLAG_CUSTOM_IO;
	
	if ((ret = avformat_open_input(&ifmt_ctx, NULL, NULL, NULL)) < 0) {  
        AKLOGE("Cannot open input file\n");  
        return ret;
    }
	
	opts = setup_find_stream_info_opts(ifmt_ctx, NULL);
	
    if ((ret = avformat_find_stream_info(ifmt_ctx, opts)) < 0) {  
        AKLOGE("Cannot find stream information\n");  
        return ret;  
    }  
	
	AKLOGE("input_file:%s\n", ifmt_ctx->iformat->name);  
	return ret;
	
}