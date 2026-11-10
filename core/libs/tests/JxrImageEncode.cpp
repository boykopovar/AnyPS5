#include "prx/libSceJxr/libSceJxr.hpp"
#include "prx/libc/include/Api.hpp"
#include "prx/libc/include/ApplicationHeap.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <source_location>
#ifdef _WIN32
#include <windows.h>
#endif

namespace {

void Require(bool condition, std::source_location location = std::source_location::current()) {
    if (condition) return;
    std::fprintf(stderr, "JXR image encode check failed at line %u\n", location.line());
    std::abort();
}

template <typename Function>
Function Exported(Function linked, const char* name) {
#ifdef _WIN32
    void* symbol = reinterpret_cast<void*>(GetProcAddress(GetModuleHandleA("libSceJxr.prx"), name));
    Require(symbol != nullptr);
    (void)linked;
    return reinterpret_cast<Function>(symbol);
#else
    (void)name;
    return linked;
#endif
}

int liveBlocks = 0;
int zeroedCallsBeforeFailure = -1;
int closed = 0;
std::int64_t positionResult = 0;

void* APS5_VABI Allocate(std::size_t bytes) {
    ++liveBlocks;
    return std::malloc(bytes ? bytes : 1);
}

void APS5_VABI Release(void* block) {
    if (block) --liveBlocks;
    std::free(block);
}

void* APS5_VABI AllocateZeroed(std::size_t count, std::size_t bytes) {
    if (zeroedCallsBeforeFailure == 0) {
        zeroedCallsBeforeFailure = -1;
        return nullptr;
    }
    if (zeroedCallsBeforeFailure > 0) --zeroedCallsBeforeFailure;
    ++liveBlocks;
    return std::calloc(count ? count : 1, bytes ? bytes : 1);
}

void* APS5_VABI Reallocate(void*, std::size_t) { return nullptr; }
void* APS5_VABI Align(std::size_t, std::size_t) { return nullptr; }
void* APS5_VABI Realign(void*, std::size_t, std::size_t) { return nullptr; }
int APS5_VABI PosixAlign(void**, std::size_t, std::size_t) { return 12; }

void InitializeHeap() {
    void* const api[10] {
        reinterpret_cast<void*>(&Allocate), reinterpret_cast<void*>(&Release), reinterpret_cast<void*>(&AllocateZeroed),
        reinterpret_cast<void*>(&Reallocate), reinterpret_cast<void*>(&Align), reinterpret_cast<void*>(&Realign),
        reinterpret_cast<void*>(&PosixAlign),
    };
    ApplicationHeapRegister_nid_no_patch(api);
}

std::int64_t APS5_VABI GetPosition(Jxr::Stream*, std::size_t* position) {
    *position = 42;
    return positionResult;
}

std::int64_t APS5_VABI CloseStream(Jxr::Stream** stream) {
    ++closed;
    *stream = nullptr;
    return -7;
}

constexpr JxrGuestGuid DontCare {0x6fddc324, 0x4e03, 0x4bfe, {0xb1, 0x85, 0x3d, 0x77, 0x76, 0x8d, 0xc9, 0x00}};
constexpr JxrGuestGuid Rgb24 {0x6fddc324, 0x4e03, 0x4bfe, {0xb1, 0x85, 0x3d, 0x77, 0x76, 0x8d, 0xc9, 0x0d}};

Jxr::ImageEncode* NewEncoder() {
    auto* encoder = static_cast<Jxr::ImageEncode*>(calloc_nid_postfix(1, sizeof(Jxr::ImageEncode)));
    Require(encoder != nullptr);
    return encoder;
}

}

