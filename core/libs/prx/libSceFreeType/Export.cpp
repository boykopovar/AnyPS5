#include "prx/libSceFreeType/include/GuestObjects.hpp"
#include FT_OUTLINE_H
#include "prx/libc/include/FilesystemError.hpp"
#include "prx/libc/include/General.hpp"

#include FT_GASP_H

#include <algorithm>
#include <bit>
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

void FinalizeSlot(void* object) {
    auto* host = static_cast<FT_GlyphSlot>(object);
    auto* face = static_cast<GuestFace*>(host->generic.data);
    face->rec.glyph = nullptr;
    if (face->slot.generic.finalizer) face->slot.generic.finalizer(&face->slot);
}

void FinalizeSize(void* object) {
    auto* host = static_cast<FT_Size>(object);
    auto* face = static_cast<GuestFace*>(host->generic.data);
    if (face->size.generic.finalizer) face->size.generic.finalizer(&face->size);
    face->rec.size = nullptr;
}

void FinalizeFace(void* object) {
    auto* host = static_cast<FT_Face>(object);
    auto* face = static_cast<GuestFace*>(host->generic.data);
    if (face->rec.generic.finalizer) face->rec.generic.finalizer(&face->rec);
}

void PublishFace(GuestLibrary* library, std::unique_ptr<GuestFace> face, FT_Face host, GuestFaceRec** aface) {
    std::unique_ptr<FT_FaceRec, decltype(&FT_Done_Face)> hostOwner(host, &FT_Done_Face);
    face->library = library;
    face->host = host;
    SyncFace(*face);
    auto* owner = face.get();
    library->faces.push_back(std::move(face));
    host->generic = {owner, FinalizeFace};
    if (host->glyph) host->glyph->generic = {owner, FinalizeSlot};
    if (host->size) host->size->generic = {owner, FinalizeSize};
    *aface = &owner->rec;
    hostOwner.release();
}

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
    auto library = std::make_unique<GuestLibrary>();
    const FT_Error error = FT_Init_FreeType(&library->host);
    if (error != 0) return error;
    *alibrary = library.release();
    return 0;
}

int APS5_VABI FT_Done_FreeType_nid_postfix(GuestLibrary* library) {
    const FT_Error error = FT_Done_FreeType(library ? library->host : nullptr);
    if (error == 0) delete library;
    return error;
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
    PublishFace(library, std::move(face), host, aface);
    return 0;
}

int APS5_VABI FT_New_Memory_Face_nid_postfix(GuestLibrary* library, const unsigned char* fileBase, std::int64_t fileSize, std::int64_t faceIndex, GuestFaceRec** aface) {
    if (!fileBase) return FT_Err_Invalid_Argument;
    if (!library) return FT_Err_Invalid_Library_Handle;
    if (fileSize < std::numeric_limits<FT_Long>::min() || fileSize > std::numeric_limits<FT_Long>::max()) {
        throw std::runtime_error("FT_New_Memory_Face: a font buffer size outside the host FT_Long range is not implemented");
    }
    auto face = std::make_unique<GuestFace>();
    FT_Face host = nullptr;
    const FT_Error error = FT_New_Memory_Face(library->host, fileBase, static_cast<FT_Long>(fileSize), static_cast<FT_Long>(OpenFaceIndex(faceIndex)), aface ? &host : nullptr);
    if (error != 0 || !aface) return error;
    PublishFace(library, std::move(face), host, aface);
    return 0;
}

int APS5_VABI FT_Done_Face_nid_postfix(GuestFaceRec* face) {
    GuestFace* owner = FaceOf(face);
    const FT_Error error = FT_Done_Face(owner ? owner->host : nullptr);
    if (error == 0) owner->library->faces.remove_if([owner](const auto& entry) { return entry.get() == owner; });
    return error;
}

int APS5_VABI FT_Set_Pixel_Sizes_nid_postfix(GuestFaceRec* face, std::uint32_t pixel_width, std::uint32_t pixel_height) {
    GuestFace* owner = FaceOf(face);
    const FT_Error error = FT_Set_Pixel_Sizes(owner ? owner->host : nullptr, pixel_width, pixel_height);
    if (owner) SyncSize(*owner);
    return error;
}

