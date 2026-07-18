#ifndef HAWK_UTILS_HPP
#define HAWK_UTILS_HPP

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace hawk::utils {

/**
 * Convert byte size to human-readable format (e.g., "2.5 MB")
 */
std::string human_size(std::size_t bytes);

/**
 * Trim whitespace from both ends of a string
 */
std::string trim(const std::string& str);

/**
 * Trim whitespace from both ends of a string_view
 */
std::string_view trim(std::string_view str);

/**
 * Split string by delimiter
 */
std::vector<std::string_view> split(std::string_view str, char delimiter);

/**
 * Check if string ends with suffix
 */
bool ends_with(std::string_view str, std::string_view suffix);

/**
 * Case-insensitive string comparison
 */
bool iequals(std::string_view a, std::string_view b);

/**
 * Convert string to lowercase
 */
std::string to_lower(std::string_view str);

/**
 * Parse a string as an integer.
 * Returns true if parsing succeeded and consumed the entire string, false otherwise.
 */
bool parse_int(std::string_view s, std::int64_t& out);

/**
 * Parse a string as a double.
 * Returns true if parsing succeeded and consumed the entire string, false otherwise.
 */
bool parse_double(std::string_view s, double& out);

/**
 * Search for a substring in a string, with optional case sensitivity.
 * Returns true if the needle is found in the haystack, false otherwise.
 */
bool contains(std::string_view haystack, std::string_view needle, bool case_sensitive = true);

/**
 * Compares two string_views. Returns negative, zero, or positive like strcmp.
 * Case-insensitive comparison does not allocate.
 */
int compare_strings(std::string_view a, std::string_view b, bool case_sensitive = true);

} // namespace hawk::utils

#endif // HAWK_UTILS_HPP
