#include <hawk/core/record_parser.hpp>

#include <hawk/core/types.hpp>

#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace hawk {

std::vector<std::string> CSVRecordParser::parse_record(std::string_view line) const {
    std::vector<std::string> fields;
    std::string current;

    bool in_quotes = false;

    for (FileOffset i = 0; i < line.size(); ++i) {
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
