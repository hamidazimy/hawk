#include "renderers.hpp"

#include <helpers/output_decorator.hpp>

#include <hawk/hawk.hpp>

#include <algorithm>
#include <cstddef>
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
                  std::ostream& sout)
{
    for (std::size_t i = 0; i < column_widths.size(); ++i)
        sout << std::setw(column_widths[i]) << std::setfill(ch) << "" << " ";
    sout << "\n" << std::setfill(' ');
}

void render_header(const std::vector<std::string>& column_names,
                   const std::vector<std::size_t>& column_widths,
                   std::ostream& sout)
{
    render_hline('-', column_widths, sout);
    for (std::size_t i = 0; i < column_widths.size(); ++i)
        sout << std::setw(column_widths[i]) << std::left << column_names[i] << " ";
    sout << "\n";
    render_hline('-', column_widths, sout);
}

void render_row(const hawk::Row& row,
                const std::vector<std::size_t>& column_widths,
                std::ostream& sout)
{
    for (std::size_t i = 0; i < row.length(); ++i) {
        auto field = row.at(i);
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
    std::size_t MAX_COL_WIDTH = 20;
    std::size_t MIN_COL_WIDTH = 4;
    std::vector<std::size_t> column_widths(schema.column_count(), 0);
    for (std::size_t i = 0; i < column_widths.size(); ++i) {
        column_widths[i] = std::max(MIN_COL_WIDTH, schema.column_names()[i].size());
    }
    for (const auto& row : res.rows) {
        for (std::size_t i = 0; i < row.length(); ++i) {
            column_widths[i] = std::min(MAX_COL_WIDTH, std::max(column_widths[i], row[i].size()));
        }
    }
    render_header(schema.column_names(), column_widths, sout);
    for (const auto& row : res.rows) {
        render_row(row, column_widths, sout);
    }
}

void render_impl(const hawk::CountResult& res,
                 const hawk::Schema&,
                 std::ostream& sout)
{
    sout << "Count: " << res.count << "\n";
}

void render_impl(const hawk::ColumnsResult& res,
                 const hawk::Schema&,
                 std::ostream& sout)
{
    std::size_t column_count = res.columns.size();
    for (std::size_t i = 0; i < column_count; ++i) {
        sout << std::setw(8) << std::left << "$col" + std::to_string(i + 1) << res.columns.at(i) << std::endl;
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

void render_impl(const hawk::SuccessResult& res,
                 const hawk::Schema&,
                 std::ostream& sout)
{
    sout << "Command executed successfully in " << res.execution_time_ms << " ms.\n";
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
    sout << hawk::cli::error_log("Error: " + message) << "\n";
}

} // namespace renderers
} // namespace hawk::cli
