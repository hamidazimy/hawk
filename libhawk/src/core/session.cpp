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
#include <functional>
#include <numeric>
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
        column_names = parsed_first_record;
    } else {
        // Generate default column names
        for (size_t i = 0; i < column_count; ++i) {
            column_names.push_back("$col" + std::to_string(i + 1));
        }
    }

    schema_ = Schema(column_count);
    schema_.set_column_names(column_names);

    current_view_ = make_base_view();
}

View Session::make_base_view() const {
    RecordCount header_size = config_.has_header.value() ? 1 : 0;
    std::vector<RecordIndex> indices(source_->record_count() - header_size);
    std::iota(indices.begin(), indices.end(), header_size);
    return View(std::move(indices));
}

RecordCount Session::visible_row_count() const noexcept {
    return current_view_.size();
}

Row Session::get_row(RecordIndex visible_index) const {
    auto physical_index = current_view_.map_to_physical_index(visible_index);
    return get_physical_row(physical_index);
}

Row Session::get_physical_row(RecordIndex physical_index) const {
    auto record = std::string(source_->get_record(physical_index));
    auto fields = parser_->parse_record(record);
    return Row(physical_index, fields);
}

CommandResult Session::execute(const LibCommand& command) {
    return std::visit(
        [this](const auto& cmd) -> CommandResult {
            return execute_impl(cmd);
        },
        command
    );
}

CommandResult Session::execute_impl(const HeadCommand& cmd) {
    RecordCount max_visible_records = this->visible_row_count();
    RecordCount count = std::min(cmd.max_records, max_visible_records);

    std::vector<Row> rows;
    rows.reserve(count);

    for (RecordIndex i = 0; i < count; ++i) {
        rows.push_back(this->get_row(i));
    }

    return RowsResult{std::move(rows)};
}

CommandResult Session::execute_impl(const FilterCommand& cmd) {
    auto column_index = schema_.find_column(cmd.column);

    if (!column_index) {
        return ErrorResult{"Unknown column: " + cmd.column};
    }

    current_view_ = current_view_.filter(
        [&](RecordIndex row_index) {
            Row row = get_physical_row(row_index);
            return evaluate(std::string{row[*column_index]}, cmd.op, cmd.value);
        }
    );

    return CountResult{current_view_.size()};
}

bool Session::evaluate(const std::string& lhs,
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

    bool lhs_is_num = try_parse_double(lhs, lhs_num);
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
