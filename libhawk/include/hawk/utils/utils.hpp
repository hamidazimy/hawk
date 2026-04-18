#ifndef HAWK_UTILS_HPP
#define HAWK_UTILS_HPP

#include <cstddef>
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
std::vector<std::string> split(const std::string& str, char delimiter);

/**
 * Split string by delimiter (string_view version for performance)
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
std::string to_lower(std::string str);

} // namespace hawk::utils

#endif // HAWK_UTILS_HPP
