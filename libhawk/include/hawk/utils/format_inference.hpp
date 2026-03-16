#ifndef HAWK_FORMAT_INFERER_HPP
#define HAWK_FORMAT_INFERER_HPP

#include <cstddef>
#include <string>
#include <vector>

namespace hawk { class RecordSource; }

namespace hawk::inference {

namespace detectors {

char detect_delimiter(const std::vector<std::string>& sample_lines);
bool looks_like_header(const std::string& line);
std::size_t detect_column_count(const std::vector<std::string>& sample_lines, char delimiter);

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
    std::vector<std::string> notes;              // human-readable explanations
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

    FormatInferenceResult infer_from_file(const std::string& path) const;

private:
    Options options_;
};

} // namespace hawk::inference

#endif // HAWK_FORMAT_INFERER_HPP
