#ifndef _TS_PROXY_H_
#define _TS_PROXY_H_

int tsproxy_init(char *port);

void tsplay_video(char *url);

char *tsget_url(void);

void stop_tsproxy(void);

int tsproxy_uninit(void);

#endif