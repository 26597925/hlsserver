#include "hlsenc.h"
#include "log.h"

#define GXTV  "User-Agent: Mozilla/4.0 (compatible; MS IE 6.0; (ziva))\r\nReferer: http://www.gxtv.cn/swf/new_videoplayer.swf\r\n"

static long long get_time()
{
	struct timeval tv;
    gettimeofday(&tv, NULL);
	return ((long long)tv.tv_sec) * 1000 + tv.tv_usec / 1000;	
}

//http://blog.csdn.net/wishfly/article/details/51821731
static int write_buffer(void *opaque, uint8_t *buf, int buf_size)
{  
	message_t *message = (message_t *)opaque;
	message->ptr = realloc(message->ptr, message->len + buf_size + 1);
	memcpy(&(message->ptr[message->len]), buf, buf_size);
	message->len += buf_size;
	message->ptr[message->len] = 0;
	//printf("call %s %s %d %d\n",__FILE__,__FUNCTION__,__LINE__, buf_size);
	return 0;
}

static int hls_mux_init(HLSContext *hls)
{
	AVFormatContext *s = hls->input_avf;
    AVFormatContext *oc;
    int i;

    hls->avf = oc = avformat_alloc_context();
    if (!oc)
        return AVERROR(ENOMEM);

    oc->oformat            = hls->oformat;
    oc->interrupt_callback = s->interrupt_callback;

    for (i = 0; i < s->nb_streams; i++) {
        AVStream *st;
        if (!(st = avformat_new_stream(oc, NULL)))
            return AVERROR(ENOMEM);
        avcodec_copy_context(st->codec, s->streams[i]->codec);
        st->sample_aspect_ratio = s->streams[i]->sample_aspect_ratio;
		st->time_base  = s->streams[i]->time_base;
		st->codec->codec_tag = 0;
		if (oc->oformat->flags & AVFMT_GLOBALHEADER)
		st->codec->flags |= CODEC_FLAG_GLOBAL_HEADER;
    }

    return 0;
}

static void hls_start(HLSContext *hls)
{
    AVFormatContext *s = hls->input_avf;
    AVFormatContext *oc = hls->avf;
    hls->number++;
	
	unsigned char *buffer = (unsigned char *)av_malloc(IO_BUFFER_SIZE);
	message_t *message = av_mallocz(sizeof(message_t) + IO_BUFFER_SIZE);
	AVIOContext *avio_out = avio_alloc_context(buffer, IO_BUFFER_SIZE, AVIO_FLAG_WRITE, message, NULL, &write_buffer, NULL);
	oc->pb = avio_out;
	oc->flags = AVFMT_FLAG_CUSTOM_IO;

    if (oc->oformat->priv_class && oc->priv_data)
        av_opt_set(oc->priv_data, "mpegts_flags", "resend_headers", 0);

}

static int hls_write_header(HLSContext *hls)
{
	int ret, i;
	AVFormatContext *s = hls->input_avf;
    hls->recording_time = hls->time * AV_TIME_BASE;
    hls->start_pts      = AV_NOPTS_VALUE;

    for (i = 0; i < s->nb_streams; i++)
        hls->has_video +=
            s->streams[i]->codec->codec_type == AVMEDIA_TYPE_VIDEO;

    if (hls->has_video > 1)
        av_log(s, AV_LOG_WARNING,
               "More than a single video stream present, "
               "expect issues decoding it.\n");

    hls->oformat = av_guess_format("mpegts", NULL, NULL);

    if (!hls->oformat) {
        ret = AVERROR_MUXER_NOT_FOUND;
        goto fail;
    }
	
    if ((ret = hls_mux_init(hls)) < 0)
        goto fail;

	hls_start(hls);

    if ((ret = avformat_write_header(hls->avf, NULL)) < 0)
        return ret;

fail:
    if (ret) {
        if (hls->avf)
            avformat_free_context(hls->avf);
    }
    return ret;
}

static int write_tsbuffer(HLSContext *hls)
{
	AVFormatContext *oc = hls->avf;
	AVIOContext *avio_out = oc->pb;
	if(avio_out->opaque != NULL)
	{
		avio_flush(avio_out);
		message_t *message = (message_t *) avio_out->opaque;
		if(message->len == 0)
		{
			//AKLOGE("call %s %s %d %s\n",__FILE__,__FUNCTION__,__LINE__, oc->filename);
			av_free(message);
			return 0;
		}
		
		//AKLOGE("call %s %s %d %p\n",__FILE__,__FUNCTION__,__LINE__, message);
		//printf("call %s %s %d %p\n",__FILE__,__FUNCTION__,__LINE__, message);
		put_queue(hls->queue, message);
		av_freep(&avio_out->buffer);
		av_free(avio_out);
		return 1;
	}
	return 0;
}