int APS5_VABI FT_Set_Char_Size_nid_postfix(GuestFaceRec* face, std::int64_t charWidth, std::int64_t charHeight,
                                        std::uint32_t horizontalResolution, std::uint32_t verticalResolution) {
    GuestFace* owner = FaceOf(face);
    if (!owner) return FT_Err_Invalid_Face_Handle;
    if (!owner->host->size) return FT_Err_Invalid_Size_Handle;
    if (!charWidth) charWidth = charHeight;
    else if (!charHeight) charHeight = charWidth;
    if (charWidth < 64) charWidth = 64;
    if (charHeight < 64) charHeight = 64;
    if (!horizontalResolution) horizontalResolution = verticalResolution;
    else if (!verticalResolution) verticalResolution = horizontalResolution;
    if (!horizontalResolution) horizontalResolution = verticalResolution = 72;
    const auto maximum = std::numeric_limits<FT_Long>::max();
    if (charWidth > (maximum - 36) / horizontalResolution || charHeight > (maximum - 36) / verticalResolution) {
        throw std::runtime_error("FT_Set_Char_Size: a character size or resolution outside the host arithmetic range is not implemented");
    }
    const FT_Error error = FT_Set_Char_Size(owner->host, static_cast<FT_F26Dot6>(charWidth), static_cast<FT_F26Dot6>(charHeight),
                                          horizontalResolution, verticalResolution);
    SyncSize(*owner);
    return error;
}

std::uint32_t APS5_VABI FT_Get_Char_Index_nid_postfix(GuestFaceRec* face, std::uint64_t charcode) {
    const GuestFace* owner = FaceOf(face);
    return FT_Get_Char_Index(owner ? owner->host : nullptr, static_cast<FT_ULong>(static_cast<std::uint32_t>(charcode)));
}

int APS5_VABI FT_Get_Kerning_nid_postfix(GuestFaceRec* face, std::uint32_t leftGlyph, std::uint32_t rightGlyph, std::uint32_t kernMode, GuestVector* kerning) {
    const GuestFace* owner = FaceOf(face);
    if (!owner || !kerning) return FT_Get_Kerning(owner ? owner->host : nullptr, leftGlyph, rightGlyph, kernMode, nullptr);
    if (leftGlyph > std::numeric_limits<std::uint16_t>::max() && FT_IS_SFNT(owner->host)) {
        *kerning = {0, 0};
        return 0;
    }
    FT_Vector result{};
    const FT_Error error = FT_Get_Kerning(owner->host, leftGlyph, rightGlyph, kernMode, &result);
    *kerning = {result.x, result.y};
    return error;
}

int APS5_VABI FT_Load_Glyph_nid_postfix(GuestFaceRec* face, std::uint32_t glyph_index, std::int32_t load_flags) {
    GuestFace* owner = FaceOf(face);
    const FT_Error error = FT_Load_Glyph(owner ? owner->host : nullptr, glyph_index, load_flags);
    if (owner) SyncSlot(*owner);
    return error;
}

int APS5_VABI FT_Get_Gasp_nid_postfix(GuestFaceRec* face, std::uint32_t ppem) {
    const GuestFace* owner = FaceOf(face);
    return FT_Get_Gasp(owner ? owner->host : nullptr, ppem);
}

int APS5_VABI FT_Load_Char_nid_postfix(GuestFaceRec* face, std::uint64_t charCode, std::int32_t loadFlags) {
    GuestFace* owner = FaceOf(face);
    const FT_Error error = FT_Load_Char(owner ? owner->host : nullptr, static_cast<FT_ULong>(static_cast<std::uint32_t>(charCode)), loadFlags);
    if (owner) SyncSlot(*owner);
    return error;
}

int APS5_VABI FT_Outline_Get_Orientation_nid_postfix(GuestOutline* outline) {
    if (!outline || outline->n_points == 0) return FT_ORIENTATION_TRUETYPE;
    const auto initial = outline->points[0];
    GuestBBox bounds{initial.x, initial.y, initial.x, initial.y};
    for (unsigned index = 1; index < outline->n_points; ++index) {
        const auto point = outline->points[index];
        bounds.xMin = std::min(bounds.xMin, point.x);
        bounds.yMin = std::min(bounds.yMin, point.y);
        bounds.xMax = std::max(bounds.xMax, point.x);
        bounds.yMax = std::max(bounds.yMax, point.y);
    }
    if (bounds.xMin == bounds.xMax || bounds.yMin == bounds.yMax ||
        bounds.xMin < -0x1000000 || bounds.yMin < -0x1000000 ||
        bounds.xMax > 0x1000000 || bounds.yMax > 0x1000000) return FT_ORIENTATION_NONE;
    const auto xMagnitude = static_cast<std::uint32_t>((bounds.xMin < 0 ? -bounds.xMin : bounds.xMin) |
                                                       (bounds.xMax < 0 ? -bounds.xMax : bounds.xMax));
    const int xShift = std::max(0, static_cast<int>(std::bit_width(xMagnitude)) - 15);
    const int yShift = std::max(0, static_cast<int>(std::bit_width(static_cast<std::uint32_t>(bounds.yMax - bounds.yMin))) - 15);
    std::int64_t area = 0;
    unsigned first = 0;
    for (unsigned contour = 0; contour < outline->n_contours; ++contour) {
        const unsigned last = outline->contours[contour];
        auto previousX = outline->points[last].x >> xShift;
        auto previousY = outline->points[last].y >> yShift;
        for (unsigned index = first; index <= last; ++index) {
            const auto x = outline->points[index].x >> xShift;
            const auto y = outline->points[index].y >> yShift;
            area += (y - previousY) * (x + previousX);
            previousX = x;
            previousY = y;
        }
        first = last + 1;
    }
    return area > 0 ? FT_ORIENTATION_POSTSCRIPT : area < 0 ? FT_ORIENTATION_TRUETYPE : FT_ORIENTATION_NONE;
}

