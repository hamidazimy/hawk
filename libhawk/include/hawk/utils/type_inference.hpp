#ifndef HAWK_TYPE_INFERER_HPP
#define HAWK_TYPE_INFERER_HPP

#include <cstddef>
#include <optional>
#include <string>
#include <string_view>

namespace hawk { class RecordSource; }
namespace hawk { class RecordParser; }
namespace hawk { struct SessionConfig; }
namespace hawk { class Schema; }
namespace hawk { enum class ColumnType; }

namespace hawk {

class TypeInferrer {
public:
    struct Options {
        std::size_t max_sample_rows      = 0;   // 0 = full scan
        std::size_t datetime_sample_rows = 100; // rows used for datetime pattern detection
    };

    TypeInferrer() : options_({}) {}
    explicit TypeInferrer(Options options) : options_(options) {}

    Schema infer(
        const RecordSource& source,
        const RecordParser& parser,
        const SessionConfig& config
    ) const;

private:
    struct ColumnState {
        bool could_be_integer  = true;
        bool could_be_float    = true;
        bool could_be_datetime = true;
        bool nullable          = false;
        bool saw_value         = false;  // any non-empty field observed
        std::optional<std::string> datetime_pattern;
        bool (*datetime_pre_screen)(std::string_view) = nullptr;
    };

    static ColumnType resolve_type(const ColumnState& state);

private:
    Options options_;
};

} // namespace hawk

#endif // HAWK_TYPE_INFERRER_HPP
