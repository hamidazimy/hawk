#include <hawk/core/record_source.hpp>

#include <hawk/core/types.hpp>
#include <hawk/core/log_file.hpp>

#include <stdexcept>

namespace hawk {

CSVRecordSource::CSVRecordSource(const std::string& path)
    : file_(path)
{
    build_index();
}

void CSVRecordSource::build_index() {
    auto data = file_.bytes();
    auto size = file_.size();

    record_offsets_.clear();
    record_offsets_.reserve(size / 80);
    record_offsets_.push_back(0);

    bool in_quotes = false;

    for (FileOffset i = 0; i < size; ++i) {
        char c = data[i];
        if (c == '"') {
            // handle escaped quote ""
            if (in_quotes && i + 1 < size && data[i + 1] == '"') {
                ++i;
                continue;
            }
            in_quotes = !in_quotes;
        }
        else if (c == '\n' && !in_quotes) {
            FileOffset next = i + 1;
            if (next < size) {
                record_offsets_.push_back(next);
            }
        }
    }
}

std::string_view CSVRecordSource::get_record(RecordIndex index) const {
    if (index >= record_offsets_.size())
        throw std::out_of_range("Record index out of range");

    auto data = file_.bytes();
    auto size = file_.size();

    FileOffset start = record_offsets_[index];
    FileOffset end = (index + 1 < record_offsets_.size()) ? record_offsets_[index + 1] : size;

    // Trim trailing newline characters
    while (end > start && (data[end - 1] == '\n' || data[end - 1] == '\r')) {
        --end;
    }

    return file_.read_bytes(start, end - start);
}

} // namespace hawk
