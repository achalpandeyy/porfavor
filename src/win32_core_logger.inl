// TODO: Display a message box on failure.

static void core_logger__write_to_console(const char *message, core_logger_color_t color)
{
    // TODO: Would there be any practical difference in using STD_ERROR_HANDLE specifically
    // for error messages?
    HANDLE stdout_handle = GetStdHandle(STD_OUTPUT_HANDLE);
    if (stdout_handle == INVALID_HANDLE_VALUE)
        assert(false);

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

    if (SetConsoleTextAttribute(stdout_handle, color_code) == 0)
        assert(false);

    const size_t message_length = strlen(message);
    assert(message_length <= ~0u);
    DWORD written_length;
    // TODO: Why not use the unicode variant?
    if ((WriteConsoleA(stdout_handle, message, (uint32_t)(message_length), &written_length, 0) == 0) || (written_length != message_length))
        assert(false);

    // Reset the color to white.
    int retval = SetConsoleTextAttribute(stdout_handle, 0x7);
    assert(retval != 0);
}