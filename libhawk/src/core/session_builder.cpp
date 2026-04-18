#include <hawk/core/session_builder.hpp>

#include <hawk/core/session.hpp>
#include <hawk/core/session_config.hpp>
#include <hawk/core/record_source.hpp>
#include <hawk/core/record_parser.hpp>
#include <hawk/core/schema.hpp>
#include <hawk/utils/type_inference.hpp>

#include <stdexcept>

namespace hawk {

SessionBuilder SessionBuilder::open(const std::string& path) {
    return SessionBuilder(std::make_unique<CSVRecordSource>(path));
}

std::unique_ptr<Session> SessionBuilder::build(const SessionConfig& config) {
    if (!source_)
        throw std::logic_error(
            "SessionBuilder already consumed — cannot call build() twice");

    auto parser = std::make_unique<CSVRecordParser>(config.delimiter);

    TypeInferrer inferrer;
    Schema schema = inferrer.infer(*source_, *parser, config);

    return std::unique_ptr<Session>(
        new Session(config, std::move(source_), std::move(parser), std::move(schema))
    );
}

} // namespace hawk
