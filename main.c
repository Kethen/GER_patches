#include <stdarg.h>
#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>

#include <time.h>

#include <MinHook.h>

#include <synchapi.h>
#include <profileapi.h>

#include "log.h"

// qlib::net::ConnectionManager::debugDumpConnections
void __attribute__((thiscall)) dump_connection(uint32_t ctx){
}

void log_func_disabled(const char *fmt, ...){
}

void log_func_no_break(const char *fmt, ...){
	va_list args;
	va_start(args, fmt);
	char log_buf[4096] = {0};
	vsprintf(log_buf, fmt, args);
	va_end(args);
	LOG("GAMELOG: %s\n", log_buf);
}

void log_func(const char *fmt, ...){
	va_list args;
	va_start(args, fmt);
	char log_buf[4096] = {0};
	vsprintf(log_buf, fmt, args);
	va_end(args);
	LOG("GAMELOG: %s", log_buf);
}

void (__attribute__((stdcall))*sleep_orig)(DWORD ms) = NULL;
void __attribute__((stdcall)) sleep_logged(DWORD ms){
	LOG("%s: sleeping %d ms from 0x%x\n", __func__, ms, __builtin_return_address(0));
	sleep_orig(ms);
}

uint64_t tick_per_ms = 0;

static void busynanosleep(uint64_t sleep_time_ns){
	struct timespec begin = {0};
	clock_gettime(CLOCK_MONOTONIC, &begin);
	while(true){
		static struct timespec now = {0};
		clock_gettime(CLOCK_MONOTONIC, &now);
		if (begin.tv_sec != now.tv_sec || now.tv_nsec - begin.tv_nsec > sleep_time_ns){
			break;
		}
		//Sleep(0);
	}
}

void (__attribute__((thiscall))*delay_orig)(uint32_t offset_obj, uint64_t to_delay_ticks) = NULL;
void __attribute__((thiscall))delay(uint32_t offset_obj, uint64_t to_delay_ticks){
	static struct timespec last_call = {0};
	uint64_t to_delay_ms = to_delay_ticks / tick_per_ms;

//	if (to_delay_ms > 14 && to_delay_ms < 19){
	if (true){
		uint64_t to_delay_ns = 0;
		if (to_delay_ms > 14 && to_delay_ms < 19){
			to_delay_ns = 1000000000 / 60;
		}else{
			to_delay_ns = to_delay_ms * 1000000;
		}
		struct timespec now = {0};
		clock_gettime(CLOCK_MONOTONIC, &now);
		uint64_t time_since_last_call = (now.tv_nsec - last_call.tv_nsec);
		if (to_delay_ns > time_since_last_call){
			busynanosleep(to_delay_ns - time_since_last_call);		
		}
		LARGE_INTEGER now_win = {0};
		QueryPerformanceCounter(&now_win);
		((int *)offset_obj)[0x28 / sizeof(int)] = now_win.u.LowPart - ((int *)offset_obj)[0x10 / sizeof(int)];
		((int *)offset_obj)[0x2c / sizeof(int)] = now_win.u.HighPart - ((int *)offset_obj)[0x14 / sizeof(int)] - (int)(((int *)offset_obj)[0x10 / sizeof(int)] > now_win.u.LowPart);	
		clock_gettime(CLOCK_MONOTONIC, &last_call);
	}else{
		delay_orig(offset_obj, to_delay_ticks);
	}
}

void hook(){
	MH_STATUS init_status = MH_Initialize();
	if (init_status != MH_OK && init_status != MH_ERROR_ALREADY_INITIALIZED){
		LOG("%s: minhook init failed, %d\n", __func__, init_status);
		exit(1);
	}

	#define HOOK(target, detour, trampoline){ \
		MH_STATUS ret = MH_CreateHook((void *)target, (void *)detour, (void *)trampoline); \
		if (ret != MH_OK){ \
			LOG("%s: failed creating hook for 0x%x, %d\n", __func__, target, ret); \
			exit(1); \
		} \
		ret = MH_EnableHook((void *)target); \
		if (ret != MH_OK){ \
			LOG("%s: failed enabling hook for 0x%x, %d\n", __func__, target, ret); \
			exit(1); \
		} \
		LOG("%s: hooked 0x%x\n", __func__, target); \
	}

	// PC log
	//HOOK(0x01b1d968, log_func, NULL);
	HOOK(0x01b1d968, log_func_disabled, NULL);
	// qlib/sony/steam logs
	//HOOK(0x018c6f44, log_func_no_break, NULL);
	HOOK(0x018c6f44, log_func_disabled, NULL);
	//HOOK(Sleep, sleep_logged, &sleep_orig);
	HOOK(0x0213350f, delay, &delay_orig);
	// generates connection debug logs, disable it here
	HOOK(0x017ff641, dump_connection, NULL);
}

__attribute__((constructor))
int init(){
	log_init();
	hook();
	uint64_t tick_per_second = 0;
	QueryPerformanceFrequency((void *)&tick_per_second);
	tick_per_ms = tick_per_second / 1000;
	LOG("%s: tick per ms %d\n", __func__, tick_per_ms);
}
