#include "prx/libfreetype/include/Gzip.hpp"
#include <ft2build.h>
#include FT_FREETYPE_H
#include FT_GZIP_H
#include <limits>

namespace {

FreeType::GuestMemoryRec* GuestOf(FT_Memory memory) {
    return static_cast<FreeType::GuestMemoryRec*>(memory->user);
}

void* Allocate(FT_Memory memory, long size) {
    auto* guest = GuestOf(memory);
    return guest->alloc(guest, size);
}

void Release(FT_Memory memory, void* block) {
    auto* guest = GuestOf(memory);
    guest->free(guest, block);
}

void* Reallocate(FT_Memory memory, long current, long size, void* block) {
    auto* guest = GuestOf(memory);
    return guest->realloc(guest, current, size, block);
}

}

extern "C" {

std::int32_t APS5_VABI FT_Gzip_Uncompress_nid_postfix(FreeType::GuestMemoryRec* memory, std::uint8_t* output, std::uint64_t* outputLength, const std::uint8_t* input, std::uint64_t inputLength) {
    if (!memory || !outputLength || !output) return FT_Err_Invalid_Argument;
    if (*outputLength > std::numeric_limits<FT_ULong>::max() || inputLength > std::numeric_limits<FT_ULong>::max()) return FT_Err_Invalid_Argument;
    FT_MemoryRec_ host{memory, Allocate, Release, Reallocate};
    FT_ULong length = static_cast<FT_ULong>(*outputLength);
    const FT_Error error = FT_Gzip_Uncompress(&host, output, &length, input, static_cast<FT_ULong>(inputLength));
    if (error == FT_Err_Ok) *outputLength = length;
    return error;
}

}
