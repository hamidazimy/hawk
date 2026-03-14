#ifndef HAWK_PARSERS_HPP
#define HAWK_PARSERS_HPP

#include <string>
#include <string_view>
#include <vector>

namespace hawk {

class Parser {
public:
    virtual ~Parser() = default;

    virtual std::vector<std::string> parse(std::string_view record) const = 0;
};


class CSVParser : public Parser {
public:
    explicit CSVParser(char delimiter = ',');

    std::vector<std::string> parse(std::string_view record) const override;

private:
    char delimiter_;
};

} // namespace hawk

#endif // HAWK_PARSERS_HPP
