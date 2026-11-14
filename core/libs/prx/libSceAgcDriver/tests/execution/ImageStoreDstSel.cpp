#include "prx/libSceAgcDriver/Execution/include/VulkanDevice.hpp"
#include "prx/libSceAgcDriver/Graphics/include/ColorTargetLayout.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Draw.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Texture.hpp"
#include "prx/libSceAgcDriver/Graphics/include/TextureTiling.hpp"
#include "prx/libc/include/GuestAllocations.hpp"
#include "Recompiler.hpp"
#include "VulkanTestDevice.hpp"
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif
#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <optional>
#include <span>
#include <string>
#include <vector>

namespace {

using AgcDriver::Graphics::Require;
using ShaderRecompiler::ShaderStage;

constexpr std::uint32_t Threads = 32;
constexpr std::uint32_t Width = 32;
constexpr std::uint32_t Height = 8;
constexpr std::uint32_t Type2D = 9;
constexpr std::size_t TexelBytes = 8192;
constexpr std::size_t BlockBytes = 65536;
constexpr std::uint32_t FormatCount = 12;
constexpr std::uint32_t TupleCount = 14;
constexpr std::uint32_t RowCount = 2;

alignas(256) std::array<std::uint32_t, Threads * 4> Input{};
alignas(256) std::array<std::byte, 16384> Pixels{};

alignas(256) constexpr std::array<std::uint32_t, 6> VertexCode{
    0xe0382000, 0x80000005, 0xbf8c3f70, 0xf80008cf, 0x03020100, 0xbf810000,
};

alignas(256) constexpr std::array<std::uint32_t, 12> StoreCode{
    0x34020084, 0xe0381000, 0x80001401, 0xbf8c3f70, 0x7e3c0300, 0x7e3e0280, 0xf0201f08, 0x0001141e,
    0x7e3e0281, 0xf0201508, 0x0001141e, 0xbf810000,
};

constexpr std::array<std::uint32_t, RowCount> Dmasks{0xfu, 0x5u};

class GuestBlock {
public:
    GuestBlock() {
#ifdef _WIN32
        block = static_cast<std::uint8_t*>(VirtualAlloc(nullptr, BlockBytes, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE));
#else
        block = static_cast<std::uint8_t*>(std::aligned_alloc(BlockBytes, BlockBytes));
#endif
        Require(block != nullptr, "image store dst_sel: cannot allocate the guest block");
        GuestAllocations::Mutation().Add(block, BlockBytes, true, true, true);
    }

    ~GuestBlock() {
        GuestAllocations::Mutation().Remove(block);
#ifdef _WIN32
        VirtualFree(block, 0, MEM_RELEASE);
#else
        std::free(block);
#endif
    }

    GuestBlock(const GuestBlock&) = delete;
    GuestBlock& operator=(const GuestBlock&) = delete;

