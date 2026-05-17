#ifndef HAWK_CLI_REPL_HPP
#define HAWK_CLI_REPL_HPP

#include <cli/cli_commands.hpp>

#include <hawk/hawk.hpp>

#include <replxx.hxx>

#include <cstddef>
#include <memory>
#include <string>

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
    std::string prompt() const;

    bool execute(const CliCommand& cmd);

    bool execute_impl(const CliCommandExport&);
    bool execute_impl(const CliCommandHelp&);
    bool execute_impl(const CliCommandExit&);

private:
    std::unique_ptr<Session> session_;
    replxx::Replxx editor_;
    std::size_t terminal_width_ = 80;
};

} // namespace hawk::cli

#endif // HAWK_CLI_REPL_HPP
