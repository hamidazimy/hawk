#ifndef HAWK_CLI_UTILS_HPP
#define HAWK_CLI_UTILS_HPP

#include <cstdint>
#include <string>
#include <optional>

namespace hawk::cli {
namespace utils {

std::optional<char> parse_delimiter(const std::string& str);

std::uint8_t digits(std::uint64_t n);

} // namespace utils
} // namespace hawk::cli

#endif // HAWK_CLI_UTILS_HPP
