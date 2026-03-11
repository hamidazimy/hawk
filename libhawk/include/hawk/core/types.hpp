#ifndef HAWK_TYPES_HPP
#define HAWK_TYPES_HPP

namespace hawk {

enum class LineEnding {
    LF,
    CRLF,
    MIXED,
    UNKNOWN
};

enum class FilterOp {
    EQ,  // ==
    NEQ, // !=
    GT,  // >
    LT,  // <
    GTE, // >=
    LTE  // <=
};

} // namespace hawk

#endif // HAWK_TYPES_HPP
