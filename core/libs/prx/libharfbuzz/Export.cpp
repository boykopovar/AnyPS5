#include <cstdint>
#include <type_traits>
#include "prx/libSceFreeType/include/GuestObjects.hpp"
#include "prx/libharfbuzz/include/InventoryExports.hpp"
#include "prx/libc/include/General.hpp"
#include "IcuScriptNames.hpp"

#include <hb-ft.h>

namespace {

template <typename T>
bool GuestCopyInPreservingErrno(T& out, const void* source) {
    const bool ok = GuestCopyIn_nid_no_patch(&out, source, sizeof(T));
    return ok;
}

template <typename T>
bool GuestCopyOutPreservingErrno(void* destination, const T& value) {
    const bool ok = GuestCopyOut_nid_no_patch(destination, &value, sizeof(T));
    return ok;
}

bool GuestWritablePreservingErrno(void* destination, std::size_t bytes) {
    const bool ok = GuestWritable_nid_no_patch(destination, bytes);
    return ok;
}

void APS5_VABI FinalizeCachedFace(void* object) {
    auto* face = static_cast<FreeType::GuestFaceRec*>(object);
    std::remove_reference_t<decltype(face->generic)> generic{};
    if (!GuestCopyInPreservingErrno(generic, &face->generic)) return;
    hb_face_destroy(static_cast<hb_face_t*>(generic.data));
}

}

extern "C" {

hb_face_t* APS5_VABI hb_ft_face_create_cached_nid_postfix(FreeType::GuestFaceRec* face) {
    if (!face) NotImplemented_nid_no_patch(__func__);
    FreeType::GuestFaceRec guestFace{};
    if (!GuestCopyInPreservingErrno(guestFace, face)) NotImplemented_nid_no_patch(__func__);
    const auto* owner = static_cast<const FreeType::GuestFace*>(guestFace.internal);
    if (!owner || &owner->rec != face) NotImplemented_nid_no_patch(__func__);
    FT_Face host = nullptr;
    bool finalized = true;
    if (!GuestCopyInPreservingErrno(host, &owner->host) ||
        !GuestCopyInPreservingErrno(finalized, &owner->finalized) ||
        !host || finalized) {
        NotImplemented_nid_no_patch(__func__);
    }

    auto generic = guestFace.generic;
    if (!generic.data || generic.finalizer != FinalizeCachedFace) {
        if (!GuestWritablePreservingErrno(&face->generic, sizeof(face->generic))) {
            NotImplemented_nid_no_patch(__func__);
        }
        if (generic.finalizer) generic.finalizer(face);
        generic = {hb_ft_face_create(host, nullptr), FinalizeCachedFace};
        if (!GuestCopyOutPreservingErrno(&face->generic, generic)) {
            hb_face_destroy(static_cast<hb_face_t*>(generic.data));
            NotImplemented_nid_no_patch(__func__);
        }
    }
    return hb_face_reference(static_cast<hb_face_t*>(generic.data));
}

std::uint32_t APS5_VABI hb_icu_script_to_script_nid_postfix(std::int32_t scriptCode) {
    if (scriptCode < 0 || static_cast<std::size_t>(scriptCode) >= HarfBuzz::IcuScriptNames.size()) return HB_SCRIPT_INVALID;
    const auto name = HarfBuzz::IcuScriptNames[static_cast<std::size_t>(scriptCode)];
    return hb_script_from_string(name.data(), static_cast<int>(name.size()));
}

std::uint32_t APS5_VABI hb_ot_tag_to_script_nid_postfix(std::uint32_t tag) {
    const auto digit = static_cast<std::uint8_t>(tag & 0xFFu);
    if (digit == '2' || digit == '3') {
        switch (tag & 0xFFFFFF32u) {
        case 0x626E6732u:
            return 0x42656E67u;
        case 0x64657632u:
            return 0x44657661u;
        case 0x676A7232u:
            return 0x47756A72u;
        case 0x67757232u:
            return 0x47757275u;
        case 0x6B6E6432u:
            return 0x4B6E6461u;
        case 0x6D6C6D32u:
            return 0x4D6C796Du;
        case 0x6F727932u:
            return 0x4F727961u;
        case 0x746D6C32u:
            return 0x54616D6Cu;
        case 0x74656C32u:
            return 0x54656C75u;
        case 0x6D796D32u:
            return 0x4D796D72u;
        default:
            return 0x5A7A7A7Au;
        }
    }

    if (tag == 0x44464C54u) {
        return 0;
    }
    if (tag == 0x6D617468u) {
        return 0x5A6D7468u;
    }

    if ((tag & 0x0000FF00u) == 0x00002000u) {
        tag |= (tag >> 8) & 0x0000FF00u;
    }
    if ((tag & 0x000000FFu) == 0x00000020u) {
        tag |= (tag >> 8) & 0x000000FFu;
    }

    return tag & ~0x20000000u;
}

}