int main() {
    InitializeHeap();
    {
        auto* encoder = NewEncoder();
        Require(PKImageEncode_SetPixelFormat_nid_postfix(encoder, Rgb24) == 0);
        Require(std::memcmp(&encoder->pixelFormat, &Rgb24, 16) == 0);
        Require(encoder->width == 0 && encoder->stream == nullptr);
        free_nid_postfix(encoder);
    }
    {
        auto* encoder = NewEncoder();
        Require(PKImageEncode_SetSize_nid_postfix(encoder, 640, 360) == 0 && encoder->width == 640 && encoder->height == 360);
        Require(PKImageEncode_SetSize_nid_postfix(encoder, -1, -2) == 0 && encoder->width == 0xffffffff && encoder->height == 0xfffffffe);
        Require(encoder->currentLine == 0);
        free_nid_postfix(encoder);
    }
    {
        auto* encoder = NewEncoder();
        Require(PKImageEncode_SetResolution_nid_postfix(encoder, 72.5f, -300.0f) == 0);
        Require(encoder->resolutionX == 72.5f && encoder->resolutionY == -300.0f && encoder->frames == 0);
        free_nid_postfix(encoder);
    }
    {
        const std::uint8_t context[4] {1, 2, 3, 4};
        Require(PKImageEncode_SetColorContext_nid_postfix(nullptr, context, 4) == -2);
        Require(PKImageEncode_SetColorContext_nid_postfix(nullptr, nullptr, 0) == -2);
    }
    {
        int metadata = 0;
        Require(PKImageEncode_SetDescriptiveMetadata_nid_postfix(nullptr, &metadata) == -2);
    }
    {
        std::uint8_t pixels[12] {};
        Require(PKImageEncode_WritePixels_nid_postfix(nullptr, 1, pixels, 12) == -3);
    }
    {
        auto* encoder = NewEncoder();
        Jxr::Stream stream {};
        stream.getPosition = &GetPosition;
        Require(PKImageEncode_Initialize_nid_postfix(encoder, &stream, nullptr, 0) == 0);
        Require(encoder->stream == &stream && encoder->start == 42 && encoder->frames == 1);
        Require(encoder->resolutionX == 96.0f && encoder->resolutionY == 96.0f);
        Require(std::memcmp(&encoder->pixelFormat, &DontCare, 16) == 0);
        positionResult = -102;
        encoder->start = 0;
        Require(PKImageEncode_Initialize_nid_postfix(encoder, &stream, nullptr, 0) == -102 && encoder->start == 42);
        positionResult = 0;
        free_nid_postfix(encoder);
    }
    {
        auto* encoder = NewEncoder();
        Jxr::Stream stream {};
        stream.close = &CloseStream;
        encoder->stream = &stream;
        Require(liveBlocks == 1 && PKImageEncode_Release_nid_postfix(&encoder) == 0);
        Require(encoder == nullptr && closed == 1 && liveBlocks == 0);
    }
    {
        auto* encoder = reinterpret_cast<Jxr::ImageEncode*>(1);
        zeroedCallsBeforeFailure = 0;
        Require(PKImageEncode_Create_nid_postfix(&encoder) == -101 && encoder == nullptr && liveBlocks == 0);
        Require(PKImageEncode_Create_nid_postfix(&encoder) == 0 && encoder && liveBlocks == 1);
        Require(encoder->initialize == Exported(&PKImageEncode_Initialize_nid_postfix, "PKImageEncode_Initialize_nid_postfix") && encoder->setSize == Exported(&PKImageEncode_SetSize_nid_postfix, "PKImageEncode_SetSize_nid_postfix"));
        Require(encoder->setPixelFormat == Exported(&PKImageEncode_SetPixelFormat_nid_postfix, "PKImageEncode_SetPixelFormat_nid_postfix"));
        Require(encoder->setResolution == Exported(&PKImageEncode_SetResolution_nid_postfix, "PKImageEncode_SetResolution_nid_postfix"));
        Require(encoder->setColorContext == Exported(&PKImageEncode_SetColorContext_nid_postfix, "PKImageEncode_SetColorContext_nid_postfix"));
        Require(encoder->setDescriptiveMetadata == Exported(&PKImageEncode_SetDescriptiveMetadata_nid_postfix, "PKImageEncode_SetDescriptiveMetadata_nid_postfix"));
        Require(encoder->writePixels == Exported(&PKImageEncode_WritePixels_nid_postfix, "PKImageEncode_WritePixels_nid_postfix") && encoder->writeSource == nullptr);
        Require(encoder->transcode == nullptr && encoder->release == Exported(&PKImageEncode_Release_nid_postfix, "PKImageEncode_Release_nid_postfix"));
        Require(encoder->terminate && encoder->createNewFrame && encoder->wmp == 0 && encoder->stream == nullptr);
        Require(encoder->writePixelsBandedBegin(encoder, nullptr) == -3);
        Require(encoder->writePixelsBanded(encoder, 1, nullptr, 0, 1) == -3 && encoder->writePixelsBandedEnd(encoder) == -3);
        Require(encoder->setSize(encoder, 8, 4) == 0 && encoder->width == 8 && encoder->height == 4);
        Jxr::Stream stream {};
        stream.getPosition = &GetPosition;
        stream.close = &CloseStream;
        Require(encoder->initialize(encoder, &stream, nullptr, 0) == 0 && encoder->start == 42);
        closed = 0;
        Require(encoder->release(&encoder) == 0 && !encoder && closed == 1 && liveBlocks == 0);
    }
}
