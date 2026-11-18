#include "prx/libSceFreeType/include/Api.hpp"
#include "prx/libSceFreeType/include/GuestObjects.hpp"
#include "prx/libc/include/GuestArena.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"

#include FT_TYPE1_TABLES_H

#include <cstdint>
#include <cstring>
#include <stdexcept>
#include <vector>

using namespace FreeType;

namespace {

bool HostLongValue(std::int32_t key) {
    switch (key) {
    case PS_DICT_FONT_MATRIX:
    case PS_DICT_FONT_BBOX:
    case PS_DICT_BLUE_SCALE:
    case PS_DICT_PASSWORD:
    case PS_DICT_LANGUAGE_GROUP:
    case PS_DICT_ITALIC_ANGLE:
        return true;
    default:
        return false;
    }
}

std::int64_t WidenedLongValue(FT_Face host, std::int32_t key, std::uint32_t index, void* value, std::int64_t valueLength) {
    const FT_Long required = FT_Get_PS_Font_Value(host, static_cast<PS_Dict_Keys>(key), index, nullptr, 0);
    if (required <= 0) return required;
    if (required != static_cast<FT_Long>(sizeof(FT_Long))) {
        throw std::runtime_error("FT_Get_PS_Font_Value: unexpected native size for a long value");
    }
    constexpr std::int64_t guestSize = sizeof(std::int64_t);
    if (!value || valueLength < guestSize) return guestSize;
    FT_Long native = 0;
    if (FT_Get_PS_Font_Value(host, static_cast<PS_Dict_Keys>(key), index, &native, required) != required) {
        throw std::runtime_error("FT_Get_PS_Font_Value: the native value changed size between calls");
    }
    const std::int64_t widened = native;
    GuestArena::HostWrite write(value, sizeof(widened));
    std::memcpy(value, &widened, sizeof(widened));
    return guestSize;
}

}

extern "C" {

std::int64_t APS5_VABI FT_Get_PS_Font_Value_nid_postfix(GuestFaceRec* face, std::int32_t key, std::uint32_t index, void* value, std::int64_t valueLength) {
    const GuestFace* owner = FaceOf(face);
    if (!owner) return FT_Get_PS_Font_Value(nullptr, static_cast<PS_Dict_Keys>(key), index, nullptr, 0);
    if (sizeof(FT_Long) != sizeof(std::int64_t) && HostLongValue(key)) return WidenedLongValue(owner->host, key, index, value, valueLength);
    const FT_Long required = FT_Get_PS_Font_Value(owner->host, static_cast<PS_Dict_Keys>(key), index, nullptr, 0);
    if (required <= 0 || !value || valueLength < required) return required;
    std::vector<unsigned char> native(static_cast<std::size_t>(required));
    const FT_Long length = FT_Get_PS_Font_Value(owner->host, static_cast<PS_Dict_Keys>(key), index, native.data(), required);
    if (length >= 0 && length <= required) {
        GuestArena::HostWrite write(value, static_cast<std::size_t>(length));
        std::memcpy(value, native.data(), static_cast<std::size_t>(length));
    }
    return length;
}

}
