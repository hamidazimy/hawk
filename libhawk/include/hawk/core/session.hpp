#ifndef HAWK_SESSION_HPP
#define HAWK_SESSION_HPP

#include <hawk/core/types.hpp>
#include <hawk/core/record_source.hpp>
#include <hawk/core/record_parser.hpp>
#include <hawk/core/schema.hpp>
#include <hawk/core/view.hpp>
#include <hawk/core/commands.hpp>
#include <hawk/core/results.hpp>

#include <memory>
#include <optional>
#include <string>
#include <string_view>

namespace hawk { class Row; }
namespace hawk { enum class FilterOp; }
namespace hawk { namespace inference { struct FormatInferenceResult; } }

namespace hawk {

struct SessionConfig {
    std::optional<char> delimiter;
    std::optional<bool> has_header;

    bool empty() const noexcept;
    bool complete() const noexcept;

    void resolve_with_inference(const inference::FormatInferenceResult& inference_result);
};

class Session {
public:
    static std::unique_ptr<Session> create_from_file(
        const std::string& path,
        const SessionConfig& config
    );

    const SessionConfig& config() const noexcept { return config_; }
    const RecordSource& source() const noexcept { return *source_; }
    const Schema& schema() const noexcept { return schema_; }
    const View& view() const noexcept { return current_view_; }

    RecordCount visible_row_count() const noexcept;
    Row get_view_record(RecordIndex view_index) const;
    Row get_file_record(RecordIndex file_index) const;

    CommandResult execute(const LibCommand& command);

private:
    Session(
        const SessionConfig& config,
        std::unique_ptr<RecordSource> source
    );

    CommandResult execute_impl(const ColumnsCommand&);
    CommandResult execute_impl(const CountCommand&);
    CommandResult execute_impl(const PeekCommand&);
    CommandResult execute_impl(const HeadCommand&);
    CommandResult execute_impl(const TailCommand&);
    CommandResult execute_impl(const FilterCommand&);
    CommandResult execute_impl(const ExportCommand&);
    // Add new overloads here when commands are introduced.

    bool evaluate(const std::string_view& lhs, FilterOp op, const std::string& rhs) const;

private:
    const SessionConfig config_;
    std::unique_ptr<RecordSource> source_;
    std::unique_ptr<RecordParser> parser_;
    Schema schema_;
    View current_view_;
};

} // namespace hawk

#endif // HAWK_SESSION_HPP
