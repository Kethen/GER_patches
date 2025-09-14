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
	struct timespec begin = {0};
	clock_gettime(CLOCK_MONOTONIC, &begin);
	va_list args;
	va_start(args, fmt);
	char log_buf[4096] = {0};
	vsprintf(log_buf, fmt, args);
	va_end(args);
	LOG("GAMELOG: %000d.%000000000d 0x%x thread %u %s\n", begin.tv_sec, begin.tv_nsec, __builtin_return_address(0), GetCurrentThreadId(), log_buf);
}

void log_func_with_level_disabled(uint32_t level, char *fmt, ...){
}

void log_func_with_level(uint32_t level, char *fmt, ...){
	struct timespec begin = {0};
	clock_gettime(CLOCK_MONOTONIC, &begin);
	va_list args;
	va_start(args, fmt);
	char log_buf[4096] = {0};
	vsprintf(log_buf, fmt, args);
	va_end(args);
	LOG("GAMELOG_LVL: %000d.%000000000d 0x%x thread %u (0x%x) %s\n", begin.tv_sec, begin.tv_nsec, __builtin_return_address(0), GetCurrentThreadId(), level, log_buf);
}

void log_func(const char *fmt, ...){
	struct timespec begin = {0};
	clock_gettime(CLOCK_MONOTONIC, &begin);
	va_list args;
	va_start(args, fmt);
	char log_buf[4096] = {0};
	vsprintf(log_buf, fmt, args);
	va_end(args);
	LOG("GAMELOG: %000d.%000000000d 0x%x thread %u %s", begin.tv_sec, begin.tv_nsec, __builtin_return_address(0), GetCurrentThreadId(), log_buf);
}

void (__attribute__((stdcall))*sleep_orig)(DWORD ms) = NULL;
void __attribute__((stdcall)) sleep_logged(DWORD ms){
	LOG("%s: sleeping %d ms from 0x%x\n", __func__, ms, __builtin_return_address(0));
	sleep_orig(ms);
}

uint64_t tick_per_ms = 0;

void busynanosleep(uint64_t sleep_time_ns){
	struct timespec begin = {0};
	clock_gettime(CLOCK_MONOTONIC, &begin);

	#if 1
	// only busyloop the last 1ms to encourage cpu time
	if (sleep_time_ns > 1000000){
		struct timespec sleep_time = {
			.tv_sec = 0,
			.tv_nsec = sleep_time_ns - 1000000,
		};
	}
	#endif

	while(true){
		static struct timespec now = {0};
		clock_gettime(CLOCK_MONOTONIC, &now);
		if (begin.tv_sec != now.tv_sec || now.tv_nsec - begin.tv_nsec > sleep_time_ns){
			break;
		}
		#if 0
		Sleep(0);
		#endif
	}
}

void normalnanosleep(uint64_t sleep_time_ns){
	struct timespec sleep_time = {
		.tv_sec = 0,
		.tv_nsec = sleep_time_ns
	};
	nanosleep(&sleep_time, NULL);
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
			#if 1
			busynanosleep(to_delay_ns - time_since_last_call);		
			#else
			normalnanosleep(to_delay_ns - time_since_last_call);
			#endif
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


DWORD (__attribute__((stdcall)) *WaitForSingleObject_orig)(HANDLE handle, DWORD timeout) = NULL;
DWORD __attribute__((stdcall)) WaitForSingleObject_hook(HANDLE handle, DWORD timeout){
	DWORD ret = WaitForSingleObject_orig(handle, timeout);
	uint32_t ret_addr = (uint32_t)__builtin_return_address(0);
	if (timeout != 0xFFFFFFFF && ret_addr < 0x10000000){
		LOG("%s: waiting for handle 0x%x for %d ms from 0x%x thread %u\n", __func__, handle, timeout, __builtin_return_address(0), GetCurrentThreadId());
	}
	return ret;
}

int (*wait_for_vblank_orig)(int blanks) = NULL;
int wait_for_vblank_hook(int blanks){
	#if 0
	LOG("%s: waiting for %d frames from 0x%x\n", __func__, blanks, __builtin_return_address(0));
	int ret = wait_for_vblank_orig(blanks);
	LOG("%s: ret %d\n", __func__, ret);
	return ret;
	#else
	return 1;
	#endif
}

uint32_t (*maybe_game_tick_orig)(uint32_t unk1, uint32_t unk2, uint32_t unk3) = NULL;
uint32_t maybe_game_tick_hook(uint32_t unk1, uint32_t unk2, uint32_t unk3){
	//LOG("%s: unk3 %u\n", __func__, unk3);
	uint32_t ret = maybe_game_tick_orig(unk1, unk2, unk3);
	return ret;
}

uint32_t (*run_or_cancel_task_orig)(uint32_t task_obj, uint32_t unk1, uint32_t unk2) = NULL;
uint32_t run_or_cancel_task_hook(uint32_t task_obj, uint32_t unk1, uint32_t unk2){
	// guessing param and param size on unk1 and unk2
	//LOG("%s: task type 0x%x current lock 0x%x task sleep ticks 0x%x\n", __func__, *(uint32_t*)(task_obj + 0x80), *(uint32_t*)0x01632e00, *(uint32_t*)(task_obj + 0x80));
	// force unlock networking
	*(uint32_t*)0x01632e00 = *(uint32_t*)0x01632e00 & (~0x10);
	uint32_t ret = run_or_cancel_task_orig(task_obj, unk1, unk2);
	return ret;
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
	// also PC log?
	//HOOK(0x01b1deaa, log_func, NULL);
	HOOK(0x01b1deaa, log_func_disabled, NULL);
	// qlib networking trace
	//HOOK(0x018c6ef4, log_func_no_break, NULL);
	HOOK(0x018c6ef4, log_func_disabled, NULL);
	// leveled logs?
	//HOOK(0x01b1d928, log_func_with_level, NULL);
	HOOK(0x01b1d928, log_func_with_level_disabled, NULL);
	// iskra1 trace
	HOOK(0x01b1d8e8, log_func_disabled, NULL);


	//HOOK(Sleep, sleep_logged, &sleep_orig);
	HOOK(0x0213350f, delay, &delay_orig);
	// generates connection debug logs, disable it here
	HOOK(0x017ff641, dump_connection, NULL);

	HOOK(0x01b17f2e, wait_for_vblank_hook, &wait_for_vblank_orig);

	//HOOK(0x01ee9af3, maybe_game_tick_hook, &maybe_game_tick_orig);

	//HOOK(0x01ee9edb, run_or_cancel_task_hook, &run_or_cancel_task_orig);

	#if 0
	HANDLE kernel32 = LoadLibraryA("kernel32.dll");
	void *wait_for_single_object = (void *)GetProcAddress(kernel32, "WaitForSingleObject");
	HOOK(wait_for_single_object, WaitForSingleObject_hook, &WaitForSingleObject_orig);
	#endif
}

__attribute__((constructor))
int init(){
	log_init();
	hook();
	uint64_t tick_per_second = 0;
	QueryPerformanceFrequency((void *)&tick_per_second);
	tick_per_ms = tick_per_second / 1000;
	LOG("%s: tick per ms %d\n", __func__, tick_per_ms);
	struct timespec test;
	LOG("%s: tv_sec size %u tv_nsec size %u\n", __func__, sizeof(test.tv_sec), sizeof(test.tv_nsec));
}
