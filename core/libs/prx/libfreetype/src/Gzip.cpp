#include "prx/libfreetype/include/Gzip.hpp"
#include "prx/libc/include/General.hpp"
#include <ft2build.h>
#include FT_FREETYPE_H
#include FT_GZIP_H
#include <algorithm>
#include <cerrno>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <limits>
#include <stdexcept>
#include <vector>

namespace {

struct GuestMemoryContext {
    FreeType::GuestMemoryRec* guest;
    FreeType::GuestMemoryRec record;
    std::vector<bool> allocated;
};

struct ReplayContext {
    const std::vector<bool>* allocated;
    std::size_t next;
};

GuestMemoryContext* ContextOf(FT_Memory memory) {
    return static_cast<GuestMemoryContext*>(memory->user);
}

void* Allocate(FT_Memory memory, long size) {
    auto* context = ContextOf(memory);
    void* block = context->record.alloc(context->guest, size);
    context->allocated.push_back(block != nullptr);
    return block;
}

void Release(FT_Memory memory, void* block) {
    auto* context = ContextOf(memory);
    context->record.free(context->guest, block);
}

void* Reallocate(FT_Memory memory, long current, long size, void* block) {
    auto* context = ContextOf(memory);
    return context->record.realloc(context->guest, current, size, block);
}

void* ReplayAllocate(FT_Memory memory, long size) {
    auto* replay = static_cast<ReplayContext*>(memory->user);
    const std::size_t index = replay->next++;
    if (index < replay->allocated->size() && !(*replay->allocated)[index]) return nullptr;
    return std::malloc(static_cast<std::size_t>(size));
}

void HostRelease(FT_Memory, void* block) {
    std::free(block);
}

void* HostReallocate(FT_Memory, long, long size, void* block) {
    return std::realloc(block, static_cast<std::size_t>(size));
}

std::size_t BytesProduced(const std::vector<std::uint8_t>& produced, FT_Error error, const std::vector<bool>& allocated, FT_ULong capacity,
                          const FT_Byte* input, FT_ULong inputLength) {
    std::vector<std::uint8_t> repeated(produced.size(), 0xff);
    ReplayContext replay{&allocated, 0};
    FT_MemoryRec_ host{&replay, ReplayAllocate, HostRelease, HostReallocate};
    FT_ULong length = capacity;
    if (FT_Gzip_Uncompress(&host, repeated.data(), &length, input, inputLength) != error)
        throw std::logic_error("FT_Gzip_Uncompress: the replayed decompression ended differently");
    return static_cast<std::size_t>(std::mismatch(produced.begin(), produced.end(), repeated.begin()).first - produced.begin());
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
    if (input && inputLength > 0) {
        hostInput.resize(static_cast<std::size_t>(inputLength));
        CopyGuestInBytesPreservingErrno(input, hostInput.data(), hostInput.size(), __func__);
    }
    const FT_Byte* nativeInput = input ? hostInput.data() : nullptr;
    std::vector<std::uint8_t> hostOutput(requestedLength ? static_cast<std::size_t>(requestedLength) : 1);
    GuestMemoryContext context{memory, guestMemory, {}};
    FT_MemoryRec_ host{&context, Allocate, Release, Reallocate};
    FT_ULong length = static_cast<FT_ULong>(requestedLength);
    const FT_Error error = FT_Gzip_Uncompress(&host, hostOutput.data(), &length, nativeInput, static_cast<FT_ULong>(inputLength));
    if (error == FT_Err_Array_Too_Large || error == FT_Err_Invalid_Table || error == FT_Err_Out_Of_Memory) {
        const std::size_t produced = BytesProduced(hostOutput, error, context.allocated, static_cast<FT_ULong>(requestedLength), nativeInput,
                                                   static_cast<FT_ULong>(inputLength));
        if (produced > 0) CopyGuestOutBytesPreservingErrno(output, hostOutput.data(), produced, __func__);
    }
    if (error != FT_Err_Ok || !input) return error;
    const std::uint64_t writtenLength = length;
    CopyGuestOutBytesPreservingErrno(outputLength, &requestedLength, sizeof(requestedLength), __func__);
    CopyGuestOutBytesPreservingErrno(output, hostOutput.data(), static_cast<std::size_t>(writtenLength), __func__);
    CopyGuestOutBytesPreservingErrno(outputLength, &writtenLength, sizeof(writtenLength), __func__);
    return FT_Err_Ok;
}

}
