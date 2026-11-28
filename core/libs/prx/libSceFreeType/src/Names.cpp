#include "prx/libSceFreeType/include/Api.hpp"
#include "prx/libSceFreeType/include/GuestObjects.hpp"
#include "prx/libc/include/General.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"

#include FT_SFNT_NAMES_H

#include <cerrno>
#include <cstdint>
#include <vector>

using namespace FreeType;

namespace {

bool GuestWritablePreservingErrno(void* destination, std::size_t size) {
    const int savedErrno = errno;
    const bool ok = GuestWritable_nid_no_patch(destination, size);
    errno = savedErrno;
    return ok;
}

bool GuestCopyInPreservingErrno(void* destination, const void* source, std::size_t size) {
    const int savedErrno = errno;
    const bool ok = GuestCopyIn_nid_no_patch(destination, source, size);
    errno = savedErrno;
    return ok;
}

bool GuestCopyOutPreservingErrno(void* destination, const void* source, std::size_t size) {
    const int savedErrno = errno;
    const bool ok = GuestCopyOut_nid_no_patch(destination, source, size);
    errno = savedErrno;
    return ok;
}

}

extern "C" {

int APS5_VABI FT_Get_Glyph_Name_nid_postfix(GuestFaceRec* face, std::uint32_t glyphIndex, void* buffer, std::uint32_t bufferSize) {
    const GuestFace* owner = FaceOf(face);
    const FT_Face host = owner ? owner->host : nullptr;
    if (!host || !buffer || bufferSize == 0) return FT_Get_Glyph_Name(host, glyphIndex, buffer, bufferSize);
    if (!GuestWritablePreservingErrno(buffer, bufferSize)) return FT_Err_Invalid_Argument;
    if (static_cast<std::int64_t>(glyphIndex) >= host->num_glyphs) {
        const char empty = '\0';
        return GuestCopyOutPreservingErrno(buffer, &empty, sizeof(empty)) ? FT_Err_Invalid_Glyph_Index : FT_Err_Invalid_Argument;
    }
    std::vector<char> output(bufferSize);
    if (!GuestCopyInPreservingErrno(output.data(), buffer, bufferSize)) return FT_Err_Invalid_Argument;
    const FT_Error error = FT_Get_Glyph_Name(host, glyphIndex, output.data(), bufferSize);
    if (!GuestCopyOutPreservingErrno(buffer, output.data(), bufferSize)) return FT_Err_Invalid_Argument;
    return error;
}

std::uint32_t APS5_VABI FT_Get_Name_Index_nid_postfix(GuestFaceRec* face, const char* glyphName) {
    const GuestFace* owner = FaceOf(face);
    return FT_Get_Name_Index(owner ? owner->host : nullptr, glyphName);
}

const char* APS5_VABI FT_Get_Postscript_Name_nid_postfix(GuestFaceRec* face) {
    const GuestFace* owner = FaceOf(face);
    return FT_Get_Postscript_Name(owner ? owner->host : nullptr);
}

std::uint32_t APS5_VABI FT_Get_Sfnt_Name_Count_nid_postfix(GuestFaceRec* face) {
    const GuestFace* owner = FaceOf(face);
    return FT_Get_Sfnt_Name_Count(owner ? owner->host : nullptr);
}

int APS5_VABI FT_Get_Sfnt_Name_nid_postfix(GuestFaceRec* face, std::uint32_t index, GuestSfntName* name) {
    const GuestFace* owner = FaceOf(face);
    FT_SfntName native{};
    const FT_Error error = FT_Get_Sfnt_Name(owner ? owner->host : nullptr, index, name ? &native : nullptr);
    if (!error) {
        GuestSfntName guestName{};
        guestName.platform_id = native.platform_id;
        guestName.encoding_id = native.encoding_id;
        guestName.language_id = native.language_id;
        guestName.name_id = native.name_id;
        guestName.string = native.string;
        guestName.string_len = native.string_len;
        const int savedErrno = errno;
        const bool copied = GuestCopyOut_nid_no_patch(name, &guestName, sizeof(guestName));
        errno = savedErrno;
        if (!copied) return FT_Err_Invalid_Argument;
    }
    return error;
}

}
