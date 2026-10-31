#include "prx/libSceAgcDriver/Execution/include/VulkanDevice.hpp"
#include "prx/libSceAgcDriver/Execution/include/Driver/Dispatch/DispatchCache.hpp"
#include "prx/libSceAgcDriver/Execution/include/SpannedWaves.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Draw.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Texture.hpp"
#include "prx/libc/include/GuestAllocations.hpp"
#include "Recompiler.hpp"
#include "ImageTableAbi.hpp"
#include "VulkanTestDevice.hpp"
#include <spirv/unified1/spirv.hpp>
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
#if ANYPS5_ENABLE_SPIRV_TOOLS
#include <spirv-tools/libspirv.hpp>
#endif
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
constexpr std::uint32_t Format32_32UInt = 62;
constexpr std::uint32_t Format11_11_10UInt = 34;
constexpr std::uint32_t Format8_8Srgb = 129;
constexpr std::uint32_t Type2D = 9;
constexpr std::uint32_t Type3D = 10;
constexpr std::uint32_t DepthSwizzleMode = 0x18;

alignas(256) std::array<float, Width * Height> FloatTexels{};
alignas(256) std::array<float, Width * Height> OtherFloatTexels{};
alignas(256) std::array<std::uint32_t, Width * Height> UintTexels{};
alignas(256) std::array<std::uint64_t, Width * Height> Uint64Texels{};
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

