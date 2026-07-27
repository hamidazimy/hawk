#include "console.hpp"

#include <cstdio>

#ifdef _WIN32
#include <windows.h>
#include <io.h>
#else
#include <sys/ioctl.h>
#include <unistd.h>
#endif

namespace hawk::cli {
namespace console {

namespace {
constexpr std::size_t default_terminal_width = 80;
}

void setup_console() {
#ifdef _WIN32
    // Configure UTF-8 for console input/output.
    SetConsoleCP(CP_UTF8);
    SetConsoleOutputCP(CP_UTF8);

    // Enable ANSI escape sequence support when possible.
    HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
    if (hOut == INVALID_HANDLE_VALUE) return;
    DWORD dwMode = 0;
    if (!GetConsoleMode(hOut, &dwMode)) return;
    (void)SetConsoleMode(hOut, dwMode | ENABLE_VIRTUAL_TERMINAL_PROCESSING);
#endif
}

std::size_t terminal_width(std::FILE* stream) {
#ifdef _WIN32
    HANDLE handle = INVALID_HANDLE_VALUE;
    if (stream == stdout) handle = GetStdHandle(STD_OUTPUT_HANDLE);
    else if (stream == stderr) handle = GetStdHandle(STD_ERROR_HANDLE);
    if (handle != INVALID_HANDLE_VALUE) {
        CONSOLE_SCREEN_BUFFER_INFO csbi;
        if (GetConsoleScreenBufferInfo(handle, &csbi)) {
            const auto width = csbi.srWindow.Right - csbi.srWindow.Left + 1;
            if (width > 0) return static_cast<std::size_t>(width);
        }
    }
#else
    struct winsize ws {};
    if (::ioctl(::fileno(stream), TIOCGWINSZ, &ws) == 0 && ws.ws_col > 0) {
        return static_cast<std::size_t>(ws.ws_col);
    }
#endif
    return default_terminal_width;
}

bool is_tty(std::FILE* stream) {
#ifdef _WIN32
    return ::_isatty(::_fileno(stream)) != 0;
#else
    return ::isatty(::fileno(stream)) != 0;
#endif
}

bool stdin_is_tty()  { return is_tty(stdin); }
bool stdout_is_tty() { return is_tty(stdout); }
bool stderr_is_tty() { return is_tty(stderr); }

} // namespace console
} // namespace hawk::cli
