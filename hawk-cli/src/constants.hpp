#ifndef HAWK_CLI_CONSTANTS_HPP
#define HAWK_CLI_CONSTANTS_HPP

#include <string>
#include <string_view>

namespace hawk::cli {
namespace constants {

inline constexpr std::string_view VERSION_MAJOR{"0"};
inline constexpr std::string_view VERSION_MINOR{"1"};
inline constexpr std::string_view VERSION_PATCH{"0"};

inline const std::string VERSION{
    std::string(VERSION_MAJOR) + "." +
    std::string(VERSION_MINOR) + "." +
    std::string(VERSION_PATCH)
};

inline const std::string VERSION_STR{
    "Hawk Log Analyzer v" + VERSION
};

inline const std::string WELCOME_MSG{
    VERSION_STR +
    "\nType 'help' for available commands, 'quit' to exit.\n"
};

inline constexpr std::string_view GOODBYE_MSG{"Goodbye!"};

inline constexpr std::string_view BUILT_WITH{"Built with C++17\n"};
inline constexpr std::string_view PROMPT{"hawk> "};

} // namespace constants
} // namespace hawk::cli

#endif // HAWK_CLI_CONSTANTS_HPP
