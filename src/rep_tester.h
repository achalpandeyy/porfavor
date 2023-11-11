#ifndef REP_TESTER_H
#define REP_TESTER_H

#include "porfavor_types.h"
#include "platform_metrics.h"

#include <stdio.h>

struct TrackedData
{
    u64 page_fault_count;
    // NOTE(achal): We can add more things to track here..
};

struct TimeTrackedData
{
    u64 time;
    TrackedData data;
};

struct Buffer
{
    u64 size;
    u8 *data;
};

struct RepTester
{
    u64 cpu_freq;
    u64 try_for_time;
    
    Buffer reuse_buffer;
};

struct TestParams;

struct TestFunction
{
    char const *name;
    TimeTrackedData (*fn)(TestParams *, Buffer *);
};

enum AllocationMode
{
    AllocationMode_None = 0,
    AllocationMode_malloc,
    AllocationMode_Count
};

static RepTester MakeRepTester(f64 try_for_seconds, Buffer *reuse_buffer)
{
    Win32_InitializePlatformMetrics();
    
    RepTester rep_tester = {};
    rep_tester.cpu_freq = EstimateCPUTimerFrequency(100);
    rep_tester.try_for_time = (u64)(try_for_seconds*rep_tester.cpu_freq);
    rep_tester.reuse_buffer = *reuse_buffer;
    
    return rep_tester;
}

inline static Buffer HandleAllocation(AllocationMode alloc_mode, Buffer *src_buffer)
{
    Buffer result = {};
    result.size = src_buffer->size;
    result.data = src_buffer->data;
    
    if (alloc_mode == AllocationMode_malloc)
        result.data = (u8 *)malloc(result.size);
    
    return result;
}

inline static void HandleDeallocation(AllocationMode alloc_mode, Buffer *buffer)
{
    if (alloc_mode == AllocationMode_malloc)
    {
        free(buffer->data);
        buffer->data = 0;
    }
}

inline static void PrintTime(char const *label, f64 time, u64 freq, u64 bytes_processed)
{
    f64 seconds = time/(f64)freq;
    f64 ms = seconds*1000.0;
    
    f64 gigabytes = bytes_processed/(1024.0*1024.0*1024.0);
    f64 gigabytes_per_second = gigabytes/seconds;
    printf("%s: %.0f (%.3f ms) at %.3f GB/s", label, time, ms, gigabytes_per_second);
}

inline static void PrintPageFaults(f64 page_fault_count, u64 bytes_processed)
{
    f64 kb_per_fault = (f64)bytes_processed/(page_fault_count*1024.0);
    printf(", PF: %.4f (%.4f KBs/PageFault)", page_fault_count, kb_per_fault);
}

inline static void PrintTimeWithPageFaults(char const *label, f64 time, f64 page_fault_count, u64 cpu_freq, u64 bytes_processed)
{
    PrintTime(label, time, cpu_freq, bytes_processed);
    if (page_fault_count > 0.0)
        PrintPageFaults(page_fault_count, bytes_processed);
}

inline static void BeginTime(TimeTrackedData *time_data)
{
    time_data->data.page_fault_count -= ReadOSPageFaultCount();
    
    time_data->time -= ReadCPUTimer();
}

inline static void EndTime(TimeTrackedData *time_data)
{
    time_data->time += ReadCPUTimer();
    
    time_data->data.page_fault_count += ReadOSPageFaultCount();
}

static void RunTest(RepTester *rep_tester, TestFunction *test_function, TestParams *test_params)
{
    for (u32 alloc_mode = 0; alloc_mode < AllocationMode_Count; ++alloc_mode)
    {
        printf("\n--------%s", test_function->name);
        if (alloc_mode == AllocationMode_malloc)
            printf(" + malloc");
        printf("--------\n");
        
        TimeTrackedData min_time_data = {};
        min_time_data.time = ~0ull;
        
        TimeTrackedData max_time_data = {};
        TimeTrackedData avg_time_data = {};
        
        u32 test_count = 0;
        u64 tester_start_time = ReadCPUTimer();
        u64 tester_elapsed = ReadCPUTimer()-tester_start_time;
        
        while (tester_elapsed < rep_tester->try_for_time)
        {
            ++test_count;
            
            Buffer buffer = HandleAllocation((AllocationMode)alloc_mode, &rep_tester->reuse_buffer);
            TimeTrackedData time_data = test_function->fn(test_params, &buffer);
            HandleDeallocation((AllocationMode)alloc_mode, &buffer);
            
            if (time_data.time < min_time_data.time)
            {
                min_time_data = time_data;
                
                f64 time = (f64)min_time_data.time;
                f64 pf_count = (f64)min_time_data.data.page_fault_count;
                printf("                                                                                        \r");
                PrintTimeWithPageFaults("Min Time", time, pf_count, rep_tester->cpu_freq, buffer.size);
                printf("\r");
                
                tester_start_time = ReadCPUTimer();
            }
            
            if (time_data.time > max_time_data.time)
            {
                max_time_data = time_data;
            }
            
            avg_time_data.time += time_data.time;
            avg_time_data.data.page_fault_count += time_data.data.page_fault_count;
            
            tester_elapsed = ReadCPUTimer()-tester_start_time;
        }
        
        u64 bytes_processed = rep_tester->reuse_buffer.size;
        
        // Min
        {
            f64 time = (f64)min_time_data.time;
            f64 pf_count = (f64)min_time_data.data.page_fault_count;
            PrintTimeWithPageFaults("Min Time", time, pf_count, rep_tester->cpu_freq, bytes_processed);
            printf("\n");
        }
        
        // Max
        {
            f64 time = (f64)max_time_data.time;
            f64 pf_count = (f64)max_time_data.data.page_fault_count;
            PrintTimeWithPageFaults("Max Time", time, pf_count, rep_tester->cpu_freq, bytes_processed);
            printf("\n");
        }
        
        // Avg
        {
            f64 time = (f64)avg_time_data.time/(f64)test_count;
            f64 pf_count = (f64)avg_time_data.data.page_fault_count/(f64)test_count;
            PrintTimeWithPageFaults("Avg Time", time, pf_count, rep_tester->cpu_freq, bytes_processed);
            printf("\n");
        }
    }
}

#endif // REP_TESTER_H
