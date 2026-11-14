#include "prx/libSceAjmi/libSceAjmi.hpp"

#include <algorithm>
#include <array>
#include <bit>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <exception>
#include <filesystem>
#include <limits>
#include <source_location>
#include <string>
#include <string_view>

#ifdef _WIN32
#include <windows.h>
#else
#include <dlfcn.h>
#endif

namespace {

void Require(bool condition, const char* message, std::source_location location = std::source_location::current()) {
    if (!condition) {
        std::fprintf(stderr, "Ajmi batch alias check failed at line %u: %s\n", location.line(), message);
        std::abort();
    }
}

class Module {
public:
    explicit Module(const char* path) {
#ifdef _WIN32
        handle = LoadLibraryW(std::filesystem::path(std::u8string(reinterpret_cast<const char8_t*>(path))).c_str());
#else
        handle = dlopen(path, RTLD_NOW | RTLD_LOCAL);
#endif
        Require(handle != nullptr, "load module");
    }

    ~Module() {
#ifdef _WIN32
        FreeLibrary(handle);
#else
        dlclose(handle);
#endif
    }

    template <typename TFunction>
    TFunction Import(const char* name) const {
#ifdef _WIN32
        auto symbol = GetProcAddress(handle, name);
#else
        void* symbol = dlsym(handle, name);
#endif
        Require(symbol != nullptr, name);
        return std::bit_cast<TFunction>(symbol);
    }

private:
#ifdef _WIN32
    HMODULE handle = nullptr;
#else
    void* handle = nullptr;
#endif
};

using Bytes = std::array<std::uint8_t, 512>;

template <typename TValue>
void Put(Bytes& bytes, std::size_t offset, TValue value) {
    std::memcpy(bytes.data() + offset, &value, sizeof(value));
}

std::uint32_t Word(const Bytes& bytes, std::size_t offset) {
    std::uint32_t value = 0;
    std::memcpy(&value, bytes.data() + offset, sizeof(value));
    return value;
}

void CheckInline(decltype(&sceAjmiBatchJobInlineBuffer_nid_no_patch) call) {
    std::array<std::uint8_t, 256> input{};
    for (std::size_t index = 0; index < input.size(); ++index) input[index] = static_cast<std::uint8_t>(index * 13 + 7);
    const auto original = input;
    for (const std::uint8_t fill : {0u, 0xCCu, 0xFFu}) {
        for (const std::size_t offset : {0u, 1u, 7u, 16u}) {
            for (const std::size_t size : {0u, 1u, 5u, 7u, 8u, 9u, 13u, 16u, 31u, 32u, 33u, 255u, 256u}) {
                Bytes batch{};
                batch.fill(fill);
                auto expected = batch;
                const std::size_t aligned = (size + 7) & ~std::size_t{7};
                Put(expected, offset, (Word(batch, offset) & ~0x1Fu) | 7u);
                Put(expected, offset + 4, static_cast<std::uint32_t>(aligned));
                std::copy_n(input.begin(), size, expected.begin() + static_cast<std::ptrdiff_t>(offset + 8));
                struct {
                    std::uint64_t before;
                    const void* value;
                    std::uint64_t after;
                } address{0x123456789ABCDEF0ull, nullptr, 0xFEDCBA9876543210ull};
                void* result = call(batch.data() + offset, size ? input.data() : nullptr, size, &address.value);
                Require(result == batch.data() + offset + 8 + aligned, "inline returned pointer");
                Require(address.value == batch.data() + offset + 8, "inline payload pointer");
                Require(address.before == 0x123456789ABCDEF0ull && address.after == 0xFEDCBA9876543210ull, "inline pointer output bounds");
                Require(batch == expected, "inline bytes, kept header bits, padding and bounds");
                Require(input == original, "inline input remains unchanged");
            }
        }
    }
}

void CheckRun(decltype(&sceAjmiBatchJobRunBufferRa_nid_no_patch) call) {
    std::array<std::uint8_t, 64> input{};
    std::array<std::uint8_t, 128> output{};
    std::array<std::uint8_t, 32> sideband{};
    std::array<std::uint8_t, 1> returnAddress{};
    input.fill(0x12);
    output.fill(0x34);
    sideband.fill(0x56);
    const auto originalInput = input;
    const auto originalOutput = output;
    const auto originalSideband = sideband;
    constexpr std::array<std::uint64_t, 10> flagsValues{0, 1, 0x6001, 0x0000000300000002ull, 0x0000800000001001ull, 0x0000200000000000ull, 0x0000400000000000ull, 0x0000000000001000ull, 0xFFFF1FFFFFFFE000ull, std::numeric_limits<std::uint64_t>::max()};
    struct Measurement {
        std::uint64_t flags;
        std::uint32_t header;
        std::uint32_t payload;
    };
    constexpr std::array<Measurement, 7> measurements{{
        {0xFFFFFFFFFFFFFFFFull, 0xCC380004u, 0x1FFFu},
        {0x0000800000001001ull, 0xCC200004u, 0x1001u},
        {0x0000000300000002ull, 0xCC000004u, 2u},
        {0x0000200000000000ull, 0xCC080004u, 0u},
        {0x0000400000000000ull, 0xCC100004u, 0u},
        {0x0000000000001000ull, 0xCC000004u, 0x1000u},
        {0xFFFF1FFFFFFFE000ull, 0xCC000004u, 0u}
    }};
    constexpr std::array<std::size_t, 4> sizes{0, 64, 0xFFFFFFFFull, 0x100000001ull};
    for (const std::uint8_t fill : {0u, 0xCCu, 0xFFu}) {
        for (const std::uint32_t instance : {0u, 1u, 0x12345u, 0xABCDEFu, 0xFFFFFFFFu}) {
            for (const auto flags : flagsValues) {
                for (const auto size : sizes) {
                    for (const bool withReturn : {false, true}) {
                        Bytes batch{};
                        batch.fill(fill);
                        auto expected = batch;
                        constexpr std::size_t start = 7;
                        std::size_t at = start + 8;
                        const auto buffer = [&](std::uint32_t kind, const void* pointer, std::size_t length) {
                            Put(expected, at, (Word(expected, at) & 0xFFFFFFE0u) | kind);
                            Put(expected, at + 4, static_cast<std::uint32_t>(length));
                            Put(expected, at + 8, static_cast<std::uint64_t>(reinterpret_cast<std::uintptr_t>(pointer)));
                            at += 16;
                        };
                        if (withReturn) buffer(6, returnAddress.data(), 0);
                        const void* inputPointer = size ? input.data() : nullptr;
                        void* outputPointer = size ? output.data() : nullptr;
                        void* sidebandPointer = size ? sideband.data() : nullptr;
                        const std::size_t outputSize = size * 2;
                        const std::size_t sidebandSize = size / 2;
                        buffer(1, inputPointer, size);
                        const auto kept = flags & 0x0000E00000001FFFull;
                        Put(expected, at, (Word(expected, at) & 0xFC000000u) | 4u | (static_cast<std::uint32_t>(kept >> 32) << 6));
                        Put(expected, at + 4, static_cast<std::uint32_t>(kept));
                        at += 8;
                        buffer(0x11, outputPointer, outputSize);
                        buffer(0x12, sidebandPointer, sidebandSize);
                        Put(expected, start, (Word(expected, start) & 0xFC000000u) | ((instance & 0xFFFFFu) << 6));
                        Put(expected, start + 4, static_cast<std::uint32_t>(at - start - 8));
                        void* result = call(batch.data() + start, instance, flags, inputPointer, size, outputPointer, outputSize, sidebandPointer, sidebandSize, withReturn ? returnAddress.data() : nullptr);
                        Require(result == batch.data() + at, "run returned pointer");
                        Require(at - start == (withReturn ? 0x50u : 0x40u), "run record size");
                        Require(batch == expected, "run bytes, pointer widths, size truncation, masks and bounds");
                        if (fill == 0xCC && instance == 0x12345 && size == 64) {
                            Require(Word(batch, start) == 0xCC48D140u, "measured run instance header");
                            for (const Measurement& measured : measurements) {
                                if (flags != measured.flags) continue;
                                Require(Word(batch, start + (withReturn ? 0x28 : 0x18)) == measured.header, "measured run flags header");
                                Require(Word(batch, start + (withReturn ? 0x2C : 0x1C)) == measured.payload, "measured run flags value");
                            }
                        }
                        Require(input == originalInput && output == originalOutput && sideband == originalSideband && returnAddress[0] == 0, "run only writes the batch");
                    }
                }
            }
        }
    }
}

} // namespace

int main(int argc, char** argv) {
    Require(argc == 4, "module, operation and variant arguments");
    const bool patched = std::string_view(argv[3]) == "nid";
    Require(patched || std::string_view(argv[3]) == "unpatched", "known variant");
    try {
        Module module(argv[1]);
        if (std::string_view(argv[2]) == "inline") {
            CheckInline(module.Import<decltype(&sceAjmiBatchJobInlineBuffer_nid_no_patch)>(patched ? "4MsUK9kAnKg" : "4MsUK9kAnKg_nid_no_patch_cut"));
        } else {
            Require(std::string_view(argv[2]) == "run", "known operation");
            CheckRun(module.Import<decltype(&sceAjmiBatchJobRunBufferRa_nid_no_patch)>(patched ? "m0xisYC6U2w" : "m0xisYC6U2w_nid_no_patch_cut"));
        }
    } catch (const std::exception& error) {
        std::fprintf(stderr, "%s\n", error.what());
        Require(false, "batch builder alias must write the record without throwing");
    }
}
