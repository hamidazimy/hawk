#include <hawk/core/record_parser.hpp>

#include <hawk/core/types.hpp>

#include <cstddef>
#include <string_view>
#include <vector>

namespace hawk {

std::vector<std::string_view> CSVRecordParser::parse_record(std::string_view line) const {
    std::vector<std::string_view> fields;
    fields.reserve(16); // optional heuristic

    bool in_quotes = false;
    std::size_t field_start = 0;

    for (FileOffset i = 0; i < line.size(); ++i) {
        char c = line[i];

        if (c == '"') {
            if (in_quotes && i + 1 < line.size() && line[i + 1] == '"') {
                // Escaped quote: skip next quote
                ++i;
            } else {
                in_quotes = !in_quotes;
            }
        }
        else if (c == delimiter_ && !in_quotes) {
            fields.emplace_back(line.data() + field_start, i - field_start);
            field_start = i + 1;
        }
    }

    // Last field
    fields.emplace_back(line.data() + field_start, line.size() - field_start);

    return fields;
}

} // namespace hawk
