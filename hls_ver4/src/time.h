#ifndef _TIME_H_
#define _TIME_H_

#include <sys/time.h>
#include <time.h>
 
#include <stdio.h>
#include <string.h>
 
static double difftimeval(const struct timeval *tv1, const struct timeval *tv2)
{
	double d;
	time_t s;
	suseconds_t u;

	s = tv1->tv_sec - tv2->tv_sec;
	u = tv1->tv_usec - tv2->tv_usec;
	if (u < 0)
			--s;

	d = s;
	d *= 1000000.0;
	d += u;

	return d;
}
 
static char *strftimeval(const struct timeval *tv, char *buf)
{
	struct tm      tm;
	size_t         len = 28;

	localtime_r(&tv->tv_sec, &tm);
	strftime(buf, len, "%Y-%m-%d %H:%M:%S", &tm);
	len = strlen(buf);
	sprintf(buf + len, ".%06.6d", (int)(tv->tv_usec));
	return buf;
}
 
static char *getstimeval(char *buf)
{
	struct timeval tv;
	struct tm      tm;
	size_t         len = 28;

	gettimeofday(&tv, NULL);
	localtime_r(&tv.tv_sec, &tm);
	strftime(buf, len, "%Y-%m-%d %H:%M:%S", &tm);
	len = strlen(buf);
	sprintf(buf + len, ".%06.6d", (int)(tv.tv_usec));

	return buf;
}

#endif

/*
	int
main(int argc, char *argv[])
{
        char buf[28];
        struct timeval tv1;
        struct timeval tv2;
 
        gettimeofday(&tv1, NULL);
        printf("%s\n", getstimeval(buf));
        gettimeofday(&tv2, NULL);
 
        printf("%s\n", strftimeval(&tv1, buf));
        printf("%s\n", strftimeval(&tv2, buf));
        printf("%f\n", difftimeval(&tv2, &tv1));
 
        return 0;
}
*/