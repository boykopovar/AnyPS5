#include "prx/libSceJxr/libSceJxr.hpp"
#include "prx/libc/include/GuestArena.hpp"
#include <cstddef>
#include <cstdint>
#include <stdexcept>
#include <string>

extern "C" {
std::size_t JxrCodecBytes_nid_no_patch();
std::size_t JxrCodingContextBytes_nid_no_patch();
void* JxrNextCodec_nid_no_patch(void* codec);
const char* JxrCheckImageBuffer_nid_no_patch(void* codec, std::size_t width, std::size_t rows, std::int32_t* result);
const char* JxrGetTilePos_nid_no_patch(void* codec, std::size_t column, std::size_t row);
const char* JxrInitMRPtr_nid_no_patch(void* codec);
const char* JxrAdvanceMRPtr_nid_no_patch(void* codec);
const char* JxrSwapMRPtr_nid_no_patch(void* codec);
const char* JxrAdvanceOneMBRow_nid_no_patch(void* codec);
const char* JxrResetCodingContext_nid_no_patch(void* context);
std::int32_t JxrAllocateCodingContextDec_nid_no_patch(void* codec, std::int32_t count);
}

namespace {

constexpr std::int32_t MaxCodingContexts = 4096;
constexpr std::int32_t CodingContextError = -1;

[[noreturn]] void Fail(const char* function, const char* error) {
    throw std::invalid_argument(std::string(function) + ": " + error);
}

void Check(const char* error, const char* function) {
    if (error) Fail(function, error);
}

void UpdateCodec(void* codec, const char* (*update)(void*), const char* function) {
    if (!codec) Fail(function, "null codec");
    void* next = JxrNextCodec_nid_no_patch(codec);
    GuestArena::HostWrite primary(codec, JxrCodecBytes_nid_no_patch());
    GuestArena::HostWrite secondary(next, next ? JxrCodecBytes_nid_no_patch() : 0);
    if (!primary.Open() || (next && !secondary.Open())) Fail(function, "codec is not writable");
    Check(update(codec), function);
}

} // namespace

extern "C" {

std::int32_t APS5_VABI checkImageBuffer_nid_postfix(void* codec, std::size_t width, std::size_t rows) {
    std::int32_t result = 0;
    Check(JxrCheckImageBuffer_nid_no_patch(codec, width, rows, &result), __func__);
    return result;
}

void APS5_VABI getTilePos_nid_postfix(void* codec, std::size_t column, std::size_t row) {
    if (!codec) Fail(__func__, "null codec");
    GuestArena::HostWrite write(codec, JxrCodecBytes_nid_no_patch());
    if (!write.Open()) Fail(__func__, "codec is not writable");
    Check(JxrGetTilePos_nid_no_patch(codec, column, row), __func__);
}

void APS5_VABI initMRPtr_nid_postfix(void* codec) {
    UpdateCodec(codec, JxrInitMRPtr_nid_no_patch, __func__);
}

void APS5_VABI advanceMRPtr_nid_postfix(void* codec) {
    UpdateCodec(codec, JxrAdvanceMRPtr_nid_no_patch, __func__);
}

void APS5_VABI swapMRPtr_nid_postfix(void* codec) {
    UpdateCodec(codec, JxrSwapMRPtr_nid_no_patch, __func__);
}

void APS5_VABI advanceOneMBRow_nid_postfix(void* codec) {
    UpdateCodec(codec, JxrAdvanceOneMBRow_nid_no_patch, __func__);
}

void APS5_VABI ResetCodingContext_nid_postfix(void* context) {
    if (!context) Fail(__func__, "null coding context");
    GuestArena::HostWrite write(context, JxrCodingContextBytes_nid_no_patch());
    if (!write.Open()) Fail(__func__, "coding context is not writable");
    Check(JxrResetCodingContext_nid_no_patch(context), __func__);
}

std::int32_t APS5_VABI AllocateCodingContextDec_nid_postfix(void* codec, std::int32_t count) {
    if (count < 1 || count > MaxCodingContexts || !codec) return CodingContextError;
    GuestArena::HostWrite write(codec, JxrCodecBytes_nid_no_patch());
    if (!write.Open()) Fail(__func__, "codec is not writable");
    return JxrAllocateCodingContextDec_nid_no_patch(codec, count);
}

} // extern "C"
