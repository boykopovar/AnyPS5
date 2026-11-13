#pragma once

#include <cstdint>
#include <type_traits>

namespace PAL {

struct SessionID {
    std::uint64_t m_identifier;
};

static_assert(sizeof(SessionID) == 8);
static_assert(alignof(SessionID) == 8);
static_assert(std::is_trivially_copyable_v<SessionID>);

struct KillRing {};

static_assert(sizeof(KillRing) == 1);
static_assert(std::is_empty_v<KillRing>);

}  // namespace PAL
