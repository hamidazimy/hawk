#include <hawk/core/session.hpp>

#include <hawk/core/results.hpp>
#include <hawk/utils/format_inference.hpp>
#include <hawk/utils/utils.hpp>

#include <stdexcept>
#include <variant>
#include <numeric>

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
    auto source = std::make_unique<LogFile>(filepath);
    auto session = std::unique_ptr<Session>(
        new Session(std::move(source), config)
    );
    return session;
}

Session::Session(
    std::unique_ptr<LogFile> source,
    const SessionConfig& config
)
    : source_(std::move(source))
    , config_(config)
{
    if (!config_.complete()) {
        throw std::logic_error(
            "SessionConfig must be complete before creating a Session"
        );
    }

    auto first_line = source_->sample_lines(1);

    size_t column_count = inference::detectors::detect_column_count(
        first_line,
        config_.delimiter.value()
    );

    std::vector<std::string> column_names;

    if (config_.has_header.value()) {
        column_names = utils::split(first_line.front(), config_.delimiter.value());
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
    std::size_t header_size = config_.has_header.value() ? 1 : 0;
    std::vector<std::size_t> indices(source_->row_count() - header_size);
    std::iota(indices.begin(), indices.end(), header_size);
    return View(std::move(indices));
}

std::size_t Session::visible_row_count() const noexcept {
    return current_view_.size();
}

Row Session::get_row(std::size_t visible_row_index) const {
    auto physical_row_index = current_view_.map_to_physical_index(visible_row_index);
    return get_physical_row(physical_row_index);
}

Row Session::get_physical_row(std::size_t physical_row_index) const {
    auto line = std::string(source_->get_line(physical_row_index));
    auto fields = utils::split(line, config_.delimiter.value());
    return Row(physical_row_index, fields);
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
    std::size_t max_rows = this->visible_row_count();
    std::size_t count = std::min(cmd.count, max_rows);

    std::vector<Row> rows;
    rows.reserve(count);

    for (std::size_t i = 0; i < count; ++i) {
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
        [&](std::size_t row_index) {
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
            case FilterOp::EQ:  return lhs_num == rhs_num;
            case FilterOp::NEQ: return lhs_num != rhs_num;
            case FilterOp::GT:  return lhs_num >  rhs_num;
            case FilterOp::LT:  return lhs_num <  rhs_num;
            case FilterOp::GTE: return lhs_num >= rhs_num;
            case FilterOp::LTE: return lhs_num <= rhs_num;
        }
    }

    switch (op) {
        case FilterOp::EQ:  return lhs == rhs;
        case FilterOp::NEQ: return lhs != rhs;
        case FilterOp::GT:  return lhs >  rhs;
        case FilterOp::LT:  return lhs <  rhs;
        case FilterOp::GTE: return lhs >= rhs;
        case FilterOp::LTE: return lhs <= rhs;
    }

    return false;
}

} // namespace hawk
