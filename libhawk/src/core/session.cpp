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

#include <algorithm>
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
    std::vector<std::string> column_names;
    column_names.reserve(schema_.column_count());
    for (ColumnIndex i = 0; i < schema_.column_count(); ++i) {
        column_names.push_back(schema_.column(i).name);
    }
    return ColumnsResult{std::move(column_names)};
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
        return ErrorResult{"Unknown column: " + cmd.column};
    }

    FilterPredicate predicate{*column_index, cmd.op, cmd.value};

    current_view_ = current_view_.filter(
        [this, predicate](RecordIndex row_index) {
            return predicate(make_row_from_file(row_index));
        }
    );

    return CountResult{current_view_.size()};
}

CommandResult Session::execute_impl(const SelectCommand& cmd) {
    std::vector<ColumnIndex> columns;

    for (auto column_name : cmd.columns) {
        columns.push_back(*schema_.find_column(column_name));
    }

    current_projection_.select(columns);

    return SuccessResult();
}

} // namespace hawk
