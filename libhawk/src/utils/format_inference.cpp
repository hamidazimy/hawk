#include <hawk/utils/format_inference.hpp>

#include <hawk/utils/utils.hpp>
#include <hawk/core/record_source.hpp>

#include <algorithm>
#include <cctype>
#include <memory>
#include <numeric>

namespace hawk::inference {

namespace detectors {

char detect_delimiter(const std::vector<std::string>& sample_lines) {
    static const std::vector<char> candidates = {
        ',', '\t', '|', ';', ' '
    };

    if (sample_lines.empty()) return ',';

    char best = ',';
    double best_score = -1.0;

    for (char delim : candidates) {
        std::vector<size_t> counts;
        counts.reserve(sample_lines.size());

        for (const auto& line : sample_lines) {
            counts.push_back(
                std::count(line.begin(), line.end(), delim)
            );
        }

        double mean =
            std::accumulate(counts.begin(), counts.end(), 0.0) /
            counts.size();

        if (mean < 1.0) continue;

        double variance = 0.0;
        for (auto c : counts) {
            variance += (c - mean) * (c - mean);
        }
        variance /= counts.size();

        double score = mean / (1.0 + variance);

        if (score > best_score) {
            best_score = score;
            best = delim;
        }
    }

    return best;
}

bool looks_like_header(const std::string& line) {
    size_t alpha = 0;
    size_t digit = 0;

    for (char c : line) {
        if (std::isalpha(static_cast<unsigned char>(c))) alpha++;
        else if (std::isdigit(static_cast<unsigned char>(c))) digit++;
    }

    return alpha > digit;
}

std::size_t detect_column_count(const std::vector<std::string>& sample_lines, char delimiter) {
    if (sample_lines.empty()) return 0;

    auto cols = hawk::utils::split(sample_lines.front(), delimiter);
    return cols.size();
}

} // namespace detectors

FormatInferenceResult FormatInferer::infer(const RecordSource& source) const {
    FormatInferenceResult result;

    // Disable format_inference for now.
    // TODO: Refactor.
    // source.sample_lines(options_.max_sample_lines);
    std::vector<std::string> samples = {};
    result.sampled_rows = samples.size();

    if (samples.empty()) {
        result.notes.push_back("No sample lines available");
        return result;
    }

    // Delimiter
    result.delimiter = detectors::detect_delimiter(samples);
    result.notes.push_back("Detected delimiter");

    // Column count (from first line)
    auto cols = hawk::utils::split(samples.front(), result.delimiter);
    result.column_count = cols.size();
    result.notes.push_back("Inferred column count from first row");

    // Header heuristic
    result.has_header = detectors::looks_like_header(samples.front());
    result.notes.push_back(
        result.has_header
            ? "First row looks like a header"
            : "First row looks like data"
    );

    return result;
}

FormatInferenceResult FormatInferer::infer_from_file(const std::string& path) const {
    auto source = std::make_unique<CSVRecordSource>(path);
    return infer(*source);
}

} // namespace hawk::inference
