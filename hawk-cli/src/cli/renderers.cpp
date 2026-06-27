#include "renderers.hpp"

#include <cli/commands.hpp>
#include <helpers/output_decorator.hpp>
#include <helpers/utils.hpp>

#include <hawk/hawk.hpp>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <format>
#include <iomanip>
#include <iostream>
#include <numeric>
#include <optional>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

namespace hawk::cli {
namespace renderers {

namespace {

// ----- Rendering symbols -----

constexpr std::string_view SYM_ELLIPSIS = "\u2026"; // …
constexpr std::string_view SYM_NEXTLINE = "\u21b5"; // ↵
constexpr std::string_view SYM_TAB      = "\u21e5"; // ⇥
constexpr std::string_view SYM_EMPTY    = "(empty)";

// ----- Helper functions -----

std::string_view strip_csv_quotes(std::string_view field, std::string& buf) {
    // Must start and end with the same quote character
    if (field.size() < 2) return field;
    char q = field.front();
    if ((q != '"' && q != '\'') || field.back() != q) return field;

    std::string_view inner = field.substr(1, field.size() - 2);

    // Fast path: no doubled quotes inside
    std::string doubled;
    doubled += q; doubled += q; // e.g. ""
    if (inner.find(doubled) == std::string_view::npos) return inner;

    // Slow path: unescape doubled quotes
    buf.clear();
    buf.reserve(inner.size());
    for (std::size_t i = 0; i < inner.size(); ++i) {
        buf += inner[i];
        if (inner[i] == q && i + 1 < inner.size() && inner[i + 1] == q) {
            ++i; // skip the second quote
        }
    }
    return buf;
}

// ----- Helper renderers -----

void render_hline(
    const RenderContext& ctx,
    const std::size_t index_width,
    std::string index_sep,
    const std::vector<std::size_t>& column_widths,
    char ch
) {
    if (index_width > 0) {
        ctx.sout << std::setw(index_width) << std::right << std::setfill(' ') << ""  << " " << index_sep << " ";
    }
    for (std::size_t i = 0; i < column_widths.size(); ++i) {
        if (i != 0) ctx.sout << " "; // column separator
        ctx.sout << std::setw(column_widths[i]) << std::setfill(ch) << "";
    }
    ctx.sout << "\n" << std::setfill(' ');
}

void render_colns(
    const RenderContext& ctx,
    const std::size_t index_width,
    std::string index_sep,
    const std::vector<std::size_t>& column_widths,
    char ch,
    const hawk::Projection* projection
) {
    if (index_width > 0) {
        ctx.sout << std::setw(index_width) << std::setfill(' ') << "" << " " << index_sep << " ";
    }
    ctx.sout << std::left;
    for (std::size_t i = 0; i < column_widths.size(); ++i) {
        if (i != 0) ctx.sout << " "; // column separator
        ctx.sout << std::setw(column_widths[i]) << std::setfill(ch) << std::format("[$col{}]", projection->at(i) + 1);
    }
    ctx.sout << "\n" << std::setfill(' ');
}

void render_field(
    const RenderContext& ctx,
    std::string_view field,
    std::size_t width,
    bool has_trailer = false
) {
    std::string quote_buf;
    field = strip_csv_quotes(field, quote_buf);

    // Fast path: no control characters
    if (field.find_first_of("\r\n\t") == std::string_view::npos) {
        if (field.size() <= width) {
            ctx.sout << field;
            ctx.sout << std::string(width - field.size(), ' ');
        } else {
            ctx.sout << field.substr(0, width - (has_trailer ? 0 : 1));
            ctx.sout << SYM_ELLIPSIS;
        }
        return;
    }

    // Slow path
    std::size_t col = 0;
    bool truncated = false;

    for (std::size_t i = 0; i < field.size(); ++i) {
        char c = field[i];
        if (c == '\r') continue;

        if (col == width - 1 && i + 1 < field.size()) {
            if (has_trailer) ctx.sout.put(c);
            truncated = true;
            ++col;
            break;
        } else if (c == '\n') {
            ctx.sout << SYM_NEXTLINE;
            ++col;
        } else if (c == '\t') {
            ctx.sout << SYM_TAB;
            ++col;
        } else {
            ctx.sout.put(c);
            ++col;
        }
    }

    if (col < width)
        ctx.sout << std::string(width - col, ' ');

    if (truncated)
        ctx.sout << SYM_ELLIPSIS;
}

void render_header(
    const RenderContext& ctx,
    const std::vector<std::size_t>& column_widths,
    const std::size_t index_width,
    const hawk::Projection* projection
) {
    render_colns(ctx, index_width, "┐", column_widths, '-', projection);
    if (index_width > 0) {
        ctx.sout << std::setw(index_width) << std::right << "#" << " │ ";
    }
    for (std::size_t i = 0; i < column_widths.size(); ++i) {
        if (i != 0) ctx.sout << " "; // column separator
        auto col_name = ctx.schema.column(projection->at(i)).name;
        render_field(ctx, col_name, column_widths[i]);
    }
    ctx.sout << "\n";
    render_hline(ctx, index_width, "┤", column_widths, '-');
}

void render_row_horizontal(
    const RenderContext& ctx,
    const hawk::Row& row,
    const std::vector<std::size_t>& column_widths,
    const std::size_t index_width,
    const hawk::Projection* projection
) {
    if (index_width > 0) {
        ctx.sout << std::setw(index_width) << std::right << row.index() + 1 << " │ ";
    }
    ColumnCount column_count = projection->size();
    for (std::size_t i = 0; i < column_count; ++i) {
        if (i != 0) ctx.sout << " "; // column separator
        auto field = row.get_projected(projection, i);
        render_field(ctx, field, column_widths[i]);
    }
    ctx.sout << "\n";
}

void render_records_horizontal(
    const RenderContext& ctx,
    const hawk::RecordsResult& res
) {
    const std::size_t min_col_width = 8;
    const ColumnCount column_count = res.projection->size();

    std::size_t index_width = 1;
    for (const auto& row : res.rows) {
        index_width = std::max(index_width, hawk::cli::utils::num_digits(row.index() + 1));
    }

    // Natural width: widest of header name and any data value, uncapped.
    std::vector<std::size_t> natural_widths(column_count, 0);
    for (ColumnIndex i = 0; i < column_count; ++i) {
        ColumnIndex col = res.projection->at(i);
        natural_widths[i] = std::max(min_col_width, ctx.schema.column(col).name.size());
    }
    for (const auto& row : res.rows) {
        for (std::size_t i = 0; i < column_count; ++i) {
            natural_widths[i] = std::max(natural_widths[i], row.get_projected(res.projection, i).size());
        }
    }

    // Budget: terminal width minus index gutter and one separator space per column.
    // If terminal_width is too small to even hold the index gutter, bail out and
    // let columns use their minimum widths (rows will wrap).
    const std::size_t index_gutter  = index_width + 2;
    const std::size_t separator_sum = column_count; // 1 space per column
    std::vector<std::size_t> column_widths(column_count, min_col_width);

    if (ctx.terminal_width > index_gutter + separator_sum) {
        std::size_t budget = ctx.terminal_width - index_gutter - separator_sum;

        // Distribute budget to columns in ascending order of additional need.
        // This ensures columns that need a little more get served before we
        // hit the limit, rather than one greedy wide column taking everything.
        std::vector<ColumnIndex> order(column_count);
        std::iota(order.begin(), order.end(), 0);
        std::sort(order.begin(), order.end(), [&](ColumnIndex a, ColumnIndex b) {
            return natural_widths[a] < natural_widths[b];
        });

        std::size_t columns_remaining = column_count;
        for (ColumnIndex i : order) {
            const std::size_t fair_share = budget / columns_remaining;
            const std::size_t allotment  = std::min(natural_widths[i], std::max(min_col_width, fair_share));
            column_widths[i] = allotment;
            budget -= allotment;
            --columns_remaining;
        }
    }

    render_header(ctx, column_widths, index_width, res.projection);
    for (const auto& row : res.rows) {
        render_row_horizontal(ctx, row, column_widths, index_width, res.projection);
    }
}

void render_field_vertical(
    const RenderContext& ctx,
    std::string_view field,
    std::size_t field_width,
    std::size_t sidebar_width,
    bool untruncated
) {
    if (field.empty()) {
        ctx.sout << sgr::colorize(SYM_EMPTY, "#555");
        return;
    }

    std::string quote_buf;
    field = strip_csv_quotes(field, quote_buf);

    if (!untruncated) {
        render_field(ctx, field, field_width, true);
        return;
    }

    std::size_t pos = 0;
    std::size_t col = 0;
    while (pos < field.size()) {
        char c = field[pos];

        if (c == '\r') {
            ++pos;
            continue;
        }

        if (c == '\n') {
            ++pos;
            // Pad remaining columns
            if (col < field_width) {
                ctx.sout << std::string(field_width - col, ' ');
            }
            ctx.sout << SYM_NEXTLINE << "\n " << std::string(sidebar_width, ' ') << " ";
            col = 0;
            continue;
        }

        if (col >= field_width) {
            ctx.sout << SYM_ELLIPSIS << "\n " << std::string(sidebar_width, ' ') << " ";
            col = 0;
            continue;
        }

        ctx.sout.put(c);
        ++col;
        ++pos;
    }
}

void render_row_vertical(
    const RenderContext& ctx,
    const hawk::Row& row,
    const std::size_t& sidebar_width,
    const hawk::Projection* projection,
    bool untruncated
) {
    const std::size_t min_field_width = 20;
    const std::size_t field_width = ctx.terminal_width > sidebar_width + 3
        ? ctx.terminal_width - sidebar_width - 3
        : min_field_width;
    ColumnCount column_count = projection->size();
    ctx.sout
        << sgr::rgb("#0af")
        << std::format(" {:>{}} #{}", "Record", sidebar_width, row.index() + 1)
        << sgr::rst() << "\n"
        << std::string(ctx.terminal_width, '-') << "\n";
    for (ColumnIndex i = 0; i < column_count; ++i) {
        ColumnIndex col = projection->at(i);
        auto field = row.get_projected(projection, i);
        ctx.sout << std::format(" {:<{}} ", ctx.schema.column(col).name, sidebar_width);
        render_field_vertical(ctx, field, field_width, sidebar_width, untruncated);
        ctx.sout << "\n";
    }
    ctx.sout << "\n";
}

void render_records_vertical(
    const RenderContext& ctx,
    const hawk::RecordsResult& res,
    bool untruncated
) {
    ColumnCount column_count = res.projection->size();
    std::size_t sidebar_width = 0;
    for (std::size_t i = 0; i < column_count; ++i) {
        ColumnIndex col = res.projection->at(i);
        sidebar_width = std::max(sidebar_width, ctx.schema.column(col).name.size());
    }
    ctx.sout << "\n";
    for (const auto& row : res.rows) {
        render_row_vertical(
            ctx,
            row,
            sidebar_width,
            res.projection,
            untruncated
        );
    }
    ctx.sout << std::endl;
}

// ----- Render functions implementations -----

void render_impl(
    const RenderContext& ctx,
    const hawk::ConfigResult& res,
    const RenderOptions&
) {
    render_info(std::format(
        "Configuration:"
        "\n  Delimiter:      '{}'"
        "\n  Header row:     {}"
        "\n  Line endings:   {}"
        "\n  Case sensitive: {}",
        res.delimiter,
        res.has_header ? "Yes" : "No",
        res.use_crlf ? "CRLF (Windows)" : "LF (Unix)",
        res.case_sensitive ? "Yes" : "No"
    ), ctx.sout);
}

void render_impl(
    const RenderContext& ctx,
    const hawk::RecordsResult& res,
    const RenderOptions& options
) {
    if (res.rows.empty()) {
        render_info("No records to display.", ctx.sout);
        return;
    }
    if (options.display_mode == DisplayMode::Horizontal) {
        render_records_horizontal(ctx, res);
    } else {
        render_records_vertical(ctx, res, options.display_mode == DisplayMode::VerticalUntruncated);
    }
}

void render_impl(
    const RenderContext& ctx,
    const hawk::CountResult& res,
    const RenderOptions&
) {
    render_info("Count: " + std::to_string(res.count), ctx.sout);
}

void render_impl(
    const RenderContext& ctx,
    const hawk::ColumnsResult& res,
    const RenderOptions&
) {
    std::size_t max_name_length = 0;
    for (const auto& col : res.columns)
        max_name_length = std::max(max_name_length, col.name.size());
    for (std::size_t i = 0; i < res.columns.size(); ++i) {
        const auto& col = res.columns.at(i);
        ctx.sout << std::setw(8) << std::left << "$col" + std::to_string(i + 1);
        ctx.sout << std::setw(max_name_length + 2) << std::left << col.name;
        ctx.sout << "(" << hawk::column_type_name(col.type);
        if (col.nullable) {
            ctx.sout << ", nullable";
        }
        if (col.type == ColumnType::DateTime && col.datetime_pattern.has_value()) {
            ctx.sout << ", " << col.datetime_pattern.value();
        }
        ctx.sout << ")";
        ctx.sout << std::endl;
    }
}

void render_impl(
    const RenderContext& ctx,
    const hawk::FilterResult& res,
    const RenderOptions&
) {
    render_info("Matched: " + std::to_string(res.matched), ctx.sout);
}

void render_impl(
    const RenderContext& ctx,
    const hawk::SliceResult& res,
    const RenderOptions&
) {
    render_info("Matched: " + std::to_string(res.size), ctx.sout);
}

void render_impl(
    const RenderContext& ctx,
    const hawk::SortResult& res,
    const RenderOptions&
) {
    render_info(std::format("Sorted {} row(s) by '{}' {}",
        res.row_count,
        res.column,
        res.is_desc ? "descending" : "ascending"
    ), ctx.sout);
}

void render_impl(
    const RenderContext& ctx,
    const hawk::DistinctResult& res,
    const RenderOptions&
) {
    render_info(std::format("Found {} distinct values for '{}' ({} total rows):\n",
        res.entries.size(), res.column, res.total_rows), ctx.sout);

    // Column width for alignment
    std::size_t max_value_width = 8; // minimum — length of "(empty)"
    std::size_t count_width = 5; // minimum — length of "Count"
    for (const auto& entry : res.entries) {
        max_value_width = std::max(max_value_width, entry.value.size());
        count_width = std::max(count_width, static_cast<std::size_t>(std::to_string(entry.count).size()));
    }

    ctx.sout << std::format("  {:<{}}  {}\n",
        "Value", max_value_width, "Count");
    ctx.sout << std::format("  {:<{}}  {}\n",
        std::string(max_value_width, '-'), max_value_width, std::string(count_width, '-'));

    for (const auto& entry : res.entries) {
        ctx.sout << std::format("  {:<{}}  {:>{}}\n",
            entry.value, max_value_width, entry.count, count_width);
    }
    ctx.sout << std::endl;
}

} // anonymous namespace

void render_result(
    const RenderContext& ctx,
    const hawk::CommandResult& result,
    const RenderOptions& options
) {
    if (result.error.has_value()) {
        render_error(result.error.value(), ctx.serr);
        return;
    }
    if (result.payload.has_value()) {
        std::visit(
            [&](const auto& res) {
                render_impl(ctx, res, options);
            },
            result.payload.value()
        );
    } else {
        render_success(ctx.sout);
    }
    render_warnings(result.warnings, ctx.serr);
    render_execution_time(result.execution_time_ms, ctx.sout);
}

void render_export(
    const RenderContext& ctx,
    const hawk::RecordsResult& result,
    const hawk::SessionConfig& config,
    ExportMode mode
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
                if (!first) ctx.sout << config.delimiter;
                ctx.sout << ctx.schema.column(col_idx).name;
                first = false;
            }
        } else {
            for (hawk::ColumnIndex i = 0; i < ctx.schema.column_count(); ++i) {
                if (!first) ctx.sout << config.delimiter;
                ctx.sout << ctx.schema.column(i).name;
                first = false;
            }
        }
        ctx.sout << eol;
    }

    // Render rows
    if (use_projection) {
        for (const auto& row : result.rows) {
            bool first = true;
            for (std::size_t i = 0; i < result.projection->size(); ++i) {
                if (!first) ctx.sout << config.delimiter;
                ctx.sout << row.get_projected(result.projection, i);
                first = false;
            }
            ctx.sout << eol;
        }
    } else {
        for (const auto& row : result.rows) {
            ctx.sout << row.record() << eol;
        }
    }
}

void render_execution_time(std::uint64_t ms, std::ostream& sout) {
    std::string time_str = ms == 0 ? "<1" : std::to_string(ms);
    sout << cli::sgr::colorize(std::format("({}ms)", time_str), "#555") << std::endl;
}

void render_success (                                           std::ostream& sout) {
    sout << hawk::cli::log_success("✔ Done.") << std::endl;
}

void render_info    (std::string_view message,                  std::ostream& sout) {
    sout << hawk::cli::log_info(std::string(message)) << std::endl;
}

void render_error   (std::string_view message,                  std::ostream& serr) {
    serr << hawk::cli::log_error(std::format("✘ Error: {}", message)) << std::endl;
}

void render_error   (std::string_view message) { render_error(message, std::cerr); }

void render_warning (std::string_view warning,                  std::ostream& serr) {
    serr << hawk::cli::log_warning(std::format("‼ Warning: {}", warning)) << std::endl;
}

void render_warning (std::string_view warning) { render_warning(warning, std::cerr); }

void render_warnings(const std::vector<std::string>& warnings,  std::ostream& serr) {
    for (const auto& warning : warnings) {
        render_warning(warning, serr);
    }
}

} // namespace renderers
} // namespace hawk::cli
