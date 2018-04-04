/*
 *
 * Copyright (c) wenford.li
 * wenford.li <26597925@qq.com>
 *
 */

#ifndef _HLS_SERVER_H_
#define _HLS_SERVER_H_

int start_hls_server(char *port);

int stop_hls_server(void);

int has_cache();

void clear_cache(void);

#endif