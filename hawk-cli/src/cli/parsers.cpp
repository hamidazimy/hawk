#include "parsers.hpp"

#include <cli/cli_commands.hpp>
#include <helpers/utils.hpp>

#include <algorithm>
#include <format>
#include <optional>
#include <cstddef>
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

LibCommand set_name(std::string_view args_line) {
    auto args = utils::tokenize(args_line);
    if (args.size() != 2) {
        throw std::invalid_argument{
            std::format(
                "Invalid arguments for set name command: {} \n"
                "Usage: set name <old_name> <new_name>",
                args_line
            )
        };
    }
    return SetColumnNameCommand{std::string(args[0]), std::string(args[1])};
}

LibCommand set_type(std::string_view args_line) {
    auto args = utils::tokenize(args_line);
    if (args.size() < 2) {
        throw std::invalid_argument{
            std::format(
                "Invalid arguments for set type command: {} \n"
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
                "Setting a column to datetime requires a pattern. "
                "Usage: set type <column> datetime \"<pattern>\""
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
    return SetColumnTypeCommand{column, type, datetime_pattern};
}

hawk::FilterArgs parse_filter_args(std::string_view args_line) {
    auto args = utils::tokenize(args_line);
    if (args.size() != 3) {
        throw std::invalid_argument{
            std::format("Expected <column> <op> <value>, got: {}", args_line)
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
            std::format("Only 'has' operator is supported for $row searches. Got: {}", args[1])
        };
    }
    return hawk::FilterArgs{std::string(args[0]), args[0] == "$row", op, std::string(args[2])};
}

template <typename FltrCmd>
FltrCmd parse_filter_command(std::string_view args_line) {
    auto fltrargs = parse_filter_args(args_line);
    return FltrCmd{
        std::move(fltrargs.column),
        fltrargs.row_search,
        fltrargs.op,
        std::move(fltrargs.value)
    };
}

} // namespace

LibCommand columns  (std::string_view args_line) {
    if (!args_line.empty()) {
        throw std::invalid_argument{
            std::format("columns command does not take any arguments. Got: {}", args_line)
        };
    }
    return ColumnsCommand{};
}

LibCommand set      (std::string_view args_line) {
    auto args = utils::tokenize(args_line);
    if (args.empty()) {
        throw std::invalid_argument{
            std::format(
                "set command requires additional arguments. Got: {} \n"
                "Usage: set type|name <args>"
            , args_line)
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
                "Unknown set subcommand: {}\n"
                "Supported: set type|name <args>",
                subcommand
            )
        };
    }
}

LibCommand select   (std::string_view args_line) {
    if (hawk::utils::trim(args_line) == "*") {
        return ResetCommand{.proj = true};
    }
    auto cols = parse_select_column_list(args_line);
    if (cols.empty()) {
        throw std::invalid_argument{
            "select requires at least one column.\n"
            "Usage: select <col1>,<col2>,... $colX $colN:M"
        };
    }
    return SelectCommand{std::move(cols)};
}

LibCommand select_add(std::string_view args_line) {

    auto cols = parse_select_column_list(args_line);
    if (cols.empty()) {
        throw std::invalid_argument{
            "select+ requires at least one column.\n"
            "Usage: select+ <col1>,<col2>,... $colX $colN:M"
        };
    }
    return SelectAddCommand{std::move(cols)};
}

LibCommand select_rem(std::string_view args_line) {
    auto cols = parse_select_column_list(args_line);
    if (cols.empty()) {
        throw std::invalid_argument{
            "select- requires at least one column.\n"
            "Usage: select- <col1>,<col2>,... $colX $colN:M"
        };
    }
    return DeselectCommand{std::move(cols)};
}

LibCommand count    (std::string_view args_line) {
    if (!args_line.empty()) {
        throw std::invalid_argument{
            std::format("count command does not take any arguments. Got: {}", args_line)
        };
    }
    return CountCommand{};
}

LibCommand peek     (std::string_view args_line) {
    if (args_line.empty()) {
        return RecordsCommand{0, 1}; // default to first row of view
    }
    bool raw = false;
    std::string_view index_str = hawk::utils::trim(args_line);
    if (index_str.starts_with('#')) {
        raw = true;
        index_str = index_str.substr(1);
    }

    // Check for range syntax N:M
    auto colon = index_str.find(':');
    if (colon != std::string_view::npos) {
        std::int64_t start_val, end_val;
        if (!hawk::utils::parse_int(index_str.substr(0, colon), start_val) || start_val < 1 ||
            !hawk::utils::parse_int(index_str.substr(colon + 1), end_val)  || end_val < 1) {
            throw std::invalid_argument{
                std::format("Invalid range for peek command: {}", args_line)
            };
        }
        if (start_val > end_val) {
            throw std::invalid_argument{
                std::format("Start must be <= end in peek range: {}", args_line)
            };
        }
        // CLI is 1-based inclusive --> lib is 0-based exclusive
        RecordIndex start = static_cast<RecordIndex>(start_val - 1);
        RecordIndex end   = static_cast<RecordIndex>(end_val);
        return RecordsCommand{start, end, raw};
    }

    // Single index
    std::int64_t index;
    if (!hawk::utils::parse_int(index_str, index) || index < 1) {
        throw std::invalid_argument{
            std::format("Invalid argument for peek command: {}", args_line)
        };
    }
    RecordIndex idx = static_cast<RecordIndex>(index - 1);
    return RecordsCommand{idx, idx + 1, raw};
}

LibCommand head     (std::string_view args_line) {
    std::size_t count = 10; // default
    if (!args_line.empty()) {
        std::int64_t n;
        if (!hawk::utils::parse_int(args_line, n) || n < 1) {
            throw std::invalid_argument{
                std::format("Invalid argument for head command: {}", args_line)
            };
        }
        count = static_cast<std::size_t>(n);
    }
    return RecordsCommand{0, static_cast<RecordIndex>(count)};
}

LibCommand tail     (std::string_view args_line) {
    std::size_t count = 10; // default
    if (!args_line.empty()) {
        std::int64_t n;
        if (!hawk::utils::parse_int(args_line, n) || n < 1) {
            throw std::invalid_argument{
                std::format("Invalid argument for tail command: {}", args_line)
            };
        }
        count = static_cast<std::size_t>(n);
    }
    return TailCommand{count};
} // Note: TailCommand is a temporary solution until CliCommand refactor.

LibCommand filter   (std::string_view args_line) {
    if (hawk::utils::trim(args_line) == "*") {
        return ResetCommand{.view = true};
    }
    return parse_filter_command<FilterCommand>(args_line);
}

LibCommand filter_exp(std::string_view args_line) {
    return parse_filter_command<FilterExpandCommand>(args_line);
}

LibCommand filter_exc(std::string_view args_line) {
    return parse_filter_command<FilterExcludeCommand>(args_line);
}

LibCommand sort     (std::string_view args_line) {
    auto args = utils::tokenize(args_line);
    if (args.empty() || args.size() > 2) {
        throw std::invalid_argument{
            "Usage: sort <column> [--desc|-r]"
        };
    }

    if (args.size() == 1 && args[0] == "--default") {
        return ResetCommand{.sort = true};
    }

    bool is_desc = false;
    std::string column;

    for (const auto& arg : args) {
        if (arg == "--desc" || arg == "-r") {
            is_desc = true;
        } else if (column.empty()) {
            column = arg;
        } else {
            throw std::invalid_argument{
                std::format("Unexpected argument for sort command: {}", arg)
            };
        }
    }

    if (column.empty()) {
        throw std::invalid_argument{"sort requires a column name."};
    }

    return SortCommand{std::move(column), is_desc};
}

LibCommand distinct (std::string_view args_line) {
    auto args = utils::tokenize(args_line);
    if (args.empty()) {
        throw std::invalid_argument{
            "distinct requires a column name.\n"
            "Usage: distinct <column> [-v|--sort-by-value] [-r|--desc]"
        };
    }

    bool        sort_by_value = false;
    bool        sort_desc     = false;
    std::string column;

    for (const auto& arg : args) {
        if (arg == "--sort-by-value" || arg == "-v") {
            sort_by_value = true;
        } else if (arg == "--desc" || arg == "-r") {
            sort_desc = true;
        } else if (column.empty()) {
            column = arg;
        } else {
            throw std::invalid_argument{
                std::format("Unexpected argument for distinct command: {}", arg)
            };
        }
    }

    if (column.empty()) {
        throw std::invalid_argument{"distinct requires a column name."};
    }

    return DistinctCommand{std::move(column), sort_by_value, sort_desc};
}

LibCommand reset    (std::string_view args_line) {
    auto args = utils::tokenize(args_line);

    if (args.empty()) {
        return ResetCommand::all();
    }

    ResetCommand cmd;
    for (const auto& arg : args) {
        if      (arg == "--view") cmd.view = true;
        else if (arg == "--proj") cmd.proj = true;
        else if (arg == "--sort") cmd.sort = true;
        else throw std::invalid_argument{
            std::format("Unknown argument for reset command: {}", arg)
        };
    }
    return cmd;
}

CliCommand eXport   (std::string_view args_line) {
    if (args_line.empty()) {
        throw std::invalid_argument{
            "Please provide a path."
        };
    }
    auto args = utils::tokenize(args_line);
    ExportMode mode = ExportMode::Full;
    std::string path;

    for (const auto& arg : args) {
        if (arg == "--projected") {
            mode = ExportMode::Projected;
        } else if (arg == "--full") {
            if (mode == ExportMode::Projected) {
                throw std::invalid_argument{
                    "Cannot specify both --full and --projected."
                };
            }
            mode = ExportMode::Full;
        } else if (path.empty()) {
            path = std::string(arg);
        } else {
            throw std::invalid_argument{
                std::format("Unexpected argument for export command: {}", arg)
            };
        }
    }

    if (path.empty()) {
        throw std::invalid_argument{"Please provide a path."};
    }

    return CliCommandExport{std::move(path), mode};
}

CliCommand help     (std::string_view args_line) {
    if (args_line.empty()) {
        return CliCommandHelp{std::nullopt};
    }
    auto args = utils::tokenize(args_line);
    if (args.size() != 1) {
        throw std::invalid_argument{
            std::format("help command takes at most one argument. Got: {}", args_line)
        };
    }
    return CliCommandHelp{std::optional<std::string>{args[0]}};
}

} // namespace parsers
} // namespace hawk::cli
