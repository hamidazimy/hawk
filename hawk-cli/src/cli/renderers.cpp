#include "renderers.hpp"

#include <helpers/output_decorator.hpp>
#include <helpers/utils.hpp>

#include <hawk/hawk.hpp>

#include <algorithm>
#include <cstddef>
#include <cstdint>
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

void render_hline(char ch,
                  const std::vector<std::size_t>& column_widths,
                  const std::uint8_t index_width,
                  std::string index_sep,
                  std::ostream& sout)
{
    if (index_width > 0) {
        sout << std::setw(index_width + 1) << std::setfill(' ') << "" << index_sep << " ";
    }
    for (std::size_t i = 0; i < column_widths.size(); ++i)
        sout << std::setw(column_widths[i]) << std::setfill(ch) << "" << " ";
    sout << "\n" << std::setfill(' ');
}

void render_header(const hawk::Schema& schema,
                   const hawk::Projection* projection,
                   const std::vector<std::size_t>& column_widths,
                   const std::uint8_t index_width,
                   std::ostream& sout)
{
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

void render_row(const hawk::Row& row,
                const hawk::Projection* projection,
                const std::vector<std::size_t>& column_widths,
                const std::uint8_t index_width,
                std::ostream& sout)
{
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

void render_impl(const hawk::RowsResult& res,
                 const hawk::Schema& schema,
                 std::ostream& sout)
{
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
        index_width = std::max(index_width, hawk::cli::utils::digits(row.index() + 1));
    }
    index_width = 0; // Tepmorarily disable index column. TODO: fix this!
    render_header(schema, res.projection, column_widths, index_width, sout);
    for (const auto& row : res.rows) {
        render_row(row, res.projection, column_widths, index_width, sout);
    }
}

void render_impl(const hawk::CountResult& res,
                 const hawk::Schema&,
                 std::ostream& sout)
{
    sout << cli::log_info("Count: " + std::to_string(res.count)) << std::endl;
}

void render_impl(const hawk::ColumnsResult& res,
                 const hawk::Schema&,
                 std::ostream& sout)
{
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
        sout << ")";
        sout << std::endl;
    }
}

void render_impl(const hawk::FilterResult& res,
                 const hawk::Schema&,
                 std::ostream& sout)
{
    sout << cli::log_success("Matched: " + std::to_string(res.matched)) << std::endl;
    if (res.skipped > 0) {
        sout << cli::log_success("Skipped: " + std::to_string(res.skipped)) << std::endl;
        if (!res.warning.empty()) {
            sout << cli::log_warning("Warning: " + res.warning) << std::endl;
        }
    }
}

void render_impl(const hawk::ExportResult& res,
                 const hawk::Schema&,
                 std::ostream& sout)
{
    if (res.header.has_value()) {
        sout << res.header.value() << std::endl;
    }
    for (const auto& row : res.rows) {
        sout << row.record() << std::endl;
    }
}

void render_impl(const hawk::SuccessResult&,
                 const hawk::Schema&,
                 std::ostream& sout)
{
    sout << hawk::cli::log_success("✔ Done.") << std::endl;
}

void render_impl(const hawk::ErrorResult& res,
                 const hawk::Schema&,
                 std::ostream& sout)
{
    render_error(res.message, sout);
}

} // anonymous namespace

void render_result(
    const hawk::CommandResult& result,
    const hawk::Schema& schema,
    std::ostream& sout)
{
    std::visit(
        [&](const auto& res) {
            render_impl(res, schema, sout);
        },
        result
    );
}

void render_error(const std::string& message, std::ostream& sout) {
    sout << hawk::cli::log_error("✘ Error: " + message) << std::endl;
}

} // namespace renderers
} // namespace hawk::cli
