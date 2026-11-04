#ifndef PS5LIFT_HEX_HPP
#define PS5LIFT_HEX_HPP

#include <cstdint>
#include <sstream>
#include <string>

namespace Ps5Lift {

inline std::string Hex(const std::uint64_t value) {
    std::ostringstream stream;
    stream << std::hex << value;
    return stream.str();
}

}

#endif
