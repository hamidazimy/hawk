#ifndef HAWK_CLI_CONFIG_BUILDER_HPP
#define HAWK_CLI_CONFIG_BUILDER_HPP

#include <args.hpp>

#include <hawk/hawk.hpp>

namespace hawk::cli {

SessionConfig build_config(const Args& args, const RecordSource& source);

void confirm_schema(Session& session, const Args& args);

} // namespace hawk::cli

#endif // HAWK_CLI_CONFIG_BUILDER_HPP
