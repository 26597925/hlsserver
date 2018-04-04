/*
 *
 * Copyright (c) wenford.li
 * wenford.li <26597925@qq.com>
 *
*/

#ifndef _HLS_PROXY_H_
#define _HLS_PROXY_H_

int  hls_proxy_init(char *port);

void hls_play_video(char *urls);

char *hls_get_url(void);

void hls_stop_play(void);

int  hls_proxy_uninit(void);

#endif