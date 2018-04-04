/*
 *
 * Copyright (c) wenford.li
 * wenford.li <26597925@qq.com>
 *
*/

#ifndef _PROXY_H_
#define _PROXY_H_

int proxy_init(char *port);

void play_video(char *urls);

char *get_url(void);

void stop_play(void);

int  proxy_uninit(void);

#endif