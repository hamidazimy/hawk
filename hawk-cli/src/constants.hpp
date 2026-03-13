#ifndef HAWK_CLI_CONSTANTS_HPP
#define HAWK_CLI_CONSTANTS_HPP

#include <version.hpp>

#include <string>
#include <string_view>

namespace hawk::cli {
namespace constants {

inline const std::string_view ASCII_LOGO = R"(
                          _
      /\  /\__ ___      _| | __
     / /_/ / _` \ \ /\ / / |/ /
    / __  / (_| |\ V  V /|   <
    \/ /_/ \__,_| \_/\_/ |_|\_\
)";

inline const std::string VERSION_STR{
    "Hawk Log Analyzer v" + std::string(hawk::version::PROJECT)
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
