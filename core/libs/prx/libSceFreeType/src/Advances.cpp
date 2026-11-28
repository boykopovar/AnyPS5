#include "prx/libSceFreeType/libSceFreeType.hpp"
#include "prx/libSceFreeType/include/GuestObjects.hpp"
#include "prx/libc/include/General.hpp"
#include "prx/libc/include/GuestArena.hpp"

#include FT_ADVANCES_H
#include FT_FONT_FORMATS_H
#include FT_TRUETYPE_TABLES_H

#include <algorithm>
#include <cerrno>
#include <cstdint>
#include <cstring>
#include <limits>
#include <stdexcept>
#include <vector>

using namespace FreeType;

namespace {

bool CopyGuestIn(void* destination, const void* source, std::size_t bytes) {
    const int savedErrno = errno;
    const bool copied = GuestCopyIn_nid_no_patch(destination, source, bytes);
    errno = savedErrno;
    return copied;
}

bool GuestBufferWritable(void* destination, std::size_t bytes) {
    const int savedErrno = errno;
    GuestArena::HostWrite write(destination, bytes);
    const bool writable = write.Open() && GuestWritable_nid_no_patch(destination, bytes);
    errno = savedErrno;
    return writable;
}

bool CopyGuestOut(void* destination, const void* source, std::size_t bytes) {
    const int savedErrno = errno;
    GuestArena::HostWrite write(destination, bytes);
    const bool copied = write.Open() && GuestWritable_nid_no_patch(destination, bytes)
        && GuestCopyOut_nid_no_patch(destination, source, bytes);
    errno = savedErrno;
    return copied;
}

bool FastQueryLoadsGlyph(FT_Face face, std::int32_t flags) {
    const char* format = FT_Get_Font_Format(face);
    if (!format || std::strcmp(format, "CFF") != 0) return false;
    if (!FT_IS_SFNT(face)) return true;
    if (flags & FT_LOAD_VERTICAL_LAYOUT) return !FT_HAS_VERTICAL(face);
    const auto* header = static_cast<const TT_HoriHeader*>(FT_Get_Sfnt_Table(face, ft_sfnt_hhea));
    return !header || header->number_Of_HMetrics == 0;
}

int GetAdvances(GuestFaceRec* face, std::uint32_t start, std::uint32_t count, std::int32_t flags, std::int64_t* advances, bool single) {
    GuestFace* owner = FaceOf(face);
    if (!owner) return FT_Err_Invalid_Face_Handle;
    if (!advances) return FT_Err_Invalid_Argument;
    const auto numGlyphs = static_cast<std::uint32_t>(owner->host->num_glyphs);
    const std::uint32_t end = start + count;
    if (start >= numGlyphs || end < start || end > numGlyphs) return FT_Err_Invalid_Glyph_Index;
    if (count == 0) return 0;
    if (count > std::numeric_limits<std::size_t>::max() / sizeof(std::int64_t)) return FT_Err_Invalid_Argument;
    const std::size_t guestBytes = static_cast<std::size_t>(count) * sizeof(std::int64_t);
    if (guestBytes > std::numeric_limits<std::uintptr_t>::max() - reinterpret_cast<std::uintptr_t>(advances)) {
        return FT_Err_Invalid_Argument;
    }
    if (!GuestBufferWritable(advances, guestBytes)) return FT_Err_Invalid_Argument;

    std::vector<FT_Fixed> values(count);
    std::vector<FT_Fixed> repeat;
    if constexpr (sizeof(FT_Fixed) == sizeof(std::int64_t)) {
        if (!CopyGuestIn(values.data(), advances, guestBytes)) return FT_Err_Invalid_Argument;
    }
    const auto query = [&](std::int32_t loadFlags) {
        const auto run = [&](FT_Fixed* output) {
            return single ? FT_Get_Advance(owner->host, start, loadFlags, output)
                          : FT_Get_Advances(owner->host, start, count, loadFlags, output);
        };
        if constexpr (sizeof(FT_Fixed) == sizeof(std::int64_t)) {
            return run(values.data());
        } else {
            values.assign(count, 0);
            repeat.assign(count, 1);
            const FT_Error result = run(values.data());
            if (run(repeat.data()) != result) throw std::runtime_error("FreeType advance query: native query is not repeatable");
            return result;
        }
    };
    FT_Error error = query(flags | FT_ADVANCE_FLAG_FAST_ONLY);
    bool syncSlot = error != FT_Err_Unimplemented_Feature && FastQueryLoadsGlyph(owner->host, flags);
    if (error == FT_Err_Unimplemented_Feature && !(flags & FT_ADVANCE_FLAG_FAST_ONLY)) {
        if constexpr (sizeof(FT_Fixed) == sizeof(std::int64_t)) {
            error = query(flags);
            syncSlot = true;
        } else {
            std::vector<std::int64_t> loaded(count);
            const std::int64_t factor = (flags & FT_LOAD_NO_SCALE) ? 1 : 1024;
            std::uint32_t written = 0;
            for (; written < count; ++written) {
                error = FT_Load_Glyph(owner->host, start + written, flags | FT_LOAD_ADVANCE_ONLY);
                if (error) break;
                const FT_Vector advance = owner->host->glyph->advance;
                loaded[written] = static_cast<std::int64_t>((flags & FT_LOAD_VERTICAL_LAYOUT) ? advance.y : advance.x) * factor;
            }
            SyncSlot(*owner);
            if (written != 0 && !CopyGuestOut(advances, loaded.data(), static_cast<std::size_t>(written) * sizeof(std::int64_t))) {
                return FT_Err_Invalid_Argument;
            }
            return error;
        }
    }
    if (syncSlot) SyncSlot(*owner);
    if constexpr (sizeof(FT_Fixed) == sizeof(std::int64_t)) {
        if (!CopyGuestOut(advances, values.data(), guestBytes)) return FT_Err_Invalid_Argument;
    } else {
        bool updated = false;
        for (std::uint32_t index = 0; index < count && !updated; ++index) {
            updated = values[index] == repeat[index];
        }
        if (updated) {
            std::vector<std::int64_t> guestAdvances(count);
            if (!CopyGuestIn(guestAdvances.data(), advances, guestBytes)) return FT_Err_Invalid_Argument;
            for (std::uint32_t index = 0; index < count; ++index) {
                if (values[index] == repeat[index]) guestAdvances[index] = values[index];
            }
            if (!CopyGuestOut(advances, guestAdvances.data(), guestBytes)) return FT_Err_Invalid_Argument;
        }
    }
    return error;
}

}

extern "C" {

int APS5_VABI FT_Get_Advance_nid_postfix(GuestFaceRec* face, std::uint32_t glyphIndex, std::int32_t flags, std::int64_t* advance) {
    return GetAdvances(face, glyphIndex, 1, flags, advance, true);
}

int APS5_VABI FT_Get_Advances_nid_postfix(GuestFaceRec* face, std::uint32_t start, std::uint32_t count, std::int32_t flags, std::int64_t* advances) {
    return GetAdvances(face, start, count, flags, advances, false);
}

}
