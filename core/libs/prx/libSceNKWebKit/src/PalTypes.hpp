#pragma once

#include <cstddef>
#include <cstdint>
#include <type_traits>

namespace PAL {

struct SessionID {
    std::uint64_t m_identifier;
};

static_assert(sizeof(SessionID) == 8);
static_assert(alignof(SessionID) == 8);
static_assert(std::is_trivially_copyable_v<SessionID>);

struct CryptoDigestContext;

struct CryptoDigest {
    enum class Algorithm : int {
        SHA_1,
        SHA_224,
        SHA_256,
        SHA_384,
        SHA_512,
    };

    CryptoDigestContext* m_context;
};

static_assert(sizeof(CryptoDigest) == 8);
static_assert(alignof(CryptoDigest) == 8);
static_assert(offsetof(CryptoDigest, m_context) == 0);
static_assert(sizeof(CryptoDigest::Algorithm) == 4);

}  // namespace PAL
