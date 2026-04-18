#ifndef HAWK_SESSION_BUILDER_HPP
#define HAWK_SESSION_BUILDER_HPP

#include <hawk/core/record_source.hpp>

#include <memory>
#include <string>
#include <utility>

namespace hawk { struct SessionConfig; }
namespace hawk { class Session; }

namespace hawk {

class SessionBuilder {
public:
    static SessionBuilder open(const std::string& path);

    std::unique_ptr<Session> build(const SessionConfig& config);

    const RecordSource& record_source() const noexcept { return *source_; }

private:
    explicit SessionBuilder(std::unique_ptr<RecordSource> source)
        : source_(std::move(source)) {}

    std::unique_ptr<RecordSource> source_;
};

} // namespace hawk

#endif // HAWK_SESSION_BUILDER_HPP
