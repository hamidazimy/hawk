#include "parsers.hpp"

#include <cli/cli_commands.hpp>
#include <helpers/utils.hpp>

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

LibCommand columns  (std::string_view args_line) {
    if (!args_line.empty()) {
        throw std::invalid_argument{
            std::format("columns command does not take any arguments. Got: {}", args_line)
        };
    }
    return ColumnsCommand{};
}

namespace {

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
    auto column = std::string(args[0]);
    auto type_str = args[1];
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
    } else if (args[0] == "name") {
        return set_name(args_line.substr(5)); // length of "name "
    } else if (args[0] == "type") {
        return set_type(args_line.substr(5)); // length of "type "
    } else {
        throw std::invalid_argument{
            std::format(
                "Invalid set command. Got: {} \n"
                "Only 'set type <column> <type> (<format>)' and "
                "'set name <old_name> <new_name>' are supported at the moment.",
                args_line
            )
        };
    }
}

LibCommand select   (std::string_view args_line) {
    if (args_line.empty()) {
        throw std::invalid_argument{
            "select requires at least one column. "
            "Usage: select <col1>,<col2>,..."
        };
    }
    auto col_views = hawk::utils::split(args_line, ',');
    std::vector<std::string> cols;
    cols.reserve(col_views.size());
    for (auto sv : col_views) {
        auto trimmed = hawk::utils::trim(sv);
        if (trimmed.empty()) {
            throw std::invalid_argument{
                "Empty column name in select"
            };
        }
        cols.emplace_back(trimmed);
    }
    return SelectCommand{std::move(cols)};
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
        throw std::invalid_argument{
            "Please provide a record index to show."
        };
    }
    std::int64_t index;
    if (!hawk::utils::parse_int(args_line, index) || index < 1) {
        throw std::invalid_argument{
            std::format("Invalid argument for peek command: {}", args_line)
        };
    }
    return PeekCommand{static_cast<RecordIndex>(index - 1)}; // changing 1-based indexing of cli to 0-based indexing of the lib.
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
    return HeadCommand{count};
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
}

LibCommand filter   (std::string_view args_line) {
    auto args = utils::tokenize(args_line);
    if (args.size() != 3) {
        throw std::invalid_argument{
            std::format("Invalid arguments for filter command: {}", args_line)
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
            std::format("Invalid operator in filter command: {}", args[1])
        };
    }
    if (args[0] == "$row" && op != hawk::FilterOp::HAS) {
        throw std::invalid_argument{
            std::format("Only 'has' operator is supported for $row searches. Got: {}", args[1])
        };
    }

    return FilterCommand{std::string(args[0]), args[0] == "$row", op, std::string(args[2])};
}

LibCommand reset    (std::string_view args_line) {
    if (!args_line.empty()) {
        throw std::invalid_argument{
            std::format("Invalid arguments in reset command: {}", args_line)
        };
    }
    return ResetViewCommand{};
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
