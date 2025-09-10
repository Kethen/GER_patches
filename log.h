#ifndef __LOG_H
#define __LOG_H

#include <stdio.h>
#include <pthread.h>

extern pthread_t _log_mutex;

#define LOG_PATH "GER_patches.log"

#define LOG(...) { \
	pthread_mutex_lock(&_log_mutex); \
	FILE *_handle = fopen(LOG_PATH, "ab"); \
	if (_handle != NULL){ \
		fprintf(_handle, __VA_ARGS__); \
		fclose(_handle); \
	} \
	pthread_mutex_unlock(&_log_mutex); \
}

void log_init();

#endif