alignas(256) constexpr std::array<std::uint32_t, 31> Wave32TableSamplerCode{
    0xf4080100u, 0xfa000000u, 0xf4080200u, 0xfa000010u, 0xf4080700u, 0xfa000020u, 0xf4080900u, 0xfa000040u,
    0x8f108202u, 0xf4200444u, 0x20000000u, 0x9312b011u, 0xf4280802u, 0x24000000u, 0x360200bfu, 0x7e020d01u,
    0x060202f0u, 0x100202ffu, 0x3c800000u, 0x060202f2u, 0x7e0402ffu, 0x3e000000u, 0xf09c8f08u, 0x01090401u,
    0x7e120202u, 0x34101285u, 0x4a101100u, 0x34101084u, 0xe0781000u, 0x80070408u, 0xbf810000u,
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

alignas(256) constexpr std::array<std::uint32_t, 29> SelectedTableCode{
    0xf4080100u, 0xfa000000u, 0xf4080200u, 0xfa000010u, 0xf4080700u, 0xfa000020u, 0xf4000400u, 0xfa000060u,
    0xf4200444u, 0x20000000u, 0x9312b011u, 0xf42c0802u, 0x24000000u, 0x360200bfu, 0x7e020d01u, 0x060202f0u,
    0x100202ffu, 0x3c800000u, 0x7e0402ffu, 0x3e000000u, 0xf09c8f08u, 0x01090401u, 0x7e120202u, 0x34101286u,
    0x4a101100u, 0x34101084u, 0xe0781000u, 0x80070408u, 0xbf810000u,
};

alignas(256) constexpr std::array<std::uint32_t, 31> SelectedTableCutCode{
    0xf4080100u, 0xfa000000u, 0xf4080200u, 0xfa000010u, 0xf4080700u, 0xfa000020u, 0xf4000400u, 0xfa000060u,
    0xf4200444u, 0x20000000u, 0x9312b011u, 0xf42c0802u, 0x24000000u, 0x360200bfu, 0x7e020d01u, 0x060202f0u,
    0x100202ffu, 0x3c800000u, 0x7e0402ffu, 0x3e000000u, 0x7da400ffu, 0x00001000u, 0xf09c8f08u, 0x01090401u,
    0x7e120202u, 0x34101286u, 0x4a101100u, 0x34101084u, 0xe0781000u, 0x80070408u, 0xbf810000u,
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

alignas(256) constexpr std::array<std::uint32_t, 22> ReadTableCounterLoopCode{
    0xf4080100u, 0xfa000000u, 0xf4080700u, 0xfa000020u, 0x360200bfu, 0x7e040280u, 0xbea80380u, 0x9312b028u,
    0xf42c0802u, 0x24000000u, 0xf0008f08u, 0x00090401u, 0x7e120228u, 0x34101286u, 0x4a101100u, 0x34101084u,
    0xe0781000u, 0x80070408u, 0x80288128u, 0xbf0a8328u, 0xbf85fff2u, 0xbf810000u,
};

alignas(256) constexpr std::array<std::uint32_t, 28> ReadTableSetBitLoopCode{
    0xf4080100u, 0xfa000000u, 0xf4080200u, 0xfa000010u, 0xf4080700u, 0xfa000020u, 0xf4200a44u, 0xfa000000u,
    0x360200bfu, 0x7e040280u, 0xbea80380u, 0xbeaa1329u, 0x9312b02au, 0xf42c0802u, 0x24000000u, 0xf0008f08u,
    0x00090401u, 0x7e120228u, 0x34101286u, 0x4a101100u, 0x34101084u, 0xe0781000u, 0x80070408u, 0x80288128u,
    0xbea91b2au, 0xbf078029u, 0xbf85fff0u, 0xbf810000u,
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

alignas(256) constexpr std::array<std::uint32_t, 16> StoreLaneTableCode{
    0xf4080100u, 0xfa000000u, 0xf4080200u, 0xfa000010u, 0x8f108202u, 0xf4200444u, 0x20000000u, 0x9312b011u,
    0xf42c0802u, 0x24000000u, 0x360200bfu, 0x7e040280u, 0x7e080301u, 0xf0208f08u, 0x00090401u, 0xbf810000u,
};

alignas(256) constexpr std::array<std::uint32_t, 16> AtomicAddLaneTableCode{
    0xf4080100u, 0xfa000000u, 0xf4080200u, 0xfa000010u, 0x8f108202u, 0xf4200444u, 0x20000000u, 0x9312b011u,
    0xf42c0802u, 0x24000000u, 0x360200bfu, 0x7e040280u, 0x7e080301u, 0xf0448108u, 0x00090401u, 0xbf810000u,
};

alignas(256) constexpr std::array<std::uint32_t, 18> AtomicAdd64LaneTableCode{
    0xf4080100u, 0xfa000000u, 0xf4080200u, 0xfa000010u, 0x8f108202u, 0xf4200444u, 0x20000000u, 0x9312b011u,
    0xf42c0802u, 0x24000000u, 0x360200bfu, 0x7e040280u, 0x7e080301u, 0x7e0a02ffu, 0x00000000u, 0xf0448308u,
    0x00090401u, 0xbf810000u,
};

alignas(256) constexpr std::array<std::uint32_t, 16> AtomicSwapLaneTableCode{
    0xf4080100u, 0xfa000000u, 0xf4080200u, 0xfa000010u, 0x8f108202u, 0xf4200444u, 0x20000000u, 0x9312b011u,
    0xf42c0802u, 0x24000000u, 0x360200bfu, 0x7e040280u, 0x7e080301u, 0xf03c8108u, 0x00090401u, 0xbf810000u,
};

alignas(256) constexpr std::array<std::uint32_t, 18> AtomicCmpSwapLaneTableCode{
    0xf4080100u, 0xfa000000u, 0xf4080200u, 0xfa000010u, 0x8f108202u, 0xf4200444u, 0x20000000u, 0x9312b011u,
    0xf42c0802u, 0x24000000u, 0x360200bfu, 0x7e040280u, 0x7e080301u, 0x7e0a0304u, 0x3a080881u, 0xf0408308u,
    0x00090401u, 0xbf810000u,
};

alignas(256) constexpr std::array<std::uint32_t, 17> StoreTableCutCode{
    0xf4080100u, 0xfa000000u, 0xf4080200u, 0xfa000010u, 0x8f108202u, 0xf4200444u, 0x20000000u, 0x9312b011u,
    0xf42c0802u, 0x24000000u, 0x360200bfu, 0x7e040280u, 0x7da400ffu, 0x00001000u, 0xf0208f08u, 0x00090401u,
    0xbf810000u,
};

constexpr std::uint32_t SelectedStoreValue = 0xc0de5eedu;

alignas(256) constexpr std::array<std::uint32_t, 18> SelectedStoreCode{
    0xf4080100u, 0xfa000000u, 0xf4080200u, 0xfa000010u, 0xf4000400u, 0xfa000060u, 0xf4200444u, 0x20000000u,
    0x9312b011u, 0xf42c0802u, 0x24000000u, 0x360200bfu, 0x7e040280u, 0x7e0802ffu, SelectedStoreValue, 0xf0208f08u,
    0x00090401u, 0xbf810000u,
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

alignas(256) constexpr std::array<std::uint32_t, 30> SplitRecordCode{
    0xf4080100u, 0xfa000000u, 0xf4080200u, 0xfa000010u, 0xf4080700u, 0xfa000020u, 0x8f108202u, 0xf4200444u,
    0x20000000u, 0x9312b011u, 0xf42c0802u, 0x24000000u, 0xf4280a02u, 0x24000020u, 0x360200bfu, 0x7e020d01u,
    0x060202f0u, 0x100202ffu, 0x3c800000u, 0x7e0402ffu, 0x3e000000u, 0xf09c0f08u, 0x01090401u, 0x7e120202u,
    0x34101286u, 0x4a101100u, 0x34101084u, 0xe0781000u, 0x80070408u, 0xbf810000u,
};

alignas(256) constexpr std::array<std::uint32_t, 32> IndirectRecordCode{
    0xf4080100u, 0xfa000000u, 0xf4080200u, 0xfa000010u, 0xf4080700u, 0xfa000020u, 0xf4080800u, 0xfa000030u,
    0x8f108202u, 0xf4200444u, 0x20000000u, 0x9312b011u, 0xf4240b02u, 0x24000008u, 0xf40c0916u, 0xfa000020u,
    0x360200bfu, 0x7e020d01u, 0x060202f0u, 0x100202ffu, 0x3c800000u, 0x7e0402ffu, 0x3e000000u, 0xf09c0f08u,
    0x01090401u, 0x7e120202u, 0x34101286u, 0x4a101100u, 0x34101084u, 0xe0781000u, 0x80070408u, 0xbf810000u,
};

alignas(256) constexpr std::array<std::uint32_t, 34> PointerChainRecordCode{
    0xf4080100u, 0xfa000000u, 0xf4080200u, 0xfa000010u, 0xf4080700u, 0xfa000020u, 0xf4080800u, 0xfa000030u,
    0x8f108202u, 0xf4200444u, 0x20000000u, 0x9312b011u, 0xf4240b02u, 0x24000008u, 0xf4040b96u, 0xfa000010u,
    0xf40c0917u, 0xfa000020u, 0x360200bfu, 0x7e020d01u, 0x060202f0u, 0x100202ffu, 0x3c800000u, 0x7e0402ffu,
    0x3e000000u, 0xf09c0f08u, 0x01090401u, 0x7e120202u, 0x34101286u, 0x4a101100u, 0x34101084u, 0xe0781000u,
    0x80070408u, 0xbf810000u,
};

alignas(256) std::array<std::uint32_t, Records * 16> Indirect{};
alignas(256) std::array<std::uint32_t, Records * 8> Links{};

alignas(256) constexpr std::array<std::uint32_t, 37> NestedRecordCode{
    0xf4080100u, 0xfa000000u, 0xf4080200u, 0xfa000010u, 0xf4080700u, 0xfa000020u, 0xf4080800u, 0xfa000030u,
    0x8f108302u, 0xf4200444u, 0x20000000u, 0xf4200504u, 0x20000004u, 0x8f128411u, 0x8f148514u,
    0xf4240b02u, 0x24000008u, 0xf4080616u, 0xfa0000a0u, 0xf42c090cu, 0x28000000u,
    0x360200bfu, 0x7e020d01u, 0x060202f0u, 0x100202ffu, 0x3c800000u, 0x7e0402ffu, 0x3e000000u, 0xf09c0f08u,
    0x01090401u, 0x7e120202u, 0x34101286u, 0x4a101100u, 0x34101084u, 0xe0781000u, 0x80070408u, 0xbf810000u,
};

alignas(256) constexpr std::array<std::uint32_t, 38> NestedSamplerCode{
    0xf4080100u, 0xfa000000u, 0xf4080200u, 0xfa000010u, 0xf4080700u, 0xfa000020u, 0xf40c0900u, 0xfa000030u,
    0x8f108302u, 0xf4200444u, 0x20000000u, 0xf4200504u, 0x20000004u, 0x8f128411u, 0x8f148414u,
    0xf4240b02u, 0x24000008u, 0xf4080616u, 0xfa0000a0u, 0xf428080cu, 0x28000000u,
    0x360200bfu, 0x7e020d01u, 0x060202f0u, 0x100202ffu, 0x3c800000u, 0x060202f2u, 0x7e0402ffu, 0x3e000000u, 0xf09c0f08u,
    0x01090401u, 0x7e120202u, 0x34101286u, 0x4a101100u, 0x34101084u, 0xe0781000u, 0x80070408u, 0xbf810000u,
};

alignas(256) constexpr std::array<std::uint32_t, 33> PointerSamplerCode{
    0xf4080100u, 0xfa000000u, 0xf4080200u, 0xfa000010u, 0xf4080700u, 0xfa000020u, 0xf40c0900u, 0xfa000030u,
    0x8f108302u, 0xf4200444u, 0x20000000u, 0x8f128411u, 0xf4240b02u, 0x24000008u, 0xf4080816u, 0xfa000020u,
    0x360200bfu, 0x7e020d01u, 0x060202f0u, 0x100202ffu, 0x3c800000u, 0x060202f2u, 0x7e0402ffu, 0x3e000000u, 0xf09c0f08u,
    0x01090401u, 0x7e120202u, 0x34101286u, 0x4a101100u, 0x34101084u, 0xe0781000u, 0x80070408u, 0xbf810000u,
};

alignas(256) constexpr std::array<std::uint32_t, 33> PointerImageCode{
    0xf4080100u, 0xfa000000u, 0xf4080200u, 0xfa000010u, 0xf4080700u, 0xfa000020u, 0xf4080800u, 0xfa000030u,
    0x8f108302u, 0xf4200444u, 0x20000000u, 0x8f128411u, 0xf4240b02u, 0x24000008u, 0xf40c0916u, 0xfa000040u,
    0x360200bfu, 0x7e020d01u, 0x060202f0u, 0x100202ffu, 0x3c800000u, 0x060202f2u, 0x7e0402ffu, 0x3e000000u, 0xf09c0f08u,
    0x01090401u, 0x7e120202u, 0x34101286u, 0x4a101100u, 0x34101084u, 0xe0781000u, 0x80070408u, 0xbf810000u,
};

alignas(256) constexpr std::array<std::uint32_t, 63> PointerBallotCode{
    0xf4080100u, 0xfa000000u, 0xf4080200u, 0xfa000010u, 0xf4080700u, 0xfa000020u, 0xf4080800u, 0xfa000030u,
    0x8f108402u, 0xf4280404u, 0x20000000u, 0x360200bfu, 0x7e020d01u, 0x060202f0u, 0x100202ffu, 0x3c800000u,
    0x060202f2u, 0x7e0402ffu, 0x3e000000u, 0xbf8cc07fu, 0x7d8800a0u, 0x7e140210u, 0x02061411u, 0xbeae047eu,
    0x7e280503u, 0xd4c20030u, 0x00020614u, 0xbeb22430u, 0x8f158414u, 0xf4240b02u, 0x2a000008u, 0xd4c50034u,
    0x00020680u, 0xbf8cc07fu, 0xbf068112u, 0xbf840003u, 0xbeac03ffu, 0x000000eau, 0xbf820009u, 0xbf068212u,
    0xbf840002u, 0xbeac0334u, 0xbf820004u, 0xbf068312u, 0xbf840001u, 0xbeac0390u, 0xbf800000u, 0xbf800000u,
    0xf40c0916u, 0xfa000040u, 0xbf8cc07fu, 0xf09c0f08u, 0x01090401u, 0x8afe3032u, 0xbf89ffe1u, 0xbefe042eu,
    0x7e120202u, 0x34101286u, 0x4a101100u, 0x34101084u, 0xe0781000u, 0x80070408u, 0xbf810000u,
};

alignas(256) constexpr std::array<std::uint32_t, 66> PointerCopiesCode{
    0xf4080100u, 0xfa000000u, 0xf4080200u, 0xfa000010u, 0xf4080700u, 0xfa000020u, 0xf4080800u, 0xfa000030u,
    0x8f108402u, 0xf4280404u, 0x20000000u, 0x360200bfu, 0x7e020d01u, 0x060202f0u, 0x100202ffu, 0x3c800000u,
    0x060202f2u, 0x7e0402ffu, 0x3e000000u, 0xbf8cc07fu, 0x7d8800a0u, 0x7e140210u, 0x02061411u, 0xbeae047eu,
    0x7e280503u, 0xd4c20030u, 0x00020614u, 0xbeb22430u, 0x8f158414u, 0xf4240b02u, 0x2a000008u, 0xbf8cc07fu,
    0xbf068112u, 0xbf840003u, 0xbeac03ffu, 0x000000eau, 0xbf82000eu, 0xbf068212u, 0xbf840007u, 0x3a160613u,
    0x7e2c050bu, 0x8f178416u, 0xf4200b02u, 0x2e000008u, 0xbf8cc07fu, 0xbf820004u, 0xbf068312u, 0xbf840001u,
    0xbeac0390u, 0xbf800000u, 0xbf800000u, 0xf40c0916u, 0xfa000040u, 0xbf8cc07fu, 0xf09c0f08u, 0x01090401u,
    0x8afe3032u, 0xbf89ffdeu, 0xbefe042eu, 0x7e120202u, 0x34101286u, 0x4a101100u, 0x34101084u, 0xe0781000u,
    0x80070408u, 0xbf810000u,
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
    std::vector<std::uint32_t> tableMap;
    std::vector<std::vector<std::pair<std::uint64_t, std::uint64_t>>> imageTableReadRanges;
    bool fullSubgroups = false;
};

std::vector<std::array<std::uint32_t, 4>> SamplerWords(const std::vector<std::uint32_t>& words) {
    std::vector<std::array<std::uint32_t, 4>> result;
    for (std::size_t first = 0; first + 4u <= words.size(); first += 4u) result.push_back({words[first], words[first + 1u], words[first + 2u], words[first + 3u]});
    return result;
}

ShaderRecompiler::RecompileResult Compile(std::span<const std::uint32_t> span, const void* root, std::span<const ShaderRecompiler::MemoryRegion> memory, std::uint32_t threads, std::uint32_t waveSize, const ShaderRecompiler::SpirvTarget& target) {
    const auto rootAddress = reinterpret_cast<std::uintptr_t>(root);
    const std::array<std::uint32_t, 2> userData{static_cast<std::uint32_t>(rootAddress), static_cast<std::uint32_t>(rootAddress >> 32u)};
    const ShaderRecompiler::ShaderComputeStageInfo compute{{threads, 1, 1}, 0u, {true, false, false}, false, 1};
    ShaderRecompiler::RecompileRequest request{
        {ShaderStage::Compute, reinterpret_cast<std::uintptr_t>(span.data()), span, 0, {}},
        {waveSize, 0, userData, compute, std::nullopt, std::nullopt, memory},
        target,
        {0, 0, 0, 128}
    };
    return ShaderRecompiler::Recompile(request);
}

Outcome Execute(AgcDriver::VulkanDevice& device, std::span<const std::uint32_t> span, const void* root, std::span<const ShaderRecompiler::MemoryRegion> memory, std::uint32_t groups, std::uint32_t threads, std::uint32_t waveSize) {
    std::fill(Output.begin(), Output.end(), 0xdeadbeefu);
    const auto result = Compile(span, root, memory, threads, waveSize, device.Target());
    Outcome outcome;
    outcome.variantId = result.PipelineVariantId();
    outcome.artifactId = result.variantId;
    outcome.imageTableFaults = result.imageTableFaults;
    outcome.poisonedSrtReads = result.poisonedSrtReads;
    outcome.cacheable = AgcDriver::DriverDetail::CacheableResult(result);
    outcome.imageTableReadRanges = result.imageTableReadRanges;
    outcome.fullSubgroups = result.fullSubgroups;
    for (const auto& binding : result.bindings) {
        if (binding.role == ShaderRecompiler::DescriptorRole::ImageTable || binding.role == ShaderRecompiler::DescriptorRole::ImageStorageTable) outcome.imageTableCount = binding.count;
        if (binding.role == ShaderRecompiler::DescriptorRole::ImageTableMap) outcome.tableMap = binding.guestDescriptor;
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

std::array<ShaderRecompiler::MemoryRegion, 4> TableMemory(std::span<const std::uint32_t> span, bool outputIntoTable) {
    const auto table = Buffer(Table.data(), static_cast<std::uint32_t>(sizeof(Table)));
    const auto keys = Buffer(Keys.data(), static_cast<std::uint32_t>(sizeof(Keys)));
    const auto output = outputIntoTable ? Buffer(Table.data(), static_cast<std::uint32_t>(sizeof(Table))) : Buffer(Output.data(), static_cast<std::uint32_t>(sizeof(Output)));
    std::copy(table.begin(), table.end(), Srt.begin());
    std::copy(keys.begin(), keys.end(), Srt.begin() + 4);
    std::copy(output.begin(), output.end(), Srt.begin() + 8);
    return {{
        {reinterpret_cast<std::uintptr_t>(span.data()), std::as_bytes(span)},
        {reinterpret_cast<std::uintptr_t>(Srt.data()), std::as_bytes(std::span(Srt))},
        {reinterpret_cast<std::uintptr_t>(Table.data()), std::as_bytes(std::span(Table))},
        {reinterpret_cast<std::uintptr_t>(Keys.data()), std::as_bytes(std::span(Keys))},
    }};
}

template <std::size_t CodeWords>
Outcome Run(AgcDriver::VulkanDevice& device, const std::array<std::uint32_t, CodeWords>& code, std::uint32_t groups, std::uint32_t threads, std::uint32_t waveSize, bool outputIntoTable = false) {
    const std::span<const std::uint32_t> span(code);
    const auto memory = TableMemory(span, outputIntoTable);
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

template <std::size_t CodeWords>
Outcome RunIndirectRecords(AgcDriver::VulkanDevice& device, const std::array<std::uint32_t, CodeWords>& code, std::uint32_t groups) {
    const auto table = Buffer(Table.data(), static_cast<std::uint32_t>(sizeof(Table)));
    const auto keys = Buffer(Keys.data(), static_cast<std::uint32_t>(sizeof(Keys)));
    const auto output = Buffer(Output.data(), static_cast<std::uint32_t>(sizeof(Output)));
    std::copy(table.begin(), table.end(), Srt.begin());
    std::copy(keys.begin(), keys.end(), Srt.begin() + 4);
    std::copy(output.begin(), output.end(), Srt.begin() + 8);
    std::copy(PointClamp.begin(), PointClamp.end(), Srt.begin() + 12);
    const std::span<const std::uint32_t> span(code);
    const std::array<ShaderRecompiler::MemoryRegion, 6> memory{{
        {reinterpret_cast<std::uintptr_t>(span.data()), std::as_bytes(span)},
        {reinterpret_cast<std::uintptr_t>(Srt.data()), std::as_bytes(std::span(Srt))},
        {reinterpret_cast<std::uintptr_t>(Table.data()), std::as_bytes(std::span(Table))},
        {reinterpret_cast<std::uintptr_t>(Keys.data()), std::as_bytes(std::span(Keys))},
        {reinterpret_cast<std::uintptr_t>(Indirect.data()), std::as_bytes(std::span(Indirect))},
        {reinterpret_cast<std::uintptr_t>(Links.data()), std::as_bytes(std::span(Links))},
    }};
    return Execute(device, span, Srt.data(), memory, groups, Lanes, 64);
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
    SetRecord(7, PointClamp, {0x1234u, 0x5678u, 0x9abcu, 0x1000ffacu});
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
        auto secondOpaqueBlack = opaqueBlack;
        secondOpaqueBlack[1] = (3u * 256u) << 12u;
        SetRecord(0, opaqueBlack, image);
        SetRecord(1, secondOpaqueBlack, image);
        for (const std::uint32_t record : {0u, 1u}) {
            Keys[0] = record;
            Keys[1] = record;
            requireFault(RunDirectCode(device, TableCode, PointClamp, image), "sampler table entry with an opaque black border", name + " rejected S# " + std::to_string(record) + " of two with the same reason");
        }
        SetRecord(0, PointClamp, image);
        SetRecord(1, opaqueBlack, image);
        Keys[0] = 1;
        Keys[1] = 0;
    }
    const auto cut = Run(device, TableCutCode, 1, Lanes, 64);
    Require(Holds(cut.samplerTable, opaqueBlack), "image table border: the EXEC-clear opaque-black S# was not included in the snapshot");
    Require(!Faulted(cut), "image table border: an opaque-black S# selected with EXEC clear faulted:\n" + cut.log);
    Require(std::all_of(cut.words.begin(), cut.words.end(), [](std::uint32_t word) { return word == 0xdeadbeefu; }), "image table border: the EXEC-clear shader wrote output");
    for (const std::uint32_t border : {0u, 2u}) {
        auto sampler = opaqueBlack;
        sampler[3] = border << 30u;
        const auto name = "image table border numeric class " + std::to_string(border);
        const auto direct = RunDirect(device, sampler, floatImage);
        Require(!Faulted(direct), name + ": the direct float-border control faulted");
        RequireRed(direct, 0, Lanes, [](std::uint32_t x) { return std::bit_cast<std::uint32_t>(FloatTexels[x]); }, name + " direct float texels");
        SetRecord(0, PointClamp, floatImage);
        SetRecord(1, PointClamp, Image(UintTexels.data(), Format32UInt));
        Keys[0] = 0;
        Keys[1] = 1;
        const auto inactiveImage = RunDirectCode(device, TableDirectSamplerCode, sampler, floatImage);
        Require(inactiveImage.imageTableCount == 2u, name + ": the inactive integer T# was not included in the snapshot");
        Require(!Faulted(inactiveImage) && Group(inactiveImage, 0) == Group(direct, 0), name + ": an inactive integer T# poisoned the float-border pair:\n" + inactiveImage.log);
        Keys[0] = 1;
        Keys[1] = 0;
        requireFault(RunDirectCode(device, TableDirectSamplerCode, sampler, floatImage), "border color numeric class disagrees", name + " active integer T#");
        SetRecord(0, PointClamp, floatImage);
        SetRecord(1, sampler, floatImage);
        for (const bool tableImage : {false, true}) {
            const auto pair = name + (tableImage ? " table T#/table S#" : " direct T#/table S#");
            Keys[0] = 0;
            Keys[1] = 1;
            const auto inactiveSampler = tableImage ? RunDirectCode(device, TableCode, PointClamp, floatImage) : RunDirectCode(device, TableSamplerCode, PointClamp, floatImage);
            Require(Holds(inactiveSampler.samplerTable, sampler), pair + ": the inactive border S# was not included in the snapshot");
            Require(!Faulted(inactiveSampler) && Group(inactiveSampler, 0) == Group(direct, 0), pair + ": an inactive rejected S# poisoned the active pair:\n" + inactiveSampler.log);
            Keys[0] = 1;
            Keys[1] = 0;
            const auto activeSampler = tableImage ? RunDirectCode(device, TableCode, PointClamp, floatImage) : RunDirectCode(device, TableSamplerCode, PointClamp, floatImage);
            requireFault(activeSampler, "sampler table entry with a border needs dynamic texture numeric-class pairing", pair);
        }
        const auto inactiveExec = Run(device, TableCutCode, 1, Lanes, 64);
        Require(Holds(inactiveExec.samplerTable, sampler), name + ": the EXEC-clear border S# was not included in the snapshot");
        Require(!Faulted(inactiveExec) && std::all_of(inactiveExec.words.begin(), inactiveExec.words.end(), [](std::uint32_t word) { return word == 0xdeadbeefu; }), name + ": the EXEC-clear border S# faulted or wrote output:\n" + inactiveExec.log);
    }
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

    const auto invariantPhiCode = [] {
        std::array<std::uint32_t, 39> code{};
        std::copy_n(TableCode.begin(), 19, code.begin());
        code[19] = 0xbea80380u;
        std::copy_n(TableCode.begin() + 19, 8, code.begin() + 20);
        const std::array<std::uint32_t, 11> tail{0xbf068028u, 0xbf850003u, 0xf42c0802u, 0x24000000u, 0xbf820002u, 0xf42c0802u, 0x24000000u, 0x80288128u, 0xbf0a8228u, 0xbf85ffeeu, 0xbf810000u};
        std::copy(tail.begin(), tail.end(), code.begin() + 28);
        return code;
    }();
    const auto controlPhiCode = [&] {
        std::array<std::uint32_t, 32> code{};
        std::copy_n(invariantPhiCode.begin(), 28, code.begin());
        const std::array<std::uint32_t, 4> tail{0x80288128u, 0xbf0a8228u, 0xbf85fff5u, 0xbf810000u};
        std::copy(tail.begin(), tail.end(), code.begin() + 28);
        return code;
    }();
    const auto controlPhi = Run(device, controlPhiCode, 6, Lanes, 64);
    const auto invariantPhi = Run(device, invariantPhiCode, 6, Lanes, 64);
    Require(!Faulted(invariantPhi) && !Faulted(controlPhi), "image table: invariant descriptor Phis faulted");
    Require(invariantPhi.imageTableCount == classes.imageTableCount, "image table: invariant descriptor Phis changed the distinct table elements");
    Require(invariantPhi.words == controlPhi.words, "image table: invariant descriptor Phis changed GPU sample results");
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
    Require(Faulted(unmappedUsed) && unmappedUsed.log.find("rejected by the driver") != std::string::npos && unmappedUsed.log.find("is not mapped") != std::string::npos && Zeros(unmappedUsed), "image table: an entry whose surface is not mapped was bound instead of faulting when selected:\n" + unmappedUsed.log);
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
    Require(floatControl.samplers == std::vector{LinearWrap} && uintControl.samplers == std::vector{PointWrap} && sintControl.samplers == std::vector{PointWrap}, "image table: the direct bindings under a linear S# changed their sampler copies");
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

void RequireIterations(const Outcome& outcome, std::span<const std::uint32_t> records, const std::string& what) {
    Require(!Faulted(outcome) && outcome.imageTableCount != 0u, what + ": the loop did not read through the table:\n" + outcome.log);
    for (std::uint32_t iteration = 0; iteration < records.size(); ++iteration) {
        for (std::uint32_t lane = 0; lane < Lanes; ++lane) {
            const auto* texel = Output.data() + (iteration * Lanes + lane) * 4u;
            const auto record = records[iteration];
            const auto expected = record == 0u ? std::bit_cast<std::uint32_t>(FloatTexels[lane]) : record == 1u ? UintTexels[lane] : record == 2u ? static_cast<std::uint32_t>(SintTexels[lane]) : 0u;
            Require(texel[0] == expected && (record != 6u || std::all_of(texel, texel + 4, [](std::uint32_t word) { return word == 0u; })), what + ": iteration " + std::to_string(iteration) + " read " + Hex(texel[0]) + " at lane " + std::to_string(lane) + ", expected record " + std::to_string(record) + "'s " + Hex(expected));
        }
    }
}

void RunLoopKeyTests(AgcDriver::VulkanDevice& device) {
    SetClassRecords();
    Keys = {};
    const std::array<std::uint32_t, 3> counted{0u, 1u, 2u};
    RequireIterations(Run(device, ReadTableCounterLoopCode, 1, Lanes, 64), counted, "image table loop counter key");
    Keys[0] = 0x45u;
    const std::array<std::uint32_t, 3> setBits{0u, 2u, 6u};
    RequireIterations(Run(device, ReadTableSetBitLoopCode, 1, Lanes, 64), setBits, "image table set-bit key");
    Keys[0] = 0x6u;
    const std::array<std::uint32_t, 2> otherBits{1u, 2u};
    RequireIterations(Run(device, ReadTableSetBitLoopCode, 1, Lanes, 64), otherBits, "image table set-bit key after the mask changed");
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

void RunSelectedRecordTests(AgcDriver::VulkanDevice& device) {
    Fill();
    SetClassRecords();
    Keys = {};
    Srt[24] = 12u;
    Outcome first;
    for (const auto record : {2u, 0u, 1u}) {
        Keys[3] = record;
        const auto selected = Run(device, SelectedTableCode, 1, Lanes, 64);
        Require(!Faulted(selected) && selected.imageTableCount == 1u, "image table selected record: record " + std::to_string(record) + " faulted or bound other entries:\n" + selected.log);
        const auto* words = Table.data() + record * (Stride / 4u);
        const auto control = RunDirect(device, {words[0], words[1], words[2], words[3]}, {words[4], words[5], words[6], words[7]});
        Require(Group(selected, 0) == Group(control, 0), "image table selected record: record " + std::to_string(record) + " differs from the direct binding of the same words");
        if (record == 2u) first = selected;
        Require(selected.artifactId == first.artifactId && selected.variantId == first.variantId, "image table selected record: another key changed the module");
    }

    Keys[3] = 1000u;
    const auto beyond = Run(device, SelectedTableCode, 1, Lanes, 64);
    Require(!Faulted(beyond) && Zeros(beyond), "image table selected record: a key past the table faulted or did not sample zeros:\n" + beyond.log);

    Keys[3] = 7u;
    const auto cut = Run(device, SelectedTableCutCode, 1, Lanes, 64);
    Require(!Faulted(cut), "image table selected record: an invalid record selected with EXEC clear faulted:\n" + cut.log);
    const auto invalid = Run(device, SelectedTableCode, 1, Lanes, 64);
    const auto tableAddress = static_cast<std::uint64_t>(reinterpret_cast<std::uintptr_t>(Table.data())) + 7u * Stride + 16u;
    char address[32];
    std::snprintf(address, sizeof(address), "0x%llx", static_cast<unsigned long long>(tableAddress));
    Require(Faulted(invalid) && invalid.log.find("not an image") != std::string::npos && invalid.log.find(address) != std::string::npos && invalid.log.find("00001234") != std::string::npos && Zeros(invalid), "image table selected record: an invalid record selected with EXEC set did not report its address, words and reason:\n" + invalid.log);

    Keys[3] = 0u;
    const auto aliased = Run(device, SelectedTableCode, 1, Lanes, 64, true);
    Require(Faulted(aliased) && aliased.log.find("is written by its own dispatch") != std::string::npos, "image table selected record: a dispatch storing over its selected record did not fault on use:\n" + aliased.log);
    SetClassRecords();
    Srt[24] = 0u;
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
    struct Registration {
        void* data;
        Registration(void* data, std::size_t bytes) : data(data) { GuestAllocations::Mutation().Add(data, bytes, true, true, true); }
        ~Registration() { GuestAllocations::Mutation().Remove(data); }
    } registeredUint(UintTexels.data(), sizeof(UintTexels)), registeredFloat(FloatTexels.data(), sizeof(FloatTexels)), registeredSint(SintTexels.data(), sizeof(SintTexels));
    SetClassRecords();
    const auto savedUint = UintTexels;
    const auto savedFloat = FloatTexels;
    const auto savedSint = SintTexels;
    const auto untouched = [&] { return UintTexels == savedUint && FloatTexels == savedFloat && SintTexels == savedSint; };
    Keys = {};
    Keys[0] = 1;
    const auto cut = Run(device, StoreTableCutCode, 1, Lanes, 64);
    AgcDriver::Graphics::StorageTexture::FlushAllPending("image table store test");
    Require(!Faulted(cut), "image table store: a store with EXEC clear faulted:\n" + cut.log);
    Require(untouched(), "image table store: a store with EXEC clear reached an image");
    const auto stored = Run(device, StoreLaneTableCode, 1, Lanes, 64);
    AgcDriver::Graphics::StorageTexture::FlushAllPending("image table store test");
    Require(!Faulted(stored), "image table store: a store through a table faulted:\n" + stored.log);
    Require(stored.imageTableCount == 2u, "image table store: the storage table does not hold one element per distinct T# (" + std::to_string(stored.imageTableCount) + ")");
    for (std::uint32_t x = 0; x < Width; ++x) Require(UintTexels[x] == x, "image table store: texel " + std::to_string(x) + " of the selected image is " + Hex(UintTexels[x]) + ", expected " + Hex(x));
    Require(std::equal(UintTexels.begin() + Width, UintTexels.end(), savedUint.begin() + Width), "image table store: a store to row 0 reached another row");
    Require(FloatTexels == savedFloat && SintTexels == savedSint, "image table store: a store through key 1 reached another entry's image");
    UintTexels = savedUint;
    const auto restored = Run(device, StoreTableCutCode, 1, Lanes, 64);
    AgcDriver::Graphics::StorageTexture::FlushAllPending("image table store test");
    Require(!Faulted(restored) && untouched(), "image table store: restoring the image failed:\n" + restored.log);
    Keys[0] = 6;
    const auto null = Run(device, StoreLaneTableCode, 1, Lanes, 64);
    AgcDriver::Graphics::StorageTexture::FlushAllPending("image table store test");
    Require(untouched(), "image table store: a store through a null entry reached an image:\n" + null.log);
}

void RunAtomicTests(AgcDriver::VulkanDevice& device) {
    struct Registration {
        void* data;
        Registration(void* data, std::size_t bytes) : data(data) { GuestAllocations::Mutation().Add(data, bytes, true, true, true); }
        ~Registration() { GuestAllocations::Mutation().Remove(data); }
    } registeredUint(UintTexels.data(), sizeof(UintTexels)), registeredFloat(FloatTexels.data(), sizeof(FloatTexels)), registeredSint(SintTexels.data(), sizeof(SintTexels));
    SetClassRecords();
    for (std::uint32_t x = 0; x < Width; ++x) UintTexels[x] = 0x5000u + x;
    for (std::uint32_t x = 0; x < Width; ++x) FloatTexels[x] = 1.0f;
    Keys = {};
    Keys[0] = 1;
    const auto added = Run(device, AtomicAddLaneTableCode, 1, Lanes, 64);
    AgcDriver::Graphics::StorageTexture::FlushAllPending("image table atomic test");
    Require(!Faulted(added), "image table atomic: an add through a table faulted:\n" + added.log);
    Require(added.imageTableCount != 0u, "image table atomic: the add bound no storage table elements");
    for (std::uint32_t x = 0; x < Width; ++x) Require(UintTexels[x] == 0x5000u + x + x, "image table atomic: texel " + std::to_string(x) + " of the selected image is " + Hex(UintTexels[x]) + ", expected " + Hex(0x5000u + x + x));
    const auto swapped = Run(device, AtomicSwapLaneTableCode, 1, Lanes, 64);
    AgcDriver::Graphics::StorageTexture::FlushAllPending("image table atomic test");
    Require(!Faulted(swapped), "image table atomic: an exchange through a table faulted:\n" + swapped.log);
    for (std::uint32_t x = 0; x < Width; ++x) Require(UintTexels[x] == x, "image table atomic: texel " + std::to_string(x) + " of the selected image is " + Hex(UintTexels[x]) + ", expected " + Hex(x));
    const auto compared = Run(device, AtomicCmpSwapLaneTableCode, 1, Lanes, 64);
    AgcDriver::Graphics::StorageTexture::FlushAllPending("image table atomic test");
    Require(!Faulted(compared), "image table atomic: a compare exchange through a table faulted:\n" + compared.log);
    for (std::uint32_t x = 0; x < Width; ++x) Require(UintTexels[x] == (x ^ 1u), "image table atomic: texel " + std::to_string(x) + " of the selected image is " + Hex(UintTexels[x]) + ", expected " + Hex(x ^ 1u));
    Keys[0] = 8;
    const auto addedFloat = Run(device, AtomicAddLaneTableCode, 1, Lanes, 64);
    AgcDriver::Graphics::StorageTexture::FlushAllPending("image table atomic test");
    Require(!Faulted(addedFloat), "image table atomic: an add on a float record through a table faulted:\n" + addedFloat.log);
    for (std::uint32_t x = 0; x < Width; ++x) Require(std::bit_cast<std::uint32_t>(FloatTexels[x]) == 0x3f800000u + x, "image table atomic: float texel " + std::to_string(x) + " is " + Hex(std::bit_cast<std::uint32_t>(FloatTexels[x])) + ", expected " + Hex(0x3f800000u + x));
    Keys[0] = 6;
    const auto null = Run(device, AtomicAddLaneTableCode, 1, Lanes, 64);
    AgcDriver::Graphics::StorageTexture::FlushAllPending("image table atomic test");
    for (std::uint32_t x = 0; x < Width; ++x) Require(UintTexels[x] == (x ^ 1u) && std::bit_cast<std::uint32_t>(FloatTexels[x]) == 0x3f800000u + x, "image table atomic: an add through a null entry reached an image:\n" + null.log);
}

void RunAtomic64Tests(AgcDriver::VulkanDevice& device) {
    if (!TargetHasCapability(device.Target(), spv::CapabilityInt64ImageEXT)) {
        std::puts("skipped the 64-bit table atomic rows, the device has no shaderImageInt64Atomics");
        return;
    }
    struct Registration {
        void* data;
        Registration(void* data, std::size_t bytes) : data(data) { GuestAllocations::Mutation().Add(data, bytes, true, true, true); }
        ~Registration() { GuestAllocations::Mutation().Remove(data); }
    } registered(Uint64Texels.data(), sizeof(Uint64Texels));
    SetClassRecords();
    for (std::uint32_t x = 0; x < Width; ++x) Uint64Texels[x] = 0x5000000000000000ull + x;
    SetRecord(9, PointClamp, Image(Uint64Texels.data(), Format32_32UInt));
    Keys = {};
    Keys[0] = 9;
    const auto added = Run(device, AtomicAdd64LaneTableCode, 1, Lanes, 64);
    AgcDriver::Graphics::StorageTexture::FlushAllPending("image table atomic64 test");
    Require(!Faulted(added), "image table atomic64: an add through a table faulted:\n" + added.log);
    Require(added.imageTableCount != 0u, "image table atomic64: the add bound no storage table elements");
    for (std::uint32_t x = 0; x < Width; ++x) Require(Uint64Texels[x] == 0x5000000000000000ull + x + x, "image table atomic64: texel " + std::to_string(x) + " is wrong");
    Keys[0] = 6;
    const auto null = Run(device, AtomicAdd64LaneTableCode, 1, Lanes, 64);
    AgcDriver::Graphics::StorageTexture::FlushAllPending("image table atomic64 test");
    for (std::uint32_t x = 0; x < Width; ++x) Require(Uint64Texels[x] == 0x5000000000000000ull + x + x, "image table atomic64: an add through a null entry reached an image:\n" + null.log);
}

void RunSelectedStoreTests(AgcDriver::VulkanDevice& device) {
    alignas(256) static std::array<std::uint32_t, Width * Height> storeTexels{};
    struct Registration {
        void* data;
        Registration(void* data, std::size_t bytes) : data(data) { GuestAllocations::Mutation().Add(data, bytes, true, true, true); }
        ~Registration() { GuestAllocations::Mutation().Remove(data); }
    } registered(storeTexels.data(), sizeof(storeTexels));
    for (std::uint32_t texel = 0; texel < storeTexels.size(); ++texel) storeTexels[texel] = 0x5000u + texel;
    const auto saved = storeTexels;
    SetClassRecords();
    SetRecord(9, PointClamp, Image(storeTexels.data(), Format32UInt));
    Keys = {};
    Keys[3] = 9u;
    Srt[24] = 12u;
    const auto stored = Run(device, SelectedStoreCode, 1, Lanes, 64);
    AgcDriver::Graphics::StorageTexture::FlushPending(reinterpret_cast<std::uintptr_t>(storeTexels.data()), sizeof(storeTexels), nullptr, "test");
    device.WaitIdle();
    Require(!Faulted(stored) && stored.imageTableCount == 0u, "image table selected store: an image_store through a T# whose record the CPU evaluates faulted or was bound through an image table:\n" + stored.log);
    for (std::uint32_t texel = 0; texel < storeTexels.size(); ++texel) {
        const auto expected = texel < Lanes ? SelectedStoreValue : saved[texel];
        Require(storeTexels[texel] == expected, "image table selected store: texel " + std::to_string(texel) + " holds " + std::to_string(storeTexels[texel]) + " after an image_store through a T# whose record the CPU evaluates, expected " + std::to_string(expected));
    }
}

void RunPointerTests(AgcDriver::VulkanDevice& device) {
    const auto floatImage = Image(FloatTexels.data(), Format32Float);
    const auto uintImage = Image(UintTexels.data(), Format32UInt);
    const auto sintImage = Image(SintTexels.data(), Format32SInt);
    const std::array<std::uint32_t, 4> invalid{0x1234u, 0x5678u, 0x9abcu, 0x1000ffacu};
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

void RunPointerRecordTests(AgcDriver::VulkanDevice& device) {
    Fill();
    const auto floatImage = Image(FloatTexels.data(), Format32Float);
    const auto uintImage = Image(UintTexels.data(), Format32UInt);
    const auto sintImage = Image(SintTexels.data(), Format32SInt);
    const std::array<std::array<std::uint32_t, 4>, 3> images{floatImage, uintImage, sintImage};
    const std::array<std::function<std::uint32_t(std::uint32_t)>, 3> texels{
        [](std::uint32_t x) { return std::bit_cast<std::uint32_t>(FloatTexels[x]); },
        [](std::uint32_t x) { return UintTexels[x]; },
        [](std::uint32_t x) { return static_cast<std::uint32_t>(SintTexels[x]); },
    };
    const std::array<const char*, 3> names{"float", "uint", "sint"};

    Table = {};
    Indirect = {};
    Keys = {};
    for (std::uint32_t group = 0; group < 3; ++group) Keys[group] = 2u - group;
    for (std::uint32_t record = 0; record < 3; ++record) {
        const std::uint32_t entry = 2u - record;
        std::copy(images[record].begin(), images[record].end(), Indirect.data() + entry * 16u + 8u);
        const auto pointer = static_cast<std::uint64_t>(reinterpret_cast<std::uintptr_t>(Indirect.data() + entry * 16u));
        Table[record * (Stride / 4u) + 2u] = static_cast<std::uint32_t>(pointer);
        Table[record * (Stride / 4u) + 3u] = static_cast<std::uint32_t>(pointer >> 32u);
    }
    const auto indirect = RunIndirectRecords(device, IndirectRecordCode, 3);
    Require(!Faulted(indirect), "image table: a T# behind a pointer in its record faulted:\n" + indirect.log);
    Require(indirect.imageTableCount == 3u, "image table: T#s behind pointers in their records do not hold one element per distinct T# (" + std::to_string(indirect.imageTableCount) + ")");
    for (std::uint32_t group = 0; group < 3; ++group) RequireRed(indirect, group, Lanes, texels[2u - group], std::string(names[2u - group]) + " texels of a T# behind a pointer in its record");

    const auto entry = Indirect.data() + 16u;
    std::copy(floatImage.begin(), floatImage.end(), entry + 8u);
    const auto changed = RunIndirectRecords(device, IndirectRecordCode, 3);
    RequireRed(changed, 1, Lanes, texels[0], "float texels after the T# behind record 1's pointer changed");
}

enum class NestedCase { Basic, Large, Budget };
constexpr std::uint32_t NestedParentLimit = 524288u;
constexpr std::uint32_t NestedKeyLimit = 1048576u;

void RunNestedRecordTests(AgcDriver::VulkanDevice& device, NestedCase selected) {
    Fill();
    alignas(512) std::array<std::array<std::uint32_t, 64>, 2> records{};
    alignas(256) std::array<std::array<std::uint32_t, 16>, 2> palettes{};
    std::vector<std::uint32_t> extra;
    std::vector<std::uint32_t> wideRoots;
    std::uint32_t rootBytes = 64u;
    const std::array images{Image(FloatTexels.data(), Format32Float), Image(UintTexels.data(), Format32UInt), Image(SintTexels.data(), Format32SInt)};
    const std::array<std::function<std::uint32_t(std::uint32_t)>, 3> texels{
        [](std::uint32_t x) { return std::bit_cast<std::uint32_t>(FloatTexels[x]); },
        [](std::uint32_t x) { return UintTexels[x]; },
        [](std::uint32_t x) { return static_cast<std::uint32_t>(SintTexels[x]); },
    };
    Table = {};
    Keys = {};
    for (std::uint32_t outer = 0; outer < 2u; ++outer) {
        auto descriptor = Buffer(palettes[outer].data(), 1u);
        descriptor[1] |= 64u << 16u;
        std::copy(descriptor.begin(), descriptor.end(), records[outer].begin() + 40u);
        for (std::uint32_t inner = 0; inner < 2u; ++inner) {
            const auto& image = images[outer + inner];
            std::copy(image.begin(), image.end(), palettes[outer].begin() + inner * 8u);
        }
    }
    for (std::uint32_t record = 0; record < 3u; ++record) {
        const auto pointer = reinterpret_cast<std::uintptr_t>(records[record % 2u].data());
        Table[record * 4u + 2u] = static_cast<std::uint32_t>(pointer);
        Table[record * 4u + 3u] = static_cast<std::uint32_t>(pointer >> 32u);
    }
    auto phi = std::vector<std::uint32_t>(NestedRecordCode.begin(), NestedRecordCode.end());
    phi.insert(phi.begin() + 17u, {0xbf068011u, 0xbf840002u, 0xbeac03ffu, 0x000000eau});
    const auto run = [&](std::span<const std::uint32_t> code = NestedRecordCode, std::uint32_t waveSize = 64u) {
        auto table = Buffer(Table.data(), rootBytes);
        if (!wideRoots.empty()) {
            table = Buffer(wideRoots.data(), static_cast<std::uint32_t>(wideRoots.size() / 4u));
            table[1] |= 16u << 16u;
            table[3] = 0x00005204u;
        }
        const auto keys = Buffer(Keys.data(), static_cast<std::uint32_t>(sizeof(Keys)));
        const auto output = Buffer(Output.data(), static_cast<std::uint32_t>(sizeof(Output)));
        std::copy(table.begin(), table.end(), Srt.begin());
        std::copy(keys.begin(), keys.end(), Srt.begin() + 4u);
        std::copy(output.begin(), output.end(), Srt.begin() + 8u);
        std::copy(PointClamp.begin(), PointClamp.end(), Srt.begin() + 12u);
        std::vector<ShaderRecompiler::MemoryRegion> memory{
            {reinterpret_cast<std::uintptr_t>(code.data()), std::as_bytes(code)},
            {reinterpret_cast<std::uintptr_t>(Srt.data()), std::as_bytes(std::span(Srt))},
            {reinterpret_cast<std::uintptr_t>(Table.data()), std::as_bytes(std::span(Table))},
            {reinterpret_cast<std::uintptr_t>(Keys.data()), std::as_bytes(std::span(Keys))},
            {reinterpret_cast<std::uintptr_t>(records.data()), std::as_bytes(std::span(records))},
            {reinterpret_cast<std::uintptr_t>(palettes.data()), std::as_bytes(std::span(palettes))},
        };
        if (!extra.empty()) memory.push_back({reinterpret_cast<std::uintptr_t>(extra.data()), std::as_bytes(std::span(extra))});
        if (!wideRoots.empty()) memory.push_back({reinterpret_cast<std::uintptr_t>(wideRoots.data()), std::as_bytes(std::span(wideRoots))});
        const auto inputTable = Table;
        const auto inputKeys = Keys;
        const auto inputRecords = records;
        const auto inputPalettes = palettes;
        const auto inputExtra = extra;
        const auto inputRoots = wideRoots;
        auto result = Execute(device, code, Srt.data(), memory, 4u, Lanes, waveSize);
        Require(Table == inputTable && Keys == inputKeys && records == inputRecords && palettes == inputPalettes && extra == inputExtra && wideRoots == inputRoots, "nested image table: dispatch modified its input memory");
        Require(std::all_of(Output.begin() + 4u * Lanes * 4u, Output.end(), [](auto word) { return word == 0xdeadbeefu; }), "nested image table: dispatch wrote past its output");
        return result;
    };
    const std::array<std::uint32_t, 8> keys{0u, 0u, 0u, 1u, 1u, 0u, 1u, 1u};
    std::copy(keys.begin(), keys.end(), Keys.begin());
    Keys[8] = 3u;
    if (selected != NestedCase::Basic) {
        const auto putRoot = [&](std::uint32_t index, const void* pointer) {
            const auto address = reinterpret_cast<std::uintptr_t>(pointer);
            wideRoots[index * 4u + 2u] = static_cast<std::uint32_t>(address);
            wideRoots[index * 4u + 3u] = static_cast<std::uint32_t>(address >> 32u);
        };
        if (selected == NestedCase::Large) {
            constexpr std::uint32_t Parents = 0x54ffcu;
            constexpr std::uint32_t Children = 0x20000u;
            wideRoots.resize(Parents * 4u, 0u);
            putRoot(0u, records[0].data());
            putRoot(Parents - 1u, records[1].data());
            extra.resize(Children * 8u, 0u);
            std::copy(images[0].begin(), images[0].end(), extra.begin());
            std::copy(images[2].begin(), images[2].end(), extra.end() - 8u);
            auto descriptor = Buffer(extra.data(), 0x10000u);
            descriptor[1] |= 64u << 16u;
            descriptor[3] = 0x00005204u;
            std::copy(descriptor.begin(), descriptor.end(), records[1].begin() + 40u);
            Keys = {};
            for (std::uint32_t group = 0; group < 4u; ++group) {
                Keys[group * 2u] = Parents - 1u;
                Keys[group * 2u + 1u] = group % 2u == 0u ? Children - 1u : 0u;
            }
            const auto direct = run();
            Require(!Faulted(direct), "nested image table: the reported parent and child domains were truncated");
            for (std::uint32_t group = 0; group < 4u; ++group) RequireRed(direct, group, Lanes, texels[group % 2u == 0u ? 2u : 0u], "nested image table: first and last keys of the reported domains");
            const auto merged = run(phi);
            Require(!Faulted(merged) && merged.words == direct.words, "nested image table: pointer Phi changed the large-domain readback");
            Require(merged.tableMap.size() < Children + 1024u, "nested image table: a Cartesian product was allocated");
            std::puts("nested image table: 348156 parents, 131072 inner keys and pointer Phi checked");
            return;
        }
        wideRoots.resize((NestedParentLimit + 1u) * 4u, 0u);
        putRoot(0u, records[0].data());
        putRoot(NestedParentLimit, records[1].data());
        Keys = {};
        Keys[0] = NestedParentLimit;
        const auto parentBound = run();
        Require(Faulted(parentBound) && parentBound.log.find("outside the snapshot") != std::string::npos, "nested image table: a parent beyond the scan budget did not fault");
        Require(std::all_of(parentBound.words.begin(), parentBound.words.end(), [](auto word) { return word == 0xdeadbeefu; }), "nested image table: an uncaptured parent published copied output");
        putRoot(NestedParentLimit, records[0].data());
        const auto knownAlias = run();
        Require(!Faulted(knownAlias), "nested image table: a pointer already captured at another index was refused");
        RequireRed(knownAlias, 0u, Lanes, texels[0], "nested image table: captured pointer reached past the scanned prefix");
        wideRoots.clear();
        std::copy(keys.begin(), keys.end(), Keys.begin());
        std::fprintf(stderr, "nested image table: checking snapshot budget\n");
        extra.resize((NestedKeyLimit + 1u) * 8u, 0u);
        std::copy(images[0].begin(), images[0].end(), extra.begin());
        const auto large = Buffer(extra.data(), static_cast<std::uint32_t>(extra.size() * sizeof(std::uint32_t)));
        std::copy(large.begin(), large.end(), records[0].begin() + 40u);
        Keys[0] = 0u;
        Keys[1] = 0u;
        const auto bounded = run();
        Require(Faulted(bounded) && bounded.log.find("outside the snapshot") != std::string::npos, "nested image table: an uncaptured child did not fault:\n" + bounded.log);
        Require(bounded.tableMap.size() < NestedKeyLimit + 1024u, "nested image table: the two levels produced an unbounded map");
        Require(std::all_of(bounded.words.begin(), bounded.words.end(), [](auto word) { return word == 0xdeadbeefu; }), "nested image table: a dispatch outside the snapshot published copied output");
        std::copy(large.begin(), large.end(), records[1].begin() + 40u);
        const auto sharedBudget = run();
        Require(!Faulted(sharedBudget) && sharedBudget.tableMap.size() < NestedKeyLimit + 1024u, "nested image table: equal V#s consumed the snapshot budget twice");
        RequireRed(sharedBudget, 2u, Lanes, texels[0], "nested image table: aliased parent at the snapshot budget");
        std::puts("nested image table parent and child budgets checked");
        return;
    }
    const auto original = run();
    Require(!Faulted(original), "nested image table: valid independent keys faulted:\n" + original.log);
    const auto wave32 = run(NestedRecordCode, 32u);
    Require(!Faulted(wave32) && wave32.words == original.words, "nested image table: wave32 and wave64 disagree");
    Require(original.imageTableCount == 3u, "nested image table: duplicate descriptors did not share one image");
    for (std::uint32_t group = 0; group < 4u; ++group) RequireRed(original, group, Lanes, texels[keys[group * 2u] + keys[group * 2u + 1u]], "nested image table independent keys");
    Require(original.imageTableReadRanges.size() == 1u, "nested image table: missing read ranges");
    const auto covers = [&](const void* pointer, std::size_t bytes) {
        const auto address = reinterpret_cast<std::uintptr_t>(pointer);
        return std::ranges::any_of(original.imageTableReadRanges[0], [&](const auto& range) { return address >= range.first && address - range.first <= range.second && bytes <= range.second - (address - range.first); });
    };
    for (std::uint32_t parent = 0; parent < 2u; ++parent) {
        Require(covers(records[parent].data() + 40u, 16u) && covers(palettes[parent].data(), 64u), "nested image table: a V# or its palette is absent from the write-overlap ranges");
    }
    auto loop = std::vector<std::uint32_t>(NestedRecordCode.begin(), NestedRecordCode.end() - 1u);
    const auto secondLow = static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(records[1].data()));
    loop.insert(loop.end(), {0xbf06ff2cu, secondLow, 0xbf850003u, 0xbeac03ffu, secondLow, 0xbf82ffe7u, 0xbf810000u});
    for (std::uint32_t group = 0; group < 4u; ++group) Keys[group * 2u] = 0u;
    const auto carried = run(loop);
    Require(!Faulted(carried), "nested image table: a valid pointer carried around a loop faulted");
    for (std::uint32_t group = 0; group < 4u; ++group) RequireRed(carried, group, Lanes, texels[1u + Keys[group * 2u + 1u]], "nested image table: the pointer selected on the back edge");
    auto loopFault = loop;
    loopFault[37] = 0xeau;
    loopFault[40] = 0xeau;
    const auto rejectedLoop = run(loopFault);
    Require(Faulted(rejectedLoop) && std::all_of(rejectedLoop.words.begin(), rejectedLoop.words.end(), [](auto word) { return word == 0xdeadbeefu; }), "nested image table: an invalid back-edge pointer committed partial output");
    std::copy(keys.begin(), keys.end(), Keys.begin());
    std::copy(images[0].begin(), images[0].end(), palettes[1].begin());
    const auto edited = run();
    Require(!Faulted(edited) && Group(edited, 2) == Group(original, 0) && Group(edited, 3) == Group(original, 3), "nested image table: changing only a child map retained a stale invocation");
    Require(edited.artifactId == original.artifactId, "nested image table: editing a child map recompiled the shader");
    std::copy(images[1].begin(), images[1].end(), palettes[1].begin());
    Keys[0] = 2u;
    const auto alias = run();
    Require(!Faulted(alias) && Group(alias, 0) == Group(original, 0), "nested image table: an aliased parent changed the result");
    std::copy_n(records[0].begin() + 40u, 4u, records[1].begin() + 40u);
    const auto moved = run();
    Require(!Faulted(moved) && Group(moved, 2) == Group(original, 0) && Group(moved, 3) == Group(original, 1), "nested image table: replacing the nested V# retained stale texels");
    Require(moved.artifactId == original.artifactId, "nested image table: moving a descriptor recompiled the shader");
    Keys[0] = 3u;
    const auto unmapped = run();
    Require(Faulted(unmapped) && unmapped.log.find("unmapped") != std::string::npos, "nested image table: an unreadable parent did not fault:\n" + unmapped.log);
    Require(std::all_of(unmapped.words.begin(), unmapped.words.end(), [](auto word) { return word == 0xdeadbeefu; }), "nested image table: a faulting dispatch published copied output");
    Keys[0] = 0u;
    Keys[1] = 2u;
    const auto outside = run();
    const auto outsideWords = Group(outside, 0);
    Require(!Faulted(outside) && std::all_of(outsideWords.begin(), outsideWords.end(), [](auto word) { return word == 0u; }), "nested image table: a scalar load past the nested buffer did not read zeros");
    Keys[1] = 0x08000000u;
    const auto wrapped = run();
    Require(!Faulted(wrapped) && Group(wrapped, 0) == Group(original, 0), "nested image table: the inner offset did not wrap at 32 bits");
    Keys[1] = 0u;
    records[0][43] |= 1u << 30u;
    const auto notBuffer = run();
    Require(Faulted(notBuffer) && notBuffer.log.find("not a buffer") != std::string::npos, "nested image table: a non-buffer parent was accepted:\n" + notBuffer.log);
    records[0][43] &= ~(1u << 30u);
    rootBytes = 0u;
    const auto empty = run();
    Require(Faulted(empty) && empty.log.find("unmapped") != std::string::npos, "nested image table: an empty pointer dictionary did not fault");
    rootBytes = 64u;
    const auto originalHigh = Table[7];
    const auto originalLow = Table[6];
    Table[6] = Table[2];
    Table[7] = 0xffffffffu;
    Keys = {};
    Keys[8] = 1u;
    const auto highControl = run();
    Require(!Faulted(highControl), "nested image table: a different high pointer word rejected the valid pointer");
    RequireRed(highControl, 0u, Lanes, texels[0], "nested image table: low-word alias control");
    Keys[0] = 1u;
    const auto highFault = run();
    Require(Faulted(highFault) && highFault.log.find("unmapped") != std::string::npos, "nested image table: the high pointer word was ignored");
    Table[7] = 0xfffffffeu;
    const auto movedFault = run();
    Require(Faulted(movedFault) && movedFault.log.find("unmapped") != std::string::npos, "nested image table: a changed poison pointer reused a stale map");
    Table[6] = originalLow;
    Table[7] = originalHigh;
    std::copy(keys.begin(), keys.end(), Keys.begin());
    for (std::uint32_t group = 0; group < 4u; ++group) Keys[group * 2u] = 1u;
    const auto phiValid = run(phi);
    Require(!Faulted(phiValid), "nested image table: a captured pointer selected through Phi faulted");
    for (std::uint32_t group = 0; group < 4u; ++group) RequireRed(phiValid, group, Lanes, texels[Keys[group * 2u + 1u]], "nested image table: Phi-selected pointer");
    Keys[0] = 0u;
    auto guardedPhi = phi;
    guardedPhi.insert(guardedPhi.begin() + 32u, {0xbf068011u, 0xbf850008u});
    const auto inactivePhi = run(guardedPhi);
    Require(!Faulted(inactivePhi), "nested image table: an inactive image access reported the uncaptured Phi arm");
    const auto inactiveWords = Group(inactivePhi, 0u);
    Require(std::all_of(inactiveWords.begin(), inactiveWords.end(), [](auto word) { return word == 0xdeadbeefu; }), "nested image table: the inactive image arm wrote output");
    for (std::uint32_t group = 1u; group < 4u; ++group) Require(Group(inactivePhi, group) == Group(phiValid, group), "nested image table: guarding the uncaptured arm changed active groups");
    const auto phiFault = run(phi);
    Require(Faulted(phiFault) && phiFault.log.find("outside the snapshot") != std::string::npos, "nested image table: the uncaptured Phi arm was assumed unreachable");
    Require(std::all_of(phiFault.words.begin(), phiFault.words.end(), [](auto word) { return word == 0xdeadbeefu; }), "nested image table: an uncaptured pointer published copied output");
    Require(phiFault.artifactId == phiValid.artifactId, "nested image table: the pointer choice was specialized on the CPU");
    std::puts("nested image table independent keys, alias, descriptor replacement, faults and budget checked");
}

void RunPointerChainRecordTests(AgcDriver::VulkanDevice& device) {
    Fill();
    const auto floatImage = Image(FloatTexels.data(), Format32Float);
    const auto uintImage = Image(UintTexels.data(), Format32UInt);
    const auto sintImage = Image(SintTexels.data(), Format32SInt);
    const std::array<std::array<std::uint32_t, 4>, 3> images{floatImage, uintImage, sintImage};
    const std::array<std::function<std::uint32_t(std::uint32_t)>, 3> texels{
        [](std::uint32_t x) { return std::bit_cast<std::uint32_t>(FloatTexels[x]); },
        [](std::uint32_t x) { return UintTexels[x]; },
        [](std::uint32_t x) { return static_cast<std::uint32_t>(SintTexels[x]); },
    };
    const std::array<const char*, 3> names{"float", "uint", "sint"};
    const auto store = [](std::uint32_t* at, const void* pointer) {
        const auto address = static_cast<std::uint64_t>(reinterpret_cast<std::uintptr_t>(pointer));
        at[0] = static_cast<std::uint32_t>(address);
        at[1] = static_cast<std::uint32_t>(address >> 32u);
    };

    Table = {};
    Indirect = {};
    Links = {};
    Keys = {};
    for (std::uint32_t group = 0; group < 3; ++group) Keys[group] = 2u - group;
    for (std::uint32_t record = 0; record < 3; ++record) {
        const std::uint32_t entry = 2u - record;
        std::copy(images[record].begin(), images[record].end(), Indirect.data() + entry * 16u + 8u);
        store(Links.data() + record * 8u + 4u, Indirect.data() + entry * 16u);
        store(Table.data() + record * (Stride / 4u) + 2u, Links.data() + record * 8u);
    }
    const auto chained = RunIndirectRecords(device, PointerChainRecordCode, 3);
    Require(!Faulted(chained), "image table: a T# behind two pointers from its record faulted:\n" + chained.log);
    Require(chained.imageTableCount == 3u, "image table: T#s behind two pointers from their records do not hold one element per distinct T# (" + std::to_string(chained.imageTableCount) + ")");
    for (std::uint32_t group = 0; group < 3; ++group) RequireRed(chained, group, Lanes, texels[2u - group], std::string(names[2u - group]) + " texels of a T# behind two pointers from its record");

    store(Links.data() + 8u + 4u, Indirect.data() + 2u * 16u);
    const auto relinked = RunIndirectRecords(device, PointerChainRecordCode, 3);
    RequireRed(relinked, 1, Lanes, texels[0], "float texels after record 1's second pointer moved to another T#");
}

void RunNestedSamplerTests(AgcDriver::VulkanDevice& device) {
    Fill();
    alignas(512) std::array<std::array<std::uint32_t, 64>, 2> records{};
    alignas(256) std::array<std::array<std::uint32_t, 16>, 2> palettes{};
    std::vector<std::uint32_t> extra;
    const std::array<std::array<std::array<std::uint32_t, 4>, 2>, 2> samplers{{{PointWrap, PointClamp}, {PointClamp, PointWrap}}};
    Table = {};
    Keys = {};
    for (std::uint32_t outer = 0; outer < 2u; ++outer) {
        auto descriptor = Buffer(palettes[outer].data(), 1u);
        descriptor[1] |= 64u << 16u;
        std::copy(descriptor.begin(), descriptor.end(), records[outer].begin() + 40u);
        for (std::uint32_t inner = 0; inner < 2u; ++inner) std::copy(samplers[outer][inner].begin(), samplers[outer][inner].end(), palettes[outer].begin() + inner * 4u);
    }
    for (std::uint32_t record = 0; record < 3u; ++record) {
        const auto pointer = reinterpret_cast<std::uintptr_t>(records[record % 2u].data());
        Table[record * 4u + 2u] = static_cast<std::uint32_t>(pointer);
        Table[record * 4u + 3u] = static_cast<std::uint32_t>(pointer >> 32u);
    }
    const auto image = Image(FloatTexels.data(), Format32Float);
    const auto run = [&](std::span<const std::uint32_t> code = NestedSamplerCode, std::uint32_t waveSize = 64u) {
        const auto table = Buffer(Table.data(), 64u);
        const auto keys = Buffer(Keys.data(), static_cast<std::uint32_t>(sizeof(Keys)));
        const auto output = Buffer(Output.data(), static_cast<std::uint32_t>(sizeof(Output)));
        Srt = {};
        std::copy(table.begin(), table.end(), Srt.begin());
        std::copy(keys.begin(), keys.end(), Srt.begin() + 4u);
        std::copy(output.begin(), output.end(), Srt.begin() + 8u);
        std::copy(image.begin(), image.end(), Srt.begin() + 12u);
        std::vector<ShaderRecompiler::MemoryRegion> memory{
            {reinterpret_cast<std::uintptr_t>(code.data()), std::as_bytes(code)},
            {reinterpret_cast<std::uintptr_t>(Srt.data()), std::as_bytes(std::span(Srt))},
            {reinterpret_cast<std::uintptr_t>(Table.data()), std::as_bytes(std::span(Table))},
            {reinterpret_cast<std::uintptr_t>(Keys.data()), std::as_bytes(std::span(Keys))},
            {reinterpret_cast<std::uintptr_t>(records.data()), std::as_bytes(std::span(records))},
            {reinterpret_cast<std::uintptr_t>(palettes.data()), std::as_bytes(std::span(palettes))},
        };
        if (!extra.empty()) memory.push_back({reinterpret_cast<std::uintptr_t>(extra.data()), std::as_bytes(std::span(extra))});
        const auto inputTable = Table;
        const auto inputKeys = Keys;
        const auto inputRecords = records;
        const auto inputPalettes = palettes;
        const auto inputExtra = extra;
        auto result = Execute(device, code, Srt.data(), memory, 4u, Lanes, waveSize);
        Require(Table == inputTable && Keys == inputKeys && records == inputRecords && palettes == inputPalettes && extra == inputExtra, "nested sampler table: dispatch modified its input memory");
        Require(std::all_of(Output.begin() + 4u * Lanes * 4u, Output.end(), [](auto word) { return word == 0xdeadbeefu; }), "nested sampler table: dispatch wrote past its output");
        return result;
    };
    const auto sampled = [&](std::uint32_t record, std::uint32_t child) {
        const bool wrap = samplers[record % 2u][child] == PointWrap;
        return std::function<std::uint32_t(std::uint32_t)>([wrap](std::uint32_t x) { return std::bit_cast<std::uint32_t>(FloatTexels[wrap ? x : Width - 1u]); });
    };
    const auto untouched = [](const Outcome& outcome) { return std::all_of(outcome.words.begin(), outcome.words.end(), [](auto word) { return word == 0xdeadbeefu; }); };
    const std::array<std::uint32_t, 8> keys{0u, 0u, 0u, 1u, 1u, 0u, 1u, 1u};
    std::copy(keys.begin(), keys.end(), Keys.begin());
    const auto original = run();
    Require(!Faulted(original), "nested sampler table: valid independent keys faulted:\n" + original.log);
    for (std::uint32_t group = 0; group < 4u; ++group) RequireRed(original, group, Lanes, sampled(keys[group * 2u], keys[group * 2u + 1u]), "nested sampler table: the sampler selected through record, pointer and V#");
    const auto wave32 = run(NestedSamplerCode, 32u);
    Require(!Faulted(wave32) && wave32.words == original.words, "nested sampler table: wave32 and wave64 disagree");
    std::copy(PointWrap.begin(), PointWrap.end(), palettes[1].begin());
    const auto edited = run();
    Require(!Faulted(edited) && Group(edited, 2) == Group(original, 0) && Group(edited, 3) == Group(original, 3), "nested sampler table: changing only a child S# retained a stale sampler");
    Require(edited.artifactId == original.artifactId, "nested sampler table: editing a child S# recompiled the shader");
    std::copy(PointClamp.begin(), PointClamp.end(), palettes[1].begin());
    Keys[0] = 3u;
    const auto unmapped = run();
    Require(Faulted(unmapped) && unmapped.log.find("unmapped") != std::string::npos, "nested sampler table: an unreadable parent did not fault:\n" + unmapped.log);
    Require(untouched(unmapped), "nested sampler table: a faulting dispatch published copied output");
    Keys[0] = 0u;
    records[0][43] |= 1u << 30u;
    const auto notBuffer = run();
    Require(Faulted(notBuffer) && notBuffer.log.find("not a buffer") != std::string::npos, "nested sampler table: a non-buffer parent was accepted:\n" + notBuffer.log);
    Require(untouched(notBuffer), "nested sampler table: a non-buffer parent published copied output");
    records[0][43] &= ~(1u << 30u);
    const auto savedRecords = records;
    std::copy_n(records[0].begin() + 40u, 4u, records[1].begin() + 40u);
    const auto moved = run();
    Require(!Faulted(moved) && Group(moved, 2) == Group(original, 0) && Group(moved, 3) == Group(original, 1), "nested sampler table: replacing the nested V# retained a stale sampler");
    Require(moved.artifactId == original.artifactId && moved.tableMap.size() < original.tableMap.size(), "nested sampler table: equal V#s did not share one child table");
    records = savedRecords;
    auto phi = std::vector<std::uint32_t>(NestedSamplerCode.begin(), NestedSamplerCode.end());
    phi.insert(phi.begin() + 17u, {0xbf068011u, 0xbf840002u, 0xbeac03ffu, 0x000000eau});
    for (std::uint32_t group = 0; group < 4u; ++group) Keys[group * 2u] = 1u;
    const auto phiValid = run(phi);
    Require(!Faulted(phiValid), "nested sampler table: a captured pointer selected through Phi faulted:\n" + phiValid.log);
    for (std::uint32_t group = 0; group < 4u; ++group) RequireRed(phiValid, group, Lanes, sampled(1u, Keys[group * 2u + 1u]), "nested sampler table: Phi-selected pointer");
    Keys[0] = 0u;
    const auto phiFault = run(phi);
    Require(Faulted(phiFault) && phiFault.log.find("outside the snapshot") != std::string::npos, "nested sampler table: an uncaptured pointer did not fault:\n" + phiFault.log);
    Require(untouched(phiFault), "nested sampler table: an uncaptured pointer published copied output");
    Require(phiFault.artifactId == phiValid.artifactId, "nested sampler table: the pointer choice was specialized on the CPU");
    std::copy(keys.begin(), keys.end(), Keys.begin());
    extra.assign(320u * 4u, 0u);
    for (std::uint32_t entry = 0; entry < 320u; ++entry) {
        std::copy(PointWrap.begin(), PointWrap.end(), extra.begin() + entry * 4u);
        extra[entry * 4u + 2u] |= entry;
    }
    const auto many = Buffer(extra.data(), static_cast<std::uint32_t>(extra.size() * sizeof(std::uint32_t)));
    std::copy(many.begin(), many.end(), records[0].begin() + 40u);
    Keys[1] = 319u;
    auto unkeyed = std::vector<std::uint32_t>(NestedSamplerCode.begin(), NestedSamplerCode.end());
    unkeyed.insert(unkeyed.begin() + 14u, {0x8714ff14u, 0x000001ffu});
    const auto overBudget = run(unkeyed);
    Require(Faulted(overBudget) && overBudget.log.find("over budget") != std::string::npos, "nested sampler table: an S# past the sampler budget did not fault:\n" + overBudget.log);
    Require(untouched(overBudget), "nested sampler table: an S# past the sampler budget published copied output");
    extra.assign((NestedKeyLimit + 1u) * 4u, 0u);
    std::copy(PointClamp.begin(), PointClamp.end(), extra.begin());
    const auto large = Buffer(extra.data(), static_cast<std::uint32_t>(extra.size() * sizeof(std::uint32_t)));
    std::copy(large.begin(), large.end(), records[0].begin() + 40u);
    Keys[1] = 0u;
    const auto bounded = run();
    Require(Faulted(bounded) && bounded.log.find("outside the snapshot") != std::string::npos, "nested sampler table: an uncaptured child did not fault:\n" + bounded.log);
    Require(bounded.tableMap.size() < NestedKeyLimit + 1024u, "nested sampler table: the two levels produced an unbounded map");
    Require(untouched(bounded), "nested sampler table: a dispatch outside the snapshot published copied output");
    std::copy(large.begin(), large.end(), records[1].begin() + 40u);
    const auto sharedBudget = run();
    Require(!Faulted(sharedBudget) && sharedBudget.tableMap.size() < NestedKeyLimit + 1024u, "nested sampler table: equal V#s consumed the snapshot budget twice");
    for (std::uint32_t group = 0; group < 4u; ++group) {
        const bool clamp = keys[group * 2u + 1u] == 0u;
        RequireRed(sharedBudget, group, Lanes, [clamp](std::uint32_t x) { return std::bit_cast<std::uint32_t>(FloatTexels[clamp ? Width - 1u : x]); }, "nested sampler table: aliased parent at the snapshot budget");
    }
    std::puts("nested sampler table: S# through record, pointer and V# checked on wave64 and wave32, with sharing and budgets");
}

void RunPointerSamplerTests(AgcDriver::VulkanDevice& device) {
    Fill();
    alignas(256) std::array<std::array<std::uint32_t, 64>, 2> records{};
    const std::array<std::array<std::uint32_t, 4>, 2> samplers{PointWrap, PointClamp};
    Table = {};
    Keys = {};
    const std::array images{Image(FloatTexels.data(), Format32Float), Image(OtherFloatTexels.data(), Format32Float)};
    for (std::uint32_t record = 0; record < 2u; ++record) {
        std::copy(samplers[record].begin(), samplers[record].end(), records[record].begin() + 8u);
        std::copy(images[record].begin(), images[record].end(), records[record].begin() + 16u);
    }
    for (std::uint32_t record = 0; record < 3u; ++record) {
        const auto pointer = reinterpret_cast<std::uintptr_t>(records[record % 2u].data());
        Table[record * 4u + 2u] = static_cast<std::uint32_t>(pointer);
        Table[record * 4u + 3u] = static_cast<std::uint32_t>(pointer >> 32u);
    }
    auto root = images[0];
    std::uint32_t tableType = 0u;
    const auto run = [&](std::span<const std::uint32_t> code, std::uint32_t waveSize = 64u) {
        auto table = Buffer(Table.data(), 64u);
        table[3] |= tableType;
        const auto keys = Buffer(Keys.data(), static_cast<std::uint32_t>(sizeof(Keys)));
        const auto output = Buffer(Output.data(), static_cast<std::uint32_t>(sizeof(Output)));
        Srt = {};
        std::copy(table.begin(), table.end(), Srt.begin());
        std::copy(keys.begin(), keys.end(), Srt.begin() + 4u);
        std::copy(output.begin(), output.end(), Srt.begin() + 8u);
        std::copy(root.begin(), root.end(), Srt.begin() + 12u);
        const std::vector<ShaderRecompiler::MemoryRegion> memory{
            {reinterpret_cast<std::uintptr_t>(code.data()), std::as_bytes(code)},
            {reinterpret_cast<std::uintptr_t>(Srt.data()), std::as_bytes(std::span(Srt))},
            {reinterpret_cast<std::uintptr_t>(Table.data()), std::as_bytes(std::span(Table))},
            {reinterpret_cast<std::uintptr_t>(Keys.data()), std::as_bytes(std::span(Keys))},
            {reinterpret_cast<std::uintptr_t>(records.data()), std::as_bytes(std::span(records))},
        };
        const auto inputTable = Table;
        const auto inputKeys = Keys;
        const auto inputRecords = records;
        auto result = Execute(device, code, Srt.data(), memory, 4u, Lanes, waveSize);
        Require(Table == inputTable && Keys == inputKeys && records == inputRecords, "pointer sampler table: dispatch modified its input memory");
        Require(std::all_of(Output.begin() + 4u * Lanes * 4u, Output.end(), [](auto word) { return word == 0xdeadbeefu; }), "pointer sampler table: dispatch wrote past its output");
        return result;
    };
    const auto sampled = [](bool wrap) {
        return std::function<std::uint32_t(std::uint32_t)>([wrap](std::uint32_t x) { return std::bit_cast<std::uint32_t>(FloatTexels[wrap ? x : Width - 1u]); });
    };
    const auto untouched = [](const Outcome& outcome) { return std::all_of(outcome.words.begin(), outcome.words.end(), [](auto word) { return word == 0xdeadbeefu; }); };
    const std::array<std::uint32_t, 4> keys{1u, 2u, 1u, 2u};
    for (std::uint32_t group = 0; group < 4u; ++group) Keys[group * 2u] = keys[group];
    const auto direct = run(PointerSamplerCode);
    Require(!Faulted(direct), "pointer sampler table: an S# read through a record's pointer faulted:\n" + direct.log);
    for (std::uint32_t group = 0; group < 4u; ++group) RequireRed(direct, group, Lanes, sampled(keys[group] % 2u == 0u), "pointer sampler table: the S# read through the record's pointer");
    auto phi = std::vector<std::uint32_t>(PointerSamplerCode.begin(), PointerSamplerCode.end());
    phi.insert(phi.begin() + 14u, {0xbf068011u, 0xbf840002u, 0xbeac03ffu, 0x000000eau});
    const auto original = run(phi);
    Require(!Faulted(original) && original.words == direct.words, "pointer sampler table: a captured pointer selected through Phi changed the result:\n" + original.log);
    const auto wave32 = run(phi, 32u);
    Require(!Faulted(wave32) && wave32.words == original.words, "pointer sampler table: wave32 and wave64 disagree");
    std::copy(PointWrap.begin(), PointWrap.end(), records[1].begin() + 8u);
    const auto edited = run(phi);
    Require(!Faulted(edited) && Group(edited, 0) == Group(original, 1) && Group(edited, 2) == Group(original, 3), "pointer sampler table: changing an S# retained a stale sampler");
    Require(edited.artifactId == original.artifactId, "pointer sampler table: editing an S# recompiled the shader");
    std::copy(PointClamp.begin(), PointClamp.end(), records[1].begin() + 8u);
    Keys[0] = 0u;
    const auto uncaptured = run(phi);
    Require(Faulted(uncaptured) && uncaptured.log.find("outside the snapshot") != std::string::npos, "pointer sampler table: an uncaptured pointer did not fault:\n" + uncaptured.log);
    Require(untouched(uncaptured), "pointer sampler table: an uncaptured pointer published copied output");
    Require(uncaptured.artifactId == original.artifactId, "pointer sampler table: the pointer choice was specialized on the CPU");
    auto alias = phi;
    alias[17] = static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(records[1].data()));
    const auto aliased = run(alias);
    Require(!Faulted(aliased) && Group(aliased, 0) == Group(original, 0) && Group(aliased, 1) == Group(original, 1), "pointer sampler table: a Phi constant naming another captured pointer did not read that pointer's S#:\n" + aliased.log);
    Keys[0] = 3u;
    const auto unmapped = run(phi);
    Require(Faulted(unmapped) && unmapped.log.find("unmapped") != std::string::npos, "pointer sampler table: a null pointer did not fault:\n" + unmapped.log);
    Require(untouched(unmapped), "pointer sampler table: a null pointer published copied output");
    Keys[0] = 1u;
    tableType = 1u << 30u;
    const auto notBuffer = run(phi);
    Require(Faulted(notBuffer) && notBuffer.log.find("not a buffer") != std::string::npos, "pointer sampler table: a non-buffer record table was accepted:\n" + notBuffer.log);
    Require(untouched(notBuffer), "pointer sampler table: a non-buffer record table published copied output");
    tableType = 0u;
    struct Registration {
        void* data;
        Registration(void* data, std::size_t bytes) : data(data) { GuestAllocations::Mutation().Add(data, bytes, true, true, true); }
        ~Registration() { GuestAllocations::Mutation().Remove(data); }
    };
    {
        const Registration registered(records.data(), sizeof(records));
        auto consumer = phi;
        consumer.insert(consumer.begin() + 30u, 0x7e0e0221u);
        const auto shared = run(consumer);
        Require(!Faulted(shared), "pointer sampler table: an S# with another consumer faulted:\n" + shared.log);
        for (std::uint32_t group = 0; group < 4u; ++group) {
            auto expected = Group(original, group);
            for (std::uint32_t lane = 0; lane < Lanes; ++lane) expected[lane * 4u + 3u] = samplers[keys[group] % 2u][1];
            Require(Group(shared, group) == expected, "pointer sampler table: an S# with another consumer read the wrong words");
        }
    }
    root = PointWrap;
    auto imagePhi = std::vector<std::uint32_t>(PointerImageCode.begin(), PointerImageCode.end());
    imagePhi.insert(imagePhi.begin() + 14u, {0xbf068011u, 0xbf840002u, 0xbeac03ffu, 0x000000eau});
    const auto imageDirect = run(PointerImageCode);
    const auto imageMerged = run(imagePhi);
    Require(!Faulted(imageDirect) && !Faulted(imageMerged) && imageMerged.words == imageDirect.words, "pointer image table: a captured pointer selected through Phi changed the result:\n" + imageMerged.log);
    for (std::uint32_t group = 0; group < 4u; ++group) {
        const bool other = keys[group] % 2u != 0u;
        RequireRed(imageMerged, group, Lanes, [other](std::uint32_t x) { return std::bit_cast<std::uint32_t>(other ? OtherFloatTexels[x] : FloatTexels[x]); }, "pointer image table: the T# read through a Phi-selected pointer");
    }
    Keys[0] = 0u;
    const auto imageFault = run(imagePhi);
    Require(Faulted(imageFault) && imageFault.log.find("outside the snapshot") != std::string::npos && untouched(imageFault), "pointer image table: an uncaptured pointer did not fault:\n" + imageFault.log);
    std::puts("pointer sampler table: S# and T# through a record pointer whose low word passes a Phi checked on wave64 and wave32");
}

void RunPointerArmTests(AgcDriver::VulkanDevice& device, bool copies) {
    Fill();
    alignas(256) std::array<std::array<std::uint32_t, 64>, 2> records{};
    Table = {};
    Keys = {};
    const std::array images{Image(FloatTexels.data(), Format32Float), Image(OtherFloatTexels.data(), Format32Float)};
    for (std::uint32_t record = 0; record < 2u; ++record) std::copy(images[record].begin(), images[record].end(), records[record].begin() + 16u);
    for (std::uint32_t record = 0; record < 3u; ++record) {
        const auto pointer = reinterpret_cast<std::uintptr_t>(records[record % 2u].data());
        Table[record * 4u + 2u] = static_cast<std::uint32_t>(pointer);
        Table[record * 4u + 3u] = static_cast<std::uint32_t>(pointer >> 32u);
    }
    const std::span<const std::uint32_t> code = copies ? std::span<const std::uint32_t>(PointerCopiesCode) : std::span<const std::uint32_t>(PointerBallotCode);
    const std::string name = copies ? "pointer table with a second record read" : "pointer table with a lane-mask arm";
    const auto run = [&] {
        const auto table = Buffer(Table.data(), 64u);
        const auto keys = Buffer(Keys.data(), static_cast<std::uint32_t>(sizeof(Keys)));
        const auto output = Buffer(Output.data(), static_cast<std::uint32_t>(sizeof(Output)));
        Srt = {};
        std::copy(table.begin(), table.end(), Srt.begin());
        std::copy(keys.begin(), keys.end(), Srt.begin() + 4u);
        std::copy(output.begin(), output.end(), Srt.begin() + 8u);
        std::copy(PointWrap.begin(), PointWrap.end(), Srt.begin() + 12u);
        const std::vector<ShaderRecompiler::MemoryRegion> memory{
            {reinterpret_cast<std::uintptr_t>(code.data()), std::as_bytes(code)},
            {reinterpret_cast<std::uintptr_t>(Srt.data()), std::as_bytes(std::span(Srt))},
            {reinterpret_cast<std::uintptr_t>(Table.data()), std::as_bytes(std::span(Table))},
            {reinterpret_cast<std::uintptr_t>(Keys.data()), std::as_bytes(std::span(Keys))},
            {reinterpret_cast<std::uintptr_t>(records.data()), std::as_bytes(std::span(records))},
        };
        const auto inputTable = Table;
        const auto inputRecords = records;
        auto result = Execute(device, code, Srt.data(), memory, 4u, Lanes, 64u);
        Require(Table == inputTable && records == inputRecords, name + ": dispatch modified its input memory");
        Require(std::all_of(Output.begin() + 4u * Lanes * 4u, Output.end(), [](auto word) { return word == 0xdeadbeefu; }), name + ": dispatch wrote past its output");
        return result;
    };
    const std::array<std::array<std::uint32_t, 2>, 4> keys{{{1u, 2u}, {2u, 1u}, {1u, 1u}, {2u, 2u}}};
    for (std::uint32_t group = 0; group < 4u; ++group) {
        Keys[group * 4u] = keys[group][0];
        Keys[group * 4u + 1u] = keys[group][1];
    }
    const auto sampled = [&keys](std::uint32_t group, std::uint32_t flip) {
        return std::function<std::uint32_t(std::uint32_t)>([&keys, group, flip](std::uint32_t x) {
            const auto key = keys[group][x < 32u ? 0u : 1u] ^ flip;
            return std::bit_cast<std::uint32_t>(key % 2u != 0u ? OtherFloatTexels[x] : FloatTexels[x]);
        });
    };
    const auto untouched = [](const Outcome& outcome) { return std::all_of(outcome.words.begin(), outcome.words.end(), [](auto word) { return word == 0xdeadbeefu; }); };
    const auto taken = run();
    Require(!Faulted(taken), name + ": the record's pointer faulted:\n" + taken.log);
    for (std::uint32_t group = 0; group < 4u; ++group) RequireRed(taken, group, Lanes, sampled(group, 0u), name + ": the T# read through the record's pointer in a waterfall loop");
    const auto outside = [&](std::uint32_t arm, const std::string& what) {
        Keys[2] = arm;
        const auto faulted = run();
        Keys[2] = 0u;
        Require(Faulted(faulted) && faulted.log.find("outside the snapshot") != std::string::npos, name + ": " + what + " did not fault as outside the snapshot:\n" + faulted.log);
        Require(untouched(faulted), name + ": " + what + " published copied output");
        Require(faulted.artifactId == taken.artifactId, name + ": " + what + " recompiled the shader");
    };
    outside(1u, "a constant pointer word");
    outside(3u, "a second constant pointer word");
    if (!copies) {
        outside(2u, "a lane-mask pointer word");
        std::puts("pointer table with a lane-mask arm: a T# through a record pointer whose low word passes nested Phis with constants and a Ballot word checked, and the Ballot word faults outside the snapshot");
        return;
    }
    Keys[2] = 2u;
    const auto again = run();
    Require(!Faulted(again) && again.words == taken.words, name + ": a second read of the same record changed the result:\n" + again.log);
    Keys[3] = 3u;
    const auto flipped = run();
    Keys[2] = 0u;
    Keys[3] = 0u;
    Require(!Faulted(flipped) && flipped.artifactId == taken.artifactId, name + ": a second read of another record faulted or recompiled:\n" + flipped.log);
    RequireRed(flipped, 0u, Lanes, sampled(0u, 3u), name + ": the T# behind the other record's pointer word");
    for (std::uint32_t group = 1; group < 4u; ++group) Require(Group(flipped, group) == Group(taken, group), name + ": a group that kept its record changed its result");
    std::puts("pointer table with a second record read: a T# through a record pointer whose low word passes a second read keyed by another lane checked, by the pointer's value");
}

void RunSplitRecordTests(AgcDriver::VulkanDevice& device) {
    Fill();
    const auto floatImage = Image(FloatTexels.data(), Format32Float);
    const auto uintImage = Image(UintTexels.data(), Format32UInt);
    const auto sintImage = Image(SintTexels.data(), Format32SInt);
    const std::array<std::array<std::uint32_t, 4>, 3> images{floatImage, uintImage, sintImage};
    const std::array<std::function<std::uint32_t(std::uint32_t)>, 3> texels{
        [](std::uint32_t x) { return std::bit_cast<std::uint32_t>(FloatTexels[x]); },
        [](std::uint32_t x) { return UintTexels[x]; },
        [](std::uint32_t x) { return static_cast<std::uint32_t>(SintTexels[x]); },
    };
    const std::array<const char*, 3> names{"float", "uint", "sint"};

    Table = {};
    for (std::uint32_t record = 0; record < 3; ++record) SetRecord(record, PointClamp, images[record]);
    Keys = {};
    for (std::uint32_t group = 0; group < 3; ++group) Keys[group] = 2u - group;
    const auto split = Run(device, SplitRecordCode, 3, Lanes, 64);
    Require(!Faulted(split), "image table: a T# read by two loads of its record faulted:\n" + split.log);
    Require(split.imageTableCount == 3u, "image table: a T# read by two loads of its record does not hold one element per distinct T# (" + std::to_string(split.imageTableCount) + ")");
    for (std::uint32_t group = 0; group < 3; ++group) RequireRed(split, group, Lanes, texels[2u - group], std::string(names[2u - group]) + " texels of a T# read by two loads of its record");
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
alignas(256) std::array<std::uint32_t, 8> PixelResults{};
alignas(256) std::array<std::uint32_t, 4 * 4> GraphicsTable{};
constexpr std::array<std::array<float, 4>, 3> Triangle{{{-1.0f, -1.0f, 0.5f, 1.0f}, {3.0f, -1.0f, 0.5f, 1.0f}, {-1.0f, 3.0f, 0.5f, 1.0f}}};

alignas(256) constexpr std::array<std::uint32_t, 25> SharedSamplerCode{
    0xf4080700u, 0xfa000020u, 0xf4080800u, 0xfa000030u, 0xf4080900u, 0xfa000040u, 0xf4080a00u, 0xfa000050u,
    0x7e0202ffu, 0x3c400000u, 0x7e0402ffu, 0x3e000000u,
    0xf09c8f08u, 0x01090401u, 0xf09c8f08u, 0x010a0c01u,
    0x7e100280u, 0xe0781000u, 0x80070408u, 0xe0781010u, 0x80070c08u,
    0x7e0e02f2u, 0xf800180fu, 0x07070707u, 0xbf810000u,
};

std::pair<std::uint64_t, std::uint64_t> DrawTables(AgcDriver::VulkanDevice& device, std::uint32_t vertexKey, std::uint32_t pixelKey, bool sharedSampler = false) {
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
    const std::span<const std::uint32_t> pixelCode = sharedSampler ? std::span<const std::uint32_t>(SharedSamplerCode) : std::span<const std::uint32_t>(PixelTableCode);
    if (sharedSampler) {
        const auto root = reinterpret_cast<std::uintptr_t>(Srt.data());
        pixelUserData = {static_cast<std::uint32_t>(root), static_cast<std::uint32_t>(root >> 32u)};
        std::copy(pixelOutput.begin(), pixelOutput.end(), Srt.begin() + 8);
    }
    const std::array<ShaderRecompiler::MemoryRegion, 3> pixelMemory{{{reinterpret_cast<std::uintptr_t>(pixelCode.data()), std::as_bytes(pixelCode)}, {reinterpret_cast<std::uintptr_t>(GraphicsTable.data()), std::as_bytes(std::span(GraphicsTable))}, {reinterpret_cast<std::uintptr_t>(Srt.data()), std::as_bytes(std::span(Srt))}}};
    ShaderRecompiler::RecompileRequest fragment{
        {ShaderStage::Fragment, reinterpret_cast<std::uintptr_t>(pixelCode.data()), pixelCode, 0, {}},
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

void RunSharedSamplerTests(AgcDriver::VulkanDevice& device, const std::string& mode, bool draw) {
    Require(mode == "uint" || mode == "sint" || mode == "point", "unknown shared sampler mode");
    Fill();
    const auto floatImage = Image(FloatTexels.data(), Format32Float);
    const auto integerImage = mode == "sint" ? Image(SintTexels.data(), Format32SInt) : Image(UintTexels.data(), Format32UInt);
    const auto& sampler = mode == "point" ? PointClamp : LinearWrap;
    std::array<std::uint32_t, 8> images{};
    std::copy(integerImage.begin(), integerImage.end(), images.begin());
    std::copy(floatImage.begin(), floatImage.end(), images.begin() + 4);
    std::copy(sampler.begin(), sampler.end(), Srt.begin() + 12);
    std::copy(images.begin(), images.end(), Srt.begin() + 16);
    const auto integer = mode == "sint" ? static_cast<std::uint32_t>(SintTexels[0]) : UintTexels[0];
    const auto filtered = std::bit_cast<std::uint32_t>(mode == "point" ? FloatTexels[0] : 0.5f);
    if (draw) {
        std::copy(floatImage.begin(), floatImage.end(), GraphicsTable.begin());
        DrawTables(device, 0u, 0u, true);
        Require(PixelResults[0] == integer, "shared sampler draw: integer texel changed");
        Require(PixelResults[4] == filtered, "shared sampler draw: float filtering changed");
    } else {
        alignas(256) constexpr std::array<std::uint32_t, 29> code{
            0xf4080700u, 0xfa000020u, 0xf4080800u, 0xfa000030u, 0xf4080900u, 0xfa000040u, 0xf4080a00u, 0xfa000050u,
            0x360200bfu, 0x7e020d01u, 0x060202ffu, 0x3f400000u, 0x100202ffu, 0x3c800000u, 0x7e0402ffu, 0x3e000000u,
            0xf09c8f08u, 0x01090401u, 0xf09c8f08u, 0x010a0c01u,
            0x7e120202u, 0x34101286u, 0x4a101100u, 0x34101084u,
            0xe0781000u, 0x80070408u, 0xe0781400u, 0x80070c08u, 0xbf810000u,
        };
        const auto result = RunDirectCode(device, code, sampler, images);
        Require(Output[0] == integer, "shared sampler dispatch: integer texel changed");
        Require(Output[256] == filtered, "shared sampler dispatch: float filtering changed");
        Require(Holds(result.samplers, sampler), "shared sampler dispatch: original sampler missing");
        Require(Holds(result.samplers, mode == "point" ? PointClamp : PointWrap), "shared sampler dispatch: point sampler missing");
    }
    std::printf("shared sampler %s %s: integer=%u float_bits=%u passed\n", draw ? "draw" : "dispatch", mode.c_str(), integer, filtered);
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
    for (const auto* mode : {"uint", "sint", "point"}) {
        for (const bool draw : {false, true}) RunSharedSamplerTests(device, mode, draw);
    }
}

std::array<std::uint32_t, 3> LocalSize(std::span<const std::uint32_t> words) {
    for (std::size_t at = 5; at < words.size() && (words[at] >> 16u) != 0u; at += words[at] >> 16u) {
        if ((words[at] & 0xffffu) == spv::OpExecutionMode && words[at + 2u] == spv::ExecutionModeLocalSize) return {words[at + 3u], words[at + 4u], words[at + 5u]};
    }
    return {};
}

#if ANYPS5_ENABLE_SPIRV_TOOLS
void RequireValidSpirv(const ShaderRecompiler::RecompileResult& result, const ShaderRecompiler::SpirvTarget& target, const std::string& what) {
    std::string diagnostics;
    spvtools::SpirvTools validator(target.spirvVersion >= 0x00010400u ? SPV_ENV_VULKAN_1_1_SPIRV_1_4 : SPV_ENV_VULKAN_1_1);
    validator.SetMessageConsumer([&](spv_message_level_t, const char*, const spv_position_t&, const char* message) {
        if (message != nullptr) diagnostics += std::string(message) + '\n';
    });
    const auto words = result.spirv.Words();
    Require(validator.Validate(words.data(), words.size()), what + ": the spanned module does not pass spirv-val:\n" + diagnostics);
}
#endif

struct SpanChoice {
    const char* name;
    std::span<const std::uint32_t> code;
    std::uint32_t threads;
    bool spanTableWaves;
    bool spannedWaves;
    bool spans;
};

void RunSpanChoiceTests(AgcDriver::VulkanDevice& device) {
    Fill();
    SetClassRecords();
    Keys = {};
    for (std::uint32_t group = 0; group < MaxGroups; ++group) Keys[group] = group % 2u;
    const auto floatImage = Image(FloatTexels.data(), Format32Float);
    std::copy(PointClamp.begin(), PointClamp.end(), Srt.begin() + 12);
    std::fill(Srt.begin() + 16, Srt.begin() + 24, 0u);
    std::copy(floatImage.begin(), floatImage.end(), Srt.begin() + 16);
    const std::array choices{
        SpanChoice{"a sampler table read", TableSamplerCode, Lanes, true, false, true},
        SpanChoice{"an image table read under a direct sampler", TableDirectSamplerCode, Lanes, true, false, true},
        SpanChoice{"an image and sampler table read", TableCode, Lanes, true, false, true},
        SpanChoice{"no table read", DirectCode, Lanes, true, false, false},
        SpanChoice{"a sampler table read in a 128-thread workgroup", TableSamplerCode, 2u * Lanes, true, false, false},
        SpanChoice{"a sampler table read without the option", TableSamplerCode, Lanes, false, false, false},
        SpanChoice{"no table read with every program spanned", DirectCode, Lanes, false, true, true},
    };
#if ANYPS5_ENABLE_SPIRV_TOOLS
    std::uint32_t validated = 0;
#endif
    for (const auto& choice : choices) {
        auto target = device.Target();
        target.subgroupSize = 32u;
        target.spanTableWaves = choice.spanTableWaves;
        target.spannedWaves = choice.spannedWaves;
        const auto memory = TableMemory(choice.code, false);
        const auto result = Compile(choice.code, Srt.data(), memory, choice.threads, 64u, target);
        const std::string what = std::string("span choice, ") + choice.name;
        Require(result.fullSubgroups == choice.spans, what + (choice.spans ? ": the program kept two lanes per invocation" : ": the program spans two host subgroups"));
        const std::array<std::uint32_t, 3> local{choice.spans ? choice.threads : choice.threads / 2u, 1u, 1u};
        const auto emitted = LocalSize(result.spirv.Words());
        Require(emitted == local, what + ": LocalSize is " + std::to_string(emitted[0]) + "x" + std::to_string(emitted[1]) + "x" + std::to_string(emitted[2]) + ", expected " + std::to_string(local[0]) + "x1x1");
        if (!choice.spans) continue;
#if ANYPS5_ENABLE_SPIRV_TOOLS
        RequireValidSpirv(result, target, what);
        ++validated;
#endif
    }
    alignas(512) std::array<std::array<std::uint32_t, 64>, 2> records{};
    alignas(256) std::array<std::array<std::uint32_t, 16>, 2> palettes{};
    for (std::uint32_t outer = 0; outer < 2u; ++outer) {
        auto descriptor = Buffer(palettes[outer].data(), 1u);
        descriptor[1] |= 64u << 16u;
        std::copy(descriptor.begin(), descriptor.end(), records[outer].begin() + 40u);
        std::copy(PointWrap.begin(), PointWrap.end(), palettes[outer].begin());
        std::copy(PointClamp.begin(), PointClamp.end(), palettes[outer].begin() + 4u);
    }
    Table = {};
    for (std::uint32_t record = 0; record < 2u; ++record) {
        const auto pointer = reinterpret_cast<std::uintptr_t>(records[record].data());
        Table[record * 4u + 2u] = static_cast<std::uint32_t>(pointer);
        Table[record * 4u + 3u] = static_cast<std::uint32_t>(pointer >> 32u);
    }
    const std::array<std::uint32_t, 8> nestedKeys{0u, 0u, 0u, 1u, 1u, 0u, 1u, 1u};
    Keys = {};
    std::copy(nestedKeys.begin(), nestedKeys.end(), Keys.begin());
    const auto table = Buffer(Table.data(), 64u);
    const auto keys = Buffer(Keys.data(), static_cast<std::uint32_t>(sizeof(Keys)));
    const auto output = Buffer(Output.data(), static_cast<std::uint32_t>(sizeof(Output)));
    Srt = {};
    std::copy(table.begin(), table.end(), Srt.begin());
    std::copy(keys.begin(), keys.end(), Srt.begin() + 4u);
    std::copy(output.begin(), output.end(), Srt.begin() + 8u);
    std::copy(floatImage.begin(), floatImage.end(), Srt.begin() + 12u);
    const std::span<const std::uint32_t> nested(NestedSamplerCode);
    const std::array<ShaderRecompiler::MemoryRegion, 6> nestedMemory{{
        {reinterpret_cast<std::uintptr_t>(nested.data()), std::as_bytes(nested)},
        {reinterpret_cast<std::uintptr_t>(Srt.data()), std::as_bytes(std::span(Srt))},
        {reinterpret_cast<std::uintptr_t>(Table.data()), std::as_bytes(std::span(Table))},
        {reinterpret_cast<std::uintptr_t>(Keys.data()), std::as_bytes(std::span(Keys))},
        {reinterpret_cast<std::uintptr_t>(records.data()), std::as_bytes(std::span(records))},
        {reinterpret_cast<std::uintptr_t>(palettes.data()), std::as_bytes(std::span(palettes))},
    }};
    for (const bool spanTableWaves : {false, true}) {
        auto target = device.Target();
        target.subgroupSize = 32u;
        target.spanTableWaves = spanTableWaves;
        const auto result = Compile(nested, Srt.data(), nestedMemory, Lanes, 64u, target);
        const std::string what = std::string("span choice, the nested sampler table program") + (spanTableWaves ? "" : " without the option");
        Require(result.fullSubgroups == spanTableWaves, what + (spanTableWaves ? ": the program kept two lanes per invocation" : ": the program spans two host subgroups"));
        const std::array<std::uint32_t, 3> local{spanTableWaves ? Lanes : Lanes / 2u, 1u, 1u};
        Require(LocalSize(result.spirv.Words()) == local, what + ": LocalSize is not " + std::to_string(local[0]) + "x1x1");
#if ANYPS5_ENABLE_SPIRV_TOOLS
        if (spanTableWaves) {
            RequireValidSpirv(result, target, what);
            ++validated;
        }
#endif
    }
#if ANYPS5_ENABLE_SPIRV_TOOLS
    std::printf("span choice: %zu wave64 programs and the nested sampler table program on a 32-lane host, %u spanned modules pass spirv-val\n", choices.size(), validated);
#else
    std::printf("span choice: %zu wave64 programs and the nested sampler table program on a 32-lane host, spirv-val not run (SPIRV-Tools not built)\n", choices.size());
#endif
}

void RunTableImagePerWorkgroupTests(AgcDriver::VulkanDevice& device) {
    Fill();
    const auto floatImage = Image(FloatTexels.data(), Format32Float);
    const auto otherImage = Image(OtherFloatTexels.data(), Format32Float);
    SetRecord(0, PointClamp, floatImage);
    SetRecord(1, PointClamp, otherImage);
    std::copy(PointClamp.begin(), PointClamp.end(), Srt.begin() + 12);
    Keys = {};
    for (std::uint32_t group = 0; group < MaxGroups; ++group) Keys[group] = group % 2u;
    const auto outcome = Run(device, TableDirectSamplerCode, MaxGroups, Lanes, 64);
    Require(!Faulted(outcome), "image table per workgroup: valid keys faulted:\n" + outcome.log);
    for (std::uint32_t group = 0; group < MaxGroups; ++group) {
        const auto& texels = group % 2u == 0u ? FloatTexels : OtherFloatTexels;
        RequireRed(outcome, group, Lanes, [&texels](std::uint32_t x) { return std::bit_cast<std::uint32_t>(texels[x]); }, "image table per workgroup, workgroup " + std::to_string(group) + " (T# of record " + std::to_string(group % 2u) + ")");
    }
    std::printf("image table per workgroup: %u wave64 workgroups of %u threads alternate two T#s under one direct S#, each read its own image (spanned %d)\n", MaxGroups, Lanes, outcome.fullSubgroups);
}

void RunWave32SamplerPerWorkgroupTests(AgcDriver::VulkanDevice& device) {
    Fill();
    const auto floatImage = Image(FloatTexels.data(), Format32Float);
    SetRecord(0, PointClamp, floatImage);
    SetRecord(1, PointWrap, floatImage);
    std::fill(Srt.begin() + 16, Srt.begin() + 24, 0u);
    std::copy(floatImage.begin(), floatImage.end(), Srt.begin() + 16);
    Keys = {};
    for (std::uint32_t group = 0; group < MaxGroups; ++group) Keys[group] = group % 2u;
    constexpr std::uint32_t Threads = 32;
    const auto outcome = Run(device, Wave32TableSamplerCode, MaxGroups, Threads, 32);
    Require(!Faulted(outcome), "wave32 sampler per workgroup: valid keys faulted:\n" + outcome.log);
    for (std::uint32_t group = 0; group < MaxGroups; ++group) {
        const bool wrap = group % 2u != 0u;
        RequireRed(outcome, group, Threads, [wrap](std::uint32_t x) { return std::bit_cast<std::uint32_t>(FloatTexels[wrap ? x : Width - 1u]); }, std::string("wave32 sampler per workgroup, workgroup ") + std::to_string(group) + (wrap ? " (wrapping S#)" : " (clamping S#)"));
    }
    std::printf("wave32 sampler per workgroup: %u wave32 workgroups of %u threads alternate a clamping and a wrapping table S#, each sampled with its own\n", MaxGroups, Threads);
}

int RunSpannedWaveRefusal() {
    const char* value = std::getenv("APS5_SPANNED_WAVE");
    Require(value != nullptr && !AgcDriver::ParseSpannedWaveMode(value), "spanned wave refusal: APS5_SPANNED_WAVE holds no refused value");
    const std::string refusal = std::string("APS5_SPANNED_WAVE is \"") + value + "\"";
    try {
        AgcDriver::VulkanDevice device;
    } catch (const std::exception& error) {
        const std::string message = error.what();
        if (message.find(refusal) != std::string::npos) {
            std::printf("spanned wave refusal: %s\n", error.what());
            return 0;
        }
        if (std::getenv("ANYPS5_REQUIRE_VULKAN") != nullptr) throw;
        std::printf("skipped, no usable Vulkan device: %s\n", error.what());
        return VulkanTestSkipped;
    }
    throw std::runtime_error("spanned wave refusal: a device opened with APS5_SPANNED_WAVE=" + std::string(value));
}

}

int main(int argc, char** argv) {
    try {
#ifdef _WIN32
        _putenv_s("APS5_SRGB_SHADER_DECODE", "1");
#else
        setenv("APS5_SRGB_SHADER_DECODE", "1", 1);
#endif
        if (argc == 2 && std::strcmp(argv[1], "--spanned-wave-refusal") == 0) return RunSpannedWaveRefusal();
        const auto device = OpenVulkanTestDevice();
        if (!device) return VulkanTestSkipped;
        if (argc == 2 && std::strcmp(argv[1], "--nested") == 0) {
            RunNestedRecordTests(*device, NestedCase::Basic);
            return 0;
        }
        if (argc == 2 && std::strcmp(argv[1], "--nested-large") == 0) {
            RunNestedRecordTests(*device, NestedCase::Large);
            return 0;
        }
        if (argc == 2 && std::strcmp(argv[1], "--nested-budget") == 0) {
            RunNestedRecordTests(*device, NestedCase::Budget);
            return 0;
        }
        if (argc == 2 && std::strcmp(argv[1], "--nested-sampler") == 0) {
            RunNestedSamplerTests(*device);
            return 0;
        }
        if (argc == 2 && std::strcmp(argv[1], "--pointer-phi-sampler") == 0) {
            RunPointerSamplerTests(*device);
            return 0;
        }
        if (argc == 2 && std::strcmp(argv[1], "--pointer-phi-ballot") == 0) {
            RunPointerArmTests(*device, false);
            return 0;
        }
        if (argc == 2 && std::strcmp(argv[1], "--pointer-phi-copies") == 0) {
            RunPointerArmTests(*device, true);
            return 0;
        }
        if (argc == 2 && std::strcmp(argv[1], "--span-choice") == 0) {
            RunSpanChoiceTests(*device);
            return 0;
        }
        if (argc == 2 && std::strcmp(argv[1], "--table-image-per-workgroup") == 0) {
            RunTableImagePerWorkgroupTests(*device);
            return 0;
        }
        if (argc == 2 && std::strcmp(argv[1], "--wave32-sampler-per-workgroup") == 0) {
            RunWave32SamplerPerWorkgroupTests(*device);
            return 0;
        }
        RunTests(*device);
        RunBorderSwizzleTests(*device);
        RunAnisoOverrideTests(*device);
        RunReadTests(*device);
        RunLoopKeyTests(*device);
        RunGuardedTableTests(*device);
        RunSelectedRecordTests(*device);
        RunModeTests(*device);
        RunStoreTests(*device);
        RunAtomicTests(*device);
        RunAtomic64Tests(*device);
        RunSelectedStoreTests(*device);
        RunPointerTests(*device);
        RunPointerRecordTests(*device);
        RunPointerChainRecordTests(*device);
        RunSplitRecordTests(*device);
        RunPointerStorageWriteTests(*device);
        RunGraphicsTests(*device);
        std::puts("image table tests passed");
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
