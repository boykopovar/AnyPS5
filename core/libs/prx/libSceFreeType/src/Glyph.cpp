#include "prx/libSceFreeType/include/Api.hpp"
#include "prx/libSceFreeType/include/GuestObjects.hpp"
#include "prx/libc/include/General.hpp"
#include "prx/libc/include/GuestArena.hpp"

#include FT_GLYPH_H
#include FT_STROKER_H

#include <algorithm>
#include <bit>
#include <cerrno>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <mutex>
#include <unordered_map>
#include <utility>
#include <vector>

using namespace FreeType;

namespace {

struct ErrnoGuard {
    int savedErrno = errno;
    ~ErrnoGuard() { errno = savedErrno; }
};

bool TryCopyIn(void* destination, const void* source, std::size_t bytes) {
    if (!bytes) return true;
    if (!source) return false;
    ErrnoGuard guard;
    GuestArena::GuestArenaPrepareHostRead_nid_no_patch(source, bytes);
    return GuestReadable_nid_no_patch(source, bytes) && GuestCopyIn_nid_no_patch(destination, source, bytes);
}

class GuestWriteScope {
public:
    GuestWriteScope(void* pointer, std::size_t bytes)
        : saved(), guard(pointer ? pointer : nullptr, pointer ? bytes : 0),
          valid(bytes == 0 || (pointer != nullptr && guard.Open() && GuestWritable_nid_no_patch(pointer, bytes))) {}
    bool Valid() const { return valid; }
    GuestWriteScope(const GuestWriteScope&) = delete;
    GuestWriteScope& operator=(const GuestWriteScope&) = delete;

private:
    ErrnoGuard saved;
    GuestArena::HostWrite guard;
    bool valid = false;
};

bool TryReadLibraryHost(const GuestLibrary* library, FT_Library& host) {
    if (!library) return false;
    const auto address = reinterpret_cast<std::uintptr_t>(library);
    if (address % alignof(GuestLibrary) != 0) return false;
    static const GuestLibrary layoutSample{};
    const auto hostOffset = static_cast<std::size_t>(
        reinterpret_cast<const unsigned char*>(&layoutSample.host) - reinterpret_cast<const unsigned char*>(&layoutSample));
    if (hostOffset > ~std::uintptr_t{0} - address) return false;
    return TryCopyIn(&host, reinterpret_cast<const unsigned char*>(library) + hostOffset, sizeof(host));
}

template <typename TTarget, typename TSource> TTarget Count(TSource value, const char* function) {
    if (!std::in_range<TTarget>(value)) NotImplemented_nid_no_patch(function);
    return static_cast<TTarget>(value);
}

struct Glyph {
    GuestOutlineGlyphRec outline{};
    GuestBitmapGlyphRec bitmap{};
    GuestLibrary* library = nullptr;
    bool isBitmap = false;
    std::unique_ptr<FT_GlyphRec, decltype(&FT_Done_Glyph)> host{nullptr, &FT_Done_Glyph};
    std::vector<GuestVector> points;

    GuestGlyphRec* Root() { return isBitmap ? &bitmap.root : &outline.root; }

