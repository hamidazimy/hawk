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

// Byte length (1-4) of the UTF-8 codepoint starting at str[pos]. pos must
// be < str.size(). Malformed leads and continuation bytes are treated as
// length 1; the result is always clamped to str.size() - pos, so
// pos + utf8_codepoint_length(str, pos) <= str.size() unconditionally —
// callers never need a separate bounds check.
std::size_t utf8_codepoint_length(std::string_view str, std::size_t pos);

// The longest prefix of str, in bytes, that is at most max_bytes long and
// does not split a UTF-8 codepoint. Not a validator: already-malformed
// input passes through byte-faithfully. In particular, an incomplete
// multi-byte sequence truncated by the END OF THE INPUT (e.g. a corrupted
// log field ending mid-codepoint) is treated as one atomic unit of the
// remaining bytes — included whole when the budget allows, excluded whole
// when it doesn't, never split further and never silently dropped.
std::string_view truncate_utf8(std::string_view str, std::size_t max_bytes);

} // namespace utils
} // namespace hawk::cli

#endif // HAWK_CLI_UTILS_HPP
