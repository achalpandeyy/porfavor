#include "rep_tester.h"

struct TestParams
{
};

static TimeTrackedData WriteToAllBytesForward(TestParams *params, Buffer *buffer)
{
    TimeTrackedData time_data = {};
    
    BeginTime(&time_data);
    for (u64 i = 0; i < buffer->size; ++i)
    {
        buffer->data[i] = (u8)i;
    }
    EndTime(&time_data);
    
    return time_data;
}

static TimeTrackedData WriteToAllBytesBackward(TestParams *params, Buffer *buffer)
{
    TimeTrackedData time_data = {};
    
    BeginTime(&time_data);
    for (u64 i = 0; i < buffer->size; ++i)
    {
        buffer->data[buffer->size-1-i] = (u8)i;
    }
    EndTime(&time_data);
    
    return time_data;
}

/*
NOTE(achal):
This test demonstrates how even if the Page Fault count reported by Windows, which, spoiler alert,
does not report the actual Page Fault count, .. but the bandwidth for reverse probing is bad.
*/

int main()
{
    Buffer reuse_buffer = {};
    reuse_buffer.size = 1ull*1024*1024*1024;
    reuse_buffer.data = (u8 *)malloc(reuse_buffer.size);
    
    RepTester rep_tester = MakeRepTester(10.0, &reuse_buffer);
    
    TestFunction test_functions[] =
    {
        {"WriteToAllBytesForward", WriteToAllBytesForward},
        {"WriteToAllBytesBackward", WriteToAllBytesBackward},
    };
    
    TestParams test_params = {};
    
    for (u32 fn_idx = 0; fn_idx < ArrayCount(test_functions); ++fn_idx)
    {
        TestFunction *test_function = test_functions + fn_idx;
        RunTest(&rep_tester, test_function, &test_params);
    }
    
#if 0
    u64 page_count = 4096;
    u64 page_size = 4096;
    u64 total_size = page_count*page_size;
    
    printf("Touch Count, Fault Count (Reported), Extra Count\n");
    
    for (u32 touch_count = 0; touch_count <= page_count; ++touch_count)
    {
        u8 *data = (u8 *)VirtualAlloc(0, total_size, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
        if (data)
        {
            u64 touch_size = page_size*touch_count;
            
            u64 begin_count = ReadOSPageFaultCount();
            for (u64 i = 0; i < touch_size; ++i)
            {
                data[total_size-1-i] = (u8)i;
            }
            
            u64 end_count = ReadOSPageFaultCount();
            
            u64 fault_count = end_count - begin_count;
            
            printf("%llu, %llu, %lld\n", (u64)touch_count, fault_count, fault_count-touch_count);
            
            VirtualFree(data, 0, MEM_RELEASE);
        }
        else
        {
            printf("Failed to allocate virtual address space\n");
        }
    }
#endif
    
    return 0;
}