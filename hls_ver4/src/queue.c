#include "queue.h"
#include "log.h"

static void queue_releaser(value_t value)
{
	message_t* msg = (message_t*)value.value;
	free(msg->ptr);
	free(msg);
}

queue_t *init_queue(void)
{
	queue_t *queue = (queue_t *)malloc(sizeof(queue_t));
	
	if(queue == NULL)
	{
		return NULL;
	}
	
	queue->list = key_list_create(queue_releaser);
	queue->read_pos = -1;
	queue->write_pos = -1;
	queue->is_use = 0;
		
	return queue;
}

void free_queue(queue_t *queue)
{
	if(queue == NULL)
	{
		return;
	}
	
	key_list_destroy(queue->list);
	free(queue);
}

void put_queue(queue_t *queue, message_t *message)
{
	if(queue == NULL)
	{
		return;
	}
	
	value_t value;
	value.value = message;
	queue->write_pos++;

	//AKLOGE("call %s %s %d %p\n",__FILE__, __FUNCTION__, __LINE__, queue->list);
	
	key_list_add(queue->list, queue->write_pos, value);
}

int get_queue(queue_t *queue, message_t *message)
{
	if(queue == NULL
		|| message == NULL)
	{
		return -1;
	}
	
	queue->read_pos++;
	
	if(key_list_find_key(queue->list, queue->read_pos))
	{
		value_t value;
		if(key_list_get(queue->list, queue->read_pos, &value) == 0) 
		{
			message_t* msg = (message_t*)value.value;
			message->ptr = (uint8_t *)malloc(msg->len);
			memcpy(message->ptr, msg->ptr, msg->len);
			message->len = msg->len;
			
			key_list_delete(queue->list, queue->read_pos);
			return 1;
		}
		else
		{
			queue->read_pos--;
		}
	}else
	{
		queue->read_pos--;
	}
	
	queue->is_use = 1;
	
	return 0;
}

void use_queue(queue_t *queue)
{
	if(queue == NULL)
	{
		return;
	}
	
	queue->is_use = 1;
}

int has_queue(queue_t *queue)
{
	if(queue == NULL)
	{
		return 0;
	}
	
	return key_list_count(queue->list);
}