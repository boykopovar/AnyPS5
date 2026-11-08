#ifndef CORE_LIBS_PRX_LIBC_INCLUDE_DEBUGNIDBREAKPOINTS_HPP
#define CORE_LIBS_PRX_LIBC_INCLUDE_DEBUGNIDBREAKPOINTS_HPP

#include <cstdint>
#include <string>
#include <string_view>

namespace Debug {

bool ParseNid(std::string_view text, std::uint64_t& nid);
std::string FormatNid(std::uint64_t nid);

}

#endif
