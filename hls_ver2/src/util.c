/*
 *
 * Copyright (c) wenford.li
 * wenford.li <26597925@qq.com>
 *
 */
#include "util.h"

char* join_str(char *s1, char *s2)
{  
    char *result = malloc(strlen(s1)+strlen(s2)+1); 
    if (result == NULL) return NULL;
	
    strcpy(result, s1);  
    strcat(result, s2);
	
    return result;  
}  

char* join_str1(char *s1, char *s2)
{  
    char *str3 = (char *) malloc(strlen(s1) + strlen(s2) + 1); //局部变量，用malloc申请内存  
    if (str3 == NULL) return NULL;  
    char *tempc = str3; //把首地址存下来  
    while (*s1 != '\0') {  
        *str3++ = *s1++;  
    }  
    while ((*str3++ = *s2++) != '\0') {  
        ;  
    }  
    //注意，此时指针c已经指向拼接之后的字符串的结尾'\0' !  
    return tempc;//返回值是局部malloc申请的指针变量，需在函数调用结束后free之  
}

char **split_str(char* s,const char* delim)
{
	char *ts = strdup(s);
	char *p = strtok(s, delim);
	int rows=0;
	while(p!=NULL){
		rows+=1;
		p=strtok(NULL, delim);
	}
	char **str_arr = malloc(sizeof(char)*(rows+1));
	rows = 0;
	p = strtok(ts, delim);
	while(p!=NULL){
		str_arr[rows] = p;
		rows+=1;
		p=strtok(NULL, delim);
	}
	return str_arr;
}

void split(char **arr, char *str, const char *del) 
{  
    char *s = strtok(str, del);  
    while (s != NULL) {  
        *arr++ = s;  
        s = strtok(NULL, del);  
    }  
}

void replaceFirst(char *str1, char *str2, char *str3) 
{  
    char str4[strlen(str1) + 1];  
    char *p;  
    strcpy(str4, str1);  
    if ((p = strstr(str1, str2)) != NULL)/*p指向str2在str1中第一次出现的位置*/ {  
        while (str1 != p && str1 != NULL)/*将str1指针移动到p的位置*/ {  
            str1++;  
        }  
        str1[0] = '/0'; /*将str1指针指向的值变成/0,以此来截断str1,舍弃str2及以后的内容，只保留str2以前的内容*/  
        strcat(str1, str3); /*在str1后拼接上str3,组成新str1*/  
        strcat(str1, strstr(str4, str2) + strlen(str2)); /*strstr(str4,str2)是指向str2及以后的内容(包括str2),strstr(str4,str2)+strlen(str2)就是将指针向前移动strlen(str2)位，跳过str2*/  
    }  
}

void replace(char *str1, char *str2, char *str3) 
{  
    while (strstr(str1, str2) != NULL) {  
        replaceFirst(str1, str2, str3);  
    }  
}

void substring(char *dest, char *src, int start, int end) 
{  
    char *p = src;  
    int i = start;  
    if (start > strlen(src))return;  
    if (end > strlen(src))  
        end = strlen(src);  
    while (i < end) {  
        dest[i - start] = src[i];  
        i++;  
    }  
    dest[i - start] = '\0';  
    return;  
}

char charAt(char *src, int index) {  
    char *p = src;  
    int i = 0;  
    if (index < 0 || index > strlen(src))  
        return 0;  
    while (i < index)i++;  
    return p[i];  
}

int indexOf(char *str1, char *str2) 
{  
    char *p = str1;  
    int i = 0;  
    p = strstr(str1, str2);  
    if (p == NULL)  
        return -1;  
    else {  
        while (str1 != p) {  
            str1++;  
            i++;  
        }  
    }  
    return i;  
}

int lastIndexOf(char *str1, char *str2) 
{
    char *p = str1;  
    int i = 0, len = strlen(str2);  
    p = strstr(str1, str2);  
    if (p == NULL)return -1;  
    while (p != NULL) {  
        for (; str1 != p; str1++)i++;  
        p = p + len;  
        p = strstr(p, str2);  
    }  
    return i;  
}

void ltrim(char *str) 
{  
    int i = 0, j, len = strlen(str);  
    while (str[i] != '/0') {  
        if (str[i] != 32 && str[i] != 9)break; /*32:空格,9:横向制表符*/  
        i++;  
    }  
    if (i != 0)  
        for (j = 0; j <= len - i; j++) {  
            str[j] = str[j + i]; /*将后面的字符顺势前移,补充删掉的空白位置*/  
        }  
}

void rtrim(char *str) 
{  
    char *p = str;  
    int i = strlen(str) - 1;  
    while (i >= 0) {  
        if (p[i] != 32 && p[i] != 9)break;  
        i--;  
    }  
    str[++i] = '/0';  
}

void trim(char *str) 
{  
    ltrim(str);  
    rtrim(str);  
}

void del_char(char* str,char ch)
{
    char *p = str;
    char *q = str;
    while(*q)
    {
        if (*q != ch)
        {
            *p++ = *q;
        }
        q++;
    }
    *p='\0';
}

