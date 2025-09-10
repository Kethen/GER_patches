#include "log.h"

pthread_t _log_mutex;

void log_init(){
	pthread_mutex_init(&_log_mutex, NULL);
	FILE *handle = fopen(LOG_PATH, "wb");
	if (handle != NULL){
		fprintf(handle, "");
		fclose(handle);
	}
}
