#include "renderers.hpp"

#include <cli/cli_commands.hpp>
#include <helpers/output_decorator.hpp>
#include <helpers/utils.hpp>

#include <hawk/hawk.hpp>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <format>
#include <iomanip>
#include <iostream>
#include <optional>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

namespace hawk::cli {
namespace renderers {

namespace {

// ----- Helper functions -----

void render_hline(
    char ch,
    const std::vector<std::size_t>& column_widths,
    const std::uint8_t index_width,
    std::string index_sep,
    std::ostream& sout
) {
    if (index_width > 0) {
        sout << std::setw(index_width + 1) << std::setfill(' ') << "" << index_sep << " ";
    }
    for (std::size_t i = 0; i < column_widths.size(); ++i)
        sout << std::setw(column_widths[i]) << std::setfill(ch) << "" << " ";
    sout << "\n" << std::setfill(' ');
}

void render_header(
    const hawk::Schema& schema,
    const hawk::Projection* projection,
    const std::vector<std::size_t>& column_widths,
    const std::uint8_t index_width,
    std::ostream& sout
) {
    render_hline('-', column_widths, index_width, "┐", sout);
    if (index_width > 0) {
        sout << std::setw(index_width) << std::right << "#" << " │ ";
    }
    for (std::size_t i = 0; i < column_widths.size(); ++i) {
        ColumnIndex col = projection->at(i);
        sout << std::setw(column_widths[i]) << std::left << schema.column(col).name << " ";
    }
    sout << "\n";
    render_hline('-', column_widths, index_width, "┤", sout);
}

void render_row(
    const hawk::Row& row,
    const hawk::Projection* projection,
    const std::vector<std::size_t>& column_widths,
    const std::uint8_t index_width,
    std::ostream& sout
) {
    if (index_width > 0) {
        sout << std::setw(index_width) << std::right << row.index() + 1 << " │ ";
    }

    ColumnCount column_count = projection->size();
    for (std::size_t i = 0; i < column_count; ++i) {
        auto field = row.get_projected(projection, i);
        const auto width = column_widths[i];

        std::string trimmed;

        if (field.size() > width) {
            trimmed.reserve(width);
            trimmed.append(field.data(), width - 3);
            trimmed += "...";
        } else {
            trimmed = field;
        }

        sout << std::setw(width) << std::left << trimmed << " ";
    }
    sout << "\n";
}

// ----- Render functions implementations -----

void render_impl(
    const hawk::RowsResult& res,
    const hawk::Schema& schema,
    std::ostream& sout
) {
    const std::size_t MAX_COL_WIDTH = 20;
    const std::size_t MIN_COL_WIDTH = 4;
    ColumnCount column_count = res.projection->size();
    std::vector<std::size_t> column_widths(column_count, 0);
    std::uint8_t index_width = 1;
    for (std::size_t i = 0; i < column_count; ++i) {
        ColumnIndex col = res.projection->at(i);
        column_widths[i] = std::max(MIN_COL_WIDTH, schema.column(col).name.size());
    }
    for (const auto& row : res.rows) {
        for (std::size_t i= 0; i < column_count; ++i) {
            column_widths[i] = std::min(
                MAX_COL_WIDTH,
                std::max(
                    column_widths[i],
                    row.get_projected(res.projection, i).size()
                )
            );
        }
        index_width = std::max(index_width, hawk::cli::utils::num_digits(row.index() + 1));
    }
    index_width = 0; // Tepmorarily disable index column. TODO: fix this!
    render_header(schema, res.projection, column_widths, index_width, sout);
    for (const auto& row : res.rows) {
        render_row(row, res.projection, column_widths, index_width, sout);
    }
}

void render_impl(
    const hawk::CountResult& res,
    const hawk::Schema&,
    std::ostream& sout
) {
    sout << cli::log_info("Count: " + std::to_string(res.count)) << std::endl;
}

void render_impl(
    const hawk::ColumnsResult& res,
    const hawk::Schema&,
    std::ostream& sout
) {
    std::size_t max_name_length = 0;
    for (const auto& col : res.columns)
        max_name_length = std::max(max_name_length, col.name.size());
    for (std::size_t i = 0; i < res.columns.size(); ++i) {
        const auto& col = res.columns.at(i);
        sout << std::setw(8) << std::left << "$col" + std::to_string(i + 1);
        sout << std::setw(max_name_length + 2) << std::left << col.name;
        sout << "(" << hawk::column_type_name(col.type);
        if (col.nullable) {
            sout << ", nullable";
        }
        if (col.type == ColumnType::DateTime && col.datetime_pattern.has_value()) {
            sout << ", " << col.datetime_pattern.value();
        }
        sout << ")";
        sout << std::endl;
    }
}

void render_impl(
    const hawk::FilterResult& res,
    const hawk::Schema&,
    std::ostream& sout
) {
    sout << cli::log_success("Matched: " + std::to_string(res.matched)) << std::endl;
}

void render_impl(
    const hawk::SortResult& res,
    const hawk::Schema&,
    std::ostream& sout
) {
    sout << cli::log_success(std::format("Sorted {} row(s) by '{}' {}",
        res.row_count,
        res.column,
        res.is_desc ? "descending" : "ascending"
    )) << std::endl;
}

void render_impl(
    const hawk::DistinctResult& res,
    const hawk::Schema&,
    std::ostream& sout)
{
    sout << cli::log_success(std::format("Found {} distinct values for '{}' ({} total rows):\n\n",
        res.entries.size(), res.column, res.total_rows));

    // Column width for alignment
    std::size_t max_value_width = 7; // minimum — length of "(empty)"
    std::size_t count_width = 5; // minimum — length of "Count"
    for (const auto& entry : res.entries) {
        max_value_width = std::max(max_value_width, entry.value.size());
        count_width = std::max(count_width, static_cast<std::size_t>(std::to_string(entry.count).size()));
    }

    sout << std::format("  {:<{}}  {}\n",
        "Value", max_value_width, "Count");
    sout << std::format("  {:<{}}  {}\n",
        std::string(max_value_width, '-'), max_value_width, std::string(count_width, '-'));

    for (const auto& entry : res.entries) {
        sout << std::format("  {:<{}}  {:>{}}\n",
            entry.value, max_value_width, entry.count, count_width);
    }
    sout << std::endl;
}

} // anonymous namespace

void render_result(
    const hawk::CommandResult& result,
    const hawk::Schema& schema,
    std::ostream& sout
) {
    if (result.error.has_value()) {
        render_error(result.error.value(), sout);
        return;
    }
    if (result.payload.has_value()) {
        std::visit(
            [&](const auto& res) {
                render_impl(res, schema, sout);
            },
            result.payload.value()
        );
    } else {
        render_success(sout);
    }
    render_warnings(result.warnings, sout);
    render_execution_time(result.execution_time_ms, sout);
}

void render_export(
    const hawk::RowsResult& result,
    const hawk::Schema& schema,
    const hawk::SessionConfig& config,
    ExportMode mode,
    std::ostream& out
) {
    const std::string_view eol = config.use_crlf ? "\r\n" : "\n";
    const bool use_projection = mode == ExportMode::Projected
                                && result.projection
                                && !result.projection->is_identity();

    // Render header
    if (config.has_header) {
        bool first = true;
        if (use_projection) {
            for (auto col_idx : result.projection->columns()) {
                if (!first) out << config.delimiter;
                out << schema.column(col_idx).name;
                first = false;
            }
        } else {
            for (hawk::ColumnIndex i = 0; i < schema.column_count(); ++i) {
                if (!first) out << config.delimiter;
                out << schema.column(i).name;
                first = false;
            }
        }
        out << eol;
    }

    // Render rows
    if (use_projection) {
        for (const auto& row : result.rows) {
            bool first = true;
            for (std::size_t i = 0; i < result.projection->size(); ++i) {
                if (!first) out << config.delimiter;
                out << row.get_projected(result.projection, i);
                first = false;
            }
            out << eol;
        }
    } else {
        for (const auto& row : result.rows) {
            out << row.record() << eol;
        }
    }
}

void render_execution_time(std::uint64_t ms, std::ostream& sout) {
    std::string time_str = ms == 0 ? "<1" : std::to_string(ms);
    sout << cli::sgr::colorize(std::format("({}ms)", time_str), "#555") << std::endl;
}

void render_success(std::ostream& sout) {
    sout << hawk::cli::log_success("✔ Done.") << std::endl;
}

void render_error(const std::string& message, std::ostream& sout) {
    sout << hawk::cli::log_error("✘ Error: " + message) << std::endl;
}

void render_warning(const std::string& warning, std::ostream& sout) {
    sout << hawk::cli::log_warning("‼ Warning: " + warning) << std::endl;
}

void render_warnings(const std::vector<std::string>& warnings, std::ostream& sout) {
    for (const auto& warning : warnings) {
        render_warning(warning, sout);
    }
}

} // namespace renderers
} // namespace hawk::cli
