#include <algorithm>
#include <cctype>
#include <compare>
#include <cstddef>
#include <cstdint>
#include <charconv>
#include <cmath>
#include <iomanip>
#include <iterator>
#include <sstream>
#include <string>
#include <string_view>
#include <system_error>

namespace hawk::utils {

std::string human_size(std::size_t bytes) {
    const char* units[] = {"B", "KB", "MB", "GB", "TB"};
    int unit_index = 0;
    double size = static_cast<double>(bytes);

    while (size >= 1024.0 && unit_index < 4) {
        size /= 1024.0;
        unit_index++;
    }

    std::ostringstream oss;
    oss << std::fixed << std::setprecision(2) << size << " " << units[unit_index];
    return oss.str();
}

std::string trim(const std::string& str) {
    auto start = std::find_if_not(str.begin(), str.end(),
                                   [](unsigned char ch) { return std::isspace(ch); });
    auto end = std::find_if_not(str.rbegin(), str.rend(),
                                 [](unsigned char ch) { return std::isspace(ch); }).base();

    return (start < end) ? std::string(start, end) : std::string();
}

std::string_view trim(std::string_view str) {
    auto is_space = [](unsigned char ch) {
        return std::isspace(ch);
    };

    auto start = std::find_if_not(str.begin(), str.end(), is_space);
    auto end = std::find_if_not(str.rbegin(), str.rend(), is_space).base();

    if (start >= end) {
        return std::string_view{};
    }

    return std::string_view(start, static_cast<size_t>(end - start));
}

bool ends_with(std::string_view str, std::string_view suffix) {
    return str.size() >= suffix.size() &&
           str.compare(str.size() - suffix.size(), suffix.size(), suffix) == 0;
}

bool iequals(std::string_view a, std::string_view b) {
    return std::equal(a.begin(), a.end(), b.begin(), b.end(),
                     [](char a, char b) {
                         return std::tolower(static_cast<unsigned char>(a)) ==
                                std::tolower(static_cast<unsigned char>(b));
                     });
}

std::string to_lower(std::string_view str) {
    std::string result(str);
    std::transform(result.begin(), result.end(), result.begin(),
                   [](unsigned char c) { return std::tolower(c); });
    return result;
}

bool parse_int(std::string_view s, std::int64_t& out) {
    auto [ptr, ec] = std::from_chars(s.data(), s.data() + s.size(), out);
    return ec == std::errc{} && ptr == s.data() + s.size();
}

bool parse_double(std::string_view s, double& out) {
    const char* begin = s.data();
    const char* end   = s.data() + s.size();

    auto result = std::from_chars(begin, end, out);
    return result.ec == std::errc{} && result.ptr == end && std::isfinite(out);
}

bool contains(std::string_view haystack, std::string_view needle, bool case_sensitive) {
    if (needle.empty()) return true;
    if (needle.size() > haystack.size()) return false;
    auto it = std::search(
        haystack.begin(), haystack.end(),
        needle.begin(),   needle.end(),
        [case_sensitive](unsigned char a, unsigned char b) {
            return case_sensitive
                ? a == b
                : std::tolower(a) == std::tolower(b);
        }
    );
    return it != haystack.end();
}

int compare_strings(std::string_view a, std::string_view b, bool case_sensitive) {
    if (case_sensitive) {
        if (a == b) return 0;
        return a < b ? -1 : 1;
    }
    std::size_t len = std::min(a.size(), b.size());
    for (std::size_t i = 0; i < len; ++i) {
        int ca = std::tolower(static_cast<unsigned char>(a[i]));
        int cb = std::tolower(static_cast<unsigned char>(b[i]));
        if (ca != cb) return ca - cb;
    }
    if (a.size() == b.size()) return 0;
    return a.size() < b.size() ? -1 : 1;
}

} // namespace hawk::utils
