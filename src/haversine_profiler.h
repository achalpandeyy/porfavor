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
    
#ifdef HAVERSINE_DEBUG
    b32 closed;
#endif
};

struct Profiler
{
    ProfileAnchor anchors[1+15];
    u64 elapsed;
    u64 active_anchor_id;
};
static Profiler g_Profiler;

static inline void BeginProfiler() { g_Profiler.elapsed = __rdtsc(); }
static inline void EndProfiler()
{
    g_Profiler.elapsed = __rdtsc() - g_Profiler.elapsed;
#ifdef HAVERSINE_DEBUG
    for (u32 i = 0; i < ArrayCount(g_Profiler.anchors); ++i)
    {
        ProfileAnchor *anchor = g_Profiler.anchors + i;
        if (!anchor->label)
            continue;
        
        if (!anchor->closed)
            fprintf(stdout, "ERROR: ProfileAnchor was not closed: %s\n", anchor->label);
    }
    
#endif
}

struct ProfileScope
{
    u64 tsc_begin;
    ProfileAnchor *anchor;
    ProfileAnchor *parent_anchor;
};

#define BEGIN_PROFILE_SCOPE_(label, id) ProfileScope _prof_scope_##label = BeginProfileScope(#label, id)
#define BEGIN_PROFILE_SCOPE(label) BEGIN_PROFILE_SCOPE_(label, __COUNTER__+1)
#define END_PROFILE_SCOPE(label) EndProfileScope(&_prof_scope_##label)

#define BEGIN_PROFILE_FUNCTION_(label, id) ProfileScope _prof_scope_func = BeginProfileScope(label, id)
#define BEGIN_PROFILE_FUNCTION BEGIN_PROFILE_FUNCTION_(__func__, __COUNTER__+1)
#define END_PROFILE_FUNCTION EndProfileScope(&_prof_scope_func)

static inline ProfileScope BeginProfileScope(char *label, u32 id)
{
    assert((id != 0) && "0 is reserved for the invalid anchor");
    assert(id < ArrayCount(g_Profiler.anchors));
    
    ProfileAnchor *anchor = g_Profiler.anchors + id;
    ++anchor->hit_count;
    anchor->label = label;
#ifdef HAVERSINE_DEBUG
    anchor->closed = 0;
#endif
    
    ProfileScope result = { };
    result.anchor = anchor;
    result.parent_anchor = g_Profiler.anchors + g_Profiler.active_anchor_id;
    result.tsc_begin = __rdtsc();
    
    g_Profiler.active_anchor_id = id;
    
    return result;
}

static inline void EndProfileScope(ProfileScope *scope)
{
    u64 elapsed = __rdtsc() - scope->tsc_begin;
    
    ProfileAnchor *anchor = scope->anchor;
    anchor->elapsed_total += elapsed;
#ifdef HAVERSINE_DEBUG
    anchor->closed = 1;
#endif
    
    if (scope->parent_anchor->label)
        scope->parent_anchor->elapsed_children += elapsed;
    
    g_Profiler.active_anchor_id = scope->parent_anchor-g_Profiler.anchors;
}

#ifdef __cplusplus
struct AutocloseProfileScope
{
    ProfileScope scope;
    AutocloseProfileScope(char *label, u32 id) { scope = BeginProfileScope(label, id); }
    ~AutocloseProfileScope() { EndProfileScope(&scope); }
};
#define AUTOCLOSE_PROFILE_SCOPE__(label, id) AutocloseProfileScope _auto_prof_scope_##label(#label, id)
#define AUTOCLOSE_PROFILE_SCOPE_(label, id) AUTOCLOSE_PROFILE_SCOPE__(label, id)
#define AUTOCLOSE_PROFILE_SCOPE(label) AUTOCLOSE_PROFILE_SCOPE_(label, __COUNTER__+1)

#define AUTOCLOSE_PROFILE_FUNCTION_(func_name, id) AutocloseProfileScope _auto_prof_scope_func(func_name, id)
#define AUTOCLOSE_PROFILE_FUNCTION AUTOCLOSE_PROFILE_FUNCTION_(__func__, __COUNTER__+1)
#endif

#endif //HAVERSINE_PROFILER_H
