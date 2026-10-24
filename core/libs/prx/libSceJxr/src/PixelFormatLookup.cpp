#include "prx/libSceJxr/GuestPixelInfo.h"
#include "prx/libc/include/general/VabiMacros.hpp"
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <stdexcept>

extern "C" std::int64_t JxrLookup_nid_no_patch(JxrPixelInfo* value, std::uint8_t mode);

static_assert(sizeof(JxrPixelInfo) == 56 && alignof(JxrPixelInfo) == 8);
static_assert(offsetof(JxrPixelInfo, flags) == 32 && offsetof(JxrPixelInfo, interpretation) == 40);

extern "C" std::int64_t APS5_VABI PixelFormatLookup_nid_postfix(JxrPixelInfo* value, std::uint8_t mode) {
    if (mode > 1) return JxrLookup_nid_no_patch(nullptr, mode);
    if (value == nullptr) throw std::invalid_argument("PixelFormatLookup: null record behavior is unknown");
    JxrPixelInfo local{};
    auto* bytes = reinterpret_cast<std::byte*>(value);
    if (mode == 0) {
        std::memcpy(&local.guid, bytes, sizeof(local.guid));
        if (local.guid == nullptr) throw std::invalid_argument("PixelFormatLookup: null GUID behavior is unknown");
    } else {
        std::memcpy(&local.flags, bytes + offsetof(JxrPixelInfo, flags), sizeof(local.flags));
        std::memcpy(&local.interpretation, bytes + offsetof(JxrPixelInfo, interpretation), 4 * sizeof(std::uint32_t));
    }
    const auto result = JxrLookup_nid_no_patch(&local, mode);
    if (result == 0) {
        std::memcpy(bytes, &local, offsetof(JxrPixelInfo, bitsPerUnit) + sizeof(local.bitsPerUnit));
        std::memcpy(bytes + offsetof(JxrPixelInfo, flags), &local.flags, sizeof(local) - offsetof(JxrPixelInfo, flags));
    }
    return result;
}
