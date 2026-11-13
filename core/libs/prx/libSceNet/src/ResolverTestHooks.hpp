#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <vector>

namespace GuestSceNetTest {
using ResolverLookup = bool (*)(const char*, int, std::vector<std::array<std::uint8_t, 16>>&);
void SetResolverLookupForTests_nid_no_patch(ResolverLookup lookup) noexcept;
using ResolverReverseLookup = int (*)(const void*, std::size_t, char*, std::size_t);
void SetResolverReverseLookupForTests_nid_no_patch(ResolverReverseLookup lookup) noexcept;
}
