#include <hawk/core/session.hpp>

#include <hawk/core/types.hpp>
#include <hawk/core/record_source.hpp>
#include <hawk/core/session_config.hpp>
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
#include <cstdint>
#include <format>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <variant>
#include <vector>

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
{
    current_view_ = View::identity(row_count());
    current_projection_ = Projection(schema_.column_count());
}

Row Session::make_row_from_view(RecordIndex view_index) const {
    auto file_index = current_view_.at(view_index);
    return make_row_from_file(file_index);
}

Row Session::make_row_from_file(RecordIndex file_index) const {
    auto record = source_->get_record(to_source_index(file_index));
    return Row(file_index, record, parser_.get());
}

CommandResult Session::execute(const LibCommand& command) {
    return std::visit(
        [this](const auto& cmd) -> CommandResult {
            return execute_impl(cmd);
        },
        command
    );
}

CommandResult Session::execute_impl(const ExportCommand&) {
    auto count = current_view_.size();
    std::vector<Row> rows;
    rows.reserve(count);

    for (RecordIndex i = 0; i < count; ++i) {
        rows.emplace_back(make_row_from_view(i));
    }

    std::string_view header = {};
    if (config_.has_header) {
        header = source_->get_record(0);
    }
    return ExportResult{header, std::move(rows)};
}

CommandResult Session::execute_impl(const ColumnsCommand&) {
    return ColumnsResult{std::move(schema_.columns())};
}

CommandResult Session::execute_impl(const SetColumnTypeCommand& cmd) {
    auto column_index = schema_.find_column(cmd.column);
    if (!column_index) {
        return ErrorResult{
            std::format("Unknown column: {}", cmd.column)
        };
    }

    if (cmd.type == ColumnType::DateTime && !cmd.datetime_pattern.has_value()) {
        return ErrorResult{
            "Setting a column to datetime requires a pattern. "
            "Usage: set type <column> datetime \"<pattern>\""
        };
    }

    // NOTE: This does not invalidate the current view. Any existing filtered
    // view was built under the previous type interpretation. Whether that
    // produces meaningful results is the analyst's responsibility.
    // Revisit if view invalidation on type change becomes necessary.
    schema_.set_column_type(*column_index, cmd.type, cmd.datetime_pattern);

    return SuccessResult{};
}

CommandResult Session::execute_impl(const SelectCommand& cmd) {
    std::vector<ColumnIndex> columns;

    for (auto column_name : cmd.columns) {
        columns.push_back(*schema_.find_column(column_name));
    }

    current_projection_.select(columns);

    return SuccessResult{};
}

CommandResult Session::execute_impl(const CountCommand&) {
    return CountResult{current_view_.size()};
}

CommandResult Session::execute_impl(const PeekCommand& cmd) {
    if (cmd.index >= row_count()) {
        return ErrorResult{"Index out of range"};
    }

    return RowsResult{
        {make_row_from_file(cmd.index)},
        &current_projection_
    };
}

CommandResult Session::execute_impl(const HeadCommand& cmd) {
    RecordCount max_visible_records = visible_row_count();
    RecordCount count = std::min(cmd.max_records, max_visible_records);

    std::vector<Row> rows;
    rows.reserve(count);

    for (RecordIndex i = 0; i < count; ++i) {
        rows.emplace_back(make_row_from_view(i));
    }

    return RowsResult{
        std::move(rows),
        &current_projection_
    };
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

    return RowsResult{
        std::move(rows),
        &current_projection_
    };
}

CommandResult Session::execute_impl(const FilterCommand& cmd) {
    auto column_index = schema_.find_column(cmd.column);
    if (!column_index) {
        return ErrorResult{
            std::format("Unknown column: {}", cmd.column)
        };
    }

    const ColumnType column_type = schema_.column_type(*column_index);

    // Validate RHS and resolve datetime pattern before scanning.
    std::optional<std::string> datetime_pattern;
    if (column_type == ColumnType::DateTime) {
        const auto& col_schema = schema_.column(*column_index);
        if (!col_schema.datetime_pattern.has_value()) {
            return ErrorResult{
                std::format(
                    "Column '{}' has no datetime pattern — cannot filter",
                    cmd.column
                )
            };
        }
        datetime_pattern = col_schema.datetime_pattern;
        if (!utils::parse_datetime(cmd.value, *datetime_pattern)) {
            return ErrorResult{
                std::format(
                    "Filter value '{}' cannot be parsed "
                    "as datetime pattern '{}' for column '{}'",
                    cmd.value, *datetime_pattern, cmd.column
                )
            };
        }
    }

    // Validate RHS is parseable as the column's declared type before scanning.
    if (column_type == ColumnType::Integer) {
        std::int64_t dummy;
        if (!utils::parse_int(cmd.value, dummy)) {
            return ErrorResult{
                std::format(
                    "Filter value '{}' cannot be parsed as {} for column '{}'",
                    cmd.value, column_type_name(column_type), cmd.column
                )
            };
        }
    }
    if (column_type == ColumnType::Float) {
        double dummy;
        if (!utils::parse_double(cmd.value, dummy)) {
            return ErrorResult{
                std::format(
                    "Filter value '{}' cannot be parsed as {} for column '{}'",
                    cmd.value, column_type_name(column_type), cmd.column
                )
            };
        }
    }

    FilterPredicate predicate{*column_index, column_type, cmd.op, cmd.value, datetime_pattern};

    current_view_ = current_view_.filter(
        [this, &predicate](RecordIndex row_index) {
            return predicate(make_row_from_file(row_index));
        }
    );

    if (predicate.skipped > 0) {
        return FilterResult{
            current_view_.size(),
            predicate.skipped,
            std::format(
                "{} row(s) skipped: field could not be parsed as {}",
                predicate.skipped, column_type_name(column_type)
            )
        };
    }

    return FilterResult{current_view_.size(), 0, ""};
}

CommandResult Session::execute_impl(const ResetViewCommand&) {
    current_view_ = View::identity(row_count());
    return SuccessResult{};
}

} // namespace hawk