int APS5_VABI FT_Render_Glyph_nid_postfix(GuestGlyphSlotRec* slot, std::int32_t render_mode) {
    GuestFace* owner = FaceOf(slot);
    if (owner) WriteBackOutline(*owner);
    const FT_Error error = FT_Render_Glyph(owner ? owner->host->glyph : nullptr, static_cast<FT_Render_Mode>(render_mode));
    if (owner) SyncSlot(*owner);
    return error;
}

int APS5_VABI FT_Select_Charmap_nid_postfix(GuestFaceRec* face, std::int32_t encoding) {
    GuestFace* owner = FaceOf(face);
    const FT_Error error = FT_Select_Charmap(owner ? owner->host : nullptr, static_cast<FT_Encoding>(encoding));
    if (error == 0) owner->rec.charmap = owner->charmapPointers.at(FT_Get_Charmap_Index(owner->host->charmap));
    return error;
}

int APS5_VABI FT_Get_SubGlyph_Info_nid_postfix(GuestGlyphSlotRec* slot, std::uint32_t subIndex, std::int32_t* index, std::uint32_t* flags,
                                              std::int32_t* arg1, std::int32_t* arg2, GuestMatrix* transform) {
    const GuestFace* owner = FaceOf(slot);
    FT_Int hostIndex = 0;
    FT_UInt hostFlags = 0;
    FT_Int hostArg1 = 0;
    FT_Int hostArg2 = 0;
    FT_Matrix hostTransform{};
    const FT_Error error = FT_Get_SubGlyph_Info(owner ? owner->host->glyph : nullptr, subIndex, &hostIndex, &hostFlags, &hostArg1, &hostArg2, &hostTransform);
    if (error != 0) return error;
    *index = hostIndex;
    *flags = hostFlags;
    *arg1 = hostArg1;
    *arg2 = hostArg2;
    *transform = {hostTransform.xx, hostTransform.xy, hostTransform.yx, hostTransform.yy};
    return error;
}

int APS5_VABI FT_Set_Charmap_nid_postfix(GuestFaceRec* face, GuestCharMapRec* charmap) {
    GuestFace* owner = FaceOf(face);
    if (!owner || !charmap || owner->charmapPointers.empty()) return FT_Set_Charmap(owner ? owner->host : nullptr, nullptr);
    for (std::size_t index = 0; index < owner->charmapPointers.size(); ++index) {
        if (owner->charmapPointers[index] != charmap) continue;
        const FT_Error error = FT_Set_Charmap(owner->host, owner->host->charmaps[index]);
        if (error == 0) owner->rec.charmap = charmap;
        return error;
    }
    return FT_Err_Invalid_Argument;
}

std::uint64_t APS5_VABI FT_Get_First_Char_nid_postfix(GuestFaceRec* face, std::uint32_t* glyphIndex) {
    const GuestFace* owner = FaceOf(face);
    return FT_Get_First_Char(owner ? owner->host : nullptr, glyphIndex);
}

std::uint64_t APS5_VABI FT_Get_Next_Char_nid_postfix(GuestFaceRec* face, std::uint64_t charcode, std::uint32_t* glyphIndex) {
    const GuestFace* owner = FaceOf(face);
    return FT_Get_Next_Char(owner ? owner->host : nullptr, static_cast<FT_ULong>(static_cast<std::uint32_t>(charcode)), glyphIndex);
}

void APS5_VABI FT_Set_Transform_nid_postfix(GuestFaceRec* face, const GuestMatrix* matrix, const GuestVector* delta) {
    GuestFace* owner = FaceOf(face);
    if (!owner) return;
    const auto component = [](std::int64_t value) {
        if (value < std::numeric_limits<FT_Long>::min() || value > std::numeric_limits<FT_Long>::max()) {
            throw std::runtime_error("FT_Set_Transform: a component outside the host FT_Long range is not implemented");
        }
        return static_cast<FT_Long>(value);
    };
    FT_Matrix nativeMatrix{};
    FT_Vector nativeDelta{};
    if (matrix) nativeMatrix = {component(matrix->xx), component(matrix->xy), component(matrix->yx), component(matrix->yy)};
    if (delta) nativeDelta = {component(delta->x), component(delta->y)};
    FT_Set_Transform(owner->host, matrix ? &nativeMatrix : nullptr, delta ? &nativeDelta : nullptr);
}

}
