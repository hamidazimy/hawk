#ifndef HAWK_RECORD_PARSER_HPP
#define HAWK_RECORD_PARSER_HPP

#include <string_view>
#include <vector>

namespace hawk {

class RecordParser {
public:
    virtual ~RecordParser() = default;

    virtual std::vector<std::string_view> parse_record(std::string_view record) const = 0;
};

class CSVRecordParser : public RecordParser {
public:
    explicit CSVRecordParser(char delimiter = ',')
        : delimiter_(delimiter) {}

    std::vector<std::string_view> parse_record(std::string_view record) const override;

private:
    char delimiter_;
};

} // namespace hawk

#endif // HAWK_RECORD_PARSER_HPP
