#include "prx/libSceNKWebKit/include/WtfCryptoUtilities.hpp"
#include <cstddef>
#include <cstdint>
#include <stdexcept>

namespace {

[[gnu::noinline]] void CheckArguments(const void* first, const void* second, std::size_t length) {
    if (length != 0 && (!first || !second)) throw std::invalid_argument("WTF::constantTimeMemcmp: null non-empty buffer");
}

}

extern "C" {

[[gnu::noinline]] int APS5_VABI _ZN3WTF18constantTimeMemcmpEPKvS1_m(
    const void* first, const void* second, std::size_t length) {
    CheckArguments(first, second, length);
    const auto* a = static_cast<const std::uint8_t*>(first);
    const auto* b = static_cast<const std::uint8_t*>(second);
    std::uint8_t mismatch = 0;
    for (std::size_t i = 0; i < length; ++i) mismatch |= a[i] ^ b[i];
    return mismatch;
}

}
