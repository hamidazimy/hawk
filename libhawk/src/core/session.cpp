#include <hawk/core/session.hpp>

#include <hawk/core/types.hpp>
#include <hawk/core/record_source.hpp>
#include <hawk/core/schema.hpp>
#include <hawk/core/row.hpp>
#include <hawk/core/view.hpp>
#include <hawk/core/column.hpp>
#include <hawk/core/projection.hpp>
#include <hawk/core/filter.hpp>
#include <hawk/core/commands.hpp>
#include <hawk/core/results.hpp>
#include <hawk/utils/utils.hpp>
#include <hawk/utils/datetime_parser.hpp>

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <format>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <variant>
#include <vector>

namespace hawk { struct SessionConfig; }
namespace hawk { class RecordParser; }

namespace hawk {

Session::Session(
    const SessionConfig& config,
    std::unique_ptr<RecordSource> source,
    std::unique_ptr<RecordParser> parser,
    Schema schema
)
    : config_(config)
    , source_(std::move(source))
    , parser_(std::move(parser))
    , schema_(std::move(schema))
    , current_view_(View::identity(row_count()))
    , current_projection_(schema_.column_count())
{}

std::string_view Session::raw_record_from_file(RecordIndex file_index) const {
    return source_->get_record(to_source_index(file_index));
}

std::string_view Session::raw_record_from_view(RecordIndex view_index) const {
    auto file_index = current_view_.at(view_index);
    return raw_record_from_file(file_index);
}

Row Session::make_row_from_file(RecordIndex file_index) const {
    auto record = raw_record_from_file(file_index);
    return Row(file_index, record, parser_.get());
}

Row Session::make_row_from_view(RecordIndex view_index) const {
    auto file_index = current_view_.at(view_index);
    return make_row_from_file(file_index);
}

CommandResult Session::execute(const LibCommand& command) {
    const auto t0 = std::chrono::steady_clock::now();

    auto result = std::visit(
        [this](const auto& cmd) -> CommandResult {
            return execute_impl(cmd);
        },
        command
    );

    const auto t1 = std::chrono::steady_clock::now();

    result.execution_time_ms =
        std::chrono::duration_cast<std::chrono::milliseconds>(t1 - t0).count();

    return result;
}

// --- Command implementations ---

namespace {
// --- Internal helpers ---

std::optional<std::string> resolve_columns(
    const Schema& schema,
    const std::vector<std::string>& names,
    std::vector<ColumnIndex>& out)
{
    out.reserve(names.size());
    for (const auto& name : names) {
        auto index = schema.find_column(name);
        if (!index.has_value()) {
            return std::format("Unknown column: {}", name);
        }
        out.push_back(*index);
    }
    return std::nullopt;
}

} // namespace

CommandResult Session::execute_impl(const RowsCommand&) {
    auto count = current_view_.size();
    std::vector<Row> rows;
    rows.reserve(count);

    for (RecordIndex i = 0; i < count; ++i) {
        rows.emplace_back(make_row_from_view(i));
    }

    return CommandResult::ok(RowsResult{
        std::move(rows),
        &current_projection_
    });
}

CommandResult Session::execute_impl(const ColumnsCommand&) {
    return CommandResult::ok(ColumnsResult{
        std::move(schema_.columns())
    });
}

CommandResult Session::execute_impl(const SetColumnNameCommand& cmd) {
    auto column_index = schema_.find_column(cmd.old_name);
    if (!column_index) {
        return CommandResult::err(std::format(
            "Unknown column: {}",
            cmd.old_name
        ));
    }

    schema_.set_column_name(*column_index, cmd.new_name);
    return CommandResult::ok();
}

CommandResult Session::execute_impl(const SetColumnTypeCommand& cmd) {
    auto column_index = schema_.find_column(cmd.column);
    if (!column_index) {
        return CommandResult::err(std::format(
            "Unknown column: {}",
            cmd.column
        ));
    }

    if (cmd.type == ColumnType::DateTime && !cmd.datetime_pattern.has_value()) {
        return CommandResult::err(
            "Setting a column to datetime requires a pattern."
        );
    }

    // NOTE: This does not invalidate the current view. Any existing filtered
    // view was built under the previous type interpretation. Whether that
    // produces meaningful results is the analyst's responsibility.
    // Revisit if view invalidation on type change becomes necessary.
    schema_.set_column_type(*column_index, cmd.type, cmd.datetime_pattern);

    return CommandResult::ok();
}

CommandResult Session::execute_impl(const SelectCommand& cmd) {
    std::vector<ColumnIndex> indices;
    if (auto err = resolve_columns(schema_, cmd.columns, indices)) {
        return CommandResult::err(*err);
    }
    current_projection_.select(std::move(indices));
    return CommandResult::ok();
}

CommandResult Session::execute_impl(const SelectAddCommand& cmd) {
    std::vector<ColumnIndex> indices;
    if (auto err = resolve_columns(schema_, cmd.columns, indices)) {
        return CommandResult::err(*err);
    }
    current_projection_.add(indices);
    return CommandResult::ok();
}

CommandResult Session::execute_impl(const DeselectCommand& cmd) {
    std::vector<ColumnIndex> indices;
    if (auto err = resolve_columns(schema_, cmd.columns, indices)) {
        return CommandResult::err(*err);
    }
    if (current_projection_.size() - indices.size() < 1) {
        return CommandResult::err(
            "Cannot remove all columns — at least one must remain. "
            "Use 'select' to choose a new set."
        );
    }
    current_projection_.drop(indices);
    return CommandResult::ok();
}

CommandResult Session::execute_impl(const CountCommand&) {
    return CommandResult::ok(CountResult{
        current_view_.size()
    });
}

CommandResult Session::execute_impl(const PeekCommand& cmd) {
    if (cmd.index >= row_count()) {
        return CommandResult::err(std::format(
            "Index out of range: {} (total records: {})",
            cmd.index, row_count()
        ));
    }

    return CommandResult::ok(RowsResult{
        {make_row_from_file(cmd.index)},
        &current_projection_
    });
}

CommandResult Session::execute_impl(const HeadCommand& cmd) {
    RecordCount max_visible_records = visible_row_count();
    RecordCount count = std::min(cmd.max_records, max_visible_records);

    std::vector<Row> rows;
    rows.reserve(count);

    for (RecordIndex i = 0; i < count; ++i) {
        rows.emplace_back(make_row_from_view(i));
    }

    return CommandResult::ok(RowsResult{
        std::move(rows),
        &current_projection_
    });
}

CommandResult Session::execute_impl(const TailCommand& cmd) {
    RecordCount max_visible_records = visible_row_count();
    RecordCount count = std::min(cmd.max_records, max_visible_records);

    std::vector<Row> rows;
    rows.reserve(count);

    const RecordIndex start = max_visible_records - count;

    for (RecordIndex i = start; i < max_visible_records; ++i) {
        rows.emplace_back(make_row_from_view(i));
    }

    return CommandResult::ok(RowsResult{
        std::move(rows),
        &current_projection_
    });
}

CommandResult Session::execute_impl(const FilterCommand& cmd) {
    if (cmd.row_search) {
        if (cmd.op != FilterOp::HAS) {
            return CommandResult::err("$row only supports the 'has' operator");
        }
        RowSearchPredicate predicate{cmd.value};
        current_view_ = current_view_.filter(
            [this, &predicate](RecordIndex row_index) {
                return predicate(raw_record_from_file(row_index));
            }
        );
        return CommandResult::ok(FilterResult{current_view_.size(), 0});
    }

    auto column_index = schema_.find_column(cmd.column);
    if (!column_index) {
        return CommandResult::err(std::format(
            "Unknown column: {}",
            cmd.column
        ));
    }

    const ColumnType column_type = schema_.column_type(*column_index);

    // Validate operator is compatible with column type before scanning.
    if (cmd.op == FilterOp::HAS && column_type != ColumnType::String) {
        return CommandResult::err(std::format(
            "Operator 'has' is only valid for string columns, column '{}' is {}",
            cmd.column, column_type_name(column_type)
        ));
    }

    // Validate RHS and resolve datetime pattern before scanning.
    std::optional<std::string> datetime_pattern;
    if (column_type == ColumnType::DateTime) {
        const auto& col_schema = schema_.column(*column_index);
        if (!col_schema.datetime_pattern.has_value()) {
            return CommandResult::err(std::format(
                "Column '{}' has no datetime pattern — cannot filter",
                cmd.column
            ));
        }
        datetime_pattern = col_schema.datetime_pattern;
        if (!utils::parse_datetime(cmd.value, *datetime_pattern)) {
            return CommandResult::err(std::format(
                "Filter value '{}' cannot be parsed "
                "as datetime pattern '{}' for column '{}'",
                cmd.value, *datetime_pattern, cmd.column
            ));
        }
    }

    // Validate RHS is parseable as the column's declared type before scanning.
    if (column_type == ColumnType::Integer) {
        std::int64_t dummy;
        if (!utils::parse_int(cmd.value, dummy)) {
            return CommandResult::err(std::format(
                "Filter value '{}' cannot be parsed as {} for column '{}'",
                cmd.value, column_type_name(column_type), cmd.column
            ));
        }
    }
    if (column_type == ColumnType::Float) {
        double dummy;
        if (!utils::parse_double(cmd.value, dummy)) {
            return CommandResult::err(std::format(
                "Filter value '{}' cannot be parsed as {} for column '{}'",
                cmd.value, column_type_name(column_type), cmd.column
            ));
        }
    }

    FilterPredicate predicate{*column_index, column_type, cmd.op, cmd.value, datetime_pattern};

    current_view_ = current_view_.filter(
        [this, &predicate](RecordIndex row_index) {
            return predicate(make_row_from_file(row_index));
        }
    );

    auto result = CommandResult::ok(FilterResult{
        current_view_.size(),
        predicate.skipped
    });

    if (predicate.skipped > 0) {
        result.warnings.push_back(std::format(
            "{} row(s) skipped: field could not be parsed as {}",
            predicate.skipped, column_type_name(column_type)
        ));
    }

    return result;
}

CommandResult Session::execute_impl(const ResetViewCommand&) {
    current_view_ = View::identity(row_count());
    return CommandResult::ok();
}

} // namespace hawk
