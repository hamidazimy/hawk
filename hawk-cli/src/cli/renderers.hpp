#ifndef HAWK_CLI_RENDERERS_HPP
#define HAWK_CLI_RENDERERS_HPP

#include <hawk/hawk.hpp>

#include <iostream>

namespace hawk::cli {
namespace renderers {

void render(const hawk::CommandResult& result, std::ostream& sout = std::cout);

void render_rows(const hawk::RowsResult& rows, std::ostream& sout = std::cout);

void render_count(const hawk::CountResult& count, std::ostream& sout = std::cout);

void render_success(const hawk::SuccessResult& success, std::ostream& sout = std::cout);

void render_error(const hawk::ErrorResult& error, std::ostream& sout = std::cerr);

} // namespace renderers
} // namespace hawk::cli

#endif // HAWK_CLI_RENDERERS_HPP