char* itoa(int num, char* str, int radix)
{/*索引表*/
	char index[] = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ";
	unsigned unum;/*中间变量*/
	int i=0,j,k;
	/*确定unum的值*/
	if(radix==10&&num<0)/*十进制负数*/
	{
		unum=(unsigned)-num;
		str[i++] = '-';
	}
	else unum = (unsigned)num;/*其他情况*/
	/*转换*/
	do{
		str[i++]=index[unum%(unsigned)radix];
		unum/=radix;
	}while(unum);
	str[i]='\0';
	/*逆序*/
	if(str[0]=='-')k=1;/*十进制负数*/
	else k=0;
	char temp;
	for(j=k;j<=(i-1)/2;j++)
	{
		temp=str[j];
		str[j]=str[i-1+k-j];
		str[i-1+k-j]=temp;
	}
	return str;
}

int av_strstart(const char *str, const char *pfx, const char **ptr)
{
    while (*pfx && *pfx == *str) {
        pfx++;
        str++;
    }
    if (!*pfx && ptr)
        *ptr = str;
    return !*pfx;
}

size_t av_strlcpy(char *dst, const char *src, size_t size)
{
    size_t len = 0;
    while (++len < size && *src)
        *dst++ = *src++;
    if (len <= size)
        *dst = 0;
    return len + strlen(src) - 1;
}

size_t av_strlcat(char *dst, const char *src, size_t size)
{
    size_t len = strlen(dst);
    if (size <= len + 1)
        return len + strlen(src);
    return len + av_strlcpy(dst + len, src, size - len);
}

/**
* make absolute url
*/
void ff_make_absolute_url(char *buf, int size, const char *base,
                          const char *rel)
{
    char *sep, *path_query;
    /* Absolute path, relative to the current server */
    if (base && strstr(base, "://") && rel[0] == '/') {
        if (base != buf)
            av_strlcpy(buf, base, size);
        sep = strstr(buf, "://");
        if (sep) {
            /* Take scheme from base url */
            if (rel[1] == '/')
                sep[1] = '\0';
            else {
                /* Take scheme and host from base url */
                sep += 3;
                sep = strchr(sep, '/');
                if (sep)
                    *sep = '\0';
            }
        }
        av_strlcat(buf, rel, size);
        return;
    }
    /* If rel actually is an absolute url, just copy it */
    if (!base || strstr(rel, "://") || rel[0] == '/') {
        av_strlcpy(buf, rel, size);
        return;
    }
    if (base != buf)
        av_strlcpy(buf, base, size);

    /* Strip off any query string from base */
    path_query = strchr(buf, '?');
    if (path_query != NULL)
        *path_query = '\0';

    /* Is relative path just a new query part? */
    if (rel[0] == '?') {
        av_strlcat(buf, rel, size);
        return;
    }

    /* Remove the file name from the base url */
    sep = strrchr(buf, '/');
    if (sep)
        sep[1] = '\0';
    else
        buf[0] = '\0';
    while (av_strstart(rel, "../", NULL) && sep) {
        /* Remove the path delimiter at the end */
        sep[0] = '\0';
        sep = strrchr(buf, '/');
        /* If the next directory name to pop off is "..", break here */
        if (!strcmp(sep ? &sep[1] : buf, "..")) {
            /* Readd the slash we just removed */
            av_strlcat(buf, "/", size);
            break;
        }
        /* Cut off the directory name */
        if (sep)
            sep[1] = '\0';
        else
            buf[0] = '\0';
        rel += 3;
    }
    av_strlcat(buf, rel, size);
}

char* jstringTostring(JNIEnv* env, jstring jstr)
{
       char* rtn = NULL;
       jclass clsstring = (*env)->FindClass(env, "java/lang/String");
       jstring strencode = (*env)->NewStringUTF(env, "utf-8");
       jmethodID mid = (*env)->GetMethodID(env, clsstring, "getBytes", "(Ljava/lang/String;)[B");
       jbyteArray barr= (jbyteArray)(*env)->CallObjectMethod(env, jstr, mid, strencode);
       jsize alen = (*env)->GetArrayLength(env, barr);
       jbyte* ba = (*env)->GetByteArrayElements(env, barr, JNI_FALSE);
       if (alen > 0)
       {
			 rtn = (char*)malloc(alen + 1);
			 memcpy(rtn, ba, alen);
			 rtn[alen] = 0;
       }
       (*env)->ReleaseByteArrayElements(env, barr, ba, 0);
       return rtn;
}

jstring stoJstring(JNIEnv* env, const char* str)
{
	    int strLen = strlen(str);
		jclass     jstrObj   = (*env)->FindClass(env, "java/lang/String");
		jmethodID  methodId  = (*env)->GetMethodID(env, jstrObj, "<init>", "([BLjava/lang/String;)V");
		jbyteArray byteArray = (*env)->NewByteArray(env, strLen);
		jstring    encode    = (*env)->NewStringUTF(env, "utf-8");
		(*env)->SetByteArrayRegion(env, byteArray, 0, strLen, (jbyte*)str);
		return (jstring)(*env)->NewObject(env, jstrObj, methodId, byteArray, encode);
}