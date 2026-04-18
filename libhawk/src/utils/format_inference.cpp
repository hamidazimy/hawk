#include <hawk/utils/format_inference.hpp>

#include <hawk/core/types.hpp>
#include <hawk/core/record_source.hpp>
#include <hawk/utils/utils.hpp>

#include <algorithm>
#include <charconv>
#include <numeric>
#include <system_error>

namespace hawk::inference {

namespace detectors {

bool detect_delimiter(
    const std::vector<std::string_view>& samples,
    char& out_delimiter
) {
    static const std::vector<char> candidates = {
        ',', '\t', '|', ';', ' '
    };

    if (samples.empty()) return false;

    char best = ',';
    double best_score = -1.0;

    for (char delim : candidates) {
        std::vector<size_t> counts;
        counts.reserve(samples.size());
        for (const auto& line : samples)
            counts.push_back(std::count(line.begin(), line.end(), delim));

        double mean = std::accumulate(counts.begin(), counts.end(), 0.0) / counts.size();
        if (mean < 1.0) continue;

        double variance = 0.0;
        for (auto c : counts)
            variance += (c - mean) * (c - mean);
        variance /= counts.size();

        double score = mean / (1.0 + variance);
        if (score > best_score) {
            best_score = score;
            best = delim;
        }
    }

    if (best_score <= 0.0) return false;

    out_delimiter = best;
    return true;
}

bool detect_quotes(
    const std::vector<std::string_view>& samples,
    char& out_quote_char
) {
    for (const auto& line : samples) {
        if (line.find('"') != std::string_view::npos) {
            out_quote_char = '"';
            return true;
        }
    }
    return false;
}

bool detect_column_count(
    const std::vector<std::string_view>& samples,
    char delimiter,
    ColumnCount& out_column_count
) {
    if (samples.empty()) return false;
    out_column_count = hawk::utils::split(samples.front(), delimiter).size();
    return true;
}

bool detect_header(
    const std::vector<std::string_view>& samples,
    char delimiter,
    bool& out_has_header
) {
    if (samples.empty()) return false;
    auto first_fields = hawk::utils::split(samples.front(), delimiter);
    if (first_fields.empty()) return false;
    for (const auto& field : first_fields) {
        auto trimmed = hawk::utils::trim(field);
        if (trimmed.empty()) continue;
        double tmp;
        const char* begin = trimmed.data();
        const char* end   = trimmed.data() + trimmed.size();
        auto res = std::from_chars(begin, end, tmp);
        if (res.ec == std::errc{} && res.ptr == end) {
            out_has_header = false;
            return true;
        }
    }
    out_has_header = true;
    return true;
}

} // namespace detectors

FormatInferenceResult FormatInferer::infer(const RecordSource& source) const {
    FormatInferenceResult result;

    const RecordCount total = source.record_count();
    result.total_rows_estimate = total;

    if (total == 0) {
        result.notes.push_back("Source contains no records");
        return result;
    }

    const RecordCount sample_count =
        std::min(static_cast<RecordCount>(options_.max_sample_lines), total);

    std::vector<std::string_view> samples;
    samples.reserve(sample_count);
    for (RecordCount i = 0; i < sample_count; ++i)
        samples.push_back(source.get_record(i));

    result.sampled_rows = samples.size();

    // Delimiter
    if (detectors::detect_delimiter(samples, result.delimiter))
        result.notes.push_back(
            std::string("Detected delimiter: '") + result.delimiter + "'");
    else
        result.notes.push_back(
            "Could not detect delimiter confidently — defaulting to ','");

    // Column count
    if (detectors::detect_column_count(samples, result.delimiter, result.column_count))
        result.notes.push_back(
            "Inferred " + std::to_string(result.column_count) + " column(s) from first record");
    else
        result.notes.push_back("Could not determine column count");

    // Header
    if (detectors::detect_header(samples, result.delimiter, result.has_header))
        result.notes.push_back(
            result.has_header
                ? "First row looks like a header"
                : "First row looks like data");
    else
        result.notes.push_back("Could not determine whether first row is a header");

    // Quotes
    if (detectors::detect_quotes(samples, result.quote_char)) {
        result.uses_quotes = true;
        result.notes.push_back(
            std::string("Detected quoted fields (quote char: '") +
            result.quote_char + "')");
    }

    return result;
}

} // namespace hawk::inference
