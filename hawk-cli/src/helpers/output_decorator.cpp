#include "output_decorator.hpp"

#include <cctype>
#include <cstdint>

namespace hawk::cli {

namespace sgr {

std::string sgr(const std::string& code) {
    return "\x1b[" + code + "m";
}

std::string rgb(const std::string& rgb_code) {
    // Validate color string (should be 3 or 6 hex digits)
    if (rgb_code.length() != 4 && rgb_code.length() != 7) {
        return "";
    }
    if (rgb_code[0] != '#') {
        return "";
    }
    for (char c : rgb_code.substr(1, rgb_code.length() - 1)) {
        if (!std::isxdigit(c)) {
            return "";
        }
    }

    uint8_t r = 0, g = 0, b = 0;
    if (rgb_code.length() == 4) {
        r = stoi(rgb_code.substr(1, 1) + rgb_code.substr(1, 1), 0 , 16);
        g = stoi(rgb_code.substr(2, 1) + rgb_code.substr(2, 1), 0 , 16);
        b = stoi(rgb_code.substr(3, 1) + rgb_code.substr(3, 1), 0 , 16);
    } else if (rgb_code.length() == 7) {
        r = stoi(rgb_code.substr(1, 2), 0 , 16);
        g = stoi(rgb_code.substr(3, 2), 0 , 16);
        b = stoi(rgb_code.substr(5, 2), 0 , 16);
    }

    return sgr("38;2;" + std::to_string(r) + ";" + std::to_string(g) + ";" + std::to_string(b));
}

std::string colorize(const std::string& output, const std::string& color) {
    return rgb(color) + output + RESET;
}

} // namespace sgr

std::string info_log(std::string log_line) {
    return sgr::colorize(log_line, "#11C");
}

std::string error_log(std::string log_line) {
    return sgr::colorize(log_line, "#C11");
}

std::string warning_log(std::string log_line) {
    return sgr::colorize(log_line, "#CC1");
}

} // namespace hawk::cli
