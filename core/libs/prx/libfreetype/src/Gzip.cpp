#include "prx/libfreetype/include/Gzip.hpp"
#include "prx/libc/include/General.hpp"
#include <ft2build.h>
#include FT_FREETYPE_H
#include FT_GZIP_H
#include <cerrno>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <vector>

namespace {

struct GuestMemoryContext {
    FreeType::GuestMemoryRec* guest;
    FreeType::GuestMemoryRec record;
};

GuestMemoryContext* ContextOf(FT_Memory memory) {
    return static_cast<GuestMemoryContext*>(memory->user);
}

void* Allocate(FT_Memory memory, long size) {
    auto* context = ContextOf(memory);
    return context->record.alloc(context->guest, size);
}

void Release(FT_Memory memory, void* block) {
    auto* context = ContextOf(memory);
    context->record.free(context->guest, block);
}

void* Reallocate(FT_Memory memory, long current, long size, void* block) {
    auto* context = ContextOf(memory);
    return context->record.realloc(context->guest, current, size, block);
}

void CopyGuestInBytesPreservingErrno(const void* guestAddress, void* hostBuffer, std::size_t bytes, const char* caller) {
    const int savedErrno = errno;
    const bool ok = GuestCopyIn_nid_no_patch(hostBuffer, guestAddress, bytes);
    errno = savedErrno;
    if (!ok) NotImplemented_nid_no_patch(caller);
}

void CopyGuestOutBytesPreservingErrno(void* guestAddress, const void* hostBuffer, std::size_t bytes, const char* caller) {
    const int savedErrno = errno;
    const bool ok = GuestCopyOut_nid_no_patch(guestAddress, hostBuffer, bytes);
    errno = savedErrno;
    if (!ok) NotImplemented_nid_no_patch(caller);
}

}

extern "C" {

std::int32_t APS5_VABI FT_Gzip_Uncompress_nid_postfix(FreeType::GuestMemoryRec* memory, std::uint8_t* output, std::uint64_t* outputLength, const std::uint8_t* input, std::uint64_t inputLength) {
    if (!memory || !outputLength || !output) return FT_Err_Invalid_Argument;
    std::uint64_t requestedLength = 0;
    CopyGuestInBytesPreservingErrno(outputLength, &requestedLength, sizeof(requestedLength), __func__);
    if (requestedLength > std::numeric_limits<FT_ULong>::max() || inputLength > std::numeric_limits<FT_ULong>::max()) return FT_Err_Invalid_Argument;
    FreeType::GuestMemoryRec guestMemory{};
    CopyGuestInBytesPreservingErrno(memory, &guestMemory, sizeof(guestMemory), __func__);
    std::vector<std::uint8_t> hostInput;
    if (inputLength > 0) {
        if (!input) return FT_Err_Invalid_Argument;
        hostInput.resize(static_cast<std::size_t>(inputLength));
        CopyGuestInBytesPreservingErrno(input, hostInput.data(), hostInput.size(), __func__);
    }
    std::vector<std::uint8_t> hostOutput(static_cast<std::size_t>(requestedLength));
    GuestMemoryContext context{memory, guestMemory};
    FT_MemoryRec_ host{&context, Allocate, Release, Reallocate};
    FT_ULong length = static_cast<FT_ULong>(requestedLength);
    const FT_Error error = FT_Gzip_Uncompress(&host, hostOutput.data(), &length, hostInput.data(), static_cast<FT_ULong>(inputLength));
    if (error != FT_Err_Ok) return error;
    const std::uint64_t writtenLength = length;
    CopyGuestOutBytesPreservingErrno(outputLength, &requestedLength, sizeof(requestedLength), __func__);
    CopyGuestOutBytesPreservingErrno(output, hostOutput.data(), static_cast<std::size_t>(writtenLength), __func__);
    CopyGuestOutBytesPreservingErrno(outputLength, &writtenLength, sizeof(writtenLength), __func__);
    return FT_Err_Ok;
}

}
