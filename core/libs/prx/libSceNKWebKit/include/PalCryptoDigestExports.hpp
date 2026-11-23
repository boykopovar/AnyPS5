#pragma once

#include "SceTypes.hpp"
#include "prx/libc/include/Api.hpp"
#include "prx/libSceNKWebKit/src/PalTypes.hpp"
#include "prx/libSceNKWebKit/src/WtfVector.hpp"
#include <cstddef>
#include <cstdint>
#include <memory>
#include <type_traits>

extern "C" {

void APS5_VABI _ZN3PAL12CryptoDigestD1Ev(PAL::CryptoDigest* self);
void APS5_VABI _ZN3PAL12CryptoDigestD2Ev(PAL::CryptoDigest* self);

}

namespace PAL {

struct CryptoDigestDelete {
    void operator()(CryptoDigest* digest) const {
        _ZN3PAL12CryptoDigestD1Ev(digest);
        _ZdlPv_nid_postfix(digest);
    }
};

using CryptoDigestPtr = std::unique_ptr<CryptoDigest, CryptoDigestDelete>;

static_assert(sizeof(CryptoDigestPtr) == 8);
static_assert(!std::is_trivially_copyable_v<CryptoDigestPtr>);

}  // namespace PAL

extern "C" {

PAL::CryptoDigestPtr APS5_VABI _ZN3PAL12CryptoDigest6createENS0_9AlgorithmE(PAL::CryptoDigest::Algorithm algorithm);
void APS5_VABI _ZN3PAL12CryptoDigest8addBytesEPKvm(PAL::CryptoDigest* self, const void* input, std::size_t length);
NKWebKit::WtfAbi::Vector<std::uint8_t> APS5_VABI _ZN3PAL12CryptoDigest11computeHashEv(PAL::CryptoDigest* self);

}
