#ifndef __HLSSENC_H__
#define __HLSSENC_H__

#include <float.h>
#include <stdint.h>

#include "libavutil/mathematics.h"
#include "libavutil/parseutils.h"
#include "libavutil/avstring.h"
#include "libavutil/opt.h"
#include "libavutil/log.h"
#include "libavformat/avformat.h"
#include "queue.h"
#include "thread.h"

#define IO_BUFFER_SIZE      32768

typedef struct HLSContext {
	char *input_url;
	AVFormatContext *input_avf;
	int video_stream_idx;
	int audio_stream_idx;
	AVBitStreamFilterContext *vbsf_h264_toannexb;
	AVBitStreamFilterContext *vbsf_aac_adtstoasc;
   
    AVOutputFormat *oformat;
    AVFormatContext *avf;
    int64_t recording_time;
    int has_video;
    int64_t start_pts;
    int64_t end_pts;
	
	float time;
	unsigned number;
	queue_t *queue;
	
	int is_stop;
	thread_t *thread;
	
} HLSContext;

HLSContext *init_hlscontex(char *input_url);

void start_slice(HLSContext *hls);

void stop_slice(HLSContext *hls);

#endif
