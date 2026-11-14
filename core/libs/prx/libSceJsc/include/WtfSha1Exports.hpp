#pragma once

#include "prx/libc/include/general/VabiMacros.hpp"

#include <array>
#include <cstddef>
#include <cstdint>

namespace Jsc::WtfAbi {

struct SHA1 {
    std::uint8_t buffer[64];
    std::size_t cursor;
    std::uint64_t totalBytes;
    std::uint32_t hash[5];
};

using SHA1Digest = std::array<std::uint8_t, 20>;

static_assert(sizeof(SHA1) == 104);
static_assert(alignof(SHA1) == 8);
static_assert(offsetof(SHA1, buffer) == 0);
static_assert(offsetof(SHA1, cursor) == 64);
static_assert(offsetof(SHA1, totalBytes) == 72);
static_assert(offsetof(SHA1, hash) == 80);
static_assert(sizeof(SHA1Digest) == 20);

}  // namespace Jsc::WtfAbi

extern "C" {

void APS5_VABI _ZN3WTF4SHA111computeHashERSt5arrayIhLm20EE(Jsc::WtfAbi::SHA1* self, Jsc::WtfAbi::SHA1Digest* digest);

}
