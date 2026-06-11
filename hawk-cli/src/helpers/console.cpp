#include "console.hpp"

#include <cstdio>

#ifdef _WIN32
#include <windows.h>
#include <io.h>
#endif

#ifdef __unix__
#include <sys/ioctl.h>
#include <unistd.h>
#endif

namespace hawk::cli {
namespace console {

void setup_console() {
#ifdef _WIN32
    HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
    if (hOut == INVALID_HANDLE_VALUE) return;
    DWORD dwMode = 0;
    if (!GetConsoleMode(hOut, &dwMode)) return;
    SetConsoleMode(hOut, dwMode | ENABLE_VIRTUAL_TERMINAL_PROCESSING);
    SetConsoleCP(CP_UTF8);
    SetConsoleOutputCP(CP_UTF8);
#endif
}

int terminal_width() {
#ifdef _WIN32
    CONSOLE_SCREEN_BUFFER_INFO csbi;
    if (GetConsoleScreenBufferInfo(GetStdHandle(STD_OUTPUT_HANDLE), &csbi)) {
        return csbi.srWindow.Right - csbi.srWindow.Left + 1;
    }
    return 80; // fallback
#elif defined(__unix__)
    struct winsize w;
    if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &w) == 0) {
        return w.ws_col;
    }
    return 80; // fallback
#else
    return 80;
#endif
}

bool is_tty(std::FILE* stream) {
#ifdef _WIN32
    return _isatty(_fileno(stream)) != 0;
#else
    return isatty(fileno(stream)) != 0;
#endif
}

bool stdin_is_tty()  { return is_tty(stdin); }
bool stdout_is_tty() { return is_tty(stdout); }
bool stderr_is_tty() { return is_tty(stderr); }

} // namespace console
} // namespace hawk::cli
