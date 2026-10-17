#include "prx/libSceFreeType/include/GuestObjects.hpp"
#include "prx/libc/include/FilesystemError.hpp"
#include "prx/libc/include/General.hpp"

#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <limits>
#include <memory>
#include <stdexcept>
#include <vector>

using namespace FreeType;

namespace {

bool ReadFontFile(const char* path, std::vector<unsigned char>& bytes) {
    std::filesystem::path hostPath;
    if (ResolvePathError_nid_no_patch(path, hostPath) != 0) return false;
    std::ifstream file(hostPath, std::ios::binary);
    if (!file) return false;
    bytes.assign(std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>());
    return !file.bad();
}

std::int64_t OpenFaceIndex(std::int64_t faceIndex) {
    if (faceIndex > 0) return faceIndex & 0x7FFFFFFF;
    return -static_cast<std::int64_t>((0 - static_cast<std::uint64_t>(faceIndex)) & 0x7FFFFFFFu);
}

FT_Pos HostPosition(std::int64_t value) {
    if (value < std::numeric_limits<FT_Pos>::min() || value > std::numeric_limits<FT_Pos>::max()) {
        throw std::runtime_error("FT_Render_Glyph: an outline coordinate outside the host FT_Pos range is not implemented");
    }
    return static_cast<FT_Pos>(value);
}

void WriteBackOutline(GuestFace& face) {
    const FT_GlyphSlot host = face.host->glyph;
    if (!host || host->format != FT_GLYPH_FORMAT_OUTLINE) return;
    const auto& guest = face.slot.outline;
    auto& outline = host->outline;
    if (guest.n_points != outline.n_points || guest.n_contours != outline.n_contours || guest.tags != outline.tags || guest.contours != outline.contours ||
        guest.points != (face.outlinePoints.empty() ? nullptr : face.outlinePoints.data())) {
        throw std::runtime_error("FT_Render_Glyph: an outline whose arrays the guest replaced is not implemented");
    }
    for (std::size_t index = 0; index < face.outlinePoints.size(); ++index) {
        outline.points[index].x = HostPosition(face.outlinePoints[index].x);
        outline.points[index].y = HostPosition(face.outlinePoints[index].y);
    }
    outline.flags = guest.flags;
}

}

extern "C" {

int APS5_VABI FT_Init_FreeType_nid_postfix(GuestLibrary** alibrary) {
    if (!alibrary) return FT_Err_Invalid_Argument;
    FT_Library host = nullptr;
    const FT_Error error = FT_Init_FreeType(&host);
    if (error != 0) return error;
    *alibrary = new GuestLibrary{host};
    return 0;
}

int APS5_VABI FT_New_Face_nid_postfix(GuestLibrary* library, const char* filepathname, std::int64_t face_index, GuestFaceRec** aface) {
    if (!filepathname) return FT_Err_Invalid_Argument;
    const std::int64_t faceIndex = OpenFaceIndex(face_index);
    if (!library) return FT_Err_Invalid_Library_Handle;
    auto face = std::make_unique<GuestFace>();
    if (!ReadFontFile(filepathname, face->bytes)) return FT_Err_Cannot_Open_Resource;
    if (face->bytes.size() > static_cast<std::size_t>(std::numeric_limits<FT_Long>::max())) {
        throw std::runtime_error("FT_New_Face: a font file larger than the host FT_Long range is not implemented");
    }
    FT_Face host = nullptr;
    const FT_Error error = FT_New_Memory_Face(library->host, face->bytes.data(), static_cast<FT_Long>(face->bytes.size()), static_cast<FT_Long>(faceIndex), aface ? &host : nullptr);
    if (error != 0 || !aface) return error;
    face->library = library;
    face->host = host;
    SyncFace(*face);
    *aface = &face.release()->rec;
    return 0;
}

int APS5_VABI FT_Set_Pixel_Sizes_nid_postfix(GuestFaceRec* face, std::uint32_t pixel_width, std::uint32_t pixel_height) {
    GuestFace* owner = FaceOf(face);
    const FT_Error error = FT_Set_Pixel_Sizes(owner ? owner->host : nullptr, pixel_width, pixel_height);
    if (owner) SyncSize(*owner);
    return error;
}

std::uint32_t APS5_VABI FT_Get_Char_Index_nid_postfix(GuestFaceRec* face, std::uint64_t charcode) {
    const GuestFace* owner = FaceOf(face);
    return FT_Get_Char_Index(owner ? owner->host : nullptr, static_cast<FT_ULong>(static_cast<std::uint32_t>(charcode)));
}

int APS5_VABI FT_Load_Glyph_nid_postfix(GuestFaceRec* face, std::uint32_t glyph_index, std::int32_t load_flags) {
    GuestFace* owner = FaceOf(face);
    const FT_Error error = FT_Load_Glyph(owner ? owner->host : nullptr, glyph_index, load_flags);
    if (owner) SyncSlot(*owner);
    return error;
}

int APS5_VABI FT_Render_Glyph_nid_postfix(GuestGlyphSlotRec* slot, std::int32_t render_mode) {
    GuestFace* owner = FaceOf(slot);
    if (owner) WriteBackOutline(*owner);
    const FT_Error error = FT_Render_Glyph(owner ? owner->host->glyph : nullptr, static_cast<FT_Render_Mode>(render_mode));
    if (owner) SyncSlot(*owner);
    return error;
}

}
