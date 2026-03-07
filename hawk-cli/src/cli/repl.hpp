#ifndef HAWK_CLI_REPL_HPP
#define HAWK_CLI_REPL_HPP

#include <cli/cli_commands.hpp>
#include <cli/command_info.hpp>

#include <hawk/hawk.hpp>

#include <memory>
#include <string>
#include <variant>
#include <vector>

namespace hawk::cli {

class REPL {
public:
    explicit REPL(std::unique_ptr<Session> session);

    // Start interactive loop
    void run();

    // Accessors for testing
    Session& session() { return *session_; }
    const Session& session() const { return *session_; }

private:
    bool execute(const CliCommand& cmd);

    bool execute_impl(const CliCommandHelp&);
    bool execute_impl(const CliCommandExit&);

private:
    std::unique_ptr<Session> session_;
};

} // namespace hawk::cli

#endif // HAWK_CLI_REPL_HPP
