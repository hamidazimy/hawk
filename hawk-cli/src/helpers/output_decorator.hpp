#ifndef HAWK_CLI_OUTPUT_DECORATOR_HPP
#define HAWK_CLI_OUTPUT_DECORATOR_HPP

#include <string>

namespace hawk::cli {

namespace sgr {

inline const std::string RESET = "\x1b[0m";

std::string sgr(const std::string& code);

std::string rgb(const std::string& rgb_code);

std::string colorize(const std::string& output, const std::string& color);

} // namespace sgr

std::string info_log(std::string log_line);

std::string error_log(std::string log_line);

std::string warning_log(std::string log_line);

} // namespace hawk::cli

#endif // HAWK_CLI_OUTPUT_DECORATOR_HPP
