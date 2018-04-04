#ifndef _QUEUE_H_
#define _QUEUE_H_

#include "key_list.h"

typedef struct message{
	
	uint8_t *ptr;
	
	size_t len;
	
} message_t;

typedef struct queue {
	
	key_list_t *list;
	
	key_t read_pos;
	
	key_t write_pos;
	
	int is_use;
	
} queue_t;

queue_t *init_queue(void);

void free_queue(queue_t *queue);

void put_queue(queue_t *queue, message_t *message);

int get_queue(queue_t *queue, message_t *message);

void use_queue(queue_t *queue);

int has_queue(queue_t *queue);

#endif