static int hls_write_packet(HLSContext *hls, AVPacket *pkt)
{
    AVFormatContext *s = hls->input_avf;
	
    AVFormatContext *oc = hls->avf;	
    AVStream *st = s->streams[pkt->stream_index];
    int64_t end_pts = hls->recording_time * hls->number;
    int ret, can_split = 1;

    if (hls->start_pts == AV_NOPTS_VALUE) {
        hls->start_pts = pkt->pts;
        hls->end_pts   = pkt->pts;
    }

   /* if (hls->has_video) {
        can_split = st->codec->codec_type == AVMEDIA_TYPE_VIDEO &&
                    pkt->flags & AV_PKT_FLAG_KEY;
    }
    if (pkt->pts == AV_NOPTS_VALUE)
        can_split = 0;*/

	//AKLOGE("call %s %s %d %lld %lld\n",__FILE__,__FUNCTION__,__LINE__, pkt->pts, end_pts);
	
    if (/*can_split &&*/ av_compare_ts(pkt->pts - hls->start_pts, st->time_base,
		end_pts, AV_TIME_BASE_Q) >= 0)
	{
		//AKLOGE("call %s %s %d %lld\n",__FILE__,__FUNCTION__,__LINE__, get_time());
        av_write_frame(oc, NULL);
		write_tsbuffer(hls);
	
		hls->end_pts = pkt->pts;

        hls_start(hls);

    }

    ret = ff_write_chained(oc, pkt->stream_index, pkt, s, 0);

    return ret;
}

static void close_avf(HLSContext *hls)
{
	AVFormatContext *s = hls->input_avf;
	avformat_close_input(&s);
	hls->input_avf = NULL;
	av_free(hls->input_url);

	AVFormatContext *oc = hls->avf;
	if(oc != NULL)
	{
		AVIOContext *avio_out = oc->pb;
		avio_flush(avio_out);
		av_freep(&avio_out->buffer);
		message_t *message = (message_t *)avio_out->opaque;
		av_free(message->ptr);
		av_free(message);
		av_free(avio_out);
		avformat_free_context(oc);  
	}
	
	//thread_destroy(hls->thread);
	av_free(hls);
}

static void hls_write_trailer(HLSContext *hls)
{
	//AKLOGE("call %s %s %d \n",__FILE__,__FUNCTION__,__LINE__);
	AVFormatContext *s = hls->input_avf;
    AVFormatContext *oc = hls->avf;
    av_write_trailer(oc);
	AVIOContext *avio_out = oc->pb;
	avio_flush(avio_out);
	av_freep(&avio_out->buffer);
	message_t *message = (message_t *)avio_out->opaque;
	av_free(message->ptr);
	av_free(message);
	av_free(avio_out);
	avformat_free_context(oc);  
    hls->avf = NULL;
	
	avformat_close_input(&s);
	hls->input_avf = NULL;
	
	if (hls->vbsf_h264_toannexb != NULL)
	{
		av_bitstream_filter_close(hls->vbsf_h264_toannexb);
		hls->vbsf_h264_toannexb = NULL;
	}
	
	if (hls->vbsf_aac_adtstoasc != NULL)
	{
		av_bitstream_filter_close(hls->vbsf_aac_adtstoasc);
		hls->vbsf_aac_adtstoasc = NULL;
	}
	
	av_free(hls->input_url);
	//thread_destroy(hls->thread);
	av_free(hls);
	//AKLOGE("call %s %s %d \n",__FILE__,__FUNCTION__,__LINE__);
}

static int init_demux(HLSContext *hls)
{
	int i;
	AVDictionary *format_opts = NULL;
	AVFormatContext *input_avf = NULL;
	
	//AKLOGE("call %s %s %d %s\n",__FILE__,__FUNCTION__,__LINE__, hls->input_url);
	char *url = hls->input_url;
	
	if((strncmp(url, "http://", strlen("http://")) 
		|| strncmp(url, "https://", strlen("https://"))))
	{
		
		if(strstr(url, "gxtv.cn") > 0)
		{
			av_dict_set(&format_opts, "headers", GXTV, 0);
		}
		
	}
	
	//AKLOGE("call %s %s %d %lld\n",__FILE__,__FUNCTION__,__LINE__, get_time());
	if (avformat_open_input(&input_avf, hls->input_url, NULL, &format_opts) != 0)
	{
		return -1;
	}
	
	//AKLOGE("call %s %s %d %lld\n",__FILE__,__FUNCTION__,__LINE__, get_time());
	if(avformat_find_stream_info(input_avf, NULL) < 0)
	{
		return -1;
	}
	//AKLOGE("call %s %s %d %lld\n",__FILE__,__FUNCTION__,__LINE__, get_time());
	
	av_dump_format(input_avf, -1, hls->input_url, 0);
	
	for (i = 0; i < input_avf->nb_streams; i++)
	{
		if (input_avf->streams[i]->codec->codec_type == AVMEDIA_TYPE_VIDEO)
		{
			hls->video_stream_idx = i;
		}
		else if (input_avf->streams[i]->codec->codec_type == AVMEDIA_TYPE_AUDIO)
		{
			hls->audio_stream_idx = i;
		}
	}
	
	//AKLOGE("call %s %s %d %s\n",__FILE__, __FUNCTION__, __LINE__, input_avf->iformat->name);
	
	printf("call %s %s %d %s\n",__FILE__, __FUNCTION__, __LINE__, input_avf->iformat->name);
	
	if ((strstr(input_avf->iformat->name, "flv") != NULL) || 
		(strstr(input_avf->iformat->name, "mp4") != NULL) || 
		(strstr(input_avf->iformat->name, "mov") != NULL))
	{
		printf("call %s %s %d %d\n",__FILE__, __FUNCTION__, __LINE__, hls->video_stream_idx);
		if (input_avf->streams[hls->video_stream_idx]->codec->codec_id == AV_CODEC_ID_H264)  //AV_CODEC_ID_H264
		{
			hls->vbsf_h264_toannexb = av_bitstream_filter_init("h264_mp4toannexb"); 
		}
		if (input_avf->streams[hls->audio_stream_idx]->codec->codec_id == AV_CODEC_ID_AAC) //AV_CODEC_ID_AAC
		{
			hls->vbsf_aac_adtstoasc = av_bitstream_filter_init("aac_adtstoasc");
		}
	}
	
	hls->input_avf = input_avf;

	return 0;
}

