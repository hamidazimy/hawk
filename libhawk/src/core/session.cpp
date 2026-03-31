#include <hawk/core/session.hpp>

#include <hawk/core/types.hpp>
#include <hawk/core/record_source.hpp>
#include <hawk/core/record_parser.hpp>
#include <hawk/core/schema.hpp>
#include <hawk/core/row.hpp>
#include <hawk/core/view.hpp>
#include <hawk/core/filter.hpp>
#include <hawk/core/commands.hpp>
#include <hawk/core/results.hpp>
#include <hawk/utils/format_inference.hpp>

#include <algorithm>
#include <cstdlib>
#include <stdexcept>
#include <string_view>
#include <utility>
#include <variant>
#include <vector>

namespace hawk {

//  ---- SessionConfig ----

bool SessionConfig::empty() const noexcept {
    return !delimiter.has_value()
        && !has_header.has_value();
}

bool SessionConfig::complete() const noexcept {
    return delimiter.has_value()
        && has_header.has_value();
}

void SessionConfig::resolve_with_inference(const inference::FormatInferenceResult& inference_result) {
    if (!delimiter.has_value() && inference_result.delimiter != '\0') {
        delimiter = inference_result.delimiter;
    }

    if (!has_header.has_value()) {
        has_header = inference_result.has_header;
    }
}

// ---- Session ----

std::unique_ptr<Session> Session::create_from_file(const std::string& filepath, const SessionConfig& config) {
    auto source = std::make_unique<CSVRecordSource>(filepath);
    auto session = std::unique_ptr<Session>(
        new Session(config, std::move(source))
    );
    return session;
}

Session::Session(
    const SessionConfig& config,
    std::unique_ptr<RecordSource> source
)
    : config_(config)
    , source_(std::move(source))
{
    if (!config_.complete()) {
        throw std::logic_error(
            "SessionConfig must be complete before creating a Session"
        );
    }

    parser_ = std::make_unique<CSVRecordParser>(config_.delimiter.value());

    auto parsed_first_record = parser_->parse_record(source_->get_record(0));
    std::size_t column_count = parsed_first_record.size();

    std::vector<std::string> column_names;

    if (config_.has_header.value()) {
        for (std::size_t i = 0; i < column_count; ++i) {
            column_names.push_back(std::string(parsed_first_record.at(i)));
        }
    } else {
        // Generate default column names??
        for (std::size_t i = 0; i < column_count; ++i) {
            // column_names.push_back("$col" + std::to_string(i + 1));
            column_names.push_back("");
        }
    }

    schema_ = Schema(column_count);
    schema_.set_column_names(column_names);

    current_view_ = View::identity(row_count());
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
    if (config_.has_header.value()) {
        header = source_->get_record(0);
    }
    return ExportResult{header, std::move(rows)};
}

CommandResult Session::execute_impl(const ColumnsCommand&) {
    return ColumnsResult{schema_.column_names()};
}

CommandResult Session::execute_impl(const CountCommand&) {
    return CountResult{current_view_.size()};
}

CommandResult Session::execute_impl(const PeekCommand& cmd) {
    if (cmd.index >= row_count()) {
        return ErrorResult{"Index out of range"};
    }

    return RowsResult{{make_row_from_file(cmd.index)}};
}

CommandResult Session::execute_impl(const HeadCommand& cmd) {
    RecordCount max_visible_records = visible_row_count();
    RecordCount count = std::min(cmd.max_records, max_visible_records);

    std::vector<Row> rows;
    rows.reserve(count);

    for (RecordIndex i = 0; i < count; ++i) {
        rows.emplace_back(make_row_from_view(i));
    }

    return RowsResult{std::move(rows)};
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

    return RowsResult{std::move(rows)};
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

} // namespace hawk
