#ifndef HAWK_FILTER_HPP
#define HAWK_FILTER_HPP

namespace hawk {

enum class FilterOp {
    EQ, // ==
    NE, // !=
    GT, // >
    LT, // <
    GE, // >=
    LE  // <=
};

} // namespace hawk

#endif // HAWK_FILTER_HPP
