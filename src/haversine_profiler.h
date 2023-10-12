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

struct ProfileScope
{
    u64 tsc_begin;
    u64 tsc_total;
    u64 tsc_children;
    char *label;
    u64 hit_count;
    
#ifdef HAVERSINE_DEBUG
    b32 closed;
#endif
};

#define BEGIN_PROFILE_SCOPE_(label, id) BeginProfileScope(label, id)
#define BEGIN_PROFILE_SCOPE(label) BEGIN_PROFILE_SCOPE_(label, __COUNTER__)
// TODO(achal): Can I not make this take a label to construct the profile scope identifier???
#define END_PROFILE_SCOPE(scope) EndProfileScope(scope)

#define BEGIN_PROFILE_FUNCTION_(func_name) ProfileScope *_prof_func_##func_name = BEGIN_PROFILE_SCOPE(func_name)
#define BEGIN_PROFILE_FUNCTION BEGIN_PROFILE_FUNCTION_(__func__)
#define END_PROFILE_FUNCTION_(func_name) END_PROFILE_SCOPE(_prof_func_##func_name) 
#define END_PROFILE_FUNCTION END_PROFILE_FUNCTION_(__func__)

struct Profiler
{
    ProfileScope scopes[16];
    u64 tsc;
    u64 active_scope_id;
};
static Profiler g_Profiler;

static inline void BeginProfiler() { g_Profiler.tsc = __rdtsc(); }
static inline void EndProfiler()
{
    g_Profiler.tsc = __rdtsc() - g_Profiler.tsc;
    
#ifdef HAVERSINE_DEBUG
    for (ProfileScope *scope = g_Profiler.scopes; scope->label; ++scope)
    {
        if (!scope->closed)
            fprintf(stderr, "ERROR: Profile scope was not closed: %s\n", scope->label); 
    }
#endif
}

static inline ProfileScope * BeginProfileScope(char *label, u32 id)
{
    assert(id < ArrayCount(g_Profiler.scopes));
    
    g_Profiler.active_scope_id = id;
    
    ProfileScope *result = &g_Profiler.scopes[id];
    result->tsc_begin = __rdtsc();
    result->label = label;
    ++result->hit_count;
    
    return result;
}

static inline void EndProfileScope(ProfileScope *scope)
{
    u64 scope_id = scope - g_Profiler.scopes;
    if (g_Profiler.active_scope_id != scope_id)
    {
        assert(g_Profiler.active_scope_id < ArrayCount(g_Profiler.scopes));
        
        scope->tsc_children = g_Profiler.scopes[g_Profiler.active_scope_id].tsc_total;
        g_Profiler.active_scope_id = scope_id;
    }
    
    u64 tsc_elapsed = __rdtsc() - scope->tsc_begin;
    scope->tsc_total += tsc_elapsed;
    
#ifdef HAVERSINE_DEBUG
    scope->closed = 1;
#endif
}

#ifdef __cplusplus
struct AutocloseProfileScope
{
    ProfileScope *scope;
    ~AutocloseProfileScope() { END_PROFILE_SCOPE(scope); }
};
#define AUTOCLOSE_PROFILE_SCOPE__(label, id) AutocloseProfileScope _auto_prof_scope_##id = { BeginProfileScope(label, id) };
#define AUTOCLOSE_PROFILE_SCOPE_(label, id) AUTOCLOSE_PROFILE_SCOPE__(label, id)
#define AUTOCLOSE_PROFILE_SCOPE(label) AUTOCLOSE_PROFILE_SCOPE_(label, __COUNTER__)

#define AUTOCLOSE_PROFILE_FUNCTION_(func_name) AUTOCLOSE_PROFILE_SCOPE(func_name) 
#define AUTOCLOSE_PROFILE_FUNCTION AUTOCLOSE_PROFILE_FUNCTION_(__func__)
#endif

#endif //HAVERSINE_PROFILER_H
