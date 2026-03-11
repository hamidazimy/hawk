#ifndef HAWK_SESSION_HPP
#define HAWK_SESSION_HPP

#include <hawk/core/log_file.hpp>
#include <hawk/core/schema.hpp>
#include <hawk/core/row.hpp>
#include <hawk/core/view.hpp>
#include <hawk/core/commands.hpp>
#include <hawk/core/results.hpp>

#include <hawk/utils/format_inference.hpp>

#include <memory>
#include <string>
#include <variant>

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

    const LogFile& source() const noexcept { return *source_; }
    const SessionConfig& config() const noexcept { return config_; }
    const Schema& schema() const noexcept { return schema_; }
    const View& view() const noexcept { return current_view_; }

    std::size_t visible_row_count() const noexcept;
    Row get_row(std::size_t visible_row_index) const;
    Row get_physical_row(std::size_t physical_row_index) const;

    CommandResult execute(const LibCommand& command);

private:
    Session(
        std::unique_ptr<LogFile> source,
        const SessionConfig& config
    );

    CommandResult execute_impl(const HeadCommand&);
    CommandResult execute_impl(const FilterCommand&);
    // Add new overloads here when commands are introduced.

    bool evaluate(const std::string& lhs, FilterOp op, const std::string& rhs) const;

    // ----- Internal Helpers -----
    View make_base_view() const;

private:
    std::unique_ptr<LogFile> source_;
    const SessionConfig config_;
    Schema schema_;
    View current_view_;
};

} // namespace hawk

#endif // HAWK_SESSION_HPP
