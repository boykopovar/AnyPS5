#include "prx/libc/include/Api.hpp"
#include "prx/libSceJxr/libSceJxr.hpp"
#include "prx/libSceJxr/Factory.hpp"
#include "prx/libc/include/ApplicationHeap.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <stdexcept>

namespace {

constexpr std::int64_t OutOfMemory = -101;
constexpr std::int64_t UnsupportedFormat = -106;

bool failAllocation = false;
int liveBlocks = 0;

void Require(bool value, const char* message) {
    if (value) return;
    std::fprintf(stderr, "JXR factory: %s\n", message);
    std::abort();
}

template<typename TAction>
void RequireInvalidArgument(TAction action, const char* message) {
    try {
        action();
    } catch (const std::invalid_argument&) {
        return;
    }
    Require(false, message);
}

void* APS5_VABI Allocate(std::size_t size) {
    ++liveBlocks;
    return std::malloc(size);
}
void APS5_VABI Free(void* pointer) {
    if (pointer) --liveBlocks;
    std::free(pointer);
}
void* APS5_VABI Calloc(std::size_t count, std::size_t size) {
    if (failAllocation) return nullptr;
    ++liveBlocks;
    return std::calloc(count, size);
}
void* APS5_VABI Reallocate(void* pointer, std::size_t size) { return std::realloc(pointer, size); }
void* APS5_VABI Align(std::size_t, std::size_t size) { return std::malloc(size); }
void* APS5_VABI Realign(void* pointer, std::size_t size, std::size_t) { return std::realloc(pointer, size); }
int APS5_VABI PosixAlign(void** pointer, std::size_t, std::size_t size) {
    *pointer = std::malloc(size);
    return *pointer ? 0 : 12;
}

void InitializeHeap() {
    alignas(16) static std::array<std::byte, 0x40> process{};
    alignas(16) static std::array<std::byte, 0x38> libc{};
    alignas(16) static std::array<std::byte, 0x78> replacement{};
    const auto write = [](auto& data, std::size_t offset, auto value) {
        std::memcpy(data.data() + offset, &value, sizeof(value));
    };
    write(process, 0, std::uint64_t{0x40});
    write(process, 8, std::uint32_t{0x4942524f});
    write(process, 0x38, libc.data());
    write(libc, 0, std::uint64_t{0x38});
    write(libc, 0x30, replacement.data());
    write(replacement, 0, std::uint64_t{0x78});
    write(replacement, 8, std::uint64_t{2});
    write(replacement, 0x20, reinterpret_cast<void*>(&Allocate));
    write(replacement, 0x28, reinterpret_cast<void*>(&Free));
    write(replacement, 0x30, reinterpret_cast<void*>(&Calloc));
    write(replacement, 0x38, reinterpret_cast<void*>(&Reallocate));
    write(replacement, 0x40, reinterpret_cast<void*>(&Align));
    write(replacement, 0x48, reinterpret_cast<void*>(&Realign));
    write(replacement, 0x50, reinterpret_cast<void*>(&PosixAlign));
    ApplicationHeapInitialize_nid_no_patch(process.data());
}

template<typename TStruct>
bool IsZero(const TStruct* value) {
    std::array<std::byte, sizeof(TStruct)> zero{};
    return std::memcmp(value, zero.data(), sizeof(TStruct)) == 0;
}

void Factory() {
    void* raw = reinterpret_cast<void*>(1);
    Require(PKCreateFactory_nid_postfix(&raw, 0x0101) == 0 && raw, "factory creation fails");
    auto* factory = static_cast<Jxr::Factory*>(raw);
    Require(factory->createStream != nullptr, "CreateStream slot");
    Require(factory->createStreamFromMemory != nullptr, "memory stream slot");
    Require(factory->createStreamFromFilename != nullptr, "file stream slot");
    Require(factory->release != nullptr, "Release slot");

    void* stream = nullptr;
    Require(factory->createStream(reinterpret_cast<Jxr::PlainStream**>(&stream)) == 0 && stream, "stream creation fails");
    Require(IsZero(static_cast<Jxr::PlainStream*>(stream)), "stream is not zeroed");
    Require(PKFree_nid_postfix(&stream) == 0 && !stream, "stream release fails");

    Require(factory->release(&factory) == 0 && !factory, "factory release fails");
    RequireInvalidArgument([] { PKCreateFactory_nid_postfix(nullptr, 0); }, "null factory pointer is accepted");
    RequireInvalidArgument([] { PKCreateFactory_CreateStream_nid_postfix(nullptr); }, "null stream pointer is accepted");

    failAllocation = true;
    raw = reinterpret_cast<void*>(1);
    Require(PKCreateFactory_nid_postfix(&raw, 0) == OutOfMemory && !raw, "factory allocation failure code");
    stream = reinterpret_cast<void*>(1);
    Require(PKCreateFactory_CreateStream_nid_postfix(&stream) == OutOfMemory && !stream, "stream allocation failure code");
    failAllocation = false;
}

void Codecs() {
    void* raw = nullptr;
    Require(PKCreateCodecFactory_nid_postfix(&raw, 0x0101) == 0 && raw, "codec factory creation fails");
    auto* factory = static_cast<Jxr::CodecFactory*>(raw);
    Require(factory->createCodec != nullptr, "CreateCodec slot");
    Require(factory->createFormatConverter != nullptr, "CreateFormatConverter slot");
    Require(factory->release != nullptr, "Release slot");

    void* codec = reinterpret_cast<void*>(1);
    for (const std::uint32_t iid : {0u, 1u, 2u, 100u, 202u, 0xffffffffu}) {
        Require(factory->createCodec(&iid, &codec) == UnsupportedFormat, "unknown IID code");
    }
    Require(codec == reinterpret_cast<void*>(1), "unknown IID writes the codec");
    RequireInvalidArgument([&] { factory->createCodec(nullptr, &codec); }, "null IID is accepted");
    RequireInvalidArgument([&] {
        const std::uint32_t iid = 1;
        factory->createCodec(&iid, nullptr);
    }, "null codec pointer is accepted");

    void* converter = nullptr;
    Require(factory->createFormatConverter(&converter) == 0 && converter, "converter creation fails");
    auto* value = static_cast<Jxr::FormatConverter*>(converter);
    Require(value->release != nullptr, "converter Release slot");
    Require(value->convert != nullptr, "converter Convert slot");
    Require(value->decoder == nullptr, "converter decoder is set");
    Require(PKFree_nid_postfix(&converter) == 0 && !converter, "converter release fails");
    RequireInvalidArgument([&] { factory->createFormatConverter(nullptr); }, "null converter pointer is accepted");

    failAllocation = true;
    converter = reinterpret_cast<void*>(1);
    Require(factory->createFormatConverter(&converter) == OutOfMemory && !converter, "converter allocation failure code");
    void* failed = reinterpret_cast<void*>(1);
    Require(PKCreateCodecFactory_nid_postfix(&failed, 0) == OutOfMemory && !failed, "codec factory allocation failure code");
    failAllocation = false;

    Require(factory->release(&factory) == 0 && !factory, "codec factory release fails");
}

}

int main() {
    InitializeHeap();
    Factory();
    Codecs();
    Require(liveBlocks == 0, "blocks leaked");
    return 0;
}
