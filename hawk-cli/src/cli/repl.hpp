#ifndef HAWK_CLI_REPL_HPP
#define HAWK_CLI_REPL_HPP

#include <cli/commands.hpp>
#include <cli/renderers.hpp>

#include <hawk/hawk.hpp>

#include <replxx.hxx>

#include <cstddef>
#include <iostream>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

namespace hawk::cli {

struct ExitRequested {};

class REPL {
public:
    explicit REPL(std::unique_ptr<Session> session);

    // Start interactive loop
    void run();

    // Accessors for testing
    Session& session() { return *session_; }
    const Session& session() const { return *session_; }

private:
    static bool is_excluded_from_history(std::string_view);

    std::string prompt() const;

    renderers::RenderContext make_ctx() const {
        return {session_->schema(), terminal_width_, std::cout, std::cerr};
    }

    void dispatch(const hawk::LibCommand& cmd, const renderers::RenderOptions& options = {});

    void execute(const CliCommand& cmd);

    // Command implementations

    void execute_impl(const CliConfig&);
    void execute_impl(const CliColumns&);
    void execute_impl(const CliSetName&);
    void execute_impl(const CliSetType&);
    void execute_impl(const CliSelect&);
    void execute_impl(const CliSelectAdd&);
    void execute_impl(const CliDeselect&);
    void execute_impl(const CliCount&);
    void execute_impl(const CliPeek&);
    void execute_impl(const CliHead&);
    void execute_impl(const CliTail&);
    void execute_impl(const CliFilter&);
    void execute_impl(const CliFilterExp&);
    void execute_impl(const CliFilterExc&);
    void execute_impl(const CliSlice&);
    void execute_impl(const CliSort&);
    void execute_impl(const CliDistinct&);
    void execute_impl(const CliReset&);
    void execute_impl(const CliExport&);
    void execute_impl(const CliHistory&);
    void execute_impl(const CliHelp&);
    void execute_impl(const CliExit&) { throw ExitRequested{}; };

private:
    std::unique_ptr<Session> session_;
    std::vector<std::string> input_history_;
    replxx::Replxx editor_;
    std::size_t terminal_width_ = 80;
};

} // namespace hawk::cli

#endif // HAWK_CLI_REPL_HPP