static void *task_loop(void *arg) 
{	
	int ret;
	AVPacket packet;
	av_init_packet(&packet);
	
	HLSContext *hls = (HLSContext *)arg;
	
	AKLOGE("call %s %s %d \n",__FILE__, __FUNCTION__, __LINE__);
	
	ret = init_demux(hls);
	
	if(ret < 0)
	{
		AKLOGE("call %s %s %d \n",__FILE__, __FUNCTION__, __LINE__);
		close_avf(hls);
		return 0;
	}
	
	ret = hls_write_header(hls);
	
	if(ret < 0)
	{
		AKLOGE("call %s %s %d \n",__FILE__, __FUNCTION__, __LINE__);
		close_avf(hls);
		return 0;
	}
	
	while (!thread_is_interrupted(hls->thread))
	{
		
		AVStream *in_stream, *out_stream;
		
		if (av_read_frame(hls->input_avf, &packet) < 0) 
		{
			break;
		}

		if (av_dup_packet(&packet) < 0) 
		{
			av_free_packet(&packet);
			break;
		}
		
		if (packet.stream_index == hls->video_stream_idx ) 
		{	
			if (hls->vbsf_h264_toannexb != NULL)
			{
				AVStream *ovideo_st = hls->input_avf->streams[hls->video_stream_idx]->codec;
				av_bitstream_filter_filter(hls->vbsf_h264_toannexb, ovideo_st, NULL, &packet.data, &packet.size, packet.data, packet.size, 0); 
			}
		}else if(packet.stream_index == hls->audio_stream_idx)
		{
			if (hls->vbsf_aac_adtstoasc != NULL)
			{
				AVStream *oaudio_st = hls->input_avf->streams[hls->audio_stream_idx]->codec;
				av_bitstream_filter_filter(hls->vbsf_aac_adtstoasc, oaudio_st, NULL, &packet.data, &packet.size, packet.data, packet.size, 0); 
			}
		}
		
		hls_write_packet(hls, &packet);
		
		av_free_packet(&packet);
		
	}
	
	hls_write_trailer(hls);
	
	AKLOGE("call %s %s %d \n",__FILE__, __FUNCTION__, __LINE__);
	
	return 0;
}

HLSContext *init_hlscontex(char *input_url)
{	
	HLSContext *hls = (HLSContext *)av_malloc(sizeof(HLSContext));
	hls->queue = init_queue();
	if(hls->queue == NULL)
	{
		free(hls);
		return NULL;
	}

	hls->input_url = strdup(input_url);
	hls->input_avf = NULL;
	hls->avf = NULL;
	hls->oformat = NULL;
	hls->recording_time = 0;
	hls->has_video = 0;
	hls->video_stream_idx = -1;
	hls->audio_stream_idx = -1;
	hls->vbsf_h264_toannexb = NULL;
	hls->vbsf_aac_adtstoasc = NULL;
	
	hls->time = 2.0f;
	hls->number = 0;
	
	hls->is_stop = 0;
	hls->thread = thread_create();
	
	return hls;
}

void start_slice(HLSContext *hls)
{	
	if(hls == NULL)
		return;
	
	thread_start(hls->thread, task_loop, hls);
}

void stop_slice(HLSContext *hls)
{
	if(hls == NULL)
		return;

	AKLOGE("call %s %s %d %p\n",__FILE__, __FUNCTION__, __LINE__, hls);
	AKLOGE("call %s %s %d %p\n",__FILE__, __FUNCTION__, __LINE__, hls->thread);
	thread_stop(hls->thread, NULL);
	AKLOGE("call %s %s %d \n",__FILE__, __FUNCTION__, __LINE__);
}
