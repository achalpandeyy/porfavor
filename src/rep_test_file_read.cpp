#include "rep_tester.h"

#include <io.h>
#include <fcntl.h>

struct TestParams
{
    char const *path;
};

static TimeTrackedData freadTest(TestParams *params, Buffer *buffer)
{
    FILE *file = fopen(params->path, "rb");
    
    TimeTrackedData time_data = {};
    
    BeginTime(&time_data);
    size_t retval = fread(buffer->data, 1, buffer->size, file);
    EndTime(&time_data);
    
    if (retval != buffer->size)
    {
        assert(0);
    }
    
    fclose(file);
    
    return time_data;
}

static TimeTrackedData _readTest(TestParams *params, Buffer *buffer)
{
    int file = _open(params->path, _O_RDONLY|_O_BINARY);
    
    TimeTrackedData time_data = {};
    if (file != -1)
    {
        u64 read_offset = 0;
        u64 size_remaining = buffer->size;
        
        while (size_remaining)
        {
            s32 read_size = 0x7FFFFFFF;
            if (read_size > size_remaining)
                read_size = (s32)size_remaining;
            
            BeginTime(&time_data);
            int retval = _read(file, buffer->data+read_offset, read_size);
            EndTime(&time_data);
            
            assert(retval == read_size);
            
            size_remaining -= read_size;
            read_offset += read_size;
        }
        
        assert(read_offset == buffer->size);
    }
    else
    {
        assert(0);
    }
    
    _close(file);
    return time_data;
}

static TimeTrackedData ReadFileTest(TestParams *params, Buffer *buffer)
{
    HANDLE file_handle = CreateFileA(params->path, GENERIC_READ, FILE_SHARE_READ|FILE_SHARE_WRITE, 0, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, 0);
    
    TimeTrackedData time_data = {};
    
    if (file_handle != INVALID_HANDLE_VALUE)
    {
        u64 read_offset = 0;
        u64 size_remaining = buffer->size;
        
        while (size_remaining)
        {
            u32 read_size = ~0u;
            if (read_size > size_remaining)
                read_size = (u32)size_remaining;
            
            u32 bytes_read;
            BeginTime(&time_data);
            BOOL retval = ReadFile(file_handle, buffer->data+read_offset, read_size, (LPDWORD)&bytes_read, 0);
            EndTime(&time_data);
            
            assert(retval != 0);
            assert(bytes_read == read_size);
            
            size_remaining -= read_size;
            read_offset += read_size;
        }
        
        assert(read_offset == buffer->size);
    }
    else
    {
        assert(0);
    }
    
    CloseHandle(file_handle);
    return time_data;
}

int main()
{
    char const path[] = "data/haversine_input_10000000.json";
    
    Buffer reuse_buffer = {};
    reuse_buffer.size = GetFileSize(path);
    reuse_buffer.data = (u8 *)malloc(reuse_buffer.size);
    
    RepTester rep_tester = MakeRepTester(10.0, &reuse_buffer);
    
    TestFunction test_functions[] =
    {
        {"fread", freadTest},
        {"ReadFile", ReadFileTest},
        {"_read", _readTest},
    };
    
    TestParams test_params = {};
    test_params.path = path;
    
    for (u32 fn_idx = 0; fn_idx < ArrayCount(test_functions); ++fn_idx)
    {
        TestFunction *test_function = test_functions + fn_idx;
        RunTest(&rep_tester, test_function, &test_params);
    }
    
    return 0;
}