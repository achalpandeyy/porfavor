#define CORE_LOGGER_NO_IMPL
#include "logger.h"

#ifdef CORE_USE_LINUX
static void core_logger__write_to_console(const char *message, core_logger_color_t color)
{
    const char * color_strings[] =
    {
        "1;32", // BrightGreen
        "1;30", // Gray
        "1;33", // BrightYellow
        "1;31", // BrightRed
    };
    
    printf("\033[%sm%s\033[0m", color_strings[color], message);
}
#elif defined(CORE_USE_WINDOWS)
static void core_logger__write_to_console(const char *message, core_logger_color_t color)
{
    // TODO(achal):
    // 1. Display a message box on failure.
    // 2. Would there be any practical difference in using STD_ERROR_HANDLE specifically
    // for error messages? One difference I can think of is: if someone redirects the output
    // to stdout to void, such as /dev/null on bash, then it should still display the stderr logs.
    
    HANDLE stdout_handle = GetStdHandle(STD_OUTPUT_HANDLE);
    // TODO(achal): Not sure if this is the best way to handle this right now.
    if (stdout_handle == INVALID_HANDLE_VALUE)
        return;
    
    WORD color_code;
    switch (color)
    {
        case core_logger_color_green:
        color_code = 2;
        break;
        
        case core_logger_color_gray:
        color_code = 8;
        break;
        
        case core_logger_color_yellow:
        color_code = 6;
        break;
        
        case core_logger_color_red:
        color_code = 4;
        break;
        
        default:
        assert(false);
        color_code = (unsigned short)-1;
    }
    
    // TODO(achal): Why is this assert going off even when I have the return statement above???
    // if (SetConsoleTextAttribute(stdout_handle, color_code) == 0)
    // assert(false);
    SetConsoleTextAttribute(stdout_handle, color_code);
    
    const size_t message_length = strlen(message);
    assert(message_length <= ~0u);
    DWORD written_length;
    // TODO(achal): Why not use the unicode variant?
    {
        WriteConsoleA(stdout_handle, message, (uint32_t)(message_length), &written_length, 0);
        // TODO(achal): Why is this assert going off???
        // const int retval = WriteConsoleA(stdout_handle, message, (uint32_t)(message_length), &written_length, 0);
        // assert(retval != 0);
        // assert(written_length == message_length);
    }
    
    // Reset the color to white.
    {
        SetConsoleTextAttribute(stdout_handle, 0x7);
        // TODO(achal): Why is this assert going off???
        // const int retval = SetConsoleTextAttribute(stdout_handle, 0x7);
        // assert(retval==0);
        // assert(retval != 0);
    }
}
#endif

core_logger_api void core_logger_log(core_logger_level_t level, const char *format, ...)
{
    uint32_t len = 0;
    char message_buffer[32*1024];
    
    const char *level_tags[] = {"[INFO]: ", "[DEBUG]: ", "[WARNING]: ", "[ERROR]: ", "[FATAL]: "};
    assert(core_array_count(level_tags) == core_logger_level_count);
    
    assert(level < core_logger_level_count);
    int chars_written = sprintf(message_buffer, "%s", level_tags[level]);
    len += chars_written;
    
    va_list args;
    va_start(args, format);
    chars_written = vsprintf(message_buffer+len, format, args);
    len += chars_written;
    va_end(args);
    
    message_buffer[len++] = '\n';
    message_buffer[len] = '\0';
    
    assert(len < sizeof(message_buffer));
    
    core_logger_color_t color;
    switch (level)
    {
        case core_logger_level_info:
        color = core_logger_color_green;
        break;
        
        case core_logger_level_debug:
        color = core_logger_color_gray;
        break;
        
        case core_logger_level_warning:
        color = core_logger_color_yellow;
        break;
        
        case core_logger_level_error: // [[fallthrough]]
        case core_logger_level_fatal:
        color = core_logger_color_red;
        break;
        
        default:
        color = core_logger_color_count;
        assert(false);
    }
    
    core_logger__write_to_console(message_buffer, color);
}