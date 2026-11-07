#pragma once

#include <array>
#include <cstdint>
#include <vector>

namespace GuestSceNetTest {
using ResolverLookup = bool (*)(const char*, int, std::vector<std::array<std::uint8_t, 16>>&);
void SetResolverLookupForTests_nid_no_patch(ResolverLookup lookup) noexcept;
}
