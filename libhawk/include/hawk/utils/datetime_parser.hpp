#ifndef HAWK_DATETIME_PARSER_HPP
#define HAWK_DATETIME_PARSER_HPP

#include <hawk/core/types.hpp>

#include <optional>
#include <string>
#include <string_view>

namespace hawk::utils {

// Parses a datetime field using a user-facing pattern string.
// Pattern vocabulary: YYYY MM DD hh mm ss f+ Z +tz
// Returns std::nullopt if the input does not match the pattern.
// Note: common_log and epoch formats are currently not supported.
std::optional<SysTicks> parse_datetime(std::string_view s, std::string_view pattern);

// Validates that a pattern string is well-formed.
// Returns an error message if invalid, std::nullopt if valid.
std::optional<std::string> validate_datetime_pattern(std::string_view pattern);

} // namespace hawk::utils

#endif // HAWK_DATETIME_PARSER_HPP
