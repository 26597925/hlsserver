/*
 *
 * Copyright (c) wenford.li
 * wenford.li <26597925@qq.com>
 *
 */
 
#include <stdio.h>
#include <curl/curl.h>
 
struct memory_stru {
  char 	 *memory;
  size_t size;
};
 
typedef struct http {
	
	CURL *curl;
	
	char *redirect_url;

	char *headers;

	int timeout;

	int location;

} http_t;

http_t *request(char *url);

void add_header(http_t *http, char *header);

void add_params(http_t *http, char *param);

void add_proxy(http_t *http, char *proxy_port, char *proxy_user_password);

void set_proxy_type(http_t *http, long type);

void add_cookie(http_t *http, char *cookie);

void set_refer(http_t *http, char *refer);

void set_user_agent(http_t *http, char *use_agent);

void set_time_out(http_t *http, int tout);

void set_follow_location(http_t *http, int flocation);

void get_redirect_url(http_t *http);

struct memory_stru response(http_t *http);

void stop_http(http_t *http);
