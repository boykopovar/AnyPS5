#include "prx/libSceLibcInternal/include/Iomanip.hpp"

#include <bit>
#include <stdexcept>

namespace {

static_assert(offsetof(GuestLocale::IosBase, flags) == 0x18);

void APS5_VABI SetFlags(GuestLocale::IosBase* stream, std::int32_t mask) {
    if (stream == nullptr) throw std::invalid_argument("setiosflags: null stream");
    const auto bits = static_cast<std::uint32_t>(mask);
    const auto flags = static_cast<std::uint32_t>(stream->flags);
    stream->flags = std::bit_cast<std::int32_t>((flags & ~bits) | (bits & 0xffffu));
}

void APS5_VABI ResetFlags(GuestLocale::IosBase* stream, std::int32_t mask) {
    if (stream == nullptr) throw std::invalid_argument("resetiosflags: null stream");
    stream->flags = std::bit_cast<std::int32_t>(static_cast<std::uint32_t>(stream->flags) & ~static_cast<std::uint32_t>(mask));
}

}

extern "C" {

StreamFlagManipulator APS5_VABI _ZSt11setiosflagsNSt5_IosbIiE9_FmtflagsE_nid_postfix(std::int32_t mask) {
    return {&SetFlags, mask};
}

StreamFlagManipulator APS5_VABI _ZSt13resetiosflagsNSt5_IosbIiE9_FmtflagsE_nid_postfix(std::int32_t mask) {
    return {&ResetFlags, mask};
}

}
