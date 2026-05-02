#ifndef HAWK_SESSION_CONFIG_HPP
#define HAWK_SESSION_CONFIG_HPP

namespace hawk {

struct SessionConfig {
    bool use_crlf = false;
    char delimiter = ',';
    bool has_header = false;
};

} // namespace hawk

#endif // HAWK_SESSION_CONFIG_HPP
