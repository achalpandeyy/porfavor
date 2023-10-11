#include <assert.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>

typedef uint64_t u64;
typedef double f64;

#ifdef _WIN32
#include <intrin.h>
#define WIN32_LEAN_AND_MEAN
#include <windows.h>

static inline u64 GetOSTimerFrequency()
{
    LARGE_INTEGER large_int;
    BOOL retval = QueryPerformanceFrequency(&large_int);
    assert(retval != 0);
    u64 result = large_int.QuadPart;
    return result;
}

static inline u64 GetOSTimer()
{
    LARGE_INTEGER large_int;
    BOOL retval = QueryPerformanceCounter(&large_int);
    assert(retval != 0);
    u64 result = large_int.QuadPart;
    return result;
}
#endif

int main(int argc, char **argv)
{
    u64 ms_to_wait = 1000;
    if (argc == 2)
    {
        ms_to_wait = atol(argv[1]);
    }
    else if (argc > 2)
    {
        fprintf(stdout, "Usage: estimate_processor_frequency_rdtsc.exe [milliseconds]");
        return -1;
    }
    
    fprintf(stdout, "Milliseconds to wait: %llu\n", ms_to_wait);
    
    u64 os_timer_freq = GetOSTimerFrequency();
    u64 os_wait_time = (os_timer_freq*ms_to_wait)/1000;
    
    u64 os_timer_begin = GetOSTimer();
    u64 os_timer_elapsed = 0;
    
    u64 cpu_timer_begin = __rdtsc();
    while (os_timer_elapsed < os_wait_time)
    {
        u64 os_timer_end = GetOSTimer();
        os_timer_elapsed = os_timer_end - os_timer_begin;
    }
    u64 cpu_timer_end = __rdtsc();
    
    u64 cpu_timer_elapsed = cpu_timer_end - cpu_timer_begin;
    u64 cpu_timer_freq = os_timer_freq*cpu_timer_elapsed/(os_timer_elapsed);
    
    fprintf(stdout, "Processor Frequency (estimate): %.9f GHz\n", (f64)cpu_timer_freq/1000000000.0);
    
    return 0;
}