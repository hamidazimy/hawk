#ifndef HAWK_CLI_UTILS_HPP
#define HAWK_CLI_UTILS_HPP

#include <string>
#include <optional>

namespace hawk::cli {
namespace utils {

std::optional<char> parse_delimiter(const std::string& str);

} // namespace utils
} // namespace hawk::cli

#endif // HAWK_CLI_UTILS_HPP