    void Sync(const char* function) {
        auto* root = Root();
        if (root->format != static_cast<std::uint32_t>(host->format)) NotImplemented_nid_no_patch(function);
        host->advance.x = Count<FT_Pos>(root->advance.x, function);
        host->advance.y = Count<FT_Pos>(root->advance.y, function);
        if (isBitmap) {
            auto* native = reinterpret_cast<FT_BitmapGlyph>(host.get());
            auto& guest = bitmap;
            if (guest.bitmap.buffer != native->bitmap.buffer) NotImplemented_nid_no_patch(function);
            native->left = guest.left;
            native->top = guest.top;
            native->bitmap = {guest.bitmap.rows, guest.bitmap.width, guest.bitmap.pitch, guest.bitmap.buffer,
                              guest.bitmap.num_grays, guest.bitmap.pixel_mode, guest.bitmap.palette_mode, guest.bitmap.palette};
            return;
        }
        auto& guest = outline.outline;
        auto& native = reinterpret_cast<FT_OutlineGlyph>(host.get())->outline;
        const auto nativePoints = Count<std::uint16_t>(native.n_points, function);
        const auto nativeContours = Count<std::uint16_t>(native.n_contours, function);
        if (guest.n_points != nativePoints || guest.n_contours != nativeContours || guest.points != (points.empty() ? nullptr : points.data()) ||
            static_cast<const void*>(guest.tags) != static_cast<const void*>(native.tags) ||
            static_cast<const void*>(guest.contours) != static_cast<const void*>(native.contours) ||
            (guest.flags & FT_OUTLINE_OWNER) != (native.flags & FT_OUTLINE_OWNER)) {
            NotImplemented_nid_no_patch(function);
        }
        if (native.n_points && (!native.points || !guest.points)) NotImplemented_nid_no_patch(function);
        for (std::size_t index = 0; index < points.size(); ++index) {
            native.points[index] = {Count<FT_Pos>(guest.points[index].x, function), Count<FT_Pos>(guest.points[index].y, function)};
        }
        native.flags = guest.flags;
    }

    void Publish(const char* function, const std::vector<GuestVector>* sourcePoints = nullptr) {
        *Root() = {library, host->clazz, static_cast<std::uint32_t>(host->format), {host->advance.x, host->advance.y}};
        if (isBitmap) {
            const auto* native = reinterpret_cast<FT_BitmapGlyph>(host.get());
            bitmap.left = native->left;
            bitmap.top = native->top;
            const auto& image = native->bitmap;
            bitmap.bitmap = {image.rows, image.width, image.pitch, image.buffer, image.num_grays, image.pixel_mode, image.palette_mode, image.palette};
        } else {
            const auto& native = reinterpret_cast<FT_OutlineGlyph>(host.get())->outline;
            points.resize(native.n_points);
            if (native.n_points && !native.points) NotImplemented_nid_no_patch(function);
            if (sourcePoints && sourcePoints->size() < points.size()) NotImplemented_nid_no_patch(function);
            for (std::size_t index = 0; index < points.size(); ++index) {
                points[index] = sourcePoints ? (*sourcePoints)[index]
                                             : GuestVector{static_cast<std::int64_t>(native.points[index].x), static_cast<std::int64_t>(native.points[index].y)};
            }
            outline.outline = {Count<std::uint16_t>(native.n_contours, function), Count<std::uint16_t>(native.n_points, function),
                               points.empty() ? nullptr : points.data(), reinterpret_cast<unsigned char*>(native.tags),
                               reinterpret_cast<std::uint16_t*>(native.contours), native.flags};
        }
    }
};

std::mutex glyphMutex;
std::unordered_map<GuestGlyphRec*, std::unique_ptr<Glyph>> glyphs;

FT_Vector Advance(const GuestVector& advance) {
    constexpr std::int64_t limit = 0x200000;
    return {static_cast<FT_Pos>(std::clamp(advance.x, -limit, limit)), static_cast<FT_Pos>(std::clamp(advance.y, -limit, limit))};
}

bool ValidAdvance(const GuestVector& advance) {
    return advance.x > -0x200000 && advance.x < 0x200000 && advance.y > -0x200000 && advance.y < 0x200000;
}

FT_Bitmap Bitmap(const GuestBitmap& bitmap) {
    return {bitmap.rows, bitmap.width, bitmap.pitch, bitmap.buffer, bitmap.num_grays, bitmap.pixel_mode, bitmap.palette_mode, bitmap.palette};
}

struct Outline {
    FT_Outline native{};
    std::vector<FT_Vector> points;
    std::vector<GuestVector> guestPoints;
    std::vector<unsigned char> tags;
    std::vector<std::uint16_t> contours;
    bool valid = true;

