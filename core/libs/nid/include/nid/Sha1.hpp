#ifndef NID_SHA1_HPP
#define NID_SHA1_HPP

#include <cstdint>
#include <array>
#include <vector>

namespace Nid {

std::array<std::uint8_t, 20> Sha1(const std::vector<std::uint8_t>& data);

}

#endif
