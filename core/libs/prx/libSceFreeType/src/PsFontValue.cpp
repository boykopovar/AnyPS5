#include "prx/libSceFreeType/include/GuestObjects.hpp"
#include "prx/libc/include/GuestArena.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"

#include FT_TYPE1_TABLES_H

#include <cstdint>
#include <cstring>
#include <stdexcept>
#include <vector>

using namespace FreeType;

extern "C" {

std::int64_t APS5_VABI FT_Get_PS_Font_Value_nid_postfix(GuestFaceRec* face, std::int32_t key, std::uint32_t index, void* value, std::int64_t valueLength) {
    const GuestFace* owner = FaceOf(face);
    if (owner && sizeof(FT_Long) != 8) {
        throw std::runtime_error("FT_Get_PS_Font_Value: the LP64 value layouts need a 64-bit host FT_Long, which is not implemented");
    }
    if (!owner) return FT_Get_PS_Font_Value(nullptr, static_cast<PS_Dict_Keys>(key), index, nullptr, 0);
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
