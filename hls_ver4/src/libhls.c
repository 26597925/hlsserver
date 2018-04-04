#define JNI_FALSE false
#define JNI_TRUE true

#include <stdio.h>
#include <ctype.h>
#include <semaphore.h>
#include <sys/syscall.h>
#include <android/log.h>
#include "jni.h"
#include "util.h"
#include "thread.h"
#include "tsproxy.h"

#define LOG_TAG "HLS"
#define BSW_SIGN "4110790856"
#define AKLOGE(fmt, ...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, fmt, ##__VA_ARGS__)
#define AKLOGI(fmt, ...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, fmt, ##__VA_ARGS__)

static const char* kClassPathName = "com/mylove/galaxy/HlsServer";

typedef struct player {
	
	char *url;
	
	thread_t *thread;
	
	pthread_mutex_t mutex;
	
	pthread_cond_t is_wait; 
	
	int flags;
	
} player_t;

static player_t player;
static JavaVM *hls_vm;
static jclass hls_cls = NULL;

static void signal_handler(int sig_num) {

}

static void play_url()
{
	JNIEnv *env;
	(*hls_vm)->AttachCurrentThread(hls_vm, (void **) &env, NULL);
	
	jmethodID hlsmid = (*env)->GetStaticMethodID(env, hls_cls, "startUrl", "(Ljava/lang/String;)V");
	
	if (hlsmid) {
		if(player.url == NULL)
			player.url = "";
		
		jstring jurl = stoJstring(env, player.url);
		(*env)->CallStaticVoidMethod(env, hls_cls, hlsmid, jurl);
	}
	
	(*hls_vm)->DetachCurrentThread(hls_vm);
}

static void *task_loop(void *arg) 
{
	while(!thread_is_interrupted(player.thread))
	{
		if(player.flags){
			player.flags = 0;
			play_url();
			
			pthread_mutex_lock(&(player.mutex));
			pthread_cond_wait(&(player.is_wait), &(player.mutex));
			pthread_mutex_unlock(&(player.mutex));
		}else{
			char *url = tsget_url();
			if(url != NULL && url != player.url){
				player.url = url;
				player.flags = 1;
			}
		}
	}
	return 0;
}

static int validation_sign(JNIEnv* env)
{
	int sign_flag = -1;
	
	jclass theClass = (*env)->FindClass(env,"android/app/ActivityThread");
	jmethodID method = (*env)->GetStaticMethodID(env,theClass,"currentApplication","()Landroid/app/Application;");
	jobject context = (*env)->CallStaticObjectMethod(env,theClass,method);
	if(!context){AKLOGE("[x] failed to get context");}
	
	jclass native_clazz = (*env)->GetObjectClass(env, context);
	jmethodID methodID_func = (*env)->GetMethodID(env, native_clazz,
			"getPackageManager", "()Landroid/content/pm/PackageManager;");
	jobject package_manager = (*env)->CallObjectMethod(env, context, methodID_func);
	jclass pm_clazz = (*env)->GetObjectClass(env, package_manager);
	jmethodID methodID_pm = (*env)->GetMethodID(env, pm_clazz,
			"getPackageInfo", "(Ljava/lang/String;I)Landroid/content/pm/PackageInfo;");
	jmethodID methodID_pmName=(*env)->GetMethodID(env,native_clazz,"getPackageName","()Ljava/lang/String;");
	jstring packageName=(jstring)(*env)->CallObjectMethod(env,context,methodID_pmName);
	jobject package_info = (*env)->CallObjectMethod(env, package_manager,
			methodID_pm, packageName, 64);
	jclass pi_clazz = (*env)->GetObjectClass(env, package_info);
	jfieldID fieldID_signatures = (*env)->GetFieldID(env, pi_clazz,
			"signatures", "[Landroid/content/pm/Signature;");
	jobjectArray signatures = (*env)->GetObjectField(env, package_info, fieldID_signatures);
	jobject signature = (*env)->GetObjectArrayElement(env, signatures, 0);
	jclass s_clazz = (*env)->GetObjectClass(env, signature);
	jmethodID methodID_hc = (*env)->GetMethodID(env, s_clazz, "hashCode", "()I");
	int hash_code = (*env)->CallIntMethod(env, signature, methodID_hc);
	char str[10];
	sprintf(str, "%u", hash_code);
	if(strcmp(BSW_SIGN, str) == 0)
	{
		sign_flag=1;
	}
	return sign_flag;
}

static jint native_startServer(JNIEnv* env, jobject thiz, jstring jparam){
	
	char *cparam = jstringTostring(env, jparam);
	char **params = split_str(cparam, "#");
	tsproxy_init(params[0]);
	
	player.flags = 0;
	player.url = NULL;
	player.thread = thread_create();
	pthread_mutex_init(&(player.mutex), NULL);
	thread_start(player.thread, task_loop, NULL);
	
	return 1;
}

static void native_stopPlay(JNIEnv* env, jobject thiz){
	stop_tsproxy();
}

static void native_playUrl(JNIEnv* env, jobject thiz, jstring jurl){
	player.flags = 0;
	player.url = NULL;
	char *curl = jstringTostring(env, jurl);

	if(curl == NULL || strlen(curl) == 0)
		return;
	
	tsplay_video(curl);
	pthread_cond_broadcast(&(player.is_wait));
	
}

static jint native_stopServer(JNIEnv* env, jobject thiz){
	
	tsproxy_uninit();
	
	thread_stop(player.thread, NULL);
	pthread_mutex_destroy(&(player.mutex)); 
    thread_destroy(player.thread);
	
	if(hls_cls != NULL){
		(*env)->DeleteGlobalRef(env, hls_cls);
		hls_cls = NULL;
	}
	
	return 1;
}

static jstring native_probeUrl(JNIEnv* env, jobject thiz, jstring jurl){
	
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
	
	if(hls_cls != NULL){
		(*env)->DeleteGlobalRef(env, hls_cls);
		hls_cls = NULL;
	}
	
	hls_cls = (*env)->NewGlobalRef(env, clazz);
	
	if ((*env)->RegisterNatives(env, clazz, gMethods,  sizeof(gMethods) / sizeof(gMethods[0])) < 0) {
		AKLOGE("RegisterNatives failed for '%s'", kClassPathName);
		(*env)->DeleteLocalRef(env, clazz);
		return JNI_FALSE;
	}
	(*env)->DeleteLocalRef(env, clazz);
	return JNI_TRUE;
}

jint JNI_OnLoad(JavaVM *vm, void* reserved)
{
    JNIEnv *env = NULL;
    jint result = -1;
	
	hls_vm = vm;
	
	if ((*vm)->GetEnv(vm, (void**)&env, JNI_VERSION_1_4) != JNI_OK)
	{
           AKLOGE("ERROR: GetEnv failed");
           goto bail;
    }
	
	/*if(validation_sign(env) < 0)
	{
		AKLOGE("love native registration failed");
        goto bail;
	}

	signal(SIGINT, signal_handler);
	signal(SIGTERM, signal_handler);
	signal(SIGSEGV, signal_handler);*/

    if (register_mylove(env) < 0) 
	{
    	AKLOGE("love native registration failed");
        goto bail;
    }
	
    result = JNI_VERSION_1_4;
	
bail:
    return result;
}