    std::uint8_t* Data() { return block; }

private:
    std::uint8_t* block = nullptr;
};

struct Format {
    const char* name;
    std::uint32_t format;
    std::uint32_t channels;
    std::uint32_t bits;
    std::array<std::uint32_t, 4> data;
};

constexpr std::array<Format, FormatCount> Formats{{
    {"8_UNORM", 1u, 1u, 8u, {0x3d81a36eu, 0x3f47dbf5u, 0x3f4fec57u, 0x3f59f55au}},
    {"16_SINT", 12u, 1u, 16u, {0x00000011u, 0x00000022u, 0x00000033u, 0x00000044u}},
    {"32_FLOAT", 22u, 1u, 32u, {0x40000000u, 0x40400000u, 0x40800000u, 0x40a00000u}},
    {"8_8_UNORM", 14u, 2u, 8u, {0x3d81a36eu, 0x3f47dbf5u, 0x3f4fec57u, 0x3f59f55au}},
    {"16_16_FLOAT", 29u, 2u, 16u, {0x40000000u, 0x40400000u, 0x40800000u, 0x40a00000u}},
    {"32_32_UINT", 62u, 2u, 32u, {0x00000011u, 0x00000022u, 0x00000033u, 0x00000044u}},
    {"8_8_8_8_UNORM", 56u, 4u, 8u, {0x3d81a36eu, 0x3f47dbf5u, 0x3f4fec57u, 0x3f59f55au}},
    {"8_8_8_8_SNORM", 57u, 4u, 8u, {0x3d81a36eu, 0x3f47dbf5u, 0x3f4fec57u, 0x3f59f55au}},
    {"8_8_8_8_UINT", 60u, 4u, 8u, {0x00000011u, 0x00000022u, 0x00000033u, 0x00000044u}},
    {"16_16_16_16_SINT", 70u, 4u, 16u, {0x00000011u, 0x00000022u, 0x00000033u, 0x00000044u}},
    {"16_16_16_16_FLOAT", 71u, 4u, 16u, {0x40000000u, 0x40400000u, 0x40800000u, 0x40a00000u}},
    {"32_32_32_32_FLOAT", 77u, 4u, 32u, {0x40000000u, 0x40400000u, 0x40800000u, 0x40a00000u}},
}};

constexpr std::array<const char*, TupleCount> TupleNames{"X,Y,Z,W", "X,Y,Z,1", "Z,Y,X,W", "X,Y,0,1", "X,0,0,0", "X,0,0,1", "Y,X,0,1", "Y,X,Z,W", "Y,0,0,1", "0,Y,Z,1", "W,Z,Y,X", "1,Y,0,W", "Y,Z,X,W", "Z,W,1,0"};
constexpr std::array<std::uint32_t, TupleCount> Swizzles{0xfacu, 0x3acu, 0xf2eu, 0x22cu, 0x004u, 0x204u, 0x225u, 0xfa5u, 0x205u, 0x3a8u, 0x977u, 0xe29u, 0xf35u, 0x07eu};

constexpr std::uint32_t Expected[FormatCount][TupleCount][RowCount][4] = {
    {
        {{0x00000010u, 0x00000000u, 0x00000000u, 0x00000000u}, {0x00000010u, 0x00000000u, 0x00000000u, 0x00000000u}},
        {{0x00000010u, 0x00000000u, 0x00000000u, 0x00000000u}, {0x00000010u, 0x00000000u, 0x00000000u, 0x00000000u}},
        {{0x000000d9u, 0x00000000u, 0x00000000u, 0x00000000u}, {0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u}},
        {{0x00000010u, 0x00000000u, 0x00000000u, 0x00000000u}, {0x00000010u, 0x00000000u, 0x00000000u, 0x00000000u}},
        {{0x00000010u, 0x00000000u, 0x00000000u, 0x00000000u}, {0x00000010u, 0x00000000u, 0x00000000u, 0x00000000u}},
        {{0x00000010u, 0x00000000u, 0x00000000u, 0x00000000u}, {0x00000010u, 0x00000000u, 0x00000000u, 0x00000000u}},
        {{0x000000d9u, 0x00000000u, 0x00000000u, 0x00000000u}, {0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u}},
        {{0x000000d9u, 0x00000000u, 0x00000000u, 0x00000000u}, {0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u}},
        {{0x000000d9u, 0x00000000u, 0x00000000u, 0x00000000u}, {0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u}},
        {{0x000000d9u, 0x00000000u, 0x00000000u, 0x00000000u}, {0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u}},
        {{0x000000d9u, 0x00000000u, 0x00000000u, 0x00000000u}, {0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u}},
        {{0x000000d9u, 0x00000000u, 0x00000000u, 0x00000000u}, {0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u}},
        {{0x000000d9u, 0x00000000u, 0x00000000u, 0x00000000u}, {0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u}},
        {{0x000000d9u, 0x00000000u, 0x00000000u, 0x00000000u}, {0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u}},
    },
    {
        {{0x00000011u, 0x00000000u, 0x00000000u, 0x00000000u}, {0x00000011u, 0x00000000u, 0x00000000u, 0x00000000u}},
        {{0x00000011u, 0x00000000u, 0x00000000u, 0x00000000u}, {0x00000011u, 0x00000000u, 0x00000000u, 0x00000000u}},
        {{0x00000044u, 0x00000000u, 0x00000000u, 0x00000000u}, {0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u}},
        {{0x00000011u, 0x00000000u, 0x00000000u, 0x00000000u}, {0x00000011u, 0x00000000u, 0x00000000u, 0x00000000u}},
        {{0x00000011u, 0x00000000u, 0x00000000u, 0x00000000u}, {0x00000011u, 0x00000000u, 0x00000000u, 0x00000000u}},
        {{0x00000011u, 0x00000000u, 0x00000000u, 0x00000000u}, {0x00000011u, 0x00000000u, 0x00000000u, 0x00000000u}},
        {{0x00000044u, 0x00000000u, 0x00000000u, 0x00000000u}, {0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u}},
        {{0x00000044u, 0x00000000u, 0x00000000u, 0x00000000u}, {0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u}},
        {{0x00000044u, 0x00000000u, 0x00000000u, 0x00000000u}, {0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u}},
        {{0x00000044u, 0x00000000u, 0x00000000u, 0x00000000u}, {0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u}},
        {{0x00000044u, 0x00000000u, 0x00000000u, 0x00000000u}, {0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u}},
        {{0x00000044u, 0x00000000u, 0x00000000u, 0x00000000u}, {0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u}},
        {{0x00000044u, 0x00000000u, 0x00000000u, 0x00000000u}, {0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u}},
        {{0x00000044u, 0x00000000u, 0x00000000u, 0x00000000u}, {0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u}},
    },
    {
        {{0x40000000u, 0x00000000u, 0x00000000u, 0x00000000u}, {0x40000000u, 0x00000000u, 0x00000000u, 0x00000000u}},
        {{0x40000000u, 0x00000000u, 0x00000000u, 0x00000000u}, {0x40000000u, 0x00000000u, 0x00000000u, 0x00000000u}},
        {{0x40a00000u, 0x00000000u, 0x00000000u, 0x00000000u}, {0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u}},
        {{0x40000000u, 0x00000000u, 0x00000000u, 0x00000000u}, {0x40000000u, 0x00000000u, 0x00000000u, 0x00000000u}},
        {{0x40000000u, 0x00000000u, 0x00000000u, 0x00000000u}, {0x40000000u, 0x00000000u, 0x00000000u, 0x00000000u}},
        {{0x40000000u, 0x00000000u, 0x00000000u, 0x00000000u}, {0x40000000u, 0x00000000u, 0x00000000u, 0x00000000u}},
        {{0x40a00000u, 0x00000000u, 0x00000000u, 0x00000000u}, {0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u}},
        {{0x40a00000u, 0x00000000u, 0x00000000u, 0x00000000u}, {0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u}},
        {{0x40a00000u, 0x00000000u, 0x00000000u, 0x00000000u}, {0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u}},
        {{0x40a00000u, 0x00000000u, 0x00000000u, 0x00000000u}, {0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u}},
        {{0x40a00000u, 0x00000000u, 0x00000000u, 0x00000000u}, {0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u}},
        {{0x40a00000u, 0x00000000u, 0x00000000u, 0x00000000u}, {0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u}},
        {{0x40a00000u, 0x00000000u, 0x00000000u, 0x00000000u}, {0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u}},
        {{0x40a00000u, 0x00000000u, 0x00000000u, 0x00000000u}, {0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u}},
    },
    {
        {{0x00000010u, 0x000000d9u, 0x00000000u, 0x00000000u}, {0x00000010u, 0x00000000u, 0x00000000u, 0x00000000u}},
        {{0x00000010u, 0x000000d9u, 0x00000000u, 0x00000000u}, {0x00000010u, 0x00000000u, 0x00000000u, 0x00000000u}},
        {{0x000000d9u, 0x00000010u, 0x00000000u, 0x00000000u}, {0x00000000u, 0x00000010u, 0x00000000u, 0x00000000u}},
        {{0x00000010u, 0x000000c7u, 0x00000000u, 0x00000000u}, {0x00000010u, 0x00000000u, 0x00000000u, 0x00000000u}},
        {{0x00000010u, 0x000000d9u, 0x00000000u, 0x00000000u}, {0x00000010u, 0x00000000u, 0x00000000u, 0x00000000u}},
        {{0x00000010u, 0x000000d9u, 0x00000000u, 0x00000000u}, {0x00000010u, 0x00000000u, 0x00000000u, 0x00000000u}},
        {{0x000000c7u, 0x00000010u, 0x00000000u, 0x00000000u}, {0x00000000u, 0x00000010u, 0x00000000u, 0x00000000u}},
        {{0x000000d9u, 0x00000010u, 0x00000000u, 0x00000000u}, {0x00000000u, 0x00000010u, 0x00000000u, 0x00000000u}},
        {{0x00000010u, 0x000000d9u, 0x00000000u, 0x00000000u}, {0x00000010u, 0x00000000u, 0x00000000u, 0x00000000u}},
        {{0x000000d9u, 0x00000010u, 0x00000000u, 0x00000000u}, {0x00000000u, 0x00000010u, 0x00000000u, 0x00000000u}},
        {{0x000000d9u, 0x00000010u, 0x00000000u, 0x00000000u}, {0x00000000u, 0x00000010u, 0x00000000u, 0x00000000u}},
        {{0x000000c7u, 0x00000010u, 0x00000000u, 0x00000000u}, {0x00000000u, 0x00000010u, 0x00000000u, 0x00000000u}},
        {{0x000000d9u, 0x00000010u, 0x00000000u, 0x00000000u}, {0x00000000u, 0x00000010u, 0x00000000u, 0x00000000u}},
        {{0x000000c7u, 0x00000010u, 0x00000000u, 0x00000000u}, {0x00000000u, 0x00000010u, 0x00000000u, 0x00000000u}},
    },
    {
        {{0x00004000u, 0x00004500u, 0x00000000u, 0x00000000u}, {0x00004000u, 0x00000000u, 0x00000000u, 0x00000000u}},
        {{0x00004000u, 0x00004500u, 0x00000000u, 0x00000000u}, {0x00004000u, 0x00000000u, 0x00000000u, 0x00000000u}},
        {{0x00004500u, 0x00004000u, 0x00000000u, 0x00000000u}, {0x00000000u, 0x00004000u, 0x00000000u, 0x00000000u}},
        {{0x00004000u, 0x00004200u, 0x00000000u, 0x00000000u}, {0x00004000u, 0x00000000u, 0x00000000u, 0x00000000u}},
        {{0x00004000u, 0x00004500u, 0x00000000u, 0x00000000u}, {0x00004000u, 0x00000000u, 0x00000000u, 0x00000000u}},
        {{0x00004000u, 0x00004500u, 0x00000000u, 0x00000000u}, {0x00004000u, 0x00000000u, 0x00000000u, 0x00000000u}},
        {{0x00004200u, 0x00004000u, 0x00000000u, 0x00000000u}, {0x00000000u, 0x00004000u, 0x00000000u, 0x00000000u}},
        {{0x00004500u, 0x00004000u, 0x00000000u, 0x00000000u}, {0x00000000u, 0x00004000u, 0x00000000u, 0x00000000u}},
        {{0x00004000u, 0x00004500u, 0x00000000u, 0x00000000u}, {0x00004000u, 0x00000000u, 0x00000000u, 0x00000000u}},
        {{0x00004500u, 0x00004000u, 0x00000000u, 0x00000000u}, {0x00000000u, 0x00004000u, 0x00000000u, 0x00000000u}},
        {{0x00004500u, 0x00004000u, 0x00000000u, 0x00000000u}, {0x00000000u, 0x00004000u, 0x00000000u, 0x00000000u}},
        {{0x00004200u, 0x00004000u, 0x00000000u, 0x00000000u}, {0x00000000u, 0x00004000u, 0x00000000u, 0x00000000u}},
        {{0x00004500u, 0x00004000u, 0x00000000u, 0x00000000u}, {0x00000000u, 0x00004000u, 0x00000000u, 0x00000000u}},
        {{0x00004200u, 0x00004000u, 0x00000000u, 0x00000000u}, {0x00000000u, 0x00004000u, 0x00000000u, 0x00000000u}},
    },
    {
        {{0x00000011u, 0x00000044u, 0x00000000u, 0x00000000u}, {0x00000011u, 0x00000000u, 0x00000000u, 0x00000000u}},
        {{0x00000011u, 0x00000044u, 0x00000000u, 0x00000000u}, {0x00000011u, 0x00000000u, 0x00000000u, 0x00000000u}},
        {{0x00000044u, 0x00000011u, 0x00000000u, 0x00000000u}, {0x00000000u, 0x00000011u, 0x00000000u, 0x00000000u}},
        {{0x00000011u, 0x00000022u, 0x00000000u, 0x00000000u}, {0x00000011u, 0x00000000u, 0x00000000u, 0x00000000u}},
        {{0x00000011u, 0x00000044u, 0x00000000u, 0x00000000u}, {0x00000011u, 0x00000000u, 0x00000000u, 0x00000000u}},
        {{0x00000011u, 0x00000044u, 0x00000000u, 0x00000000u}, {0x00000011u, 0x00000000u, 0x00000000u, 0x00000000u}},
        {{0x00000022u, 0x00000011u, 0x00000000u, 0x00000000u}, {0x00000000u, 0x00000011u, 0x00000000u, 0x00000000u}},
        {{0x00000044u, 0x00000011u, 0x00000000u, 0x00000000u}, {0x00000000u, 0x00000011u, 0x00000000u, 0x00000000u}},
        {{0x00000011u, 0x00000044u, 0x00000000u, 0x00000000u}, {0x00000011u, 0x00000000u, 0x00000000u, 0x00000000u}},
        {{0x00000044u, 0x00000011u, 0x00000000u, 0x00000000u}, {0x00000000u, 0x00000011u, 0x00000000u, 0x00000000u}},
        {{0x00000044u, 0x00000011u, 0x00000000u, 0x00000000u}, {0x00000000u, 0x00000011u, 0x00000000u, 0x00000000u}},
        {{0x00000022u, 0x00000011u, 0x00000000u, 0x00000000u}, {0x00000000u, 0x00000011u, 0x00000000u, 0x00000000u}},
        {{0x00000044u, 0x00000011u, 0x00000000u, 0x00000000u}, {0x00000000u, 0x00000011u, 0x00000000u, 0x00000000u}},
        {{0x00000022u, 0x00000011u, 0x00000000u, 0x00000000u}, {0x00000000u, 0x00000011u, 0x00000000u, 0x00000000u}},
    },
    {
        {{0x00000010u, 0x000000c7u, 0x000000cfu, 0x000000d9u}, {0x00000010u, 0x00000000u, 0x000000c7u, 0x00000000u}},
        {{0x00000010u, 0x000000c7u, 0x000000cfu, 0x000000ffu}, {0x00000010u, 0x00000000u, 0x000000c7u, 0x000000ffu}},
        {{0x000000cfu, 0x000000c7u, 0x00000010u, 0x000000d9u}, {0x000000c7u, 0x00000000u, 0x00000010u, 0x00000000u}},
        {{0x00000010u, 0x000000c7u, 0x00000000u, 0x000000ffu}, {0x00000010u, 0x00000000u, 0x00000000u, 0x000000ffu}},
        {{0x00000000u, 0x00000000u, 0x00000000u, 0x000000d9u}, {0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u}},
        {{0x000000ffu, 0x00000000u, 0x00000000u, 0x000000d9u}, {0x000000ffu, 0x00000000u, 0x00000000u, 0x00000000u}},
        {{0x000000ffu, 0x00000000u, 0x000000d9u, 0x000000cfu}, {0x000000ffu, 0x00000000u, 0x00000000u, 0x000000c7u}},
        {{0x00000010u, 0x000000c7u, 0x000000d9u, 0x000000cfu}, {0x00000010u, 0x00000000u, 0x00000000u, 0x000000c7u}},
        {{0x000000ffu, 0x00000000u, 0x00000000u, 0x000000cfu}, {0x000000ffu, 0x00000000u, 0x00000000u, 0x000000c7u}},
        {{0x00000000u, 0x000000c7u, 0x000000cfu, 0x000000ffu}, {0x00000000u, 0x00000000u, 0x000000c7u, 0x000000ffu}},
        {{0x000000d9u, 0x000000cfu, 0x000000c7u, 0x00000010u}, {0x00000000u, 0x000000c7u, 0x00000000u, 0x00000010u}},
        {{0x000000ffu, 0x000000c7u, 0x00000000u, 0x000000d9u}, {0x000000ffu, 0x00000000u, 0x00000000u, 0x00000000u}},
        {{0x00000010u, 0x000000d9u, 0x000000c7u, 0x000000cfu}, {0x00000010u, 0x00000000u, 0x00000000u, 0x000000c7u}},
        {{0x00000000u, 0x000000ffu, 0x00000010u, 0x000000c7u}, {0x00000000u, 0x000000ffu, 0x00000010u, 0x00000000u}},
    },
    {
        {{0x00000008u, 0x00000063u, 0x00000067u, 0x0000006cu}, {0x00000008u, 0x00000000u, 0x00000063u, 0x00000000u}},
        {{0x00000008u, 0x00000063u, 0x00000067u, 0x0000007fu}, {0x00000008u, 0x00000000u, 0x00000063u, 0x0000007fu}},
        {{0x00000067u, 0x00000063u, 0x00000008u, 0x0000006cu}, {0x00000063u, 0x00000000u, 0x00000008u, 0x00000000u}},
        {{0x00000008u, 0x00000063u, 0x00000000u, 0x0000007fu}, {0x00000008u, 0x00000000u, 0x00000000u, 0x0000007fu}},
        {{0x00000000u, 0x00000000u, 0x00000000u, 0x0000006cu}, {0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u}},
        {{0x0000007fu, 0x00000000u, 0x00000000u, 0x0000006cu}, {0x0000007fu, 0x00000000u, 0x00000000u, 0x00000000u}},
        {{0x0000007fu, 0x00000000u, 0x0000006cu, 0x00000067u}, {0x0000007fu, 0x00000000u, 0x00000000u, 0x00000063u}},
        {{0x00000008u, 0x00000063u, 0x0000006cu, 0x00000067u}, {0x00000008u, 0x00000000u, 0x00000000u, 0x00000063u}},
        {{0x0000007fu, 0x00000000u, 0x00000000u, 0x00000067u}, {0x0000007fu, 0x00000000u, 0x00000000u, 0x00000063u}},
        {{0x00000000u, 0x00000063u, 0x00000067u, 0x0000007fu}, {0x00000000u, 0x00000000u, 0x00000063u, 0x0000007fu}},
        {{0x0000006cu, 0x00000067u, 0x00000063u, 0x00000008u}, {0x00000000u, 0x00000063u, 0x00000000u, 0x00000008u}},
        {{0x0000007fu, 0x00000063u, 0x00000000u, 0x0000006cu}, {0x0000007fu, 0x00000000u, 0x00000000u, 0x00000000u}},
        {{0x00000008u, 0x0000006cu, 0x00000063u, 0x00000067u}, {0x00000008u, 0x00000000u, 0x00000000u, 0x00000063u}},
        {{0x00000000u, 0x0000007fu, 0x00000008u, 0x00000063u}, {0x00000000u, 0x0000007fu, 0x00000008u, 0x00000000u}},
    },
    {
        {{0x00000011u, 0x00000022u, 0x00000033u, 0x00000044u}, {0x00000011u, 0x00000000u, 0x00000022u, 0x00000000u}},
        {{0x00000011u, 0x00000022u, 0x00000033u, 0x00000001u}, {0x00000011u, 0x00000000u, 0x00000022u, 0x00000001u}},
        {{0x00000033u, 0x00000022u, 0x00000011u, 0x00000044u}, {0x00000022u, 0x00000000u, 0x00000011u, 0x00000000u}},
        {{0x00000011u, 0x00000022u, 0x00000000u, 0x00000001u}, {0x00000011u, 0x00000000u, 0x00000000u, 0x00000001u}},
        {{0x00000000u, 0x00000000u, 0x00000000u, 0x00000044u}, {0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u}},
        {{0x00000001u, 0x00000000u, 0x00000000u, 0x00000044u}, {0x00000001u, 0x00000000u, 0x00000000u, 0x00000000u}},
        {{0x00000001u, 0x00000000u, 0x00000044u, 0x00000033u}, {0x00000001u, 0x00000000u, 0x00000000u, 0x00000022u}},
        {{0x00000011u, 0x00000022u, 0x00000044u, 0x00000033u}, {0x00000011u, 0x00000000u, 0x00000000u, 0x00000022u}},
        {{0x00000001u, 0x00000000u, 0x00000000u, 0x00000033u}, {0x00000001u, 0x00000000u, 0x00000000u, 0x00000022u}},
        {{0x00000000u, 0x00000022u, 0x00000033u, 0x00000001u}, {0x00000000u, 0x00000000u, 0x00000022u, 0x00000001u}},
        {{0x00000044u, 0x00000033u, 0x00000022u, 0x00000011u}, {0x00000000u, 0x00000022u, 0x00000000u, 0x00000011u}},
        {{0x00000001u, 0x00000022u, 0x00000000u, 0x00000044u}, {0x00000001u, 0x00000000u, 0x00000000u, 0x00000000u}},
        {{0x00000011u, 0x00000044u, 0x00000022u, 0x00000033u}, {0x00000011u, 0x00000000u, 0x00000000u, 0x00000022u}},
        {{0x00000000u, 0x00000001u, 0x00000011u, 0x00000022u}, {0x00000000u, 0x00000001u, 0x00000011u, 0x00000000u}},
    },
    {
        {{0x00000011u, 0x00000022u, 0x00000033u, 0x00000044u}, {0x00000011u, 0x00000000u, 0x00000022u, 0x00000000u}},
        {{0x00000011u, 0x00000022u, 0x00000033u, 0x00000001u}, {0x00000011u, 0x00000000u, 0x00000022u, 0x00000001u}},
        {{0x00000033u, 0x00000022u, 0x00000011u, 0x00000044u}, {0x00000022u, 0x00000000u, 0x00000011u, 0x00000000u}},
        {{0x00000011u, 0x00000022u, 0x00000000u, 0x00000001u}, {0x00000011u, 0x00000000u, 0x00000000u, 0x00000001u}},
        {{0x00000000u, 0x00000000u, 0x00000000u, 0x00000044u}, {0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u}},
        {{0x00000001u, 0x00000000u, 0x00000000u, 0x00000044u}, {0x00000001u, 0x00000000u, 0x00000000u, 0x00000000u}},
        {{0x00000001u, 0x00000000u, 0x00000044u, 0x00000033u}, {0x00000001u, 0x00000000u, 0x00000000u, 0x00000022u}},
        {{0x00000011u, 0x00000022u, 0x00000044u, 0x00000033u}, {0x00000011u, 0x00000000u, 0x00000000u, 0x00000022u}},
        {{0x00000001u, 0x00000000u, 0x00000000u, 0x00000033u}, {0x00000001u, 0x00000000u, 0x00000000u, 0x00000022u}},
        {{0x00000000u, 0x00000022u, 0x00000033u, 0x00000001u}, {0x00000000u, 0x00000000u, 0x00000022u, 0x00000001u}},
        {{0x00000044u, 0x00000033u, 0x00000022u, 0x00000011u}, {0x00000000u, 0x00000022u, 0x00000000u, 0x00000011u}},
        {{0x00000001u, 0x00000022u, 0x00000000u, 0x00000044u}, {0x00000001u, 0x00000000u, 0x00000000u, 0x00000000u}},
        {{0x00000011u, 0x00000044u, 0x00000022u, 0x00000033u}, {0x00000011u, 0x00000000u, 0x00000000u, 0x00000022u}},
        {{0x00000000u, 0x00000001u, 0x00000011u, 0x00000022u}, {0x00000000u, 0x00000001u, 0x00000011u, 0x00000000u}},
    },
    {
        {{0x00004000u, 0x00004200u, 0x00004400u, 0x00004500u}, {0x00004000u, 0x00000000u, 0x00004200u, 0x00000000u}},
        {{0x00004000u, 0x00004200u, 0x00004400u, 0x00003c00u}, {0x00004000u, 0x00000000u, 0x00004200u, 0x00003c00u}},
        {{0x00004400u, 0x00004200u, 0x00004000u, 0x00004500u}, {0x00004200u, 0x00000000u, 0x00004000u, 0x00000000u}},
        {{0x00004000u, 0x00004200u, 0x00000000u, 0x00003c00u}, {0x00004000u, 0x00000000u, 0x00000000u, 0x00003c00u}},
        {{0x00000000u, 0x00000000u, 0x00000000u, 0x00004500u}, {0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u}},
        {{0x00003c00u, 0x00000000u, 0x00000000u, 0x00004500u}, {0x00003c00u, 0x00000000u, 0x00000000u, 0x00000000u}},
        {{0x00003c00u, 0x00000000u, 0x00004500u, 0x00004400u}, {0x00003c00u, 0x00000000u, 0x00000000u, 0x00004200u}},
        {{0x00004000u, 0x00004200u, 0x00004500u, 0x00004400u}, {0x00004000u, 0x00000000u, 0x00000000u, 0x00004200u}},
        {{0x00003c00u, 0x00000000u, 0x00000000u, 0x00004400u}, {0x00003c00u, 0x00000000u, 0x00000000u, 0x00004200u}},
        {{0x00000000u, 0x00004200u, 0x00004400u, 0x00003c00u}, {0x00000000u, 0x00000000u, 0x00004200u, 0x00003c00u}},
        {{0x00004500u, 0x00004400u, 0x00004200u, 0x00004000u}, {0x00000000u, 0x00004200u, 0x00000000u, 0x00004000u}},
        {{0x00003c00u, 0x00004200u, 0x00000000u, 0x00004500u}, {0x00003c00u, 0x00000000u, 0x00000000u, 0x00000000u}},
        {{0x00004000u, 0x00004500u, 0x00004200u, 0x00004400u}, {0x00004000u, 0x00000000u, 0x00000000u, 0x00004200u}},
        {{0x00000000u, 0x00003c00u, 0x00004000u, 0x00004200u}, {0x00000000u, 0x00003c00u, 0x00004000u, 0x00000000u}},
    },
    {
        {{0x40000000u, 0x40400000u, 0x40800000u, 0x40a00000u}, {0x40000000u, 0x00000000u, 0x40400000u, 0x00000000u}},
        {{0x40000000u, 0x40400000u, 0x40800000u, 0x3f800000u}, {0x40000000u, 0x00000000u, 0x40400000u, 0x3f800000u}},
        {{0x40800000u, 0x40400000u, 0x40000000u, 0x40a00000u}, {0x40400000u, 0x00000000u, 0x40000000u, 0x00000000u}},
        {{0x40000000u, 0x40400000u, 0x00000000u, 0x3f800000u}, {0x40000000u, 0x00000000u, 0x00000000u, 0x3f800000u}},
        {{0x00000000u, 0x00000000u, 0x00000000u, 0x40a00000u}, {0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u}},
        {{0x3f800000u, 0x00000000u, 0x00000000u, 0x40a00000u}, {0x3f800000u, 0x00000000u, 0x00000000u, 0x00000000u}},
        {{0x3f800000u, 0x00000000u, 0x40a00000u, 0x40800000u}, {0x3f800000u, 0x00000000u, 0x00000000u, 0x40400000u}},
        {{0x40000000u, 0x40400000u, 0x40a00000u, 0x40800000u}, {0x40000000u, 0x00000000u, 0x00000000u, 0x40400000u}},
        {{0x3f800000u, 0x00000000u, 0x00000000u, 0x40800000u}, {0x3f800000u, 0x00000000u, 0x00000000u, 0x40400000u}},
        {{0x00000000u, 0x40400000u, 0x40800000u, 0x3f800000u}, {0x00000000u, 0x00000000u, 0x40400000u, 0x3f800000u}},
        {{0x40a00000u, 0x40800000u, 0x40400000u, 0x40000000u}, {0x00000000u, 0x40400000u, 0x00000000u, 0x40000000u}},
        {{0x3f800000u, 0x40400000u, 0x00000000u, 0x40a00000u}, {0x3f800000u, 0x00000000u, 0x00000000u, 0x00000000u}},
        {{0x40000000u, 0x40a00000u, 0x40400000u, 0x40800000u}, {0x40000000u, 0x00000000u, 0x00000000u, 0x40400000u}},
        {{0x00000000u, 0x3f800000u, 0x40000000u, 0x40400000u}, {0x00000000u, 0x3f800000u, 0x40000000u, 0x00000000u}},
    },
};

std::string Hex(std::uint32_t value) {
    char text[16];
    std::snprintf(text, sizeof(text), "0x%08x", value);
    return text;
}

std::uint64_t TexelOffset(const Format& format, std::uint32_t x, std::uint32_t y) {
    const auto mip = AgcDriver::Graphics::ComputeMipLayout(AgcDriver::Graphics::TextureTileMode::kLinear, format.format, Width, Height, 1u).at(0);
    return mip.tiledOffset + static_cast<std::uint64_t>(y) * mip.pitchBytes + static_cast<std::uint64_t>(x) * format.channels * format.bits / 8u;
}

std::array<std::uint32_t, 4> BufferDescriptor(const void* data, std::uint32_t bytes) {
    const auto address = reinterpret_cast<std::uintptr_t>(data);
    return {static_cast<std::uint32_t>(address), static_cast<std::uint32_t>((address >> 32u) & 0xffffu), bytes, 0x31016facu};
}

std::array<std::uint32_t, 8> TextureDescriptor(const void* data, std::uint32_t format, std::uint32_t swizzle) {
    const auto address = static_cast<std::uint64_t>(reinterpret_cast<std::uintptr_t>(data));
    return {
        static_cast<std::uint32_t>(address >> 8u),
        static_cast<std::uint32_t>((address >> 40u) & 0xffu) | (format << 20u) | (((Width - 1u) & 3u) << 30u),
        ((Width - 1u) >> 2u) | ((Height - 1u) << 14u),
        swizzle | (Type2D << 28u),
        0u,
        0u,
        0u,
        0u,
    };
}

std::array<std::uint32_t, 4> VertexBufferDescriptor(const void* data, std::uint32_t stride, std::uint32_t count) {
    const auto address = reinterpret_cast<std::uintptr_t>(data);
    return {static_cast<std::uint32_t>(address), static_cast<std::uint32_t>((address >> 32u) & 0xffffu) | (stride << 16u), count, 0x01016facu};
}

std::vector<std::uint32_t> PixelCode(const Format& format) {
    return {
        0x7e3c0f00u, 0x7e3e0280u, 0x7e2802ffu, format.data[0], 0x7e2a02ffu, format.data[1], 0x7e2c02ffu, format.data[2], 0x7e2e02ffu, format.data[3],
        0xf0201f08u, 0x0001141eu, 0x7e3e0281u, 0xf0201508u, 0x0001141eu, 0x7e0e02f2u, 0xf800180fu, 0x07070707u, 0xbf810000u,
    };
}

void Draw(AgcDriver::VulkanDevice& device, std::uint8_t* texels, const Format& format, std::uint32_t swizzle) {
    static const std::vector<std::array<float, 4>> triangles{
        {-1.0f, -1.0f, 0.5f, 1.0f}, {1.0f, -1.0f, 0.5f, 1.0f}, {-1.0f, 1.0f, 0.5f, 1.0f},
        {1.0f, -1.0f, 0.5f, 1.0f}, {1.0f, 1.0f, 0.5f, 1.0f}, {-1.0f, 1.0f, 0.5f, 1.0f},
    };
    const auto& target = device.Target();
    Pixels.fill(std::byte{0});
    std::memset(texels, 0xcd, TexelBytes);
    std::vector<std::uint32_t> vertexUserData(4, 0u);
    const auto vertexBuffer = VertexBufferDescriptor(triangles.data(), 16u, static_cast<std::uint32_t>(triangles.size()));
    std::copy(vertexBuffer.begin(), vertexBuffer.end(), vertexUserData.begin());
    const std::array<ShaderRecompiler::MemoryRegion, 1> vertexMemory{{{reinterpret_cast<std::uintptr_t>(VertexCode.data()), std::as_bytes(std::span(VertexCode))}}};
    ShaderRecompiler::RecompileRequest vertex{
        {ShaderStage::Vertex, reinterpret_cast<std::uintptr_t>(VertexCode.data()), VertexCode, 0, {}},
        {64, 0, vertexUserData, std::nullopt, std::nullopt, ShaderRecompiler::ShaderVertexStageInfo{}, vertexMemory},
        target,
        {0, 0, 0, 64}
    };
    vertex.useCache = false;
    const auto vertexResult = ShaderRecompiler::Recompile(vertex);
    const auto vertexPush = static_cast<std::uint32_t>(vertexResult.pushConstants.size());

    ShaderRecompiler::ShaderPixelStageInfo pixel{};
    pixel.wave32 = false;
    pixel.inputAddr = ShaderRecompiler::PixelInputBit(ShaderRecompiler::PixelInput::PositionX) | ShaderRecompiler::PixelInputBit(ShaderRecompiler::PixelInput::PositionY);
    pixel.posX = true;
    pixel.posY = true;
    pixel.targetOutputMode[0] = 9;
    pixel.targetExportMapping.fill(0xe4u);
    std::vector<std::uint32_t> pixelUserData(16, 0u);
    const auto texture = TextureDescriptor(texels, format.format, swizzle);
    std::copy(texture.begin(), texture.end(), pixelUserData.begin() + 4);
    const auto code = PixelCode(format);
    const std::span<const std::uint32_t> words(code);
    const std::array<ShaderRecompiler::MemoryRegion, 1> pixelMemory{{{reinterpret_cast<std::uintptr_t>(code.data()), std::as_bytes(words)}}};
    ShaderRecompiler::RecompileRequest fragment{
        {ShaderStage::Fragment, reinterpret_cast<std::uintptr_t>(code.data()), words, 0, {}},
        {64, 0, pixelUserData, std::nullopt, pixel, std::nullopt, pixelMemory},
        target,
        PixelPushLayout(vertexPush, target)
    };
    fragment.useCache = false;
    const auto pixelResult = ShaderRecompiler::Recompile(fragment);
    Require(pixelResult.registrationModule == target.registrationModules, std::string("image store dst_sel: the pixel shader was ") + (target.registrationModules ? "not " : "") + "emitted in registration form");
    const std::array<AgcDriver::Graphics::CompiledShader, 2> shaders{{
        {ShaderStage::Vertex, &vertexResult, 0},
        {ShaderStage::Fragment, &pixelResult, PixelPushOffset(vertexPush, target)}
    }};

    AgcDriver::Graphics::State state{};
    state.stages = {AgcDriver::Graphics::ShaderPath::Vertex, 0u, 64u, 64u, std::nullopt, std::nullopt};
    const AgcDriver::Graphics::ColorTargetLayout colorLayout(Width, 1u, AgcDriver::Graphics::ColorTileMode::Linear, 4u);
    Require(colorLayout.Bytes() <= Pixels.size(), "image store dst_sel: the color target does not fit its buffer");
    state.color = {reinterpret_cast<std::uintptr_t>(Pixels.data()), {Width, 1u}, VK_FORMAT_R8G8B8A8_UNORM, colorLayout.Bytes(), 0xe4u};
    state.colors = {state.color};
    state.hasColorTarget = true;
    state.renderExtent = {Width, 1u};
    state.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    state.viewport = {0, 1.0f, static_cast<float>(Width), -1.0f, 0, 1};
    state.negativeOneToOne = false;
    state.scissor = {{0, 0}, {Width, 1u}};
    state.cullMode = VK_CULL_MODE_NONE;
    state.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
    state.blend.colorWriteMask = 15;
    state.blends = {state.blend};
    state.blendConstants = {};
    const AgcDriver::Pm4::DrawParameters draw{0, static_cast<std::uint32_t>(triangles.size()), 0, 1, 0, false};
    device.Draw(state, draw, shaders);
    device.WaitIdle();
    AgcDriver::Graphics::StorageTexture::FlushPending(reinterpret_cast<std::uintptr_t>(texels), TexelBytes, nullptr, "test");
    device.WaitIdle();
}

void Store(AgcDriver::VulkanDevice& device, std::uint8_t* texels, const Format& format, std::uint32_t swizzle) {
    for (std::uint32_t tid = 0; tid < Threads; ++tid) {
        for (std::uint32_t index = 0; index < 4u; ++index) Input[tid * 4u + index] = format.data[index];
    }
    std::memset(texels, 0xcd, TexelBytes);
    std::vector<std::uint32_t> userData(16, 0u);
    const auto buffer = BufferDescriptor(Input.data(), static_cast<std::uint32_t>(Input.size() * 4u));
    const auto texture = TextureDescriptor(texels, format.format, swizzle);
    std::copy(buffer.begin(), buffer.end(), userData.begin());
    std::copy(texture.begin(), texture.end(), userData.begin() + 4);
    const std::span<const std::uint32_t> code(StoreCode);
    const std::array<ShaderRecompiler::MemoryRegion, 1> memory{{{reinterpret_cast<std::uintptr_t>(code.data()), std::as_bytes(code)}}};
    const ShaderRecompiler::ShaderComputeStageInfo compute{{Threads, 1, 1}, 0u, {false, false, false}, false, 1};
    ShaderRecompiler::RecompileRequest request{
        {ShaderStage::Compute, reinterpret_cast<std::uintptr_t>(code.data()), code, 0, {}},
        {32, 0, userData, compute, std::nullopt, std::nullopt, memory},
        device.Target(),
        {0, 0, 0, 128}
    };
    request.useCache = false;
    const auto result = ShaderRecompiler::Recompile(request);
    Require(!result.registrationModule, "image store dst_sel: the compute shader was emitted in registration form");
    device.Dispatch(result, 1, 1, 1, {}, reinterpret_cast<std::uintptr_t>(code.data()));
    device.WaitIdle();
    AgcDriver::Graphics::StorageTexture::FlushPending(reinterpret_cast<std::uintptr_t>(texels), TexelBytes, nullptr, "test");
    device.WaitIdle();
}

void Check(const std::uint8_t* texels, std::uint32_t formatIndex, std::uint32_t tuple, const std::string& shader) {
    const auto& format = Formats[formatIndex];
    const auto bytes = format.bits / 8u;
    for (std::uint32_t row = 0; row < RowCount; ++row) {
        for (std::uint32_t x = 0; x < Width; ++x) {
            const auto offset = TexelOffset(format, x, row);
            for (std::uint32_t channel = 0; channel < format.channels; ++channel) {
                std::uint32_t actual = 0;
                std::memcpy(&actual, texels + offset + channel * bytes, bytes);
                const auto expected = Expected[formatIndex][tuple][row][channel];
                Require(actual == expected, std::string("image_store DST_SEL ") + TupleNames[tuple] + " dmask " + Hex(Dmasks[row]) + " of " + format.name + " (" + shader + ")" +
                                                ": texel " + std::to_string(x) + " channel " + std::to_string(channel) + " is " + Hex(actual) + ", expected " + Hex(expected));
            }
        }
    }
}

}

int main() {
    try {
        const auto device = OpenVulkanTestDevice();
        if (!device) return VulkanTestSkipped;
        GuestBlock block;
        auto* texels = block.Data();
        const std::string pixelShader = device->Target().registrationModules ? "pixel shader, registration module" : "pixel shader, specialized module";
        for (std::uint32_t format = 0; format < FormatCount; ++format) {
            for (std::uint32_t tuple = 0; tuple < TupleCount; ++tuple) {
                Store(*device, texels, Formats[format], Swizzles[tuple]);
                Check(texels, format, tuple, "compute shader, specialized module");
                Draw(*device, texels, Formats[format], Swizzles[tuple]);
                Check(texels, format, tuple, pixelShader);
            }
        }
        std::printf("image store dst_sel tests passed (compute shader, specialized module; %s)\n", pixelShader.c_str());
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
