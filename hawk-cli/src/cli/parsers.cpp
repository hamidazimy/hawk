#include "parsers.hpp"

#include <stdexcept>
#include <string>
#include <string_view>

namespace hawk::cli {
namespace parsers {

HeadCommand head(std::string_view args) {
    size_t count = 10; // default
    if (!args.empty()) {
        try {
            count = std::stoul(std::string(args));
        } catch (const std::exception& e) {
            throw std::invalid_argument("Invalid argument for head command: " + std::string(args));
        }
    }
    return HeadCommand{count};
}

} // namespace parsers
} // namespace hawk::cli
