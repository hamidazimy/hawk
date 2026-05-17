#include "output_decorator.hpp"

#include <cctype>
#include <cstdint>

namespace hawk::cli {

namespace sgr {

static bool color_enabled = true;

void set_color_enabled(bool enabled) {
    color_enabled = enabled;
}

bool is_color_enabled() {
    return color_enabled;
}

std::string rst() {
    return sgr("0");
}

std::string sgr(const std::string& code) {
    return color_enabled ? "\x1b[" + code + "m" : "";
}

std::string rgb(const std::string& rgb_code) {
    if (!color_enabled) return "";
    if (rgb_code.length() != 4 && rgb_code.length() != 7) return "";
    if (rgb_code[0] != '#') return "";
    for (char c : rgb_code.substr(1, rgb_code.length() - 1)) {
        if (!std::isxdigit(c)) return "";
    }
    uint8_t r = 0, g = 0, b = 0;
    if (rgb_code.length() == 4) {
        r = stoi(rgb_code.substr(1, 1) + rgb_code.substr(1, 1), 0, 16);
        g = stoi(rgb_code.substr(2, 1) + rgb_code.substr(2, 1), 0, 16);
        b = stoi(rgb_code.substr(3, 1) + rgb_code.substr(3, 1), 0, 16);
    } else {
        r = stoi(rgb_code.substr(1, 2), 0, 16);
        g = stoi(rgb_code.substr(3, 2), 0, 16);
        b = stoi(rgb_code.substr(5, 2), 0, 16);
    }
    return sgr("38;2;" + std::to_string(r) + ";" + std::to_string(g) + ";" + std::to_string(b));
}

std::string colorize(const std::string_view& output, const std::string& color) {
    if (!color_enabled) return std::string(output);
    return rgb(color) + std::string(output) + rst();
}

} // namespace sgr

std::string log_info(std::string log_line) {
    return sgr::colorize(log_line, "#11C");
}

std::string log_error(std::string log_line) {
    return sgr::colorize(log_line, "#C11");
}

std::string log_warning(std::string log_line) {
    return sgr::colorize(log_line, "#CC1");
}

std::string log_success(std::string log_line) {
    return sgr::colorize(log_line, "#1C1");
}

} // namespace hawk::cli