    Outline(const GuestOutline& outline, const char* function) {
        native = {Count<decltype(native.n_contours)>(outline.n_contours, function), Count<decltype(native.n_points)>(outline.n_points, function), nullptr,
                  nullptr, nullptr, outline.flags};
        if (outline.n_contours > outline.n_points) {
            native.tags = reinterpret_cast<decltype(native.tags)>(outline.tags);
            native.contours = reinterpret_cast<decltype(native.contours)>(outline.contours);
            return;
        }
        if (outline.n_points && !outline.points) NotImplemented_nid_no_patch(function);
        if (outline.n_points) {
            guestPoints.resize(outline.n_points);
            if (!TryCopyIn(guestPoints.data(), outline.points, guestPoints.size() * sizeof(GuestVector))) {
                valid = false;
                return;
            }
            points.resize(outline.n_points);
            for (std::size_t index = 0; index < points.size(); ++index) {
                points[index] = {static_cast<FT_Pos>(guestPoints[index].x), static_cast<FT_Pos>(guestPoints[index].y)};
            }
            native.points = points.data();
            if (outline.tags) {
                tags.resize(outline.n_points);
                if (!TryCopyIn(tags.data(), outline.tags, tags.size())) {
                    valid = false;
                    return;
                }
                native.tags = reinterpret_cast<decltype(native.tags)>(tags.data());
            }
        }
        if (outline.n_contours && outline.contours) {
            contours.resize(outline.n_contours);
            if (!TryCopyIn(contours.data(), outline.contours, contours.size() * sizeof(std::uint16_t))) {
                valid = false;
                return;
            }
            native.contours = reinterpret_cast<decltype(native.contours)>(contours.data());
        }
    }
};

Glyph& Find(GuestGlyphRec* glyph, const char* function, bool requireLibrary = true) {
    const auto found = glyphs.find(glyph);
    if (found == glyphs.end()) NotImplemented_nid_no_patch(function);
    auto& owner = *found->second;
    if ((requireLibrary && glyph->library != owner.library) || glyph->clazz != owner.host->clazz) NotImplemented_nid_no_patch(function);
    return owner;
}

Glyph& Register(GuestLibrary* library, bool isBitmap, std::size_t pointCount) {
    auto owner = std::make_unique<Glyph>();
    owner->library = library;
    owner->isBitmap = isBitmap;
    owner->points.resize(pointCount);
    auto* address = owner->Root();
    return *glyphs.emplace(address, std::move(owner)).first->second;
}

template <typename TOperation> FT_Error Replace(GuestGlyphRec** glyph, bool destroy, const char* function, TOperation operation) {
    const FT_Bool nativeDestroy = destroy ? 1 : 0;
    if (!glyph) return operation(nullptr, nativeDestroy);
    GuestGlyphRec* oldRoot = nullptr;
    if (!TryCopyIn(&oldRoot, glyph, sizeof(oldRoot))) return FT_Err_Invalid_Argument;
    GuestWriteScope glyphWrite(glyph, sizeof(*glyph));
    if (!glyphWrite.Valid()) return FT_Err_Invalid_Argument;
    if (!oldRoot) {
        FT_Glyph empty = nullptr;
        return operation(&empty, nativeDestroy);
    }
    std::lock_guard lock(glyphMutex);
    auto& oldOwner = Find(oldRoot, function);
    oldOwner.Sync(function);
    const auto oldLibrary = oldOwner.library;
    const FT_Glyph oldHost = oldOwner.host.get();
    FT_Glyph result = oldHost;
    const FT_Error error = operation(&result, nativeDestroy);
    if (error) {
        if (result && result != oldHost) NotImplemented_nid_no_patch(function);
        oldOwner.Publish(function);
        if (!result) {
            GuestGlyphRec* cleared = nullptr;
            GuestCopyOut_nid_no_patch(glyph, &cleared, sizeof(cleared));
        }
        return error;
    }
    if (!result) NotImplemented_nid_no_patch(function);
    if (result == oldHost) {
        oldOwner.Publish(function);
        return error;
    }
    std::unique_ptr<FT_GlyphRec, decltype(&FT_Done_Glyph)> resultOwner(result, &FT_Done_Glyph);
    if (destroy) {
        static_cast<void>(oldOwner.host.release());
        glyphs.erase(oldRoot);
    }
    auto replacement = std::make_unique<Glyph>();
    replacement->library = oldLibrary;
    replacement->isBitmap = result->format == FT_GLYPH_FORMAT_BITMAP;
    if (result->format != FT_GLYPH_FORMAT_BITMAP && result->format != FT_GLYPH_FORMAT_OUTLINE) NotImplemented_nid_no_patch(function);
    replacement->host.reset(resultOwner.release());
    if (!replacement->isBitmap) {
        const auto& outline = reinterpret_cast<FT_OutlineGlyph>(result)->outline;
        replacement->points.resize(outline.n_points);
    }
    replacement->Publish(function);
    auto* newRoot = replacement->Root();
    const bool inserted = glyphs.emplace(newRoot, std::move(replacement)).second;
    if (!inserted) NotImplemented_nid_no_patch(function);
    GuestCopyOut_nid_no_patch(glyph, &newRoot, sizeof(newRoot));
    return error;
}

}

