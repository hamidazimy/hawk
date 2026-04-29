#ifndef HAWK_SESSION_HPP
#define HAWK_SESSION_HPP

#include <hawk/core/types.hpp>
#include <hawk/core/session_config.hpp>
#include <hawk/core/record_source.hpp>
#include <hawk/core/record_parser.hpp>
#include <hawk/core/schema.hpp>
#include <hawk/core/view.hpp>
#include <hawk/core/projection.hpp>
#include <hawk/core/commands.hpp>

#include <memory>

namespace hawk { class Row; }
namespace hawk { struct CommandResult; }

namespace hawk {

class Session {
    friend class SessionBuilder;

public:
    const SessionConfig& config() const noexcept { return config_; }
    const RecordSource& source() const noexcept { return *source_; }
    const Schema& schema() const noexcept { return schema_; }
    const View& view() const noexcept { return current_view_; }

    CommandResult execute(const LibCommand& command);

private:
    Session(
        const SessionConfig& config,
        std::unique_ptr<RecordSource> source,
        std::unique_ptr<RecordParser> parser,
        Schema schema
    );

    RecordIndex to_source_index(RecordIndex file_index) const noexcept {
        return file_index + static_cast<RecordIndex>(config_.has_header);
    }

    RecordCount row_count() const noexcept {
        return source_->record_count() - static_cast<RecordCount>(config_.has_header);
    }

    RecordCount visible_row_count() const noexcept { return current_view_.size(); }

    Row make_row_from_view(RecordIndex view_index) const;
    Row make_row_from_file(RecordIndex file_index) const;

    CommandResult execute_impl(const ColumnsCommand&);
    CommandResult execute_impl(const CountCommand&);
    CommandResult execute_impl(const SetColumnNameCommand&);
    CommandResult execute_impl(const SetColumnTypeCommand&);
    CommandResult execute_impl(const SelectCommand&);
    CommandResult execute_impl(const PeekCommand&);
    CommandResult execute_impl(const HeadCommand&);
    CommandResult execute_impl(const TailCommand&);
    CommandResult execute_impl(const FilterCommand&);
    CommandResult execute_impl(const ResetViewCommand&);
    CommandResult execute_impl(const ExportCommand&);

private:
    const SessionConfig config_;
    std::unique_ptr<RecordSource> source_;
    std::unique_ptr<RecordParser> parser_;
    Schema schema_;
    View current_view_;
    Projection current_projection_;
};

} // namespace hawk

#endif // HAWK_SESSION_HPP
