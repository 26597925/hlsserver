#define JNI_FALSE false
#define JNI_TRUE true

#include <stdio.h>
#include <cassert>
#include <semaphore.h>
#include <sys/syscall.h>
#include <android/log.h>
#include "jni.h"
#include "util.h"
#include "thread.h"
#include "probe.h"
#include "hls_proxy.h"

#define LOG_TAG "HLS"
#define TAG_TXT "WENFORD.LI|26597925@qq.com"
#define AKLOGE(fmt, ...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, fmt, ##__VA_ARGS__)
#define AKLOGI(fmt, ...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, fmt, ##__VA_ARGS__)

/*HLS - Apple HTTP live streaming - m3u8 
http://devimages.apple.com/iphone/samples/bipbop/bipbopall.m3u8
http://devimages.apple.com/iphone/samples/bipbop/gear1/prog_index.m3u8
http://meta.video.qiyi.com/255/dfbdc129b8d18e10d6c593ed44fa6df9.m3u8
http://3glivehntv.doplive.com.cn/video1/index_128k.m3u8

HTTP 
http://www.modrails.com/videos/passenger_nginx.mov
http://wsmp32.bbc.co.uk/

RTSP
http://m.livestream.com (site)
rtsp://xgrammyawardsx.is.livestream-api.com/livestreamiphone/grammyawards

MMS
mms://video.fjtv.net/setv
mms://ting.mop.com/mopradio
mms://112.230.192.196/zb12
*/

static const char* kClassPathName = "com/mylove/galaxy/HlsServer";

typedef struct player {
	
	jclass hlsclass;
	
	jmethodID hlsmid;
	
	char *url;
	
	sem_t sem;
	
	thread_t *thread;
	
	int flags;
	
} player_t;

typedef struct probe {
	
	sem_t sem;
	
	char *url;
	
} probe_t;

static player_t player;
static probe_t probe;

static void request_url(char* url)
{
	probe.url = url;
	sem_post(&probe.sem);
}

static void *task_loop(void *arg) 
{
	while(!thread_is_interrupted(player.thread))
	{
		char *url = hls_get_url();
		if(player.flags){
			player.flags = 0;
			sem_post(&player.sem);
		}
		if(url != NULL && url != player.url){
			player.url = url;
			player.flags = 1;
		}
	}
	return 0;
}

static jint native_startServer(JNIEnv* env, jobject thiz, jstring jparam){
	
	char *cparam = jstringTostring(env, jparam);
	char **params = split_str(cparam, "#");
	hls_proxy_init(params[0]);
	
	player.flags = 0;
	player.url = NULL;
	player.thread = thread_create();
	sem_init(&player.sem, 0, 0);
	thread_start(player.thread, task_loop, NULL);
	
	sem_init(&probe.sem, 0, 0);
	register_probe(request_url);
	
	return 1;
}

static void native_playUrl(JNIEnv* env, jobject thiz, jstring jurl){
	char *curl = jstringTostring(env, jurl);
	hls_play_video(curl);
	
	sem_wait(&player.sem);
	jurl = stoJstring(env, player.url);
	(*env)->CallStaticVoidMethod(env, player.hlsclass, player.hlsmid, jurl);
}

static void native_stopPlay(JNIEnv* env, jobject thiz){
	hls_stop_play();
}

static jint native_stopServer(JNIEnv* env, jobject thiz){
	hls_proxy_uninit();
	
	thread_stop(player.thread, NULL);
    thread_destroy(player.thread);
	
	sem_destroy(&player.sem);
	sem_destroy(&probe.sem);
	return 1;
}

static jstring native_probeUrl(JNIEnv* env, jobject thiz, jstring jurl){
	
	char *url = jstringTostring(env, jurl);
	probe_url(url);
	
	sem_wait(&probe.sem);
	jurl = stoJstring(env, probe.url);
	destroy_probe();
	return jurl;
}

static JNINativeMethod gMethods[] = {
    {"startServer",	"(Ljava/lang/String;)I", (void *)native_startServer},
	{"playUrl",	"(Ljava/lang/String;)V",	(void *)native_playUrl},
	{"stopPlay",	"()V",	(void *)native_stopPlay},
	{"stopServer",	"()I",	(void *)native_stopServer},
	{"probeUrl", "(Ljava/lang/String;)Ljava/lang/String;", (void *)native_probeUrl},
};

static int register_mylove(JNIEnv *env)
{
	jclass clazz = (*env)->FindClass(env, kClassPathName);
	if (clazz == NULL) {
		AKLOGE("Native registration unable to find class '%s'", kClassPathName);
		return JNI_FALSE;
	}
	if ((*env)->RegisterNatives(env, clazz, gMethods,  sizeof(gMethods) / sizeof(gMethods[0])) < 0) {
		AKLOGE("RegisterNatives failed for '%s'", kClassPathName);
		(*env)->DeleteLocalRef(env, clazz);
		return JNI_FALSE;
	}
	AKLOGE("call %s %s %d \n",__FILE__,__FUNCTION__,__LINE__);
	player.hlsclass = clazz;
	player.hlsmid = (*env)->GetStaticMethodID(env, clazz, "startUrl", "(Ljava/lang/String;)V");
	if (player.hlsmid == NULL) {
		return JNI_FALSE;
	}
	(*env)->DeleteLocalRef(env, clazz);
	return JNI_TRUE;
}

jint JNI_OnLoad(JavaVM *vm, void* reserved)
{
    JNIEnv* env = NULL;
    jint result = -1;
	
	if ((*vm)->GetEnv(vm, (void**)&env, JNI_VERSION_1_4) != JNI_OK)
	{
           AKLOGE("ERROR: GetEnv failed");
           goto bail;
    }
    assert(env);

    if (register_mylove(env) < 0) {
    	AKLOGE("love native registration failed");
        goto bail;
    }
	
    result = JNI_VERSION_1_6;
	
bail:
    return result;
}