int FreeType::FreeTypeNewGlyph_nid_no_patch(GuestLibrary* library, std::uint32_t format, GuestGlyphRec** target) {
    if (!library || !target) return FT_Err_Invalid_Argument;
    FT_Library hostLib = nullptr;
    if (!TryReadLibraryHost(library, hostLib) || !hostLib) return FT_Err_Invalid_Argument;
    if (!InGlyphFormatRange(format)) return FT_Err_Invalid_Glyph_Format;
    const auto nativeFormat = static_cast<FT_Glyph_Format>(format);
    if (nativeFormat != FT_GLYPH_FORMAT_BITMAP && nativeFormat != FT_GLYPH_FORMAT_OUTLINE) {
        FT_Glyph probe = nullptr;
        const FT_Error error = FT_New_Glyph(hostLib, nativeFormat, &probe);
        if (error == FT_Err_Invalid_Glyph_Format || error == FT_Err_Invalid_Argument) return error;
        if (probe) FT_Done_Glyph(probe);
        NotImplemented_nid_no_patch(__func__);
    }
    GuestWriteScope targetWrite(target, sizeof(*target));
    if (!targetWrite.Valid()) return FT_Err_Invalid_Argument;
    GuestGlyphRec* cleared = nullptr;
    if (!GuestCopyOut_nid_no_patch(target, &cleared, sizeof(cleared))) return FT_Err_Invalid_Argument;
    std::lock_guard lock(glyphMutex);
    auto& owner = Register(library, nativeFormat == FT_GLYPH_FORMAT_BITMAP, 0);
    auto* address = owner.Root();
    FT_Glyph result = nullptr;
    const FT_Error error = FT_New_Glyph(hostLib, nativeFormat, &result);
    if (error) {
        glyphs.erase(address);
        return error;
    }
    owner.host.reset(result);
    owner.Publish(__func__);
    if (!GuestCopyOut_nid_no_patch(target, &address, sizeof(address))) {
        glyphs.erase(address);
        return FT_Err_Invalid_Argument;
    }
    return 0;
}

