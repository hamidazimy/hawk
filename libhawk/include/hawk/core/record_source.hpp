#ifndef HAWK_RECORD_SOURCE_HPP
#define HAWK_RECORD_SOURCE_HPP

#include <hawk/core/types.hpp>
#include <hawk/core/log_file.hpp>

#include <string>
#include <string_view>
#include <vector>

namespace hawk {

class RecordSource {
public:
    virtual ~RecordSource() = default;

    virtual RecordCount record_count() const noexcept = 0;
    virtual std::string_view get_record(RecordIndex index) const = 0;

    virtual bool has_crlf() const noexcept = 0;
};

class CSVRecordSource : public RecordSource {
public:
    explicit CSVRecordSource(const std::string& path);

    RecordCount record_count() const noexcept override { return record_offsets_.size(); }
    std::string_view get_record(RecordIndex index) const override;

    bool has_crlf() const noexcept override { return file_.has_crlf(); }

private:
    void build_index();

private:
    LogFile file_;
    std::vector<FileOffset> record_offsets_;
};

} // namespace hawk

#endif // HAWK_RECORD_SOURCE_HPP
