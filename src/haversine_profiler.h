#ifndef HAVERSINE_PROFILER_H
#define HAVERSINE_PROFILER_H

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

static u64 EstimateCPUFrequency(u64 ms_to_wait)
{
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
    
    u64 result = cpu_timer_freq;
    return result;
}
#else
#error Only Windows supported now!
#endif

struct ProfileAnchor
{
    u64 elapsed_total;
    u64 elapsed_children;
    u64 hit_count;
    char *label;
};

struct Profiler
{
    ProfileAnchor anchors[1+15];
    u64 elapsed;
    u64 active_anchor_id;
};
static Profiler g_Profiler;

static inline void BeginProfiler() { g_Profiler.elapsed = __rdtsc(); }
static inline void EndProfiler() { g_Profiler.elapsed = __rdtsc() - g_Profiler.elapsed; }

struct ProfileScope
{
    u64 tsc_begin;
    ProfileAnchor *anchor;
    ProfileAnchor *parent_anchor;
    
    ProfileScope(char *label, u32 id)
    {
        assert((id != 0) && "0 is reserved for the invalid anchor");
        assert(id < ArrayCount(g_Profiler.anchors));
        
        anchor = g_Profiler.anchors + id;
        parent_anchor = g_Profiler.anchors + g_Profiler.active_anchor_id;
        
        ++anchor->hit_count;
        anchor->label = label;
        tsc_begin = __rdtsc();
        
        g_Profiler.active_anchor_id = id;
    }
    
    ~ProfileScope()
    {
        u64 elapsed = __rdtsc() - tsc_begin;
        anchor->elapsed_total += elapsed;
        
        b32 has_parent = (parent_anchor->label != 0);
        if (has_parent)
            parent_anchor->elapsed_children += elapsed;
        
        g_Profiler.active_anchor_id = parent_anchor-g_Profiler.anchors;
    }
};

#define CONCAT_IMPL(a, b) a##b
#define CONCAT(a, b) CONCAT_IMPL(a, b)
#define PROFILE_SCOPE(label) ProfileScope CONCAT(_prof_scope_, __LINE__)(label, __COUNTER__+1)
#define PROFILE_FUNCTION PROFILE_SCOPE(__func__)

#endif //HAVERSINE_PROFILER_H
