#include "parsers.hpp"

#include <cli/commands.hpp>
#include <helpers/utils.hpp>

#include <hawk/hawk.hpp>

#include <algorithm>
#include <format>
#include <optional>
#include <cstdint>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>
#include <utility>

namespace hawk::cli {
namespace parsers {

namespace {

bool expand_if_range(std::string_view token, std::vector<std::string>& out) {
    // Must start with $col
    if (!token.starts_with("$col")) return false;
    auto rest = token.substr(4); // after "$col"

    // Find the colon
    auto colon = rest.find(':');
    if (colon == std::string_view::npos) return false;

    auto start_str = rest.substr(0, colon);
    auto end_str   = rest.substr(colon + 1);

    std::int64_t start, end;
    if (!hawk::utils::parse_int(start_str, start) ||
        !hawk::utils::parse_int(end_str,   end)) {
        throw std::invalid_argument{
            std::format("Invalid range expression: {}", token)
        };
    }
    if (start < 1 || end < 1) {
        throw std::invalid_argument{
            std::format("Column indices in range must be >= 1: {}", token)
        };
    }
    if (start > end) {
        throw std::invalid_argument{
            std::format("Range start must be <= end: {}", token)
        };
    }

    for (std::int64_t i = start; i <= end; ++i) {
        out.push_back(std::format("$col{}", i));
    }
    return true;
}

std::vector<std::string> parse_select_column_list(std::string_view args_line) {
    std::string normalised{args_line};
    std::replace(normalised.begin(), normalised.end(), ',', ' ');
    auto tokens = utils::tokenize(normalised);

    std::vector<std::string> cols;
    cols.reserve(tokens.size());

    for (auto token : tokens) {
        if (!expand_if_range(token, cols)) {
            cols.emplace_back(hawk::utils::trim(token));
        }
    }
    return cols;
}

std::optional<std::int64_t> parse_cli_bound_optional(std::string_view s) {
    if (s.empty()) return std::nullopt;
    std::int64_t v;
    if (!hawk::utils::parse_int(s, v) || v == 0) {
        throw std::invalid_argument(
            std::format("Invalid bound: '{}' (must be non-zero integer)", s)
        );
    }
    return v;
}

CliRange parse_cli_range(std::string_view arg) {
    auto colon = arg.find(':');
    if (colon == std::string_view::npos) {
        // Bare number: positive N → :N (first N), negative N → N: (last N)
        auto v = parse_cli_bound_optional(arg);
        if (!v) {
            throw std::invalid_argument("Range cannot be empty");
        }
        if (*v > 0) return CliRange{.start = std::nullopt, .end = *v};
        else        return CliRange{.start = *v, .end = std::nullopt};
    }
    return CliRange{
        .start = parse_cli_bound_optional(arg.substr(0, colon)),
        .end   = parse_cli_bound_optional(arg.substr(colon + 1)),
    };
}

// Like parse_cli_range, but a bare number N means the single-element range
// {N, N} ("exactly row N") rather than slice's "first N" convention.
//Used by peek, whose help text promises `peek i` shows row i.
CliRange parse_cli_index_range(std::string_view arg) {
    if (arg.find(':') != std::string_view::npos) {
        return parse_cli_range(arg);
    }
    auto v = parse_cli_bound_optional(arg);
    if (!v) {
        throw std::invalid_argument("Range cannot be empty");
    }
    return CliRange{.start = *v, .end = *v};
}

CliCommand set_name(std::string_view args_line) {
    auto args = utils::tokenize(args_line);
    if (args.size() != 2) {
        throw std::invalid_argument{
            std::format(
                "Invalid arguments for set name command: {}""\n"
                "Usage: set name <old_name> <new_name>",
                args_line
            )
        };
    }
    return CliSetName{std::move(args[0]), std::move(args[1])};
}

CliCommand set_type(std::string_view args_line) {
    auto args = utils::tokenize(args_line);
    if (args.size() < 2) {
        throw std::invalid_argument{
            std::format(
                "Invalid arguments for set type command: {}""\n"
                "Usage: set type <column> <type> (<datetime format>)",
                args_line
            )
        };
    }
    auto column = args[0];
    const auto& type_str = args[1];
    ColumnType type = ColumnType::String; // default, will be overridden
    std::optional<std::string> datetime_pattern;
    if (type_str == "datetime") {
        type = ColumnType::DateTime;
        if (args.size() != 3) {
            throw std::invalid_argument{
                std::format(
                    "Setting a column to datetime requires a pattern.""\n"
                    "Usage: set type <column> datetime \"<pattern>\""
                )
            };
        }
        auto pattern = std::string(args[2]);
        if (pattern.empty()) {
            throw std::invalid_argument{
                "Datetime pattern cannot be empty"
            };
        }
        auto error = hawk::utils::validate_datetime_pattern(pattern);
        if (error.has_value()) {
            throw std::invalid_argument{
                *error
            };
        }
        datetime_pattern = pattern;
    } else if (type_str == "string") {
        type = ColumnType::String;
    } else if (type_str == "integer" || type_str == "int") {
        type = ColumnType::Integer;
    } else if (type_str == "float" || type_str == "double") {
        type = ColumnType::Float;
    } else {
        throw std::invalid_argument{
            std::format(
                "Invalid column type in set command: {}",
                type_str
            )
        };
    }
    return CliSetType{std::move(args[0]), type, datetime_pattern};
}

hawk::FilterArgs parse_filter_args(std::string_view args_line) {
    auto args = utils::tokenize(args_line);
    if (args.size() != 3) {
        throw std::invalid_argument{
            std::format(
                "Expected <column> <op> <value>, got: {}",
                args_line
            )
        };
    }
    hawk::FilterOp op;
    if (args[1] == "has") {
        op = hawk::FilterOp::HAS;
    } else if (args[1] == "==") {
        op = hawk::FilterOp::EQ;
    } else if (args[1] == "!=") {
        op = hawk::FilterOp::NE;
    } else if (args[1] == ">") {
        op = hawk::FilterOp::GT;
    } else if (args[1] == "<") {
        op = hawk::FilterOp::LT;
    } else if (args[1] == ">=") {
        op = hawk::FilterOp::GE;
    } else if (args[1] == "<=") {
        op = hawk::FilterOp::LE;
    } else {
        throw std::invalid_argument{
            std::format("Invalid operator: {}", args[1])
        };
    }
    if (args[0] == "$row" && op != hawk::FilterOp::HAS) {
        throw std::invalid_argument{
            std::format(
                "Only 'has' operator is supported for $row searches. Got: {}",
                args[1]
            )
        };
    }
    return hawk::FilterArgs{std::string(args[0]), args[0] == "$row", op, std::string(args[2])};
}

} // namespace

CliCommand config       (std::string_view args_line) {
    if (!args_line.empty()) {
        throw std::invalid_argument{
            std::format(
                "config command does not take any arguments. Got: {}",
                args_line
            )
        };
    }
    return CliConfig{};
}

CliCommand columns      (std::string_view args_line) {
    if (!args_line.empty()) {
        throw std::invalid_argument{
            std::format(
                "columns command does not take any arguments. Got: {}",
                args_line
            )
        };
    }
    return CliColumns{};
}

CliCommand set          (std::string_view args_line) {
    auto args = utils::tokenize(args_line);
    if (args.empty()) {
        throw std::invalid_argument{
            std::format(
                "set command requires additional arguments. Got: {}""\n"
                "Usage: set type|name <args>",
                args_line
            )
        };
    }

    // Find where the subcommand argument starts in the original line
    auto subcommand = args[0];
    auto rest = hawk::utils::trim(args_line.substr(subcommand.size()));

    if (subcommand == "name") {
        return set_name(rest);
    } else if (subcommand == "type") {
        return set_type(rest);
    } else {
        throw std::invalid_argument{
            std::format(
                "Unknown set subcommand: {}""\n"
                "Supported: set type|name <args>",
                subcommand
            )
        };
    }
}

CliCommand select       (std::string_view args_line) {
    if (hawk::utils::trim(args_line) == "*") {
        return CliReset{.proj = true};
    }
    auto cols = parse_select_column_list(args_line);
    if (cols.empty()) {
        throw std::invalid_argument{
            "select requires at least one column.""\n"
            "Usage: select <col1>,<col2>,... $colX $colN:M"
        };
    }
    return CliSelect{std::move(cols)};
}

CliCommand select_add   (std::string_view args_line) {
    auto cols = parse_select_column_list(args_line);
    if (cols.empty()) {
        throw std::invalid_argument{
            "select+ requires at least one column.""\n"
            "Usage: select+ <col1>,<col2>,... $colX $colN:M"
        };
    }
    return CliSelectAdd{std::move(cols)};
}

CliCommand select_rem   (std::string_view args_line) {
    auto cols = parse_select_column_list(args_line);
    if (cols.empty()) {
        throw std::invalid_argument{
            "select- requires at least one column.""\n"
            "Usage: select- <col1>,<col2>,... $colX $colN:M"
        };
    }
    return CliDeselect{std::move(cols)};
}

CliCommand count        (std::string_view args_line) {
    if (!args_line.empty()) {
        throw std::invalid_argument{
            std::format(
                "count command does not take any arguments. Got: {}",
                args_line
            )
        };
    }
    return CliCount{};
}

CliCommand peek         (std::string_view args_line) {
    DisplayMode mode = DisplayMode::Vertical;
    std::optional<std::string_view> range_arg;
    auto args = utils::tokenize(args_line);
    for (auto arg : args) {
        if (arg == "--untruncated" || arg == "-u") {
            mode = DisplayMode::VerticalUntruncated;
        } else if (range_arg) {
            throw std::invalid_argument(
                std::format(
                    "peek takes at most one range argument. Got extra: {}",
                    arg
                )
            );
        } else {
            range_arg = arg;
        }
    }

    // No argument: default to first row of view.
    if (!range_arg) {
        return CliPeek{
            .range = CliRange{.start = 1, .end = 1},
            .raw   = false,
            .mode  = mode,
        };
    }

    std::string_view arg = *range_arg;
    bool raw = false;
    if (arg.starts_with('#')) {
        raw = true;
        arg = arg.substr(1);
        if (arg.empty()) {
            throw std::invalid_argument("peek: expected index after '#'");
        }
    }

    return CliPeek{
        .range = parse_cli_index_range(arg),
        .raw   = raw,
        .mode  = mode,
    };
}

CliCommand head         (std::string_view args_line) {
    RecordCount count = 10;
    DisplayMode mode  = DisplayMode::Horizontal;

    auto args = utils::tokenize(args_line);
    for (const auto& arg : args) {
        if (arg == "--vertical" || arg == "-v") {
            if (mode == DisplayMode::Horizontal)
                mode = DisplayMode::Vertical;
        } else if (arg == "--untruncated" || arg == "-u") {
            mode = DisplayMode::VerticalUntruncated;
        } else {
            std::int64_t n;
            if (!hawk::utils::parse_int(arg, n) || n < 1) {
                throw std::invalid_argument{
                    std::format(
                        "Invalid argument for head command: {}",
                        arg
                    )
                };
            }
            count = static_cast<RecordCount>(n);
        }
    }

    return CliHead{count, mode};
}

CliCommand tail         (std::string_view args_line) {
    RecordCount count = 10;
    DisplayMode mode  = DisplayMode::Horizontal;

    auto args = utils::tokenize(args_line);
    for (const auto& arg : args) {
        if (arg == "--vertical" || arg == "-v") {
            if (mode == DisplayMode::Horizontal)
                mode = DisplayMode::Vertical;
        } else if (arg == "--untruncated" || arg == "-u") {
            mode = DisplayMode::VerticalUntruncated;
        } else {
            std::int64_t n;
            if (!hawk::utils::parse_int(arg, n) || n < 1) {
                throw std::invalid_argument{
                    std::format(
                        "Invalid argument for tail command: {}",
                        arg
                    )
                };
            }
            count = static_cast<RecordCount>(n);
        }
    }

    return CliTail{count, mode};
}

CliCommand filter       (std::string_view args_line) {
    if (hawk::utils::trim(args_line) == "*") {
        return CliReset{.view = true};
    }
    return CliFilter{parse_filter_args(args_line)};
}

CliCommand filter_exp   (std::string_view args_line) {
    return CliFilterExp{parse_filter_args(args_line)};
}

CliCommand filter_exc   (std::string_view args_line) {
    return CliFilterExc{parse_filter_args(args_line)};
}

CliCommand slice        (std::string_view args_line) {
    auto args = utils::tokenize(args_line);
    if (args.size() != 1) {
        throw std::invalid_argument{
            std::format(
                "slice command takes exactly one argument. Got: {}",
                args_line
            )
        };
    }
    return CliSlice{parse_cli_range(args[0])};
}

CliCommand sort         (std::string_view args_line) {
    auto args = utils::tokenize(args_line);
    if (args.empty()) {
        throw std::invalid_argument{
            "Usage: sort <column> [--desc|-r] or sort --default"
        };
    }

    if (args.size() == 1)
        if (args[0] == "--default" || args[0] == "-d")
            return CliReset{.sort = true};

    bool is_desc = false;
    std::string column;

    for (const auto& arg : args) {
        if (arg == "--desc" || arg == "-r") {
            is_desc = true;
        } else if (column.empty()) {
            column = arg;
        } else {
            throw std::invalid_argument{
                std::format(
                    "Unexpected argument for sort command: {}",
                    arg
                )
            };
        }
    }

    if (column.empty()) {
        throw std::invalid_argument{
            "sort requires a column name."
        };
    }

    return CliSort{std::move(column), is_desc};
}

CliCommand distinct     (std::string_view args_line) {
    auto args = utils::tokenize(args_line);
    if (args.empty()) {
        throw std::invalid_argument{
            "distinct requires a column name.""\n"
            "Usage: distinct <column> [-v|--sort-by-value] [-r|--reverse]"
        };
    }

    bool        sort_by_value = false;
    bool        sort_desc     = false;
    std::string column;

    for (const auto& arg : args) {
        if (arg == "--sort-by-value" || arg == "-v") {
            sort_by_value = true;
        } else if (arg == "--reverse" || arg == "-r") {
            sort_desc = true;
        } else if (column.empty()) {
            column = arg;
        } else {
            throw std::invalid_argument{
                std::format(
                    "Unexpected argument for distinct command: {}",
                    arg
                )
            };
        }
    }

    if (column.empty()) {
        throw std::invalid_argument{
            "distinct requires a column name."
        };
    }

    return CliDistinct{std::move(column), sort_by_value, sort_desc};
}

CliCommand reset        (std::string_view args_line) {
    auto args = utils::tokenize(args_line);

    if (args.empty()) {
        return CliReset{true, true, true};
    }

    CliReset cmd;
    for (const auto& arg : args) {
        if      (arg == "--view" || arg == "-v") cmd.view = true;
        else if (arg == "--proj" || arg == "-p") cmd.proj = true;
        else if (arg == "--sort" || arg == "-s") cmd.sort = true;
        else throw std::invalid_argument{
            std::format(
                "Unknown argument for reset command: {}",
                arg
            )
        };
    }
    return cmd;
}

CliCommand eXport       (std::string_view args_line) {
    if (args_line.empty()) {
        throw std::invalid_argument{
            "Please provide a path."
        };
    }
    auto args = utils::tokenize(args_line);
    ExportMode mode = ExportMode::Full;
    std::string path;

    for (const auto& arg : args) {
        if (arg == "--projected" || arg == "-p") {
            mode = ExportMode::Projected;
        } else if (path.empty()) {
            path = std::string(arg);
        } else {
            throw std::invalid_argument{
                std::format(
                    "Unexpected argument for export command: {}",
                    arg
                )
            };
        }
    }

    if (path.empty()) {
        throw std::invalid_argument{
            "Please provide a path."
        };
    }

    return CliExport{std::move(path), mode};
}

CliCommand history      (std::string_view args_line) {
    if (args_line.empty()) return CliHistory{};

    auto args = cli::utils::tokenize(args_line);
    if (args.size() == 2 && (args[0] == "--save" || args[0] == "-s")) {
        return CliHistory{.save_path = std::string(args[1])};
    }

    throw std::invalid_argument("Usage: history [--save|-s <path>]");
}

CliCommand help         (std::string_view args_line) {
    if (args_line.empty()) {
        return CliHelp{std::nullopt};
    }
    auto args = utils::tokenize(args_line);
    if (args.size() != 1) {
        throw std::invalid_argument{
            std::format(
                "help command takes at most one argument. Got: {}",
                args_line
            )
        };
    }
    return CliHelp{std::optional<std::string>{args[0]}};
}

CliCommand exit         (std::string_view args_line) {
    if (!args_line.empty()) {
        throw std::invalid_argument{
            std::format(
                "exit command does not take any arguments. Got: {}",
                args_line
            )
        };
    }
    return CliExit{};
}

} // namespace parsers
} // namespace hawk::cli
