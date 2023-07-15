#define CORE_LOGGER_NO_IMPLEMENTATION
#include "core_logger.h"

#ifdef CORE_USE_LINUX
#include "linux_core_logger.inl"
#elif defined(CORE_USE_WINDOWS)
#include "win32_core_logger.inl"
#endif

core_logger_api void core_logger_log(core_logger_level_t level, const char *format, ...)
{
    uint32_t len = 0;
    char message_buffer[32*1024];

    const char *level_tags[] = {"[INFO]: ", "[DEBUG]: ", "[WARNING]: ", "[ERROR]: ", "[FATAL]: "};
    // assert(core_array_count(level_tags) == core_logger_level_count));

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