#include "renderers.hpp"

#include <helpers/output_decorator.hpp>

#include <hawk/hawk.hpp>

#include <iomanip>
#include <iostream>
#include <string>
#include <vector>

namespace hawk::cli {
namespace renderers {

namespace {

void render_impl(const hawk::RowsResult& res,
                 const hawk::Schema& schema,
                 std::ostream& sout)
{
        std::vector<int> column_widths(schema.column_count(), 0);
    for (std::size_t i = 0; i < schema.column_count(); ++i) {
        column_widths[i] = std::max(column_widths[i], static_cast<int>(schema.column_names()[i].size()));
    }
    for (const auto& row : res.rows) {
        for (std::size_t i = 0; i < row.fields().size(); ++i) {
            column_widths[i] = std::max(column_widths[i], static_cast<int>(row[i].size()));
        }
    }
    for (std::size_t i = 0; i < schema.column_count(); ++i) {
        sout << std::setw(column_widths[i]) << std::setfill('-') << "" << " ";
    }
    sout << "\n" << std::setfill(' ');
    for (std::size_t i = 0; i < schema.column_count(); ++i) {
        sout << std::setw(column_widths[i]) << std::left << schema.column_names()[i] << " ";
    }
    sout << "\n";
    for (std::size_t i = 0; i < schema.column_count(); ++i) {
        sout << std::setw(column_widths[i]) << std::setfill('-') << "" << " ";
    }
    sout << "\n" << std::setfill(' ');
    for (const auto& row : res.rows) {
        for (std::size_t i = 0; i < row.fields().size(); ++i) {
            sout << std::setw(column_widths[i]) << std::left << row[i] << " ";
        }
        sout << "\n";
    }
}

void render_impl(const hawk::CountResult& res,
                 const hawk::Schema&,
                 std::ostream& sout)
{
    sout << "Count: " << res.count << "\n";
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
