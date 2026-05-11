#ifndef HAWK_CLI_OUTPUT_DECORATOR_HPP
#define HAWK_CLI_OUTPUT_DECORATOR_HPP

#include <string>
#include <string_view>

namespace hawk::cli {

namespace sgr {

inline const std::string RESET = "\x1b[0m";

std::string sgr(const std::string& code);

std::string rgb(const std::string& rgb_code);

std::string colorize(const std::string_view& output, const std::string& color);

} // namespace sgr

std::string log_info(std::string log_line);

std::string log_error(std::string log_line);

std::string log_warning(std::string log_line);

std::string log_success(std::string log_line);

} // namespace hawk::cli

#endif // HAWK_CLI_OUTPUT_DECORATOR_HPP
