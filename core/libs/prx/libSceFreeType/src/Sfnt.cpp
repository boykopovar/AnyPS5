#include "prx/libSceFreeType/include/GuestObjects.hpp"
#include "prx/libc/include/GuestArena.hpp"

#include FT_TRUETYPE_TABLES_H

#include <algorithm>
#include <bit>
#include <cstdint>
#include <limits>
#include <stdexcept>

using namespace FreeType;

namespace {

FT_Error QueryTableLength(FT_Face face, std::uint64_t tag, FT_ULong& length) {
    length = 0;
    if constexpr (sizeof(FT_ULong) < sizeof(tag)) {
        if (tag > std::numeric_limits<FT_ULong>::max()) {
            const auto error = FT_Load_Sfnt_Table(face, 0, 0, nullptr, &length);
            return error != 0 ? error : FT_Err_Table_Missing;
        }
    }
    return FT_Load_Sfnt_Table(face, static_cast<FT_ULong>(tag), 0, nullptr, &length);
}

}

extern "C" {

int APS5_VABI FT_Sfnt_Table_Info_nid_postfix(GuestFaceRec* face, std::uint32_t tableIndex, std::uint64_t* tag, std::uint64_t* length) {
    const GuestFace* owner = FaceOf(face);
    FT_ULong nativeTag = 0;
    FT_ULong nativeLength = 0;
    const auto error = FT_Sfnt_Table_Info(owner ? owner->host : nullptr, tableIndex, tag ? &nativeTag : nullptr, length ? &nativeLength : nullptr);
    if (error == 0) {
        if (tag) {
            GuestArena::HostWrite write(tag, sizeof(*tag));
            *tag = nativeTag;
        }
        if (length) {
            GuestArena::HostWrite write(length, sizeof(*length));
            *length = nativeLength;
        }
    }
    return error;
}

int APS5_VABI FT_Load_Sfnt_Table_nid_postfix(GuestFaceRec* face, std::uint64_t tag, std::int64_t offset, unsigned char* buffer, std::uint64_t* length) {
    const GuestFace* owner = FaceOf(face);
    const FT_Face native = owner ? owner->host : nullptr;
    FT_ULong tableSize = 0;
    const auto error = QueryTableLength(native, tag, tableSize);
    if (error != 0) return error;
    const std::uint64_t requested = length ? *length : tableSize;
    if (length && requested == 0) {
        GuestArena::HostWrite write(length, sizeof(*length));
        *length = tableSize;
        return 0;
    }
    FT_ULong fileSize = tableSize;
    if (tag != 0) {
        const auto fileError = QueryTableLength(native, 0, fileSize);
        if (fileError != 0) return fileError;
    }
    FT_ULong nativeTag = static_cast<FT_ULong>(tag);
    FT_Long nativeOffset = 0;
    if constexpr (sizeof(FT_Long) < sizeof(offset)) {
        std::uint64_t absolute = static_cast<std::uint64_t>(offset);
        if (tag != 0) {
            FT_Stream stream = native->stream;
            if (stream->read) throw std::runtime_error("FT_Load_Sfnt_Table: tagged reads through a native stream callback are not implemented on this host");
            const auto position = stream->pos;
            FT_ULong probeLength = 1;
            unsigned char probe = 0;
            const auto probeError = FT_Load_Sfnt_Table(native, nativeTag, 0, &probe, &probeLength);
            const auto afterProbe = stream->pos;
            stream->pos = position;
            if (probeError != 0) return probeError;
            absolute += static_cast<std::uint64_t>(afterProbe - 1);
        }
        if (absolute >= fileSize) return FT_Err_Invalid_Stream_Operation;
        nativeOffset = std::bit_cast<FT_Long>(static_cast<FT_ULong>(absolute));
        nativeTag = 0;
    } else {
        nativeOffset = static_cast<FT_Long>(offset);
    }
    FT_ULong nativeLength = static_cast<FT_ULong>(std::min(requested, static_cast<std::uint64_t>(std::numeric_limits<FT_ULong>::max())));
    GuestArena::HostWrite write(buffer, static_cast<std::size_t>(std::min<std::uint64_t>(nativeLength, fileSize)));
    const auto readError = FT_Load_Sfnt_Table(native, nativeTag, nativeOffset, buffer, &nativeLength);
    if (readError == 0 && requested > std::numeric_limits<FT_ULong>::max()) return FT_Err_Invalid_Stream_Operation;
    return readError;
}

}
