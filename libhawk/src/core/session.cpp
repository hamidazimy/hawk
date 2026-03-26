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

    current_view_ = View::identity(source_->record_count(), config_.has_header.value() ? 1 : 0);
}

RecordCount Session::visible_row_count() const noexcept {
    return current_view_.size();
}

Row Session::get_view_record(RecordIndex view_index) const {
    auto file_index = current_view_.at(view_index);
    return get_file_record(file_index);
}

Row Session::get_file_record(RecordIndex file_index) const {
    auto record = source_->get_record(file_index);
    auto fields = parser_->parse_record(record);
    return Row(file_index, record, fields);
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
        rows.push_back(this->get_view_record(i));
    }

    if (config_.has_header.value_or(false)) {
        return ExportResult{source_->get_record(0), std::move(rows)};
    }
    return ExportResult{{}, std::move(rows)};
}

CommandResult Session::execute_impl(const ColumnsCommand&) {
    return ColumnsResult{schema_.column_names()};
}

CommandResult Session::execute_impl(const CountCommand&) {
    return CountResult{current_view_.size()};
}

CommandResult Session::execute_impl(const HeadCommand& cmd) {
    RecordCount max_visible_records = this->visible_row_count();
    RecordCount count = std::min(cmd.max_records, max_visible_records);

    std::vector<Row> rows;
    rows.reserve(count);

    for (RecordIndex i = 0; i < count; ++i) {
        rows.push_back(this->get_view_record(i));
    }

    return RowsResult{std::move(rows)};
}

CommandResult Session::execute_impl(const TailCommand& cmd) {
    RecordCount max_visible_records = this->visible_row_count();
    RecordCount count = std::min(cmd.max_records, max_visible_records);

    std::vector<Row> rows;
    rows.reserve(count);

    for (RecordIndex i = max_visible_records - count; i < max_visible_records; ++i) {
        rows.push_back(this->get_view_record(i));
    }

    return RowsResult{std::move(rows)};
}

CommandResult Session::execute_impl(const FilterCommand& cmd) {
    auto column_index = schema_.find_column(cmd.column);

    if (!column_index) {
        return ErrorResult{"Unknown column: " + cmd.column};
    }

    const auto col_idx = *column_index;

    current_view_ = current_view_.filter(
        [&](RecordIndex row_index) {
            const auto& record = source_->get_record(row_index);
            auto fields = parser_->parse_record(record);
            return evaluate(fields[col_idx], cmd.op, cmd.value);
        }
    );

    return CountResult{current_view_.size()};
}

bool Session::evaluate(const std::string_view& lhs,
                       FilterOp op,
                       const std::string& rhs) const
{
    auto try_parse_double = [](const std::string& s, double& out) -> bool {
        char* end = nullptr;
        out = std::strtod(s.c_str(), &end);
        return end != s.c_str() && *end == '\0';
    };

    double lhs_num;
    double rhs_num;

    bool lhs_is_num = try_parse_double(std::string(lhs), lhs_num);
    bool rhs_is_num = try_parse_double(rhs, rhs_num);

    if (lhs_is_num && rhs_is_num) {
        switch (op) {
            case FilterOp::EQ: return lhs_num == rhs_num;
            case FilterOp::NE: return lhs_num != rhs_num;
            case FilterOp::GT: return lhs_num >  rhs_num;
            case FilterOp::LT: return lhs_num <  rhs_num;
            case FilterOp::GE: return lhs_num >= rhs_num;
            case FilterOp::LE: return lhs_num <= rhs_num;
        }
    }

    switch (op) {
        case FilterOp::EQ: return lhs == rhs;
        case FilterOp::NE: return lhs != rhs;
        case FilterOp::GT: return lhs >  rhs;
        case FilterOp::LT: return lhs <  rhs;
        case FilterOp::GE: return lhs >= rhs;
        case FilterOp::LE: return lhs <= rhs;
    }

    return false;
}

} // namespace hawk