extern "C" {

int APS5_VABI FT_Get_Glyph_nid_postfix(GuestGlyphSlotRec* slot, GuestGlyphRec** target) {
    if (!slot) return FT_Err_Invalid_Slot_Handle;
    if (!target) return FT_Err_Invalid_Argument;
    GuestGlyphSlotRec slotCopy{};
    if (!TryCopyIn(&slotCopy, slot, sizeof(slotCopy))) return FT_Err_Invalid_Slot_Handle;
    GuestWriteScope targetWrite(target, sizeof(*target));
    if (!targetWrite.Valid()) return FT_Err_Invalid_Argument;
    auto* library = static_cast<GuestLibrary*>(slotCopy.library);
    FT_Library hostLib = nullptr;
    if (library && !TryReadLibraryHost(library, hostLib)) return FT_Err_Invalid_Library_Handle;
    FT_GlyphSlotRec native{};
    native.library = hostLib;
    native.format = static_cast<FT_Glyph_Format>(slotCopy.format);
    native.advance = Advance(slotCopy.advance);
    if (!library || !ValidAdvance(slotCopy.advance)) {
        FT_Glyph result = reinterpret_cast<FT_Glyph>(&native);
        const FT_Error error = FT_Get_Glyph(&native, &result);
        if (!result) {
            GuestGlyphRec* cleared = nullptr;
            GuestCopyOut_nid_no_patch(target, &cleared, sizeof(cleared));
        }
        return error;
    }
    if (native.format != FT_GLYPH_FORMAT_BITMAP && native.format != FT_GLYPH_FORMAT_OUTLINE) {
        FT_Glyph probe = nullptr;
        const FT_Error error = FT_New_Glyph(native.library, native.format, &probe);
        if (error) return error;
        FT_Done_Glyph(probe);
        NotImplemented_nid_no_patch(__func__);
    }
    Outline outline(native.format == FT_GLYPH_FORMAT_OUTLINE ? slotCopy.outline : GuestOutline{}, __func__);
    if (!outline.valid) return FT_Err_Invalid_Argument;
    native.outline = outline.native;
    native.bitmap = Bitmap(slotCopy.bitmap);
    native.bitmap_left = slotCopy.bitmap_left;
    native.bitmap_top = slotCopy.bitmap_top;
    if (native.format == FT_GLYPH_FORMAT_BITMAP) {
        const auto* face = FaceOf(slot);
        if (!face || slot != &face->slot || slotCopy.library != face->library) NotImplemented_nid_no_patch(__func__);
        const auto host = face->host->glyph;
        if (slotCopy.bitmap.buffer != host->bitmap.buffer) NotImplemented_nid_no_patch(__func__);
        native.internal = host->internal;
    }
    std::lock_guard lock(glyphMutex);
    auto& owner = Register(library, native.format == FT_GLYPH_FORMAT_BITMAP, native.outline.n_points);
    auto* address = owner.Root();
    FT_Glyph result = reinterpret_cast<FT_Glyph>(&native);
    const FT_Error error = FT_Get_Glyph(&native, &result);
    if (error) {
        glyphs.erase(address);
        if (!result) {
            GuestGlyphRec* cleared = nullptr;
            GuestCopyOut_nid_no_patch(target, &cleared, sizeof(cleared));
        }
        return error;
    }
    owner.host.reset(result);
    owner.Publish(__func__, native.format == FT_GLYPH_FORMAT_OUTLINE ? &outline.guestPoints : nullptr);
    if (!GuestCopyOut_nid_no_patch(target, &address, sizeof(address))) {
        glyphs.erase(address);
        return FT_Err_Invalid_Argument;
    }
    return 0;
}

int APS5_VABI FT_Glyph_Copy_nid_postfix(GuestGlyphRec* source, GuestGlyphRec** target) {
    if (!target || !source) return FT_Err_Invalid_Argument;
    GuestGlyphRec sourceCopy{};
    if (!TryCopyIn(&sourceCopy, source, sizeof(sourceCopy)) || !sourceCopy.clazz) return FT_Err_Invalid_Argument;
    GuestWriteScope targetWrite(target, sizeof(*target));
    if (!targetWrite.Valid()) return FT_Err_Invalid_Argument;
    std::lock_guard lock(glyphMutex);
    auto& original = Find(source, __func__);
    GuestGlyphRec* cleared = nullptr;
    if (!GuestCopyOut_nid_no_patch(target, &cleared, sizeof(cleared))) return FT_Err_Invalid_Argument;
    if (sourceCopy.library != original.library || sourceCopy.clazz != original.host->clazz) NotImplemented_nid_no_patch(__func__);
    const GuestVector advance = sourceCopy.advance;
    FT_OutlineGlyphRec nativeOutline{};
    FT_BitmapGlyphRec nativeBitmap{};
    FT_Glyph native = original.isBitmap ? &nativeBitmap.root : &nativeOutline.root;
    *native = *original.host;
    native->format = static_cast<FT_Glyph_Format>(sourceCopy.format);
    Outline outline(original.isBitmap ? GuestOutline{} : original.outline.outline, __func__);
    if (!outline.valid) return FT_Err_Invalid_Argument;
    nativeOutline.outline = outline.native;
    nativeBitmap.bitmap = Bitmap(original.bitmap.bitmap);
    nativeBitmap.left = original.bitmap.left;
    nativeBitmap.top = original.bitmap.top;
    auto& copy = Register(original.library, original.isBitmap, outline.native.n_points);
    auto* address = copy.Root();
    FT_Glyph result = nullptr;
    const FT_Error error = FT_Glyph_Copy(native, &result);
    if (error) {
        glyphs.erase(address);
        return error;
    }
    copy.host.reset(result);
    copy.Publish(__func__, original.isBitmap ? nullptr : &outline.guestPoints);
    address->advance = advance;
    if (!GuestCopyOut_nid_no_patch(target, &address, sizeof(address))) {
        glyphs.erase(address);
        return FT_Err_Invalid_Argument;
    }
    return 0;
}

void APS5_VABI FreeTypeGlyphGetCBox_nid_no_patch(GuestGlyphRec* glyph, std::uint32_t mode, GuestBBox* box) {
    if (!box) return;
    GuestWriteScope boxWrite(box, sizeof(*box));
    if (!boxWrite.Valid()) return;
    GuestBBox result{};
    if (!GuestCopyOut_nid_no_patch(box, &result, sizeof(result))) return;
    if (!glyph) return;
    GuestGlyphRec glyphCopy{};
    if (!TryCopyIn(&glyphCopy, glyph, sizeof(glyphCopy)) || !glyphCopy.clazz) return;
    std::lock_guard lock(glyphMutex);
    auto& owner = Find(glyph, "FT_Glyph_Get_CBox_nid_postfix", false);
    if (owner.isBitmap) {
        const auto& image = owner.bitmap;
        const auto fits = [](std::int32_t bearing) {
            return bearing >= std::numeric_limits<std::int32_t>::min() / 64 && bearing <= std::numeric_limits<std::int32_t>::max() / 64;
        };
        if (!fits(image.left) || !fits(image.top)) NotImplemented_nid_no_patch("FT_Glyph_Get_CBox_nid_postfix");
        result.xMin = static_cast<std::int64_t>(image.left) * 64;
        result.xMax = result.xMin + static_cast<std::int64_t>(image.bitmap.width * std::uint32_t{64});
        result.yMax = static_cast<std::int64_t>(image.top) * 64;
        result.yMin = result.yMax - static_cast<std::int64_t>(image.bitmap.rows * std::uint32_t{64});
    } else {
        FT_Outline_Get_CBox_nid_postfix(&owner.outline.outline, &result);
    }
    if (mode == FT_GLYPH_BBOX_GRIDFIT || mode == FT_GLYPH_BBOX_PIXELS) {
        const auto ceiling = [](std::int64_t value) {
            return std::bit_cast<std::int64_t>((std::bit_cast<std::uint64_t>(value) + 63) & ~std::uint64_t{63});
        };
        result.xMin &= ~std::int64_t{63};
        result.yMin &= ~std::int64_t{63};
        result.xMax = ceiling(result.xMax);
        result.yMax = ceiling(result.yMax);
    }
    if (mode == FT_GLYPH_BBOX_TRUNCATE || mode == FT_GLYPH_BBOX_PIXELS) {
        result.xMin >>= 6;
        result.yMin >>= 6;
        result.xMax >>= 6;
        result.yMax >>= 6;
    }
    GuestCopyOut_nid_no_patch(box, &result, sizeof(result));
}

void APS5_VABI FT_Glyph_Get_CBox_nid_postfix(GuestGlyphRec* glyph, std::uint32_t mode, GuestBBox* box) {
    FreeTypeGlyphGetCBox_nid_no_patch(glyph, mode, box);
}

void APS5_VABI FT_Done_Glyph_nid_postfix(GuestGlyphRec* glyph) {
    if (!glyph) return;
    std::lock_guard lock(glyphMutex);
    Find(glyph, __func__);
    glyphs.erase(glyph);
}

}

