#ifndef HAVERSINE_PROFILER_H
#define HAVERSINE_PROFILER_H

#include "platform_metrics.h"

#ifndef READ_SCOPE_TIMER
#define READ_SCOPE_TIMER ReadCPUTimer
#endif

static u64 EstimateScopeTimerFrequency(u64 ms_to_wait)
{
    u64 os_timer_freq = GetOSTimerFrequency();
    u64 os_wait_time = (os_timer_freq*ms_to_wait)/1000;
    
    u64 os_timer_elapsed = 0;
    u64 os_timer_begin = ReadOSTimer();
    
    u64 scope_timer_begin = READ_SCOPE_TIMER();
    while (os_timer_elapsed < os_wait_time)
    {
        u64 os_timer_end = ReadOSTimer();
        os_timer_elapsed = os_timer_end - os_timer_begin;
    }
    u64 scope_timer_end = READ_SCOPE_TIMER();
    
    u64 scope_timer_elapsed = scope_timer_end - scope_timer_begin;
    u64 scope_timer_freq = (os_timer_freq*scope_timer_elapsed)/os_timer_elapsed;
    
    return scope_timer_freq;
}

struct ProfileAnchor
{
    u64 elapsed_inclusive;
    u64 elapsed_exclusive;
    u64 hit_count;
    u64 bytes_processed;
    char *label;
};

struct Profiler
{
    u64 elapsed;
    
#ifdef ENABLE_PROFILER
    u32 active_anchor_id;
    ProfileAnchor anchors[1+31];
#endif
};
static Profiler g_Profiler;

static inline void BeginProfiler() { g_Profiler.elapsed = READ_SCOPE_TIMER(); }
static inline void EndProfiler() { g_Profiler.elapsed = READ_SCOPE_TIMER() - g_Profiler.elapsed; }

#ifdef ENABLE_PROFILER
struct ProfileScope
{
    u64 tsc_begin;
    u32 anchor_id;
    u32 parent_anchor_id;
    char *label;
    u64 old_elapsed_inclusive;
    
    ProfileScope(char *label_, u32 id, u64 bytes_processed)
    {
        assert((id != 0) && "0 is reserved for the invalid anchor");
        assert(id < ArrayCount(g_Profiler.anchors));
        
        anchor_id = id;
        parent_anchor_id = g_Profiler.active_anchor_id;
        label = label_;
        
        ProfileAnchor *anchor = g_Profiler.anchors + anchor_id;
        anchor->bytes_processed += bytes_processed;
        old_elapsed_inclusive = anchor->elapsed_inclusive;
        
        g_Profiler.active_anchor_id = id;
        
        tsc_begin = READ_SCOPE_TIMER();
    }
    
    ~ProfileScope()
    {
        u64 elapsed = READ_SCOPE_TIMER() - tsc_begin;
        
        ProfileAnchor *anchor = g_Profiler.anchors + anchor_id;
        ProfileAnchor *parent_anchor = g_Profiler.anchors + parent_anchor_id;
        
        if (anchor->hit_count == 0)
            anchor->label = label;
        
        ++anchor->hit_count;
        anchor->elapsed_inclusive = old_elapsed_inclusive + elapsed;
        anchor->elapsed_exclusive += elapsed;
        parent_anchor->elapsed_exclusive -= elapsed;
        
        g_Profiler.active_anchor_id = parent_anchor_id;
    }
};

static inline f64 GetPercentage(u64 part, u64 whole)
{
    f64 result = ((f64)part*100.0)/(f64)whole;
    return result;
}

#define CONCAT_IMPL(a, b) a##b
#define CONCAT(a, b) CONCAT_IMPL(a, b)
#define PROFILE_SCOPE_BANDWIDTH(label, bytes) ProfileScope CONCAT(_prof_scope_, __LINE__)(label, __COUNTER__+1, bytes)
#define PROFILE_FUNCTION_BANDWIDTH(bytes) PROFILE_SCOPE_BANDWIDTH(__func__, bytes)
#define PROFILE_SCOPE(label) PROFILE_SCOPE_BANDWIDTH(label, 0)
#define PROFILE_FUNCTION PROFILE_SCOPE(__func__)

#define PROFILER_END_OF_COMPILATION_UNIT static_assert(ArrayCount(g_Profiler.anchors) >= __COUNTER__+1, "Ran out of `ProfileAnchor`s")
#else

#define PROFILE_SCOPE_BANDWIDTH(label, bytes)
#define PROFILE_FUNCTION_BANDWIDTH(bytes)
#define PROFILE_SCOPE
#define PROFILE_FUNCTION

#define PROFILER_END_OF_COMPILATION_UNIT
#endif // ENABLE_PROFILER

static void PrintPerformanceProfile()
{
    fprintf(stdout, "\nPerformance Profile:\n");
    
    u64 cpu_freq = EstimateScopeTimerFrequency(100);
    u64 total_time = g_Profiler.elapsed;
    f64 total_ms = ((f64)total_time/(f64)cpu_freq)*1000.0;
    
    fprintf(stdout, "Total time: %llu | %.4fms (CPU Frequency Estimate: %llu)\n", total_time, total_ms, cpu_freq);
    
#if ENABLE_PROFILER
    for (u32 i = 0; i < ArrayCount(g_Profiler.anchors); ++i)
    {
        ProfileAnchor *anchor = g_Profiler.anchors + i;
        if (!anchor->label)
            continue;
        
        fprintf(stdout, "\t%s[%llu]: %llu (%.3f%%)", anchor->label, anchor->hit_count, anchor->elapsed_exclusive, GetPercentage(anchor->elapsed_exclusive, total_time));
        
        if (anchor->elapsed_exclusive != anchor->elapsed_inclusive)
        {
            fprintf(stdout, ", w/children: %llu (%.3f%%)", anchor->elapsed_inclusive, GetPercentage(anchor->elapsed_inclusive, total_time));
        }
        
        if (anchor->bytes_processed)
        {
            f64 megabytes = anchor->bytes_processed/(1024.0*1024.0);
            f64 gigabytes = megabytes/1024.0;
            f64 gigabytes_per_second = cpu_freq*(gigabytes/anchor->elapsed_inclusive);
            
            fprintf(stdout, " %.3f MB at %.3f GB/s", megabytes, gigabytes_per_second);
        }
        fprintf(stdout, "\n");
    }
#endif
}

#endif // HAVERSINE_PROFILER_H
