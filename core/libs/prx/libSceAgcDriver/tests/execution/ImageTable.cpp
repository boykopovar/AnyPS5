#include "prx/libSceAgcDriver/Execution/include/VulkanDevice.hpp"
#include "prx/libSceAgcDriver/Execution/include/Driver/Dispatch/DispatchCache.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Draw.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Texture.hpp"
#include "prx/libc/include/GuestAllocations.hpp"
#include "Recompiler.hpp"
#include "ImageTableAbi.hpp"
#include "VulkanTestDevice.hpp"
#include <algorithm>
#include <array>
#include <bit>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <functional>
#include <iostream>
#include <iterator>
#include <span>
#include <string>
#include <vector>
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <io.h>
#else
#include <sys/mman.h>
#include <unistd.h>
#endif

namespace {

using AgcDriver::Graphics::Require;
using ShaderRecompiler::ShaderStage;

constexpr std::uint32_t Width = 64;
constexpr std::uint32_t Height = 4;
constexpr std::uint32_t Stride = 48;
constexpr std::uint32_t Records = 12;
constexpr std::uint32_t Lanes = 64;
constexpr std::uint32_t MaxGroups = 8;
constexpr std::uint32_t Format32UInt = 20;
constexpr std::uint32_t Format32SInt = 21;
constexpr std::uint32_t Format32Float = 22;
constexpr std::uint32_t Format11_11_10UInt = 34;
constexpr std::uint32_t Format8_8Srgb = 129;
constexpr std::uint32_t Type2D = 9;
constexpr std::uint32_t Type3D = 10;
constexpr std::uint32_t DepthSwizzleMode = 0x18;

alignas(256) std::array<float, Width * Height> FloatTexels{};
alignas(256) std::array<float, Width * Height> OtherFloatTexels{};
alignas(256) std::array<std::uint32_t, Width * Height> UintTexels{};
alignas(256) std::array<std::int32_t, Width * Height> SintTexels{};
alignas(4096) std::array<std::uint32_t, 49152> DepthStorage{};
alignas(256) std::array<std::uint32_t, Records * Stride / 4> Table{};
alignas(256) std::array<std::uint32_t, 64> Keys{};
alignas(256) std::array<std::uint32_t, MaxGroups * 128 * 4> Output{};
alignas(256) std::array<std::uint32_t, 32> Srt{};
constexpr std::uint32_t PaletteOffset = 0x40;
constexpr std::uint32_t PaletteBytes = PaletteOffset + 256u * 32u;
std::vector<std::uint32_t> Root(0x100000u);

alignas(256) constexpr std::array<std::uint32_t, 28> TableCode{
    0xf4080100u, 0xfa000000u, 0xf4080200u, 0xfa000010u, 0xf4080700u, 0xfa000020u, 0x8f108202u, 0xf4200444u,
    0x20000000u, 0x9312b011u, 0xf42c0802u, 0x24000000u, 0x360200bfu, 0x7e020d01u, 0x060202f0u, 0x100202ffu,
    0x3c800000u, 0x7e0402ffu, 0x3e000000u, 0xf09c8f08u, 0x01090401u, 0x7e120202u, 0x34101286u, 0x4a101100u,
    0x34101084u, 0xe0781000u, 0x80070408u, 0xbf810000u,
};

alignas(256) constexpr std::array<std::uint32_t, 35> GuardedTableCode{
    0xf4080100u, 0xfa000000u, 0xf4080200u, 0xfa000010u, 0xf4080700u, 0xfa000020u,
    0xf4040a00u, 0xfa000070u, 0xbf8cc07fu, 0xf4000a94u, 0xfa000000u, 0xbf8cc07fu,
    0x8f108202u, 0xf4200444u, 0x20000000u, 0x9312b011u, 0xf42c0802u, 0x24000000u,
    0x360200bfu, 0x7e020d01u, 0x060202f0u, 0x100202ffu, 0x3c800000u, 0x7e0402ffu,
    0x3e000000u, 0xf09c8f08u, 0x01090401u, 0x7e120202u, 0x34101286u, 0x4a101100u,
    0x34101084u, 0x7e08022au, 0xe0781000u, 0x80070408u, 0xbf810000u,
};

alignas(256) constexpr std::array<std::uint32_t, 30> TableDirectSamplerCode{
    0xf4080100u, 0xfa000000u, 0xf4080200u, 0xfa000010u, 0xf4080700u, 0xfa000020u, 0xf4080800u, 0xfa000030u,
    0x8f108202u, 0xf4200444u, 0x20000000u, 0x9312b011u, 0xf4280902u, 0x24000010u, 0x360200bfu, 0x7e020d01u,
    0x060202f0u, 0x100202ffu, 0x3c800000u, 0x7e0402ffu, 0x3e000000u, 0xf09c8f08u, 0x01090401u, 0x7e120202u,
    0x34101286u, 0x4a101100u, 0x34101084u, 0xe0781000u, 0x80070408u, 0xbf810000u,
};

alignas(256) constexpr std::array<std::uint32_t, 30> TableSamplerCode{
    0xf4080100u, 0xfa000000u, 0xf4080200u, 0xfa000010u, 0xf4080700u, 0xfa000020u, 0xf4080900u, 0xfa000040u,
    0x8f108202u, 0xf4200444u, 0x20000000u, 0x9312b011u, 0xf4280802u, 0x24000000u, 0x360200bfu, 0x7e020d01u,
    0x060202f0u, 0x100202ffu, 0x3c800000u, 0x7e0402ffu, 0x3e000000u, 0xf09c8f08u, 0x01090401u, 0x7e120202u,
    0x34101286u, 0x4a101100u, 0x34101084u, 0xe0781000u, 0x80070408u, 0xbf810000u,
};

alignas(256) constexpr std::array<std::uint32_t, 30> TableCutCode{
    0xf4080100u, 0xfa000000u, 0xf4080200u, 0xfa000010u, 0xf4080700u, 0xfa000020u, 0x8f108202u, 0xf4200444u,
    0x20000000u, 0x9312b011u, 0xf42c0802u, 0x24000000u, 0x360200bfu, 0x7e020d01u, 0x060202f0u, 0x100202ffu,
    0x3c800000u, 0x7e0402ffu, 0x3e000000u, 0x7da400ffu, 0x00001000u, 0xf09c8f08u, 0x01090401u, 0x7e120202u,
    0x34101286u, 0x4a101100u, 0x34101084u, 0xe0781000u, 0x80070408u, 0xbf810000u,
};

alignas(256) constexpr std::array<std::uint32_t, 32> TableWave32Code{
    0xf4080100u, 0xfa000000u, 0xf4080200u, 0xfa000010u, 0xf4080700u, 0xfa000020u, 0x7e200500u, 0x90108510u,
    0x8f118202u, 0x81101110u, 0x8f108210u, 0xf4200444u, 0x20000000u, 0x9312b011u, 0xf42c0802u, 0x24000000u,
    0x360200bfu, 0x7e020d01u, 0x060202f0u, 0x100202ffu, 0x3c800000u, 0x7e0402ffu, 0x3e000000u, 0xf09c8f08u,
    0x01090401u, 0x7e120202u, 0x34101287u, 0x4a101100u, 0x34101084u, 0xe0781000u, 0x80070408u, 0xbf810000u,
};

alignas(256) constexpr std::array<std::uint32_t, 22> DirectCode{
    0xf4080700u, 0xfa000020u, 0xf4080800u, 0xfa000030u, 0xf4080900u, 0xfa000040u, 0x360200bfu, 0x7e020d01u,
    0x060202f0u, 0x100202ffu, 0x3c800000u, 0x7e0402ffu, 0x3e000000u, 0xf09c8f08u, 0x01090401u, 0x7e120202u,
    0x34101286u, 0x4a101100u, 0x34101084u, 0xe0781000u, 0x80070408u, 0xbf810000u,
};

alignas(256) constexpr std::array<std::uint32_t, 23> ReadTableCode{
    0xf4080100u, 0xfa000000u, 0xf4080200u, 0xfa000010u, 0xf4080700u, 0xfa000020u, 0x8f108202u, 0xf4200444u,
    0x20000000u, 0x9312b011u, 0xf42c0802u, 0x24000000u, 0x360200bfu, 0x7e040280u, 0xf0008f08u, 0x00090401u,
    0x7e120202u, 0x34101286u, 0x4a101100u, 0x34101084u, 0xe0781000u, 0x80070408u, 0xbf810000u,
};

alignas(256) constexpr std::array<std::uint32_t, 25> ReadTableCutCode{
    0xf4080100u, 0xfa000000u, 0xf4080200u, 0xfa000010u, 0xf4080700u, 0xfa000020u, 0x8f108202u, 0xf4200444u,
    0x20000000u, 0x9312b011u, 0xf42c0802u, 0x24000000u, 0x360200bfu, 0x7e040280u, 0x7da400ffu, 0x00001000u,
    0xf0008f08u, 0x00090401u, 0x7e120202u, 0x34101286u, 0x4a101100u, 0x34101084u, 0xe0781000u, 0x80070408u,
    0xbf810000u,
};

alignas(256) constexpr std::array<std::uint32_t, 23> ReadTableD16Code{
    0xf4080100u, 0xfa000000u, 0xf4080200u, 0xfa000010u, 0xf4080700u, 0xfa000020u, 0x8f108202u, 0xf4200444u,
    0x20000000u, 0x9312b011u, 0xf42c0802u, 0x24000000u, 0x360200bfu, 0x7e040280u, 0xf000a388u, 0x80090401u,
    0x7e120202u, 0x34101286u, 0x4a101100u, 0x34101084u, 0xe0701000u, 0x80070408u, 0xbf810000u,
};

alignas(256) constexpr std::array<std::uint32_t, 27> ReadTableWave32Code{
    0xf4080100u, 0xfa000000u, 0xf4080200u, 0xfa000010u, 0xf4080700u, 0xfa000020u, 0x7e200500u, 0x90108510u,
    0x8f118202u, 0x81101110u, 0x8f108210u, 0xf4200444u, 0x20000000u, 0x9312b011u, 0xf42c0802u, 0x24000000u,
    0x360200bfu, 0x7e040280u, 0xf0008f08u, 0x00090401u, 0x7e120202u, 0x34101287u, 0x4a101100u, 0x34101084u,
    0xe0781000u, 0x80070408u, 0xbf810000u,
};

alignas(256) constexpr std::array<std::uint32_t, 23> ReadTable256Code{
    0xf4080100u, 0xfa000000u, 0xf4080200u, 0xfa000010u, 0xf4080700u, 0xfa000020u, 0x8f108202u, 0xf4200444u,
    0x20000000u, 0x9312b011u, 0xf42c0802u, 0x24000000u, 0x360200bfu, 0x7e040280u, 0xf0000f08u, 0x00080401u,
    0x7e120202u, 0x34101286u, 0x4a101100u, 0x34101084u, 0xe0781000u, 0x80070408u, 0xbf810000u,
};

alignas(256) constexpr std::array<std::uint32_t, 17> ReadDirectCode{
    0xf4080700u, 0xfa000020u, 0xf4080800u, 0xfa000030u, 0xf4080900u, 0xfa000040u, 0x360200bfu, 0x7e040280u,
    0xf0008f08u, 0x00090401u, 0x7e120202u, 0x34101286u, 0x4a101100u, 0x34101084u, 0xe0781000u, 0x80070408u,
    0xbf810000u,
};

alignas(256) constexpr std::array<std::uint32_t, 17> ReadDirectD16Code{
    0xf4080700u, 0xfa000020u, 0xf4080800u, 0xfa000030u, 0xf4080900u, 0xfa000040u, 0x360200bfu, 0x7e040280u,
    0xf000a388u, 0x80090401u, 0x7e120202u, 0x34101286u, 0x4a101100u, 0x34101084u, 0xe0701000u, 0x80070408u,
    0xbf810000u,
};

alignas(256) constexpr std::array<std::uint32_t, 17> ReadDirect256Code{
    0xf4080700u, 0xfa000020u, 0xf4080800u, 0xfa000030u, 0xf40c0900u, 0xfa000040u, 0x360200bfu, 0x7e040280u,
    0xf0000f08u, 0x00090401u, 0x7e120202u, 0x34101286u, 0x4a101100u, 0x34101084u, 0xe0781000u, 0x80070408u,
    0xbf810000u,
};

alignas(256) constexpr std::array<std::uint32_t, 15> StoreTableCode{
    0xf4080100u, 0xfa000000u, 0xf4080200u, 0xfa000010u, 0x8f108202u, 0xf4200444u, 0x20000000u, 0x9312b011u,
    0xf42c0802u, 0x24000000u, 0x360200bfu, 0x7e040280u, 0xf0208f08u, 0x00090401u, 0xbf810000u,
};

alignas(256) constexpr std::array<std::uint32_t, 17> StoreTableCutCode{
    0xf4080100u, 0xfa000000u, 0xf4080200u, 0xfa000010u, 0x8f108202u, 0xf4200444u, 0x20000000u, 0x9312b011u,
    0xf42c0802u, 0x24000000u, 0x360200bfu, 0x7e040280u, 0x7da400ffu, 0x00001000u, 0xf0208f08u, 0x00090401u,
    0xbf810000u,
};

alignas(256) constexpr std::array<std::uint32_t, 30> PointerCode{
    0xf4080200u, 0xfa000000u, 0xf4080700u, 0xfa000010u, 0xf4080800u, 0xfa000020u, 0x8f108202u, 0xf4200444u,
    0x20000000u, 0x8711ff11u, 0x000000ffu, 0x8f128511u, 0xf40c0900u, 0x24000040u, 0x360200bfu, 0x7e020d01u,
    0x060202f0u, 0x100202ffu, 0x3c800000u, 0x7e0402ffu, 0x3e000000u, 0xf09c0f08u, 0x01090401u, 0x7e120202u,
    0x34101286u, 0x4a101100u, 0x34101084u, 0xe0781000u, 0x80070408u, 0xbf810000u,
};

alignas(256) constexpr std::array<std::uint32_t, 32> LoadedPointerCode{
    0xf4080200u, 0xfa000000u, 0xf4080700u, 0xfa000010u, 0xf4080800u, 0xfa000020u, 0xf4040500u, 0xfa000030u,
    0x8f108202u, 0xf4200444u, 0x20000000u, 0x8711ff11u, 0x000000ffu, 0x8f128511u, 0xf40c090au, 0x241fff00u,
    0x360200bfu, 0x7e020d01u, 0x060202f0u, 0x100202ffu, 0x3c800000u, 0x7e0402ffu, 0x3e000000u, 0xf09c0f08u,
    0x01090401u, 0x7e120202u, 0x34101286u, 0x4a101100u, 0x34101084u, 0xe0781000u, 0x80070408u, 0xbf810000u,
};

alignas(256) constexpr std::array<std::uint32_t, 38> PointerStorageWriteCode{
    0xf4080200u, 0xfa000000u, 0xf4080700u, 0xfa000010u, 0xf4080800u, 0xfa000020u, 0xf4040500u, 0xfa000030u,
    0x8f108202u, 0xf4200444u, 0x20000000u, 0x8711ff11u, 0x000000ffu, 0x8f128511u, 0xf40c090au, 0x241fff00u,
    0x360200bfu, 0x7e020d01u, 0x060202f0u, 0x100202ffu, 0x3c800000u, 0x7e0402ffu, 0x3e000000u, 0xf09c0f08u,
    0x01090401u, 0x7e120202u, 0x34101286u, 0x4a101100u, 0x34101084u, 0xe0781000u, 0x80070408u,
    0xf40c0b00u, 0xfa000040u, 0x7e3c0300u, 0x7e3e0280u, 0xf0201f08u, 0x000b041eu, 0xbf810000u,
};

alignas(256) constexpr std::array<std::uint32_t, 52> PointerStorageIsolationCode{
    0xf4080200u, 0xfa000000u, 0xf4080700u, 0xfa000010u, 0xf4080800u, 0xfa000020u, 0xf4040500u, 0xfa000030u,
    0x8f108202u, 0xf4200444u, 0x20000000u, 0x8711ff11u, 0x000000ffu, 0x8f128511u, 0xf40c090au, 0x241fff00u,
    0x360200bfu, 0x7e020d01u, 0x060202f0u, 0x100202ffu, 0x3c800000u, 0x7e0402ffu, 0x3e000000u,
    0x7e120202u, 0x34101286u, 0x4a101100u, 0x34101084u, 0xf4000600u, 0xfa000060u, 0x7da80018u,
    0xf09c0f08u, 0x01090401u, 0xe0781000u, 0x80070408u, 0xbefe04c1u,
    0xf4040500u, 0xfa000038u, 0xf40c090au, 0x241fff00u, 0x7da60018u,
    0xf09c0f08u, 0x01090401u, 0xe0781000u, 0x80070408u, 0xbefe04c1u,
    0xf40c0b00u, 0xfa000040u, 0x7e3c0300u, 0x7e3e0280u, 0xf0201f08u, 0x000b041eu, 0xbf810000u,
};

alignas(256) constexpr std::array<std::uint32_t, 28> UnboundedPointerCode{
    0xf4080200u, 0xfa000000u, 0xf4080700u, 0xfa000010u, 0xf4080800u, 0xfa000020u, 0x8f108202u, 0xf4200444u,
    0x20000000u, 0x8f128511u, 0xf40c0900u, 0x24000040u, 0x360200bfu, 0x7e020d01u, 0x060202f0u, 0x100202ffu,
    0x3c800000u, 0x7e0402ffu, 0x3e000000u, 0xf09c0f08u, 0x01090401u, 0x7e120202u, 0x34101286u, 0x4a101100u,
    0x34101084u, 0xe0781000u, 0x80070408u, 0xbf810000u,
};

alignas(256) constexpr std::array<std::uint32_t, 25> ReadPointerCode{
    0xf4080200u, 0xfa000000u, 0xf4080700u, 0xfa000010u, 0xf4080800u, 0xfa000020u, 0x8f108202u, 0xf4200444u,
    0x20000000u, 0x8711ff11u, 0x000000ffu, 0x8f128511u, 0xf40c0900u, 0x24000040u, 0x360200bfu, 0x7e040280u,
    0xf0000f08u, 0x00090401u, 0x7e120202u, 0x34101286u, 0x4a101100u, 0x34101084u, 0xe0781000u, 0x80070408u,
    0xbf810000u,
};

constexpr std::array<std::uint32_t, 4> PointClamp{0x92u, (4u * 256u) << 12u, 0u, 0u};
constexpr std::array<std::uint32_t, 4> LinearWrap{0u, (4u * 256u) << 12u, (1u << 20u) | (1u << 22u), 0u};
constexpr std::array<std::uint32_t, 4> PointWrap{0u, (4u * 256u) << 12u, 1u << 24u, 0u};

std::array<std::uint32_t, 4> Image(const void* texels, std::uint32_t format, std::uint32_t type = Type2D, std::uint32_t swizzleMode = 0u) {
    const auto address = static_cast<std::uint64_t>(reinterpret_cast<std::uintptr_t>(texels));
    return {static_cast<std::uint32_t>(address >> 8u), static_cast<std::uint32_t>((address >> 40u) & 0xffu) | (format << 20u) | (((Width - 1u) & 3u) << 30u), ((Width - 1u) >> 2u) | ((Height - 1u) << 14u) | (1u << 31u), 0xfacu | (swizzleMode << 20u) | (type << 28u)};
}

std::array<std::uint32_t, 4> Buffer(const void* base, std::uint32_t bytes) {
    const auto address = static_cast<std::uint64_t>(reinterpret_cast<std::uintptr_t>(base));
    return {static_cast<std::uint32_t>(address), static_cast<std::uint32_t>((address >> 32u) & 0xffffu), bytes, 0x31016facu};
}

void SetRecord(std::uint32_t record, const std::array<std::uint32_t, 4>& sampler, const std::array<std::uint32_t, 4>& image) {
    auto* words = Table.data() + record * (Stride / 4u);
    std::fill(words, words + Stride / 4u, 0u);
    std::copy(sampler.begin(), sampler.end(), words);
    std::copy(image.begin(), image.end(), words + 4);
}

void SetWideRecord(std::uint32_t record, const std::array<std::uint32_t, 4>& image) {
    auto* words = Table.data() + record * (Stride / 4u);
    std::fill(words, words + Stride / 4u, 0u);
    std::copy(image.begin(), image.end(), words);
}

std::uint32_t Half(float value) {
    const auto bits = std::bit_cast<std::uint32_t>(value);
    return (((bits >> 23u) - 112u) << 10u) | ((bits >> 13u) & 0x3ffu);
}

std::string Hex(std::uint32_t value) {
    char text[16];
    std::snprintf(text, sizeof(text), "0x%08x", value);
    return text;
}

class StderrCapture {
public:
    StderrCapture() {
        std::fflush(stderr);
        file = std::tmpfile();
        Require(file != nullptr, "cannot open a temporary file for stderr");
#ifdef _WIN32
        saved = _dup(_fileno(stderr));
        _dup2(_fileno(file), _fileno(stderr));
#else
        saved = dup(fileno(stderr));
        dup2(fileno(file), fileno(stderr));
#endif
    }
    std::string Finish() {
        std::fflush(stderr);
#ifdef _WIN32
        _dup2(saved, _fileno(stderr));
        _close(saved);
#else
        dup2(saved, fileno(stderr));
        close(saved);
#endif
        std::rewind(file);
        std::string text;
        char buffer[4096];
        for (std::size_t read = 0; (read = std::fread(buffer, 1, sizeof(buffer), file)) != 0;) text.append(buffer, read);
        std::fclose(file);
        std::fwrite(text.data(), 1, text.size(), stderr);
        return text;
    }

private:
    std::FILE* file = nullptr;
    int saved = -1;
};

struct Outcome {
    std::vector<std::uint32_t> words;
    std::string log;
    std::uint64_t variantId = 0;
    std::uint64_t artifactId = 0;
    std::uint32_t imageTableCount = 0;
    std::uint32_t imageTableFaults = 0;
    std::uint32_t poisonedSrtReads = 0;
    bool cacheable = false;
    std::vector<std::array<std::uint32_t, 4>> samplers;
    std::vector<std::array<std::uint32_t, 4>> samplerTable;
    std::vector<std::vector<std::pair<std::uint64_t, std::uint64_t>>> imageTableReadRanges;
};

std::vector<std::array<std::uint32_t, 4>> SamplerWords(const std::vector<std::uint32_t>& words) {
    std::vector<std::array<std::uint32_t, 4>> result;
    for (std::size_t first = 0; first + 4u <= words.size(); first += 4u) result.push_back({words[first], words[first + 1u], words[first + 2u], words[first + 3u]});
    return result;
}

Outcome Execute(AgcDriver::VulkanDevice& device, std::span<const std::uint32_t> span, const void* root, std::span<const ShaderRecompiler::MemoryRegion> memory, std::uint32_t groups, std::uint32_t threads, std::uint32_t waveSize) {
    const auto rootAddress = reinterpret_cast<std::uintptr_t>(root);
    const std::array<std::uint32_t, 2> userData{static_cast<std::uint32_t>(rootAddress), static_cast<std::uint32_t>(rootAddress >> 32u)};
    std::fill(Output.begin(), Output.end(), 0xdeadbeefu);
    const ShaderRecompiler::ShaderComputeStageInfo compute{{threads, 1, 1}, 0u, {true, false, false}, false, 1};
    ShaderRecompiler::RecompileRequest request{
        {ShaderStage::Compute, reinterpret_cast<std::uintptr_t>(span.data()), span, 0, {}},
        {waveSize, 0, userData, compute, std::nullopt, std::nullopt, memory},
        device.Target(),
        {0, 0, 0, 128}
    };
    const auto result = ShaderRecompiler::Recompile(request);
    Outcome outcome;
    outcome.variantId = result.PipelineVariantId();
    outcome.artifactId = result.variantId;
    outcome.imageTableFaults = result.imageTableFaults;
    outcome.poisonedSrtReads = result.poisonedSrtReads;
    outcome.cacheable = AgcDriver::DriverDetail::CacheableResult(result);
    outcome.imageTableReadRanges = result.imageTableReadRanges;
    for (const auto& binding : result.bindings) {
        if (binding.role == ShaderRecompiler::DescriptorRole::ImageTable) outcome.imageTableCount = binding.count;
        if (binding.role == ShaderRecompiler::DescriptorRole::GuestSamplers) outcome.samplers = SamplerWords(binding.guestDescriptor);
        if (binding.role == ShaderRecompiler::DescriptorRole::SamplerTable) outcome.samplerTable = SamplerWords(binding.guestDescriptor);
    }
    StderrCapture capture;
    try {
        device.Dispatch(result, groups, 1, 1, {}, reinterpret_cast<std::uintptr_t>(span.data()));
        device.WaitIdle();
    } catch (...) {
        static_cast<void>(capture.Finish());
        throw;
    }
    outcome.log = capture.Finish();
    outcome.words.assign(Output.begin(), Output.begin() + groups * threads * 4u);
    return outcome;
}

template <std::size_t CodeWords>
Outcome Run(AgcDriver::VulkanDevice& device, const std::array<std::uint32_t, CodeWords>& code, std::uint32_t groups, std::uint32_t threads, std::uint32_t waveSize, bool outputIntoTable = false) {
    const auto table = Buffer(Table.data(), static_cast<std::uint32_t>(sizeof(Table)));
    const auto keys = Buffer(Keys.data(), static_cast<std::uint32_t>(sizeof(Keys)));
    const auto output = outputIntoTable ? Buffer(Table.data(), static_cast<std::uint32_t>(sizeof(Table))) : Buffer(Output.data(), static_cast<std::uint32_t>(sizeof(Output)));
    std::copy(table.begin(), table.end(), Srt.begin());
    std::copy(keys.begin(), keys.end(), Srt.begin() + 4);
    std::copy(output.begin(), output.end(), Srt.begin() + 8);
    const std::span<const std::uint32_t> span(code);
    const std::array<ShaderRecompiler::MemoryRegion, 4> memory{{
        {reinterpret_cast<std::uintptr_t>(span.data()), std::as_bytes(span)},
        {reinterpret_cast<std::uintptr_t>(Srt.data()), std::as_bytes(std::span(Srt))},
        {reinterpret_cast<std::uintptr_t>(Table.data()), std::as_bytes(std::span(Table))},
        {reinterpret_cast<std::uintptr_t>(Keys.data()), std::as_bytes(std::span(Keys))},
    }};
    return Execute(device, span, Srt.data(), memory, groups, threads, waveSize);
}

template <std::size_t CodeWords>
Outcome RunPointer(AgcDriver::VulkanDevice& device, const std::array<std::uint32_t, CodeWords>& code, std::uint32_t groups) {
    auto* root = Root.data();
    const auto palette = static_cast<std::uint64_t>(reinterpret_cast<std::uintptr_t>(root)) + PaletteOffset + 0x100u;
    const auto keys = Buffer(Keys.data(), static_cast<std::uint32_t>(sizeof(Keys)));
    const auto output = Buffer(Output.data(), static_cast<std::uint32_t>(sizeof(Output)));
    std::copy(keys.begin(), keys.end(), root);
    std::copy(output.begin(), output.end(), root + 4);
    std::copy(PointClamp.begin(), PointClamp.end(), root + 8);
    root[12] = static_cast<std::uint32_t>(palette);
    root[13] = static_cast<std::uint32_t>(palette >> 32u);
    const std::span<const std::uint32_t> span(code);
    const std::array<ShaderRecompiler::MemoryRegion, 3> memory{{
        {reinterpret_cast<std::uintptr_t>(span.data()), std::as_bytes(span)},
        {reinterpret_cast<std::uintptr_t>(root), std::as_bytes(std::span(root, PaletteBytes / 4u))},
        {reinterpret_cast<std::uintptr_t>(Keys.data()), std::as_bytes(std::span(Keys))},
    }};
    return Execute(device, span, root, memory, groups, Lanes, 64);
}

void SetEntry(std::uint32_t entry, const std::array<std::uint32_t, 4>& image) {
    auto* words = Root.data() + (PaletteOffset + entry * 32u) / 4u;
    std::fill(words, words + 8, 0u);
    std::copy(image.begin(), image.end(), words);
}

template <std::size_t CodeWords>
Outcome RunDirectCode(AgcDriver::VulkanDevice& device, const std::array<std::uint32_t, CodeWords>& code, const std::array<std::uint32_t, 4>& sampler, std::span<const std::uint32_t> image) {
    std::copy(sampler.begin(), sampler.end(), Srt.begin() + 12);
    std::fill(Srt.begin() + 16, Srt.begin() + 24, 0u);
    std::copy(image.begin(), image.end(), Srt.begin() + 16);
    return Run(device, code, 1, Lanes, 64);
}

Outcome RunDirect(AgcDriver::VulkanDevice& device, const std::array<std::uint32_t, 4>& sampler, const std::array<std::uint32_t, 4>& image) {
    return RunDirectCode(device, DirectCode, sampler, image);
}

std::vector<std::uint32_t> Group(const Outcome& outcome, std::uint32_t group, std::uint32_t threads = Lanes) {
    return std::vector<std::uint32_t>(outcome.words.begin() + group * threads * 4u, outcome.words.begin() + (group + 1u) * threads * 4u);
}

bool Faulted(const Outcome& outcome) {
    return outcome.log.find("image table:") != std::string::npos;
}

bool Holds(const std::vector<std::array<std::uint32_t, 4>>& samplers, const std::array<std::uint32_t, 4>& sampler) {
    return std::find(samplers.begin(), samplers.end(), sampler) != samplers.end();
}

bool TablePointCopy(const Outcome& outcome, const std::array<std::uint32_t, 4>& sampler, const std::array<std::uint32_t, 4>& point) {
    bool found = false;
    for (std::size_t entry = 0; entry + ShaderRecompiler::ImageTableAbi::SamplerEntryElements <= outcome.samplerTable.size(); entry += ShaderRecompiler::ImageTableAbi::SamplerEntryElements) {
        if (outcome.samplerTable[entry] != sampler) continue;
        found = true;
        if (outcome.samplerTable[entry + 1u] != point) return false;
    }
    return found;
}

void Fill() {
    for (std::uint32_t y = 0; y < Height; ++y) {
        for (std::uint32_t x = 0; x < Width; ++x) {
            const auto index = y * Width + x;
            FloatTexels[index] = static_cast<float>(x) + 100.0f * static_cast<float>(y) + 0.25f;
            OtherFloatTexels[index] = -static_cast<float>(x) - 0.5f;
            UintTexels[index] = 0x1000u + x + 0x100u * y;
            SintTexels[index] = -3 * static_cast<std::int32_t>(x + 1u) - 1000 * static_cast<std::int32_t>(y);
        }
    }
    for (std::uint32_t index = 0; index < DepthStorage.size(); ++index) DepthStorage[index] = 0x3f000000u + index * 0x1234u;
}

void RequireRed(const Outcome& outcome, std::uint32_t group, std::uint32_t threads, const std::function<std::uint32_t(std::uint32_t)>& expected, const std::string& what) {
    for (std::uint32_t lane = 0; lane < threads; ++lane) {
        const auto actual = outcome.words[(group * threads + lane) * 4u];
        Require(actual == expected(lane % Width), what + ": lane " + std::to_string(lane) + " is " + Hex(actual) + ", expected " + Hex(expected(lane % Width)));
    }
}

bool Zeros(const Outcome& outcome) {
    return std::all_of(outcome.words.begin(), outcome.words.end(), [](std::uint32_t word) { return word == 0u || word == 0xdeadbeefu; });
}

template <std::size_t CodeWords>
void RunWaveKeys(AgcDriver::VulkanDevice& device, const std::array<std::uint32_t, CodeWords>& code, const std::string& what) {
    if (device.Target().subgroupSize != 32u) {
        std::printf("%s: per-wave key case skipped, subgroup size %u does not hold exactly one wave32\n", what.c_str(), device.Target().subgroupSize);
        return;
    }
    for (std::uint32_t wave = 0; wave < 8; ++wave) Keys[wave] = wave % 3u;
    const auto waves = Run(device, code, 2, 128, 32);
    Require(!Faulted(waves), what + ": per-wave keys faulted:\n" + waves.log);
    for (std::uint32_t group = 0; group < 2; ++group) {
        for (std::uint32_t wave = 0; wave < 4; ++wave) {
            const auto key = (group * 4u + wave) % 3u;
            for (std::uint32_t lane = 0; lane < 32; ++lane) {
                const auto thread = wave * 32u + lane;
                const auto actual = waves.words[(group * 128u + thread) * 4u];
                const auto x = thread % Width;
                const auto expected = key == 0u ? std::bit_cast<std::uint32_t>(FloatTexels[x]) : key == 1u ? UintTexels[x] : static_cast<std::uint32_t>(SintTexels[x]);
                Require(actual == expected, what + ": wave " + std::to_string(wave) + " of group " + std::to_string(group) + " read the wrong entry at lane " + std::to_string(lane));
            }
        }
    }
}

void SetClassRecords() {
    const auto floatImage = Image(FloatTexels.data(), Format32Float);
    const auto uintImage = Image(UintTexels.data(), Format32UInt);
    const auto sintImage = Image(SintTexels.data(), Format32SInt);
    SetRecord(0, PointClamp, floatImage);
    SetRecord(1, PointClamp, uintImage);
    SetRecord(2, PointClamp, sintImage);
    SetRecord(3, LinearWrap, floatImage);
    SetRecord(4, LinearWrap, uintImage);
    SetRecord(5, LinearWrap, sintImage);
    SetRecord(6, {0u, 0u, 0u, 0u}, {0u, 0u, 0u, 0u});
    SetRecord(7, PointClamp, {0x1234u, 0x5678u, 0x9abcu, 0x0000ffacu});
    for (std::uint32_t record = 8; record < Records; ++record) SetRecord(record, PointClamp, floatImage);
}

void RunBorderSwizzleTests(AgcDriver::VulkanDevice& device) {
    constexpr std::array<std::uint32_t, 4> opaqueBlack{0x1b6u, (4u * 256u) << 12u, 0u, 1u << 30u};
    Fill();
    Table = {};
    Keys = {};
    Srt = {};
    const auto floatImage = Image(FloatTexels.data(), Format32Float);
    auto alphaImage = floatImage;
    alphaImage[3] |= 1u << 25u;
    const auto alphaControl = RunDirect(device, PointClamp, alphaImage);
    Require(!Faulted(alphaControl), "image table border: an alpha-moving BC swizzle without an opaque black border faulted");
    RequireRed(alphaControl, 0, Lanes, [](std::uint32_t x) { return std::bit_cast<std::uint32_t>(FloatTexels[x]); }, "alpha-moving BC swizzle without a border");
    std::string refusal;
    try {
        static_cast<void>(RunDirect(device, opaqueBlack, alphaImage));
    } catch (const std::exception& error) {
        refusal = error.what();
    }
    Require(refusal.find("moves the alpha channel is sampled through an opaque black border") != std::string::npos, "image table border: a direct T#/S# alpha-moving opaque-black pair was not refused: " + refusal);
    const auto requireFault = [](const Outcome& outcome, const std::string& reason, const std::string& what) {
        Require(Faulted(outcome) && outcome.log.find(reason) != std::string::npos, what + " did not report its entry rejection:\n" + outcome.log);
        if (!Zeros(outcome)) {
            const auto unexpected = std::find_if(outcome.words.begin(), outcome.words.end(), [](std::uint32_t word) { return word != 0u && word != 0xdeadbeefu; });
            Require(false, what + ": faulted output word " + std::to_string(unexpected - outcome.words.begin()) + " is " + Hex(*unexpected) + ", expected zero or untouched 0xdeadbeef");
        }
    };
    for (const auto bcSwizzle : {0u, 4u}) {
        auto image = floatImage;
        image[3] |= bcSwizzle << 25u;
        const auto name = "image table border BC" + std::to_string(bcSwizzle);
        const auto control = RunDirect(device, PointClamp, image);
        const auto directOpaque = RunDirect(device, opaqueBlack, image);
        Require(!Faulted(directOpaque) && Group(directOpaque, 0) == Group(control, 0), name + ": an allowed direct T#/S# opaque-black pair changed the result");
        RequireRed(directOpaque, 0, Lanes, [](std::uint32_t x) { return std::bit_cast<std::uint32_t>(FloatTexels[x]); }, name + " direct opaque-black texels");

        SetRecord(0, PointClamp, image);
        SetRecord(1, PointClamp, alphaImage);
        Keys[0] = 0;
        Keys[1] = 1;
        const auto inactiveImage = RunDirectCode(device, TableDirectSamplerCode, opaqueBlack, image);
        Require(inactiveImage.imageTableCount == 2u, name + ": the inactive alpha-moving T# was not included in the snapshot");
        Require(!Faulted(inactiveImage) && Group(inactiveImage, 0) == Group(directOpaque, 0), name + ": an inactive rejected table T# poisoned the active T#/direct S# pair:\n" + inactiveImage.log);
        Keys[0] = 1;
        Keys[1] = 0;
        const auto activeImage = RunDirectCode(device, TableDirectSamplerCode, opaqueBlack, image);
        requireFault(activeImage, "moves the alpha channel is sampled through an opaque black border", name + " table T#/direct S#");

        SetRecord(0, PointClamp, image);
        SetRecord(1, opaqueBlack, image);
        for (const bool tableImage : {false, true}) {
            const auto pair = name + (tableImage ? " table T#/table S#" : " direct T#/table S#");
            Keys[0] = 0;
            Keys[1] = 1;
            const auto inactiveSampler = tableImage ? RunDirectCode(device, TableCode, PointClamp, image) : RunDirectCode(device, TableSamplerCode, PointClamp, image);
            Require(Holds(inactiveSampler.samplerTable, opaqueBlack), pair + ": the inactive opaque-black S# was not included in the snapshot");
            Require(!Faulted(inactiveSampler) && Group(inactiveSampler, 0) == Group(control, 0), pair + ": an inactive rejected S# poisoned the active pair:\n" + inactiveSampler.log);
            Keys[0] = 1;
            Keys[1] = 0;
            const auto activeSampler = tableImage ? RunDirectCode(device, TableCode, PointClamp, image) : RunDirectCode(device, TableSamplerCode, PointClamp, image);
            requireFault(activeSampler, "sampler table entry with an opaque black border", pair);
        }
    }
    const auto cut = Run(device, TableCutCode, 1, Lanes, 64);
    Require(Holds(cut.samplerTable, opaqueBlack), "image table border: the EXEC-clear opaque-black S# was not included in the snapshot");
    Require(!Faulted(cut), "image table border: an opaque-black S# selected with EXEC clear faulted:\n" + cut.log);
    Require(std::all_of(cut.words.begin(), cut.words.end(), [](std::uint32_t word) { return word == 0xdeadbeefu; }), "image table border: the EXEC-clear shader wrote output");
}

void RunAnisoOverrideTests(AgcDriver::VulkanDevice& device) {
    constexpr std::uint32_t overrideBit = 1u << 29u;
    constexpr std::uint32_t anisoFilters = (2u << 20u) | (2u << 22u);
    Fill();
    Table = {};
    Keys = {};
    Srt = {};
    const auto image = Image(FloatTexels.data(), Format32Float);
    const auto control = RunDirect(device, PointClamp, image);
    const auto requireFault = [](const Outcome& outcome, const std::string& what) {
        Require(Faulted(outcome) && outcome.log.find("anisotropic ANISO_OVERRIDE needs dynamic texture mip-level pairing") != std::string::npos, what + " did not report its entry rejection:\n" + outcome.log);
        Require(Zeros(outcome), what + " returned values other than zero or the untouched sentinel");
    };
    for (const auto filters : {overrideBit, anisoFilters, overrideBit | anisoFilters}) {
        auto sampler = PointClamp;
        sampler[0] |= 2u << 9u;
        sampler[2] |= filters;
        const bool rejected = filters == (overrideBit | anisoFilters);
        const auto direct = RunDirect(device, sampler, image);
        Require(!Faulted(direct) && Group(direct, 0) == Group(control, 0), "image table ANISO_OVERRIDE: a direct pair changed its texels");
        SetRecord(0, PointClamp, image);
        SetRecord(1, sampler, image);
        Keys[0] = 0;
        Keys[1] = 1;
        const auto tableImage = RunDirectCode(device, TableDirectSamplerCode, sampler, image);
        if (rejected) requireFault(tableImage, "image table ANISO_OVERRIDE table T#/direct S#");
        else Require(!Faulted(tableImage) && Group(tableImage, 0) == Group(direct, 0), "image table ANISO_OVERRIDE: an unaffected direct sampler changed its table result");
        for (const bool tableImages : {false, true}) {
            const auto pair = std::string("image table ANISO_OVERRIDE ") + (tableImages ? "table T#/table S#" : "direct T#/table S#");
            Keys[0] = 0;
            Keys[1] = 1;
            const auto inactive = tableImages ? RunDirectCode(device, TableCode, PointClamp, image) : RunDirectCode(device, TableSamplerCode, PointClamp, image);
            Require(Holds(inactive.samplerTable, sampler), pair + ": the inactive sampler was not captured");
            Require(!Faulted(inactive) && Group(inactive, 0) == Group(control, 0), pair + ": the inactive entry changed the selected texels");
            Keys[0] = 1;
            Keys[1] = 0;
            const auto active = tableImages ? RunDirectCode(device, TableCode, PointClamp, image) : RunDirectCode(device, TableSamplerCode, PointClamp, image);
            if (rejected) requireFault(active, pair);
            else Require(!Faulted(active) && Group(active, 0) == Group(direct, 0), pair + ": an unaffected sampler changed its texels");
        }
        if (rejected) {
            alignas(256) std::array<std::uint32_t, TableDirectSamplerCode.size() + 2u> cutCode{};
            std::copy_n(TableDirectSamplerCode.begin(), 21u, cutCode.begin());
            cutCode[21] = 0x7da400ffu;
            cutCode[22] = 0x00001000u;
            std::copy(TableDirectSamplerCode.begin() + 21u, TableDirectSamplerCode.end(), cutCode.begin() + 23u);
            const auto cut = RunDirectCode(device, cutCode, sampler, image);
            Require(cut.imageTableCount == 1u && !Faulted(cut), "image table ANISO_OVERRIDE: an EXEC-clear table T#/direct S# access faulted");
            Require(std::all_of(cut.words.begin(), cut.words.end(), [](std::uint32_t word) { return word == 0xdeadbeefu; }), "image table ANISO_OVERRIDE: EXEC-clear sampling wrote output");
        }
    }
}

void RunTests(AgcDriver::VulkanDevice& device) {
    Fill();
    const auto floatImage = Image(FloatTexels.data(), Format32Float);
    const auto uintImage = Image(UintTexels.data(), Format32UInt);
    const auto sintImage = Image(SintTexels.data(), Format32SInt);
    SetClassRecords();

    Keys = {};
    for (std::uint32_t group = 0; group < 6; ++group) Keys[group] = group;
    const auto classes = Run(device, TableCode, 6, Lanes, 64);
    Require(!Faulted(classes), "image table: valid entries faulted:\n" + classes.log);
    Require(classes.imageTableCount == 3u, "image table: the table binding does not hold one element per distinct T# (" + std::to_string(classes.imageTableCount) + ")");
    for (std::uint32_t group = 0; group < 6; ++group) {
        if (group == 4u) continue;
        const auto* words = Table.data() + group * (Stride / 4u);
        const auto control = RunDirect(device, {words[0], words[1], words[2], words[3]}, {words[4], words[5], words[6], words[7]});
        Require(Group(classes, group) == Group(control, 0), "image table: record " + std::to_string(group) + " differs from the direct binding of the same words");
    }
    RequireRed(classes, 0, Lanes, [](std::uint32_t x) { return std::bit_cast<std::uint32_t>(FloatTexels[x]); }, "float texels");
    RequireRed(classes, 1, Lanes, [](std::uint32_t x) { return UintTexels[x]; }, "uint texels");
    RequireRed(classes, 2, Lanes, [](std::uint32_t x) { return static_cast<std::uint32_t>(SintTexels[x]); }, "sint texels");
    RequireRed(classes, 4, Lanes, [](std::uint32_t x) { return UintTexels[x]; }, "uint texels through a linear sampler");

    Keys[0] = 6;
    Keys[1] = 1000;
    Keys[2] = 0x10000000u;
    const auto zeros = Run(device, TableCode, 3, Lanes, 64);
    Require(!Faulted(zeros), "image table: a null entry or an out-of-range key faulted:\n" + zeros.log);
    for (std::size_t index = 0; index < 2 * Lanes * 4; ++index) {
        Require(zeros.words[index] == 0u, "image table: a null entry or an out-of-range key did not sample zeros (word " + std::to_string(index) + " is " + Hex(zeros.words[index]) + ")");
    }
    Require(Group(zeros, 2) == Group(classes, 0), "image table: a key that wraps onto record 0 did not sample record 0");
    Require(zeros.artifactId == classes.artifactId && zeros.variantId == classes.variantId, "image table: other keys changed the module");

    Keys[0] = 0x05555556u;
    const auto wrapped = Run(device, TableCode, 1, Lanes, 64);
    Require(Faulted(wrapped) && wrapped.log.find("outside the snapshot") != std::string::npos, "image table: a mid-record key did not fault as outside the snapshot:\n" + wrapped.log);

    Keys = {};
    const auto unused = Run(device, TableCode, 1, Lanes, 64);
    Require(!Faulted(unused) && Group(unused, 0) == Group(classes, 0), "image table: an unreferenced invalid entry changed the result");
    Require(unused.imageTableCount == 1u, "image table: a narrowed table bound records its keys cannot select");
    Keys[0] = 7;
    const auto cut = Run(device, TableCutCode, 1, Lanes, 64);
    Require(!Faulted(cut), "image table: an invalid entry selected with EXEC clear faulted:\n" + cut.log);
    const auto selected = Run(device, TableCode, 1, Lanes, 64);
    const auto tableAddress = static_cast<std::uint64_t>(reinterpret_cast<std::uintptr_t>(Table.data())) + 7u * Stride + 16u;
    char address[32];
    std::snprintf(address, sizeof(address), "0x%llx", static_cast<unsigned long long>(tableAddress));
    Require(Faulted(selected) && selected.log.find("not an image") != std::string::npos && selected.log.find(address) != std::string::npos && selected.log.find("00001234") != std::string::npos, "image table: an invalid entry selected with EXEC set did not report its address, words and reason:\n" + selected.log);
    Require(Zeros(selected), "image table: a faulting access did not sample zeros");

    void* reserved = nullptr;
#ifdef _WIN32
    reserved = VirtualAlloc(nullptr, 65536, MEM_RESERVE, PAGE_NOACCESS);
#else
    reserved = mmap(nullptr, 65536, PROT_NONE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (reserved == MAP_FAILED) reserved = nullptr;
#endif
    Require(reserved != nullptr, "cannot reserve an inaccessible range");
    const auto unmappedImage = Image(reserved, Format32Float);
    SetRecord(8, PointClamp, unmappedImage);
    Keys[0] = 0;
    const auto unmappedUnused = Run(device, TableCode, 1, Lanes, 64);
    Require(!Faulted(unmappedUnused) && Group(unmappedUnused, 0) == Group(classes, 0), "image table: an unreferenced unmapped entry changed the result:\n" + unmappedUnused.log);
    Keys[0] = 8;
    const auto unmappedUsed = Run(device, TableCode, 1, Lanes, 64);
    const auto unmappedControl = RunDirect(device, PointClamp, unmappedImage);
    Require(Faulted(unmappedUsed) ? unmappedUsed.log.find("rejected by the driver") != std::string::npos || unmappedUsed.log.find("unmapped") != std::string::npos : Group(unmappedUsed, 0) == Group(unmappedControl, 0), "image table: an unmapped entry neither faulted nor matched the direct binding:\n" + unmappedUsed.log);
    SetRecord(8, PointClamp, floatImage);
#ifdef _WIN32
    VirtualFree(reserved, 0, MEM_RELEASE);
#else
    munmap(reserved, 65536);
#endif

    const auto srgbImage = Image(UintTexels.data(), Format8_8Srgb);
    SetRecord(9, PointClamp, srgbImage);
    Keys[0] = 0;
    const auto srgbUnused = Run(device, TableCode, 1, Lanes, 64);
    Require(!Faulted(srgbUnused) && Group(srgbUnused, 0) == Group(classes, 0), "image table: an unreferenced 8_8_SRGB entry changed the result:\n" + srgbUnused.log);
    Keys[0] = 9;
    const auto srgbUsed = Run(device, TableCode, 1, Lanes, 64);
    Require(Faulted(srgbUsed) && srgbUsed.log.find("sRGB decode") != std::string::npos, "image table: an 8_8_SRGB entry sampled with shader decode did not fault as sRGB decode:\n" + srgbUsed.log);
    Require(Zeros(srgbUsed), "image table: a sampled 8_8_SRGB entry decoded in the shader did not sample zeros");
    std::string refusal;
    try {
        static_cast<void>(RunDirect(device, PointClamp, srgbImage));
    } catch (const std::exception& error) {
        refusal = error.what();
    }
    Require(refusal.find("samples or gathers an sRGB image the device cannot sample") != std::string::npos, "image table: the direct binding of the 8_8_SRGB entry was not refused: " + refusal);
    SetRecord(9, PointClamp, floatImage);

    Keys[0] = 0;
    const auto before = Run(device, TableCode, 1, Lanes, 64);
    SetRecord(0, PointClamp, Image(OtherFloatTexels.data(), Format32Float));
    const auto after = Run(device, TableCode, 1, Lanes, 64);
    Require(after.variantId == before.variantId && after.artifactId == before.artifactId, "image table: other table contents specialized another module");
    RequireRed(after, 0, Lanes, [](std::uint32_t x) { return std::bit_cast<std::uint32_t>(OtherFloatTexels[x]); }, "changed table contents");
    OtherFloatTexels[5] = 1234.5f;
    const auto texels = Run(device, TableCode, 1, Lanes, 64);
    RequireRed(texels, 0, Lanes, [](std::uint32_t x) { return std::bit_cast<std::uint32_t>(OtherFloatTexels[x]); }, "changed texels");
    SetRecord(0, PointClamp, floatImage);

    std::copy(LinearWrap.begin(), LinearWrap.end(), Srt.begin() + 12);
    Keys[0] = 2;
    Keys[1] = 0;
    Keys[2] = 1;
    const auto integerLinear = Run(device, TableDirectSamplerCode, 3, Lanes, 64);
    Require(!Faulted(integerLinear), "image table: a direct sampler with a table T# faulted:\n" + integerLinear.log);
    const auto sintControl = RunDirect(device, LinearWrap, sintImage);
    const auto floatControl = RunDirect(device, LinearWrap, floatImage);
    const auto uintControl = RunDirect(device, LinearWrap, uintImage);
    Require(floatControl.samplers == std::vector{LinearWrap} && uintControl.samplers == std::vector{LinearWrap} && sintControl.samplers == std::vector{PointWrap}, "image table: the direct bindings under a linear S# changed their sampler copies");
    Require(Group(integerLinear, 0) == Group(sintControl, 0) && Group(integerLinear, 1) == Group(floatControl, 0), "image table: an entry under a linear direct S# differs from the direct binding");
    RequireRed(integerLinear, 0, Lanes, [](std::uint32_t x) { return static_cast<std::uint32_t>(SintTexels[x]); }, "sint texels through a linear direct sampler");
    RequireRed(integerLinear, 2, Lanes, [](std::uint32_t x) { return UintTexels[x]; }, "uint texels through a linear direct sampler");

    Keys = {};
    Keys[0] = 1;
    const auto uintLinear = Run(device, TableDirectSamplerCode, 2, Lanes, 64);
    Require(!Faulted(uintLinear), "image table: a direct sampler with float and uint table T#s faulted:\n" + uintLinear.log);
    Require(Holds(uintLinear.samplers, LinearWrap) && Holds(uintLinear.samplers, PointWrap), "image table: a linear direct S# with float and uint table T#s has no point-filtered copy");
    Require(Group(uintLinear, 1) == Group(floatControl, 0), "image table: a float entry under a linear direct S# differs from the direct binding");
    RequireRed(uintLinear, 0, Lanes, [](std::uint32_t x) { return UintTexels[x]; }, "uint texels beside float texels through a linear direct sampler");

    Keys[0] = 4;
    const auto uintTable = Run(device, TableCode, 1, Lanes, 64);
    Require(!Faulted(uintTable), "image table: a table S# with float and uint table T#s faulted:\n" + uintTable.log);
    Require(TablePointCopy(uintTable, LinearWrap, PointWrap), "image table: a linear table S# has no point-filtered copy");
    RequireRed(uintTable, 0, Lanes, [](std::uint32_t x) { return UintTexels[x]; }, "uint texels through a linear table sampler");

    std::copy(uintImage.begin(), uintImage.end(), Srt.begin() + 16);
    Keys[0] = 4;
    Keys[1] = 1;
    const auto tableSampler = Run(device, TableSamplerCode, 2, Lanes, 64);
    Require(!Faulted(tableSampler), "image table: a table sampler with a direct T# faulted:\n" + tableSampler.log);
    Require(TablePointCopy(tableSampler, LinearWrap, PointWrap), "image table: a linear table S# with a direct uint T# has no point-filtered copy");
    RequireRed(tableSampler, 0, Lanes, [](std::uint32_t x) { return UintTexels[x]; }, "direct uint texels through a linear table sampler");
    RequireRed(tableSampler, 1, Lanes, [](std::uint32_t x) { return UintTexels[x]; }, "direct uint texels through a point table sampler");

    RunWaveKeys(device, TableWave32Code, "image table");

    Keys = {};
    const auto aliased = Run(device, TableCode, 1, Lanes, 64, true);
    Require(Faulted(aliased) && aliased.log.find("is written by its own dispatch") != std::string::npos, "image table: a dispatch storing into its own table did not fault on use:\n" + aliased.log);
    SetClassRecords();
}

void RunReadTests(AgcDriver::VulkanDevice& device) {
    SetClassRecords();
    Keys = {};
    const std::array<std::uint32_t, 5> keys{0u, 1u, 2u, 3u, 6u};
    std::copy(keys.begin(), keys.end(), Keys.begin());
    const auto reads = Run(device, ReadTableCode, 5, Lanes, 64);
    Require(!Faulted(reads), "image table read: valid entries faulted:\n" + reads.log);
    for (std::uint32_t group = 0; group < 4; ++group) {
        const auto* words = Table.data() + keys[group] * (Stride / 4u);
        const auto control = RunDirectCode(device, ReadDirectCode, PointClamp, std::span<const std::uint32_t>(words + 4, 4));
        Require(Group(reads, group) == Group(control, 0), "image table read: record " + std::to_string(keys[group]) + " differs from the direct binding of the same words");
    }
    RequireRed(reads, 0, Lanes, [](std::uint32_t x) { return std::bit_cast<std::uint32_t>(FloatTexels[x]); }, "image table read: float texels");
    RequireRed(reads, 1, Lanes, [](std::uint32_t x) { return UintTexels[x]; }, "image table read: uint texels");
    RequireRed(reads, 2, Lanes, [](std::uint32_t x) { return static_cast<std::uint32_t>(SintTexels[x]); }, "image table read: sint texels");
    const auto nullGroup = Group(reads, 4);
    Require(std::all_of(nullGroup.begin(), nullGroup.end(), [](std::uint32_t word) { return word == 0u; }), "image table read: a null entry did not read zeros");

    Keys[0] = 7;
    const auto cut = Run(device, ReadTableCutCode, 1, Lanes, 64);
    Require(!Faulted(cut), "image table read: an invalid entry read with EXEC clear faulted:\n" + cut.log);
    const auto selected = Run(device, ReadTableCode, 1, Lanes, 64);
    Require(Faulted(selected) && selected.log.find("not an image") != std::string::npos && selected.log.find("00001234") != std::string::npos, "image table read: an invalid entry read with EXEC set did not report its words and reason:\n" + selected.log);

    Keys = {};
    Keys[1] = 1u;
    const auto halves = Run(device, ReadTableD16Code, 2, Lanes, 64);
    Require(!Faulted(halves), "image table d16 read: valid entries faulted:\n" + halves.log);
    for (std::uint32_t group = 0; group < 2; ++group) {
        const auto* words = Table.data() + group * (Stride / 4u);
        const auto control = RunDirectCode(device, ReadDirectD16Code, PointClamp, std::span<const std::uint32_t>(words + 4, 4));
        Require(Group(halves, group) == Group(control, 0), "image table d16 read: record " + std::to_string(group) + " differs from the direct binding of the same words");
    }
    RequireRed(halves, 0, Lanes, [](std::uint32_t x) { return Half(FloatTexels[x]); }, "image table d16 read: float texels");
    RequireRed(halves, 1, Lanes, [](std::uint32_t x) { return UintTexels[x] & 0xffffu; }, "image table d16 read: uint texels");

    RunWaveKeys(device, ReadTableWave32Code, "image table read");
}

void RunGuardedTableTests(AgcDriver::VulkanDevice& device) {
    Keys = {};
    const auto image = Image(FloatTexels.data(), Format32Float);
    for (std::uint32_t record = 0; record < Records; ++record) SetRecord(record, PointClamp, image);
    const auto pointer = reinterpret_cast<std::uintptr_t>(&Srt[30]);
    Srt[28] = static_cast<std::uint32_t>(pointer);
    Srt[29] = static_cast<std::uint32_t>(pointer >> 32u);
    Srt[30] = 0x12345678u;
    const auto mapped = Run(device, GuardedTableCode, 1, Lanes, 64);
    Require(mapped.imageTableCount != 0u && mapped.imageTableFaults == 0u && mapped.poisonedSrtReads == 0u && mapped.cacheable && mapped.log.find("BDA access failed") == std::string::npos, "image table guard: a mapped nested read faulted or left the caches:\n" + mapped.log);
    RequireRed(mapped, 0, Lanes, [](std::uint32_t) { return 0x12345678u; }, "image table guard: mapped nested payload");
    Srt[28] = 0u;
    Srt[29] = 0u;
    const auto poisoned = Run(device, GuardedTableCode, 1, Lanes, 64);
    Require(poisoned.imageTableCount != 0u && poisoned.imageTableFaults == 0u && poisoned.poisonedSrtReads == 1u && !poisoned.cacheable, "image table guard: a poisoned nested read was not captured or stayed cacheable");
    Require(poisoned.artifactId == mapped.artifactId && poisoned.variantId == mapped.variantId, "image table guard: a poisoned nested read changed the pipeline");
    Require(poisoned.log.find("BDA access failed") != std::string::npos && poisoned.log.find("address=0x0 instruction=0x24 bytes=4") != std::string::npos, "image table guard: a valid table suppressed the nested read's completion fault:\n" + poisoned.log);
    SetClassRecords();
}

template <std::size_t TableWords, std::size_t DirectWords>
void RequireModeEntry(AgcDriver::VulkanDevice& device, const std::array<std::uint32_t, TableWords>& tableCode, const std::array<std::uint32_t, DirectWords>& directCode, std::uint32_t record, std::span<const std::uint32_t> image, const std::string& what) {
    Keys = {};
    Keys[0] = record;
    const auto table = Run(device, tableCode, 1, Lanes, 64);
    std::string refusal;
    Outcome control;
    try {
        control = RunDirectCode(device, directCode, PointClamp, image);
    } catch (const std::exception& error) {
        refusal = error.what();
    }
    if (!refusal.empty()) {
        Require(Faulted(table) && Zeros(table), what + ": the direct binding was refused (" + refusal + ") but the table entry did not fault:\n" + table.log);
        std::printf("%s: refused directly and faulted through the table: %s\n", what.c_str(), refusal.c_str());
        return;
    }
    Require(!Faulted(table), what + ": the table entry faulted:\n" + table.log);
    Require(Group(table, 0) == Group(control, 0), what + ": the table entry differs from the direct binding of the same words");
}

void RunModeTests(AgcDriver::VulkanDevice& device) {
    const auto rgb = Image(UintTexels.data(), Format11_11_10UInt);
    const auto depthBase = (reinterpret_cast<std::uintptr_t>(DepthStorage.data()) + 0xffffu) & ~std::uintptr_t{0xffffu};
    const auto depth = Image(reinterpret_cast<const void*>(depthBase), Format32UInt, Type2D, DepthSwizzleMode);
    const auto srgb = Image(UintTexels.data(), Format8_8Srgb);
    SetRecord(0, PointClamp, rgb);
    SetRecord(1, PointClamp, depth);
    SetRecord(2, PointClamp, srgb);
    SetRecord(3, PointClamp, Image(FloatTexels.data(), Format32Float));
    RequireModeEntry(device, ReadTableCode, ReadDirectCode, 0, rgb, "image table mode: 11_11_10 UINT image_load");
    RequireModeEntry(device, TableCode, DirectCode, 0, rgb, "image table mode: 11_11_10 UINT image_sample");
    RequireModeEntry(device, ReadTableCode, ReadDirectCode, 1, depth, "image table mode: depth bits image_load");
    RequireModeEntry(device, TableCode, DirectCode, 1, depth, "image table mode: depth bits image_sample");
    if (device.Target().srgbDecodeFormats != 0u) {
        RequireModeEntry(device, ReadTableCode, ReadDirectCode, 2, srgb, "image table mode: 8_8_SRGB image_load decoded in the shader");
    } else {
        std::printf("image table mode: the sRGB decode case skipped, the device samples 8_8_SRGB natively\n");
    }

    std::array<std::uint32_t, 8> volume{};
    const auto volumeWords = Image(FloatTexels.data(), Format32Float, Type3D);
    std::copy(volumeWords.begin(), volumeWords.end(), volume.begin());
    std::array<std::uint32_t, 8> plain{};
    const auto plainWords = Image(FloatTexels.data(), Format32Float);
    std::copy(plainWords.begin(), plainWords.end(), plain.begin());
    SetWideRecord(0, {volume[0], volume[1], volume[2], volume[3]});
    SetWideRecord(1, {plain[0], plain[1], plain[2], plain[3]});
    RequireModeEntry(device, ReadTable256Code, ReadDirect256Code, 0, volume, "image table mode: a 3D T# read by a 2D image_load");
    RequireModeEntry(device, ReadTable256Code, ReadDirect256Code, 1, plain, "image table mode: an r256 2D T#");
    SetClassRecords();
}

void RunStoreTests(AgcDriver::VulkanDevice& device) {
    SetClassRecords();
    const auto saved = UintTexels;
    Keys = {};
    Keys[0] = 1;
    const auto cut = Run(device, StoreTableCutCode, 1, Lanes, 64);
    Require(!Faulted(cut), "image table store: a store with EXEC clear faulted:\n" + cut.log);
    const auto stored = Run(device, StoreTableCode, 1, Lanes, 64);
    Require(Faulted(stored) && stored.log.find("unsupported operation") != std::string::npos, "image table store: an image_store through a table did not fault as an unsupported operation:\n" + stored.log);
    Require(UintTexels == saved, "image table store: an unsupported store reached the image");
}

void RunPointerTests(AgcDriver::VulkanDevice& device) {
    const auto floatImage = Image(FloatTexels.data(), Format32Float);
    const auto uintImage = Image(UintTexels.data(), Format32UInt);
    const auto sintImage = Image(SintTexels.data(), Format32SInt);
    const std::array<std::uint32_t, 4> invalid{0x1234u, 0x5678u, 0x9abcu, 0x0000ffacu};
    std::fill(Root.begin(), Root.end(), 0u);
    SetEntry(0, floatImage);
    SetEntry(1, uintImage);
    SetEntry(2, sintImage);
    SetEntry(7, invalid);
    SetEntry(255, uintImage);
    const std::array<std::uint32_t, 6> keys{0u, 1u, 2u, 3u, 0x1ffu, 0x102u};
    Keys = {};
    std::copy(keys.begin(), keys.end(), Keys.begin());
    const auto palette = RunPointer(device, PointerCode, 6);
    Require(!Faulted(palette), "pointer image table: valid entries faulted:\n" + palette.log);
    for (std::uint32_t group = 0; group < keys.size(); ++group) {
        const auto* words = Root.data() + (PaletteOffset + (keys[group] & 0xffu) * 32u) / 4u;
        if (std::all_of(words, words + 8, [](std::uint32_t word) { return word == 0u; })) {
            const auto zeros = Group(palette, group);
            Require(std::all_of(zeros.begin(), zeros.end(), [](std::uint32_t word) { return word == 0u; }), "pointer image table: a null entry did not sample zeros");
            continue;
        }
        if (keys[group] == 0x1ffu) continue;
        const auto control = RunDirect(device, PointClamp, {words[0], words[1], words[2], words[3]});
        Require(Group(palette, group) == Group(control, 0), "pointer image table: key " + Hex(keys[group]) + " differs from the direct binding of its masked entry");
    }
    RequireRed(palette, 0, Lanes, [](std::uint32_t x) { return std::bit_cast<std::uint32_t>(FloatTexels[x]); }, "pointer image table float texels");
    RequireRed(palette, 2, Lanes, [](std::uint32_t x) { return static_cast<std::uint32_t>(SintTexels[x]); }, "pointer image table sint texels");
    RequireRed(palette, 4, Lanes, [](std::uint32_t x) { return UintTexels[x]; }, "pointer image table key 0x1ff masked to entry 255");

    const auto loaded = RunPointer(device, LoadedPointerCode, 6);
    Require(!Faulted(loaded) && loaded.words == palette.words, "pointer image table: a base loaded from the SRT with a negative immediate differs:\n" + loaded.log);
    Require(loaded.artifactId != 0u && palette.artifactId != loaded.artifactId, "pointer image table: two programs share an artifact");

    Keys[0] = 7;
    const auto selected = RunPointer(device, PointerCode, 1);
    char address[32];
    std::snprintf(address, sizeof(address), "0x%llx", static_cast<unsigned long long>(reinterpret_cast<std::uintptr_t>(Root.data()) + PaletteOffset + 7u * 32u));
    Require(Faulted(selected) && selected.log.find("not an image") != std::string::npos && selected.log.find(address) != std::string::npos && selected.log.find("00001234") != std::string::npos, "pointer image table: an invalid entry did not report its address, words and reason:\n" + selected.log);
    Require(Zeros(selected), "pointer image table: a faulting access did not sample zeros");
    Require(selected.variantId == palette.variantId, "pointer image table: other keys specialized another module");

    Keys[0] = 2;
    Keys[1] = 0x08000000u;
    const auto unbounded = RunPointer(device, UnboundedPointerCode, 2);
    Require(!Faulted(unbounded) && Group(unbounded, 0) == Group(palette, 2) && Group(unbounded, 1) == Group(palette, 0), "pointer image table: an unmasked key, or one whose offset wraps to entry 0, sampled the wrong entry:\n" + unbounded.log);
    Keys[0] = 300;
    const auto past = RunPointer(device, UnboundedPointerCode, 1);
    Require(Faulted(past) && past.log.find("unmapped") != std::string::npos, "pointer image table: an entry past the captured palette did not fault as unmapped:\n" + past.log);
    Keys[0] = 0x10000u;
    const auto capped = RunPointer(device, UnboundedPointerCode, 1);
    Require(Faulted(capped) && capped.log.find("outside the snapshot") != std::string::npos, "pointer image table: a key past the key cap did not fault as outside the snapshot:\n" + capped.log);

    const std::array<std::uint32_t, 5> readKeys{0u, 1u, 2u, 3u, 0x1ffu};
    Keys = {};
    std::copy(readKeys.begin(), readKeys.end(), Keys.begin());
    const auto reads = RunPointer(device, ReadPointerCode, 5);
    Require(!Faulted(reads), "pointer image table read: valid entries faulted:\n" + reads.log);
    for (std::uint32_t group = 0; group < readKeys.size(); ++group) {
        const auto* words = Root.data() + (PaletteOffset + (readKeys[group] & 0xffu) * 32u) / 4u;
        if (std::all_of(words, words + 8, [](std::uint32_t word) { return word == 0u; })) {
            const auto zeros = Group(reads, group);
            Require(std::all_of(zeros.begin(), zeros.end(), [](std::uint32_t word) { return word == 0u; }), "pointer image table read: a null entry did not read zeros");
            continue;
        }
        const auto control = RunDirectCode(device, ReadDirectCode, PointClamp, std::span<const std::uint32_t>(words, 4));
        Require(Group(reads, group) == Group(control, 0), "pointer image table read: key " + Hex(readKeys[group]) + " differs from the direct binding of its masked entry");
    }
    RequireRed(reads, 4, Lanes, [](std::uint32_t x) { return UintTexels[x]; }, "pointer image table read: key 0x1ff masked to entry 255");
}

void RunPointerStorageWriteTests(AgcDriver::VulkanDevice& device) {
    alignas(256) static std::array<std::uint32_t, 256u * 8u> records{};
    alignas(256) static std::array<std::uint32_t, 256u * 8u> otherRecords{};
    alignas(256) static std::array<float, Width * Height> isolatedTexels{};
    struct Registration {
        void* data;
        Registration(void* data, std::size_t bytes) : data(data) { GuestAllocations::Mutation().Add(data, bytes, true, true, true); }
        ~Registration() { GuestAllocations::Mutation().Remove(data); }
    } registeredRecords(records.data(), sizeof(records)), registeredOtherTexels(OtherFloatTexels.data(), sizeof(OtherFloatTexels));
    for (std::uint32_t x = 0; x < isolatedTexels.size(); ++x) isolatedTexels[x] = 1024.0f + static_cast<float>(x);
    const auto run = [&](std::span<const std::uint32_t> code, bool alias, std::uint32_t waveSize = 64u, std::uint32_t selectedLanes = 0u) {
        records.fill(0u);
        otherRecords.fill(0u);
        const auto sampled = Image(FloatTexels.data(), Format32Float);
        std::copy(sampled.begin(), sampled.end(), records.begin());
        const auto isolated = Image(isolatedTexels.data(), Format32Float);
        std::copy(isolated.begin(), isolated.end(), otherRecords.begin());
        const auto keys = Buffer(Keys.data(), static_cast<std::uint32_t>(sizeof(Keys)));
        const auto output = Buffer(Output.data(), static_cast<std::uint32_t>(sizeof(Output)));
        const auto* storageData = alias ? static_cast<const void*>(records.data()) : OtherFloatTexels.data();
        const auto storage = Image(storageData, Format32Float);
        Srt.fill(0u);
        std::copy(keys.begin(), keys.end(), Srt.begin());
        std::copy(output.begin(), output.end(), Srt.begin() + 4);
        std::copy(PointClamp.begin(), PointClamp.end(), Srt.begin() + 8);
        const auto pointer = static_cast<std::uint64_t>(reinterpret_cast<std::uintptr_t>(records.data())) + 0x100u;
        Srt[12] = static_cast<std::uint32_t>(pointer);
        Srt[13] = static_cast<std::uint32_t>(pointer >> 32u);
        const auto otherPointer = static_cast<std::uint64_t>(reinterpret_cast<std::uintptr_t>(otherRecords.data())) + 0x100u;
        Srt[14] = static_cast<std::uint32_t>(otherPointer);
        Srt[15] = static_cast<std::uint32_t>(otherPointer >> 32u);
        std::copy(storage.begin(), storage.end(), Srt.begin() + 16);
        Srt[24] = selectedLanes;
        Keys.fill(0u);
        const std::array<ShaderRecompiler::MemoryRegion, 5> memory{{
            {reinterpret_cast<std::uintptr_t>(code.data()), std::as_bytes(code)},
            {reinterpret_cast<std::uintptr_t>(Srt.data()), std::as_bytes(std::span(Srt))},
            {reinterpret_cast<std::uintptr_t>(records.data()), std::as_bytes(std::span(records))},
            {reinterpret_cast<std::uintptr_t>(otherRecords.data()), std::as_bytes(std::span(otherRecords))},
            {reinterpret_cast<std::uintptr_t>(Keys.data()), std::as_bytes(std::span(Keys))},
        }};
        const auto outcome = Execute(device, code, Srt.data(), memory, 1u, waveSize, waveSize);
        AgcDriver::Graphics::StorageTexture::FlushPending(reinterpret_cast<std::uintptr_t>(storageData), sizeof(FloatTexels), nullptr, "test");
        device.WaitIdle();
        return outcome;
    };
    const auto separate = run(PointerStorageWriteCode, false);
    Require(!Faulted(separate), "pointer image table: a separate storage image write faulted:\n" + separate.log);
    RequireRed(separate, 0, Lanes, [](std::uint32_t x) { return std::bit_cast<std::uint32_t>(FloatTexels[x]); }, "pointer image table with a separate storage image write");
    const auto aliased = run(PointerStorageWriteCode, true);
    Require(Faulted(aliased) && aliased.log.find("is written by its own dispatch") != std::string::npos, "pointer image table: an overlapping storage image write did not fault:\n" + aliased.log);
    Require(Zeros(aliased), "pointer image table: an overlapping storage image write did not suppress the sampled value");
    const auto isolated = run(PointerStorageIsolationCode, true, 32u);
    Require(isolated.imageTableReadRanges.size() == 2u && !isolated.imageTableReadRanges[0].empty() && !isolated.imageTableReadRanges[1].empty(), "pointer image table isolation: both tables were not captured");
    for (const auto& [first, firstBytes] : isolated.imageTableReadRanges[0]) {
        for (const auto& [second, secondBytes] : isolated.imageTableReadRanges[1]) {
            Require(first <= second ? second - first >= firstBytes : first - second >= secondBytes, "pointer image table isolation: captured table ranges overlap");
        }
    }
    Require(!Faulted(isolated), "pointer image table isolation: writing the table selected with EXEC clear faulted the other table:\n" + isolated.log);
    RequireRed(isolated, 0, 32u, [](std::uint32_t x) { return std::bit_cast<std::uint32_t>(isolatedTexels[x]); }, "pointer image table isolation");
    for (std::uint32_t x = 0; x < 32u; ++x) {
        Require(records[x] == std::bit_cast<std::uint32_t>(isolatedTexels[x]), "pointer image table isolation: the storage image did not write the inactive table");
    }
    const auto selected = run(PointerStorageIsolationCode, true, 32u, 32u);
    Require(selected.imageTableReadRanges == isolated.imageTableReadRanges, "pointer image table isolation: selecting the written table changed the captured ranges");
    Require(Faulted(selected) && selected.log.find("is written by its own dispatch") != std::string::npos, "pointer image table isolation: selecting the written table did not fault:\n" + selected.log);
    Require(Zeros(selected), "pointer image table isolation: selecting the written table did not suppress the sampled value");
}

std::array<std::uint32_t, 4> VertexBuffer(const void* data, std::uint32_t stride, std::uint32_t count) {
    const auto address = reinterpret_cast<std::uintptr_t>(data);
    return {static_cast<std::uint32_t>(address), static_cast<std::uint32_t>((address >> 32u) & 0xffffu) | (stride << 16u), count, 0x01016facu};
}

alignas(256) constexpr std::array<std::uint32_t, 17> VertexTableCode{
    0x8f098408u, 0xf4280302u, 0x12000000u, 0x7e140280u, 0x7e160280u, 0xbf8cc07fu, 0xf0008f08u, 0x0003100au,
    0xbf8c3f70u, 0xe0702000u, 0x80041005u, 0xe0382000u, 0x80000005u, 0xbf8c3f70u, 0xf80008cfu, 0x03020100u,
    0xbf810000u,
};

alignas(256) constexpr std::array<std::uint32_t, 15> PixelTableCode{
    0x8f058404u, 0xf4280300u, 0x0a000000u, 0x7e140280u, 0x7e160280u, 0xbf8cc07fu, 0xf0008f08u, 0x0003100au,
    0xbf8c3f70u, 0xe0700000u, 0x80021000u, 0x7e0e02f2u, 0xf800180fu, 0x07070707u, 0xbf810000u,
};

constexpr std::uint32_t TargetWidth = 64;
constexpr std::uint32_t TargetHeight = 32;
alignas(256) std::array<std::byte, TargetWidth * TargetHeight * 4> Pixels{};
alignas(256) std::array<std::uint32_t, 4 * 4> VertexResults{};
alignas(256) std::array<std::uint32_t, 4> PixelResults{};
alignas(256) std::array<std::uint32_t, 4 * 4> GraphicsTable{};
constexpr std::array<std::array<float, 4>, 3> Triangle{{{-1.0f, -1.0f, 0.5f, 1.0f}, {3.0f, -1.0f, 0.5f, 1.0f}, {-1.0f, 3.0f, 0.5f, 1.0f}}};

std::pair<std::uint64_t, std::uint64_t> DrawTables(AgcDriver::VulkanDevice& device, std::uint32_t vertexKey, std::uint32_t pixelKey) {
    const auto target = device.Target();
    Pixels.fill(std::byte{0});
    VertexResults.fill(0xdeadbeefu);
    PixelResults.fill(0xdeadbeefu);
    const auto table = Buffer(GraphicsTable.data(), static_cast<std::uint32_t>(sizeof(GraphicsTable)));
    std::vector<std::uint32_t> vertexUserData(20, 0u);
    const auto vertices = VertexBuffer(Triangle.data(), 16u, static_cast<std::uint32_t>(Triangle.size()));
    const auto vertexOutput = VertexBuffer(VertexResults.data(), 16u, 4u);
    std::copy(vertices.begin(), vertices.end(), vertexUserData.begin());
    std::copy(table.begin(), table.end(), vertexUserData.begin() + 4);
    vertexUserData[8] = vertexKey;
    std::copy(vertexOutput.begin(), vertexOutput.end(), vertexUserData.begin() + 16);
    const std::array<ShaderRecompiler::MemoryRegion, 2> vertexMemory{{{reinterpret_cast<std::uintptr_t>(VertexTableCode.data()), std::as_bytes(std::span(VertexTableCode))}, {reinterpret_cast<std::uintptr_t>(GraphicsTable.data()), std::as_bytes(std::span(GraphicsTable))}}};
    ShaderRecompiler::RecompileRequest vertex{
        {ShaderStage::Vertex, reinterpret_cast<std::uintptr_t>(VertexTableCode.data()), VertexTableCode, 0, {}},
        {64, 0, vertexUserData, std::nullopt, std::nullopt, ShaderRecompiler::ShaderVertexStageInfo{}, vertexMemory},
        target,
        {0, 0, 0, 64}
    };
    const auto vertexResult = ShaderRecompiler::Recompile(vertex);
    const auto vertexPush = static_cast<std::uint32_t>(vertexResult.pushConstants.size());

    ShaderRecompiler::ShaderPixelStageInfo pixel{};
    pixel.wave32 = false;
    pixel.inputAddr = ShaderRecompiler::PixelInputBit(ShaderRecompiler::PixelInput::PositionX) | ShaderRecompiler::PixelInputBit(ShaderRecompiler::PixelInput::PositionY);
    pixel.posX = true;
    pixel.posY = true;
    pixel.targetOutputMode[0] = 9;
    pixel.targetExportMapping.fill(0xe4u);
    std::vector<std::uint32_t> pixelUserData(12, 0u);
    const auto pixelOutput = Buffer(PixelResults.data(), static_cast<std::uint32_t>(sizeof(PixelResults)));
    std::copy(table.begin(), table.end(), pixelUserData.begin());
    pixelUserData[4] = pixelKey;
    std::copy(pixelOutput.begin(), pixelOutput.end(), pixelUserData.begin() + 8);
    const std::array<ShaderRecompiler::MemoryRegion, 2> pixelMemory{{{reinterpret_cast<std::uintptr_t>(PixelTableCode.data()), std::as_bytes(std::span(PixelTableCode))}, {reinterpret_cast<std::uintptr_t>(GraphicsTable.data()), std::as_bytes(std::span(GraphicsTable))}}};
    ShaderRecompiler::RecompileRequest fragment{
        {ShaderStage::Fragment, reinterpret_cast<std::uintptr_t>(PixelTableCode.data()), PixelTableCode, 0, {}},
        {64, 0, pixelUserData, std::nullopt, pixel, std::nullopt, pixelMemory},
        target,
        PixelPushLayout(vertexPush, target)
    };
    const auto pixelResult = ShaderRecompiler::Recompile(fragment);
    const std::array<AgcDriver::Graphics::CompiledShader, 2> shaders{{
        {ShaderStage::Vertex, &vertexResult, 0},
        {ShaderStage::Fragment, &pixelResult, PixelPushOffset(vertexPush, target)}
    }};
    AgcDriver::Graphics::State state{};
    state.stages = {AgcDriver::Graphics::ShaderPath::Vertex, 0u, 64u, 64u, std::nullopt, std::nullopt};
    state.color = {reinterpret_cast<std::uintptr_t>(Pixels.data()), {TargetWidth, TargetHeight}, VK_FORMAT_R8G8B8A8_UNORM, Pixels.size(), 0xe4u};
    state.colors = {state.color};
    state.hasColorTarget = true;
    state.renderExtent = {TargetWidth, TargetHeight};
    state.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    state.viewport = {0, static_cast<float>(TargetHeight), static_cast<float>(TargetWidth), -static_cast<float>(TargetHeight), 0, 1};
    state.negativeOneToOne = false;
    state.scissor = {{0, 0}, {TargetWidth, TargetHeight}};
    state.cullMode = VK_CULL_MODE_NONE;
    state.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
    state.blend.colorWriteMask = 15;
    state.blends = {state.blend};
    state.blendConstants = {};
    const AgcDriver::Pm4::DrawParameters draw{0, static_cast<std::uint32_t>(Triangle.size()), 0, 1, 0, false};
    device.Draw(state, draw, shaders);
    device.WaitIdle();
    return {vertexResult.PipelineVariantId(), pixelResult.PipelineVariantId()};
}

void RunGraphicsTests(AgcDriver::VulkanDevice& device) {
    const auto floatImage = Image(FloatTexels.data(), Format32Float);
    const auto uintImage = Image(UintTexels.data(), Format32UInt);
    const auto sintImage = Image(SintTexels.data(), Format32SInt);
    GraphicsTable = {};
    std::copy(floatImage.begin(), floatImage.end(), GraphicsTable.begin());
    std::copy(uintImage.begin(), uintImage.end(), GraphicsTable.begin() + 4);
    std::copy(sintImage.begin(), sintImage.end(), GraphicsTable.begin() + 8);
    const auto first = DrawTables(device, 1u, 2u);
    for (std::uint32_t vertex = 0; vertex < 3u; ++vertex) Require(VertexResults[vertex * 4u] == UintTexels[0], "image table graphics: the vertex stage read the wrong entry (" + Hex(VertexResults[vertex * 4u]) + ")");
    Require(PixelResults[0] == static_cast<std::uint32_t>(SintTexels[0]), "image table graphics: the pixel stage read the wrong entry (" + Hex(PixelResults[0]) + ")");
    const auto second = DrawTables(device, 2u, 0u);
    for (std::uint32_t vertex = 0; vertex < 3u; ++vertex) Require(VertexResults[vertex * 4u] == static_cast<std::uint32_t>(SintTexels[0]), "image table graphics: the vertex stage read the wrong entry after the keys changed");
    Require(PixelResults[0] == std::bit_cast<std::uint32_t>(FloatTexels[0]), "image table graphics: the pixel stage read the wrong entry after the keys changed");
    Require(first == second, "image table graphics: other table keys built another pipeline");
}

}

int main() {
    try {
#ifdef _WIN32
        _putenv_s("APS5_SRGB_SHADER_DECODE", "1");
#else
        setenv("APS5_SRGB_SHADER_DECODE", "1", 1);
#endif
        const auto device = OpenVulkanTestDevice();
        if (!device) return VulkanTestSkipped;
        RunTests(*device);
        RunBorderSwizzleTests(*device);
        RunAnisoOverrideTests(*device);
        RunReadTests(*device);
        RunGuardedTableTests(*device);
        RunModeTests(*device);
        RunStoreTests(*device);
        RunPointerTests(*device);
        RunPointerStorageWriteTests(*device);
        RunGraphicsTests(*device);
        std::puts("image table tests passed");
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