extern "C" int FreeType::FreeTypeGlyphMutationToBitmapHost_nid_no_patch(GuestGlyphRec** glyph, std::int32_t renderMode, const GuestVector* origin,
                                                           std::uint8_t destroy, const char* function) {
    FT_Vector nativeOrigin{};
    const FT_Vector* nativeOriginPointer = nullptr;
    if (origin) {
        GuestVector originCopy{};
        if (!TryCopyIn(&originCopy, origin, sizeof(originCopy))) return FT_Err_Invalid_Argument;
        nativeOrigin = {Count<FT_Pos>(originCopy.x, function), Count<FT_Pos>(originCopy.y, function)};
        nativeOriginPointer = &nativeOrigin;
    }
    return Replace(glyph, destroy != 0, function, [&](FT_Glyph* native, FT_Bool nativeDestroy) {
        return FT_Glyph_To_Bitmap(native, static_cast<FT_Render_Mode>(renderMode), nativeOriginPointer, nativeDestroy);
    });
}

extern "C" int FreeType::FreeTypeGlyphMutationTransformHost_nid_no_patch(GuestGlyphRec* glyph, const GuestMatrix* matrix, const GuestVector* delta,
                                                             const char* function) {
    if (!glyph) return FT_Glyph_Transform(nullptr, nullptr, nullptr);
    FT_Matrix nativeMatrix{};
    const FT_Matrix* nativeMatrixPointer = nullptr;
    if (matrix) {
        GuestMatrix matrixCopy{};
        if (!TryCopyIn(&matrixCopy, matrix, sizeof(matrixCopy))) return FT_Err_Invalid_Argument;
        nativeMatrix = {Count<FT_Fixed>(matrixCopy.xx, function), Count<FT_Fixed>(matrixCopy.xy, function),
                        Count<FT_Fixed>(matrixCopy.yx, function), Count<FT_Fixed>(matrixCopy.yy, function)};
        nativeMatrixPointer = &nativeMatrix;
    }
    FT_Vector nativeDelta{};
    const FT_Vector* nativeDeltaPointer = nullptr;
    if (delta) {
        GuestVector deltaCopy{};
        if (!TryCopyIn(&deltaCopy, delta, sizeof(deltaCopy))) return FT_Err_Invalid_Argument;
        nativeDelta = {Count<FT_Pos>(deltaCopy.x, function), Count<FT_Pos>(deltaCopy.y, function)};
        nativeDeltaPointer = &nativeDelta;
    }
    std::lock_guard lock(glyphMutex);
    auto& owner = Find(glyph, function);
    owner.Sync(function);
    const FT_Error error = FT_Glyph_Transform(owner.host.get(), nativeMatrixPointer, nativeDeltaPointer);
    if (!error) owner.Publish(function);
    return error;
}

extern "C" int FreeType::FreeTypeGlyphMutationStrokeHost_nid_no_patch(GuestGlyphRec** glyph, void* stroker, std::uint8_t destroy, const char* function) {
    return Replace(glyph, destroy != 0, function, [&](FT_Glyph* native, FT_Bool nativeDestroy) {
        return FT_Glyph_Stroke(native, static_cast<FT_Stroker>(FreeTypeHostStroker_nid_no_patch(stroker)), nativeDestroy);
    });
}

extern "C" int FreeType::FreeTypeGlyphMutationStrokeBorderHost_nid_no_patch(GuestGlyphRec** glyph, void* stroker, std::uint8_t inside,
                                                               std::uint8_t destroy, const char* function) {
    return Replace(glyph, destroy != 0, function, [&](FT_Glyph* native, FT_Bool nativeDestroy) {
        return FT_Glyph_StrokeBorder(native, static_cast<FT_Stroker>(FreeTypeHostStroker_nid_no_patch(stroker)), static_cast<FT_Bool>(inside), nativeDestroy);
    });
}
