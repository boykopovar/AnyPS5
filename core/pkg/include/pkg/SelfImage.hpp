#ifndef PKG_SELFIMAGE_HPP
#define PKG_SELFIMAGE_HPP

#include <cstdint>
#include <vector>

namespace Pkg {

bool IsSelfImage(const std::uint8_t* bytes, std::size_t size);

std::vector<std::uint8_t> UnwrapSelfImage(const std::vector<std::uint8_t>& self);

}

#endif
