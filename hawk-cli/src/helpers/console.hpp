#ifndef HAWK_CLI_CONSOLE_HPP
#define HAWK_CLI_CONSOLE_HPP

#include <cstdio>

namespace hawk::cli {
namespace console {

void setup_console();

int terminal_width();

// Returns true if the given stream is connected to a terminal.
// Streams: STDIN, STDOUT, STDERR.
bool is_tty(std::FILE* stream);

// Convenience wrappers for the common cases.
bool stdin_is_tty();
bool stdout_is_tty();
bool stderr_is_tty();

} // namespace console
} // namespace hawk::cli

#endif // HAWK_CLI_CONSOLE_HPP
