#ifndef HAWK_CLI_UTILS_HPP
#define HAWK_CLI_UTILS_HPP

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace hawk::cli {
namespace utils {

std::vector<std::string> tokenize(std::string_view str, bool quoted = true);

std::optional<char> parse_delimiter(const std::string& str);

std::size_t num_digits(std::uint64_t n);

} // namespace utils
} // namespace hawk::cli

#endif // HAWK_CLI_UTILS_HPP
