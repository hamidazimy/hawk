#ifndef HAWK_TESTS_MEMORY_RECORD_SOURCE_HPP
#define HAWK_TESTS_MEMORY_RECORD_SOURCE_HPP

// Test-only RecordSource that serves pre-baked records from an in-memory vector,
// with no file access. Used by the inference tests (FormatInferer, TypeInferrer).
//
// Owns nothing: the string_views handed in must outlive the source. Callers pass
// string literals (static storage), so this is safe for the duration of a test.
#include <hawk/core/record_source.hpp>
#include <hawk/core/types.hpp>

#include <string_view>
#include <utility>
#include <vector>

namespace hawk::test {

class MemoryRecordSource : public hawk::RecordSource {
public:
    explicit MemoryRecordSource(std::vector<std::string_view> records,
                                bool has_crlf = false)
        : records_(std::move(records))
        , has_crlf_(has_crlf)
    {}

    hawk::RecordCount record_count() const noexcept override {
        return records_.size();
    }

    std::string_view get_record(hawk::RecordIndex i) const override {
        return records_[i];
    }

    bool has_crlf() const noexcept override { return has_crlf_; }

private:
    std::vector<std::string_view> records_;
    bool has_crlf_;
};

} // namespace hawk::test

#endif // HAWK_TESTS_MEMORY_RECORD_SOURCE_HPP
