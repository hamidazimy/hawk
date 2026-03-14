#include <hawk/core/parsers.hpp>

#include <string>
#include <string_view>
#include <vector>

namespace hawk {

CSVParser::CSVParser(char delimiter)
    : delimiter_(delimiter) {}

std::vector<std::string> CSVParser::parse(std::string_view line) const {
    std::vector<std::string> fields;
    std::string current;

    bool in_quotes = false;

    for (std::size_t i = 0; i < line.size(); ++i) {
        char c = line[i];

        if (c == '"') {
            if (in_quotes && i + 1 < line.size() && line[i + 1] == '"') {
                current += '"';
                ++i;
            } else {
                in_quotes = !in_quotes;
            }
        }
        else if (c == delimiter_ && !in_quotes) {
            fields.push_back(std::move(current));
            current.clear();
        }
        else {
            current += c;
        }
    }

    fields.push_back(std::move(current));

    return fields;
}

} // namespace hawk
