#ifndef HAWK_CLI_CONFIG_BUILDER_HPP
#define HAWK_CLI_CONFIG_BUILDER_HPP

#include <args.hpp>

#include <hawk/hawk.hpp>

#include <string>

namespace hawk::cli {

SessionConfig config_from_args(const Args& args);

void display_config(const SessionConfig& config);

void modify_config_interactively(SessionConfig& current);

SessionConfig complete_partial_config(
    SessionConfig partial,
    const std::string& filepath,
    bool no_confirm
);

SessionConfig build_config(const Args& args);

} // namespace hawk::cli

#endif // HAWK_CLI_CONFIG_BUILDER_HPP
