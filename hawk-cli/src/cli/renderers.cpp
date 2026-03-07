#include "renderers.hpp"

#include <helpers/output_decorator.hpp>

#include <hawk/hawk.hpp>

#include <iostream>

namespace hawk::cli {
namespace renderers {

template <typename> inline constexpr bool always_false = false;

void render(const hawk::CommandResult& result, std::ostream& sout) {
    std::visit([&sout](auto&& res) {
        using T = std::decay_t<decltype(res)>;
        if constexpr (std::is_same_v<T, hawk::RowsResult>) {
            render_rows(res, sout);
        } else if constexpr (std::is_same_v<T, hawk::ErrorResult>) {
            render_error(res, sout);
        } else {
            static_assert(always_false<T>, "Unhandled CommandResult type");
        }
    }, result);
}

void render_rows(const hawk::RowsResult& rows, std::ostream& sout) {
    for (const auto& row : rows.rows) {
        for (const auto& field : row.fields()) {
            sout << field << " ";
        }
        sout << "\n";
    }
}

void render_error(const hawk::ErrorResult& error, std::ostream& sout) {
    sout << hawk::cli::error_log("Error: " + error.message) << "\n";
}

} // namespace renderers
} // namespace hawk::cli
