#ifndef PLATFORM_METRICS_H
#define PLATFORM_METRICS_H

#include "porfavor_types.h"

#include <assert.h>
#include <sys/types.h>
#include <sys/stat.h>

#if defined(_WIN32) || defined( _WIN64)
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <psapi.h>

static inline u64 GetOSTimerFrequency()
{
    LARGE_INTEGER large_int;
    BOOL retval = QueryPerformanceFrequency(&large_int);
    assert(retval != 0);
    u64 result = large_int.QuadPart;
    return result;
}

static inline u64 ReadOSTimer()
{
    LARGE_INTEGER large_int;
    BOOL retval = QueryPerformanceCounter(&large_int);
    assert(retval != 0);
    u64 result = large_int.QuadPart;
    return result;
}

struct Win32_PlatformMetrics
{
    HANDLE process_handle;
};
static Win32_PlatformMetrics g_PlatformMetrics;

inline static void Win32_InitializePlatformMetrics()
{
    g_PlatformMetrics.process_handle = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, FALSE, GetCurrentProcessId());
    assert(g_PlatformMetrics.process_handle);
}

inline static void Win32_ShutdownPlatformMetrics()
{
    assert(g_PlatformMetrics.process_handle);
    CloseHandle(g_PlatformMetrics.process_handle);
}

inline static u64 ReadOSPageFaultCount()
{
    PROCESS_MEMORY_COUNTERS_EX memory_counters = {};
    memory_counters.cb = sizeof(memory_counters);
    BOOL retval = GetProcessMemoryInfo(g_PlatformMetrics.process_handle, (PROCESS_MEMORY_COUNTERS *)&memory_counters, sizeof(memory_counters));
    assert(retval != 0);
    
    u64 result = memory_counters.PageFaultCount;
    return result;
}
#else
#error Unsupported Platform!
#endif

#ifdef _MSC_VER
#include <intrin.h>
inline static u64 ReadCPUTimer()
{
    return __rdtsc();
}
#else
#error Unsupported Compiler!
#endif

static u64 EstimateCPUTimerFrequency(u64 ms_to_wait)
{
    u64 os_timer_freq = GetOSTimerFrequency();
    u64 os_wait_time = (os_timer_freq*ms_to_wait)/1000;
    
    u64 os_timer_elapsed = 0;
    u64 os_timer_begin = ReadOSTimer();
    
    u64 scope_timer_begin = ReadCPUTimer();
    while (os_timer_elapsed < os_wait_time)
    {
        u64 os_timer_end = ReadOSTimer();
        os_timer_elapsed = os_timer_end - os_timer_begin;
    }
    u64 scope_timer_end = ReadCPUTimer();
    
    u64 scope_timer_elapsed = scope_timer_end - scope_timer_begin;
    u64 scope_timer_freq = (os_timer_freq*scope_timer_elapsed)/os_timer_elapsed;
    
    return scope_timer_freq;
}

inline static u64 GetFileSize(char const *path)
{
    struct __stat64 stat;
    int retval = _stat64(path, &stat);
    assert(retval == 0);
    
    u64 file_size = stat.st_size;
    
    return file_size;
}

#endif // PLATFORM_METRICS_H
