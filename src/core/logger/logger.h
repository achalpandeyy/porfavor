#ifndef CORE_LOGGER_H
#define CORE_LOGGER_H

#include "../core.h"

#ifndef CORE_LOGGER_NO_IMPL
#define core_logger_api static
#else
// NOTE: So that the C++ compiler doesn't mangle the names of functions.
#ifdef __cplusplus
#define core_logger_api extern "C"
#else
#define core_logger_api
#endif
#endif

typedef enum
{
    core_logger_level_info = 0,
    core_logger_level_debug,
    core_logger_level_warning,
    core_logger_level_error,
    core_logger_level_fatal,
    core_logger_level_count
} core_logger_level_t;

typedef enum
{
    core_logger_color_green = 0,
    core_logger_color_gray,
    core_logger_color_yellow,
    core_logger_color_red,
    core_logger_color_count
} core_logger_color_t;

core_logger_api void core_logger_log(core_logger_level_t level, const char *format, ...);

#ifndef CORE_LOGGER_NO_IMPL
#include "logger.c"
#endif

#endif