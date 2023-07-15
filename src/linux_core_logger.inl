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