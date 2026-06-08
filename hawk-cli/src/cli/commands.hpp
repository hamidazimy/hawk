#ifndef HAWK_CLI_COMMANDS_HPP
#define HAWK_CLI_COMMANDS_HPP

#include <hawk/hawk.hpp>

#include <optional>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

namespace hawk::cli {

enum class DisplayMode  { Horizontal, Vertical, VerticalUntruncated };
enum class ExportMode   { Full, Projected };

struct CliConfig        {};
struct CliColumns       {};
struct CliSetName       { std::string old_name; std::string new_name; };
struct CliSetType       { std::string column; hawk::ColumnType type; std::optional<std::string> dt_pattern; };
struct CliSelect        { std::vector<std::string> columns; };
struct CliSelectAdd     { std::vector<std::string> columns; };
struct CliDeselect      { std::vector<std::string> columns; };
struct CliCount         {};
struct CliPeek          { std::string arg;    DisplayMode mode = DisplayMode::Vertical; };
struct CliHead          { RecordCount n = 10; DisplayMode mode = DisplayMode::Horizontal; };
struct CliTail          { RecordCount n = 10; DisplayMode mode = DisplayMode::Horizontal; };
struct CliFilter        { hawk::FilterArgs args; };
struct CliFilterExp     { hawk::FilterArgs args; };
struct CliFilterExc     { hawk::FilterArgs args; };
struct CliSort          { std::string column; bool is_desc = false; };
struct CliDistinct      { std::string column; bool sort_by_value = false; bool sort_desc = false; };
struct CliReset         { bool view = false; bool proj = false; bool sort = false; };
struct CliExport        { std::string path; ExportMode mode = ExportMode::Full; };
struct CliHistory       { std::optional<std::string> save_path; };
struct CliHelp          { std::optional<std::string> command_name; };
struct CliExit          {};

using CliCommand = std::variant<
    CliConfig,
    CliColumns,
    CliSetName,
    CliSetType,
    CliSelect,
    CliSelectAdd,
    CliDeselect,
    CliCount,
    CliPeek,
    CliHead,
    CliTail,
    CliFilter,
    CliFilterExp,
    CliFilterExc,
    CliSort,
    CliDistinct,
    CliReset,
    CliExport,
    CliHistory,
    CliHelp,
    CliExit
>;

struct CommandInfo {
    std::string_view name;
    std::string_view usage;
    std::string_view description;
    std::string_view detail;
    std::vector<std::string_view> aliases;
    CliCommand (*parser)(std::string_view);
};

} // namespace hawk::cli

#endif // HAWK_CLI_COMMANDS_HPP
