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
#include <type_traits>
#include <unordered_set>
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

// --- Internal helpers ---

namespace {

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

// TODO(C++23): replace with std::expected<FilterPredicateVariant, std::string>
struct PrepareFilterResult {
    std::optional<FilterPredicateVariant> pred;  // set on success
    std::optional<std::string>            error; // set on failure

    static PrepareFilterResult ok(FilterPredicateVariant p) {
        return {std::move(p), std::nullopt};
    }
    static PrepareFilterResult err(std::string e) {
        return {std::nullopt, std::move(e)};
    }
};

PrepareFilterResult prepare_filter(
    const Schema&    schema,
    const FilterArgs& args)
{
    if (args.row_search) {
        if (args.op != FilterOp::HAS) {
            return PrepareFilterResult::err("$row only supports the 'has' operator");
        }
        return PrepareFilterResult::ok(RowSearchPredicate{args.value});
    }

    auto column_index = schema.find_column(args.column);
    if (!column_index) {
        return PrepareFilterResult::err(std::format("Unknown column: {}", args.column));
    }

    const ColumnType column_type = schema.column_type(*column_index);

    if (args.op == FilterOp::HAS && column_type != ColumnType::String) {
        return PrepareFilterResult::err(std::format(
            "Operator 'has' is only valid for string columns, column '{}' is {}",
            args.column, column_type_name(column_type)
        ));
    }

    std::optional<std::string> datetime_pattern;
    if (column_type == ColumnType::DateTime) {
        const auto& col_schema = schema.column(*column_index);
        if (!col_schema.datetime_pattern.has_value()) {
            return PrepareFilterResult::err(std::format(
                "Column '{}' has no datetime pattern — cannot filter", args.column
            ));
        }
        datetime_pattern = col_schema.datetime_pattern;
        if (!utils::parse_datetime(args.value, *datetime_pattern)) {
            return PrepareFilterResult::err(std::format(
                "Filter value '{}' cannot be parsed as datetime pattern '{}' for column '{}'",
                args.value, *datetime_pattern, args.column
            ));
        }
    }

    if (column_type == ColumnType::Integer) {
        std::int64_t dummy;
        if (!utils::parse_int(args.value, dummy)) {
            return PrepareFilterResult::err(std::format(
                "Filter value '{}' cannot be parsed as {} for column '{}'",
                args.value, column_type_name(column_type), args.column
            ));
        }
    }

    if (column_type == ColumnType::Float) {
        double dummy;
        if (!utils::parse_double(args.value, dummy)) {
            return PrepareFilterResult::err(std::format(
                "Filter value '{}' cannot be parsed as {} for column '{}'",
                args.value, column_type_name(column_type), args.column
            ));
        }
    }

    return PrepareFilterResult::ok(FilterPredicate{*column_index, column_type, args.op, args.value, datetime_pattern});
}

} // namespace

bool Session::apply_predicate(const FilterPredicateVariant& pred_variant, RecordIndex idx) {
    return std::visit([&](auto& pred) {
        if constexpr (std::is_same_v<std::decay_t<decltype(pred)>, RowSearchPredicate>) {
            return pred(raw_record_from_file(idx));
        } else {
            return pred(make_row_from_file(idx));
        }
    }, pred_variant);
}

// --- Command implementations ---

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
    auto filter_result = prepare_filter(schema_, cmd);
    if (filter_result.error) return CommandResult::err(*filter_result.error);
    auto& pred = *filter_result.pred;

    current_view_ = current_view_.filter(
        [this, &pred](RecordIndex idx) {
            return apply_predicate(pred, idx);
        }
    );

    auto result = CommandResult::ok(FilterResult{current_view_.size(), 0});
    if (auto* col_pred = std::get_if<FilterPredicate>(&pred)) {
        result.payload = FilterResult{current_view_.size(), col_pred->skipped};
        if (col_pred->skipped > 0) {
            result.warnings.push_back(std::format(
                "{} row(s) skipped: field could not be parsed as {}",
                col_pred->skipped, column_type_name(col_pred->column_type)
            ));
        }
    }
    return result;
}

CommandResult Session::execute_impl(const FilterExpandCommand& cmd) {
    auto filter_result = prepare_filter(schema_, cmd);
    if (filter_result.error) return CommandResult::err(*filter_result.error);
    auto& pred = *filter_result.pred;

    // Build O(1) membership set from current view
    std::unordered_set<RecordIndex> existing;
    existing.reserve(current_view_.size());
    current_view_.for_each([&](RecordIndex idx) { existing.insert(idx); });

    // Scan full file for matching rows not already in the view
    std::vector<RecordIndex> new_indices;
    for (RecordIndex idx = 0; idx < row_count(); ++idx) {
    if (!existing.contains(idx) && apply_predicate(pred, idx)) {
            new_indices.push_back(idx);
        }
    }

    if (!new_indices.empty()) {
        // Merge current view and new matches, then sort to file order.
        // TODO: When a 'sort' command is added, re-apply the active sort order
        // instead of restoring file order here.
        std::vector<RecordIndex> merged;
        merged.reserve(current_view_.size() + new_indices.size());
        current_view_.for_each([&](RecordIndex idx) { merged.push_back(idx); });
        merged.insert(merged.end(), new_indices.begin(), new_indices.end());
        std::sort(merged.begin(), merged.end());
        current_view_ = View(std::move(merged), row_count());
    }

    auto result = CommandResult::ok(FilterResult{current_view_.size(), 0});
    if (auto* col_pred = std::get_if<FilterPredicate>(&pred)) {
        result.payload = FilterResult{current_view_.size(), col_pred->skipped};
        if (col_pred->skipped > 0) {
            result.warnings.push_back(std::format(
                "{} row(s) skipped: field could not be parsed as {}",
                col_pred->skipped, column_type_name(col_pred->column_type)
            ));
        }
    }
    return result;
}

CommandResult Session::execute_impl(const FilterExcludeCommand& cmd) {
    auto filter_result = prepare_filter(schema_, cmd);
    if (filter_result.error) return CommandResult::err(*filter_result.error);
    auto& pred = *filter_result.pred;

    current_view_ = current_view_.filter(
        [this, &pred](RecordIndex idx) {
            return !apply_predicate(pred, idx);
        }
    );

    auto result = CommandResult::ok(FilterResult{current_view_.size(), 0});
    if (auto* col_pred = std::get_if<FilterPredicate>(&pred)) {
        result.payload = FilterResult{current_view_.size(), col_pred->skipped};
        if (col_pred->skipped > 0) {
            result.warnings.push_back(std::format(
                "{} row(s) skipped: field could not be parsed as {}",
                col_pred->skipped, column_type_name(col_pred->column_type)
            ));
        }
    }
    return result;
}

CommandResult Session::execute_impl(const ResetViewCommand&) {
    current_view_ = View::identity(row_count());
    return CommandResult::ok();
}

} // namespace hawk
