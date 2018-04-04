#ifndef _THREAD_H_
#define _THREAD_H_

#include <pthread.h>
#include <stdlib.h>

typedef struct _thread {
    int m_is_started;
    int m_is_interrupted;
    pthread_t m_thread;
}thread_t;

static thread_t *thread_create()
{
    thread_t * thread = (thread_t *)malloc(sizeof(thread_t));
	if(thread == NULL)
		return NULL;
	
    thread->m_is_interrupted = 0;
    thread->m_is_started = 0;
    return thread;
}

static void thread_destroy(thread_t *thread)
{
    if(thread==NULL) {
        return;
    }
    free(thread);
}

static int thread_start(thread_t *thread, void *(*looper)(void *),void *arg)
{
    if(thread==NULL) {
        return -1;
    }
	
    if(thread->m_is_started) {
        return 0;
    }
	
    thread->m_is_interrupted = 0;
	thread->m_thread = (pthread_t) 0;
	pthread_attr_t attr;

	pthread_attr_init(&attr);
	pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
	
    if(pthread_create(&thread->m_thread, &attr, looper, arg) != 0) {
		pthread_attr_destroy(&attr);
        return -1;
    }
	
	pthread_attr_destroy(&attr);
    thread->m_is_started = 1;
    return 0;
}

static int thread_is_started(thread_t *thread)
{
    if(thread==NULL) {
        return 0;
    }
    return thread->m_is_started;
}

static int thread_is_interrupted(thread_t *thread) 
{
    if(thread==NULL) {
        return 1;
    }
	return thread->m_is_interrupted;
}

static int thread_stop(thread_t *thread,void (*interrupter)()) 
{
    if(thread==NULL) {
        return -1;
    }
    if(!thread->m_is_started) {
        return 0;
    }
    thread->m_is_interrupted = 1;
    if(interrupter!=NULL) {
        interrupter();
    }
	pthread_join(thread->m_thread, NULL);
    thread->m_is_started = 0;
    return 0;
}

static int thread_join(thread_t *thread)
{
    if(thread==NULL || !thread->m_is_started) {
        return -1;
    }
    pthread_join(thread->m_thread,NULL);
    return 0;
}

#endif
