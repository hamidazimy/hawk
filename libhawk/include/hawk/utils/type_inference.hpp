#ifndef HAWK_TYPE_INFERER_HPP
#define HAWK_TYPE_INFERER_HPP

#include <cstddef>
#include <optional>
#include <string_view>

namespace hawk { class RecordSource ; }
namespace hawk { class RecordParser ; }
namespace hawk { class Schema; }
namespace hawk { enum class ColumnType; }
namespace hawk { enum class DateTimeFormat; }
namespace hawk { struct SessionConfig; }

namespace hawk {

class TypeInferrer {
public:
    struct Options {
        std::size_t max_sample_rows = 0; // 0 = full scan
    };

    TypeInferrer()
        : options_({}) {}

    explicit TypeInferrer(Options options)
        : options_(options) {}

    Schema infer(
        const RecordSource& source,
        const RecordParser& parser,
        const SessionConfig& config
    ) const;

private:
    // Per-column state accumulated during scan
    struct ColumnState {
        bool could_be_integer  = true;
        bool could_be_float    = true;
        bool could_be_datetime = true;
        bool nullable          = false;
        std::optional<DateTimeFormat> datetime_format; // candidate format
    };

    // Type resolution — called after scan completes
    static ColumnType resolve_type(const ColumnState& state);

    // Field-level parsers
    static bool try_integer(std::string_view field);
    static bool try_float(std::string_view field);
    static std::optional<DateTimeFormat> try_datetime(std::string_view field);

private:
    Options options_;
};

} // namespace hawk
#endif // HAWK_TYPE_INFERRER_HPP
