#ifndef HAVERSINE_PROFILER_H
#define HAVERSINE_PROFILER_H

#ifndef READ_SCOPE_TIMER
#define READ_SCOPE_TIMER ReadCPUTimer
#endif

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

static inline u64 ReadOSTimer()
{
    LARGE_INTEGER large_int;
    BOOL retval = QueryPerformanceCounter(&large_int);
    assert(retval != 0);
    u64 result = large_int.QuadPart;
    return result;
}

inline static u64 ReadCPUTimer()
{
    return __rdtsc();
}

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
#else
#error Only Windows is supported now!
#endif

struct ProfileAnchor
{
    u64 elapsed_inclusive;
    u64 elapsed_exclusive;
    u64 hit_count;
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
    
    ProfileScope(char *label_, u32 id)
    {
        assert((id != 0) && "0 is reserved for the invalid anchor");
        assert(id < ArrayCount(g_Profiler.anchors));
        
        anchor_id = id;
        parent_anchor_id = g_Profiler.active_anchor_id;
        label = label_;
        
        ProfileAnchor *anchor = g_Profiler.anchors + anchor_id;
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

static void PrintPerformanceProfile()
{
    u64 total_time = g_Profiler.elapsed;
    
    for (u32 i = 0; i < ArrayCount(g_Profiler.anchors); ++i)
    {
        ProfileAnchor *anchor = g_Profiler.anchors + i;
        if (!anchor->label)
            continue;
        
        fprintf(stdout, "\t%s[%llu]: %llu (%.3f%%)", anchor->label, anchor->hit_count, anchor->elapsed_exclusive, GetPercentage(anchor->elapsed_exclusive, total_time));
        if (anchor->elapsed_exclusive != anchor->elapsed_inclusive)
            fprintf(stdout, ", w/children: %llu (%.3f%%)", anchor->elapsed_inclusive, GetPercentage(anchor->elapsed_inclusive, total_time));
        fprintf(stdout, "\n");
    }
}

#define CONCAT_IMPL(a, b) a##b
#define CONCAT(a, b) CONCAT_IMPL(a, b)
#define PROFILE_SCOPE(label) ProfileScope CONCAT(_prof_scope_, __LINE__)(label, __COUNTER__+1)
#define PROFILE_FUNCTION PROFILE_SCOPE(__func__)

#define PROFILER_END_OF_COMPILATION_UNIT static_assert(ArrayCount(g_Profiler.anchors) >= __COUNTER__+1, "Ran out of `ProfileAnchor`s")
#else

#define PROFILE_SCOPE
#define PROFILE_FUNCTION

#define PrintPerformanceProfile(...)

#define PROFILER_END_OF_COMPILATION_UNIT
#endif // ENABLE_PROFILER

#endif // HAVERSINE_PROFILER_H
