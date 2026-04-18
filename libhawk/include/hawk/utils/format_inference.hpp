#ifndef HAWK_FORMAT_INFERER_HPP
#define HAWK_FORMAT_INFERER_HPP

#include <hawk/core/types.hpp>

#include <cstddef>
#include <string>
#include <string_view>
#include <vector>

namespace hawk { class RecordSource; }

namespace hawk::inference {

namespace detectors {

bool detect_delimiter(
    const std::vector<std::string_view>& samples,
    char& out_delimiter
);

bool detect_quotes(
    const std::vector<std::string_view>& samples,
    char& out_quote_char
);

bool detect_column_count(
    const std::vector<std::string_view>& samples,
    char delimiter,
    ColumnCount& out_column_count
);

bool detect_header(
    const std::vector<std::string_view>& samples,
    char delimiter,
    bool& out_has_header
);

} // namespace detectors

struct FormatInferenceResult {
    // Basic file characteristics
    std::size_t sampled_rows = 0;
    std::size_t total_rows_estimate = 0;

    // Structural guesses
    char delimiter = ',';
    std::size_t column_count = 0;

    // CSV-like features
    bool has_header = false;
    bool uses_quotes = false;
    char quote_char = '"';

    // Diagnostic / transparency
    std::vector<std::string> notes; // human-readable explanations
};

class FormatInferer {
public:
    struct Options {
        std::size_t max_sample_lines = 100;
    };

    explicit FormatInferer()
        : options_() {}

    explicit FormatInferer(Options options)
        : options_(options) {}

    FormatInferenceResult infer(const RecordSource& source) const;

private:
    Options options_;
};

} // namespace hawk::inference

#endif // HAWK_FORMAT_INFERER_HPP
