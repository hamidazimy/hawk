#ifndef HAWK_ROW_HPP
#define HAWK_ROW_HPP

#include <hawk/core/types.hpp>

#include <format>
#include <optional>
#include <string_view>
#include <vector>

namespace hawk { class RecordParser; }
namespace hawk { class Projection; }

namespace hawk {

class Row {
public:
    Row(
        RecordIndex index,
        std::string_view record,
        const RecordParser* parser
    ) noexcept
        : index_(index)
        , record_(record)
        , parser_(parser)
    {}

    ~Row() = default;

    RecordIndex index() const noexcept { return index_; }

    std::string_view record() const noexcept { return record_; }

    ColumnCount length() const noexcept;

    std::string_view operator[](ColumnIndex idx) const noexcept {
        return fields_.value()[idx];
    }

    std::string_view get(ColumnIndex idx) const;

    std::string_view get_projected(const Projection* projection, ColumnIndex idx) const;

private:
    RecordIndex index_;
    std::string_view record_;
    mutable std::optional<std::vector<std::string_view>> fields_;
    const RecordParser* parser_; // Non-owning pointer to the parser used to parse the record into fields.
                                 // Row must not outlive the Session that owns the parser.
};

} // namespace hawk

#endif // HAWK_ROW_HPP
