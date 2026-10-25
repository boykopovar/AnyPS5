#include "prx/libSceAgcDriver/Execution/include/Driver.hpp"
#include "prx/libSceAgcDriver/Execution/include/VulkanDevice.hpp"
#include "prx/libc/include/GuestAllocations.hpp"
#include "Recompiler.hpp"
#include "SceShaders.hpp"
#include "VulkanTestDevice.hpp"
#include <spirv/unified1/spirv.hpp>
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif
#include <algorithm>
#include <array>
#include <bit>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <optional>
#include <span>
#include <string>
#include <tuple>
#include <unordered_set>
#include <utility>
#include <vector>

namespace {

using AgcDriver::Graphics::Require;
using ShaderRecompiler::ShaderStage;

constexpr std::uint32_t Width = 256;
constexpr std::uint32_t Height = 80;
constexpr std::array<std::uint8_t, 4> Background{16, 24, 40, 255};
constexpr std::size_t GuestBytes = 65536;
alignas(256) std::array<std::byte, Width * Height * 4> Pixels{};
std::array<std::uint32_t, 16> HullUserData{0x1000u};

alignas(256) constexpr std::array<std::uint32_t, 29> LocalCode{
    0xd5480004, 0x02090502, 0xd5480005, 0x02090102, 0xd5480006, 0x02110902, 0xd5480007, 0x02111102,
    0x7e080d04, 0x7e0a0d05, 0x7e0c0d06, 0x7e0e0d07, 0x10100cff, 0x3f19999a, 0x061010ff, 0xbf666666,
    0x3e1008ff, 0x3e800000, 0xd54b0009, 0x03c5e105, 0x161406a0, 0xd9340000, 0x0000080a, 0xd8340008,
    0x0000070a, 0xd8340010, 0x0000070a, 0xbf8cc07f, 0xbefd2106,
};

alignas(256) constexpr std::array<std::uint32_t, 49> HullCode{
    0xd7650000, 0x000100c1, 0xd7660000, 0x000200c1, 0xbf8a0000, 0x7da206f9, 0x81060000, 0xd5480014,
    0x02151101, 0x362402ff, 0x000000ff, 0x160028a0, 0xd5430015, 0x040224ff, 0x00000060, 0xdbfc0000,
    0x02000015, 0x162c2890, 0xd5430016, 0x045a24ff, 0x00000030, 0xbf8cc07f, 0xe0781000, 0x02020216,
    0x7da82881, 0xbe94047e, 0xb06a0060, 0x162602f9, 0x0086066a, 0xd8d80008, 0x06000013, 0xd8dc1008,
    0x07000013, 0xbf8cc07f, 0x7e300306, 0x7e320306, 0x7e340306, 0x7e360306, 0x34142484, 0xbefe0414,
    0xe0781000, 0x0403180a, 0x4a162481, 0x16161690, 0x4c1616ff, 0x00008000, 0xe0741000, 0x0202070b,
    0xbf810000,
};

alignas(256) constexpr std::array<std::uint32_t, 54> HullCompareBranch{
    0xd4040016, 0x00020080, 0xbeea0416, 0xbf870001, 0xbf800000, 0xd7650000, 0x000100c1, 0xd7660000,
    0x000200c1, 0xbf8a0000, 0x7da206f9, 0x81060000, 0xd5480014, 0x02151101, 0x362402ff, 0x000000ff,
    0x160028a0, 0xd5430015, 0x040224ff, 0x00000060, 0xdbfc0000, 0x02000015, 0x162c2890, 0xd5430016,
    0x045a24ff, 0x00000030, 0xbf8cc07f, 0xe0781000, 0x02020216, 0x7da82881, 0xbe94047e, 0xb06a0060,
    0x162602f9, 0x0086066a, 0xd8d80008, 0x06000013, 0xd8dc1008, 0x07000013, 0xbf8cc07f, 0x7e300306,
    0x7e320306, 0x7e340306, 0x7e360306, 0x34142484, 0xbefe0414, 0xe0781000, 0x0403180a, 0x4a162481,
    0x16161690, 0x4c1616ff, 0x00008000, 0xe0741000, 0x0202070b, 0xbf810000,
};

alignas(256) constexpr std::array<std::uint32_t, 49> HullSingleInput{
    0xd7650000, 0x000100c1, 0xd7660000, 0x000200c1, 0xbf8a0000, 0x7da206f9, 0x81060000, 0xd5480014,
    0x02151101, 0x362402ff, 0x000000ff, 0x160028a0, 0xd5430015, 0x022224ff, 0x00000020, 0xdbfc0000,
    0x02000015, 0x162c2890, 0xd5430016, 0x045a24ff, 0x00000030, 0xbf8cc07f, 0xe0781000, 0x02020216,
    0x7da82881, 0xbe94047e, 0xb06a0020, 0x162602f9, 0x0086066a, 0xd8d80008, 0x06000013, 0xd8dc0201,
    0x07000013, 0xbf8cc07f, 0x7e300306, 0x7e320306, 0x7e340306, 0x7e360306, 0x34142484, 0xbefe0414,
    0xe0781000, 0x0403180a, 0x4a162481, 0x16161690, 0x4c1616ff, 0x00008000, 0xe0741000, 0x0202070b,
    0xbf810000,
};

alignas(256) constexpr std::array<std::uint32_t, 49> HullSingleOutput{
    0xd7650000, 0x000100c1, 0xd7660000, 0x000200c1, 0xbf8a0000, 0x7da206f9, 0x81060000, 0xd5480014,
    0x02151101, 0x362402ff, 0x000000ff, 0x160028a0, 0xd5430015, 0x022224ff, 0x00000020, 0xdbfc0000,
    0x02000015, 0x7e2c0280, 0xd5430016, 0x045a24ff, 0x00000010, 0xbf8cc07f, 0xe0781000, 0x02020216,
    0xbf800000, 0xbe94047e, 0xb06a0020, 0x162602f9, 0x0086066a, 0xd8d80008, 0x06000013, 0xd8dc0201,
    0x07000013, 0xbf8cc07f, 0x7e300306, 0x7e320306, 0x7e340306, 0x7e360306, 0x34142484, 0xbefe0414,
    0xe0781000, 0x0403180a, 0x4a162481, 0x16161690, 0x4c1616ff, 0x00008000, 0xe0741000, 0x0202070b,
    0xbf810000,
};

alignas(256) constexpr std::array<std::uint32_t, 51> HullRestoredExecSaved{
    0xd7650000, 0x000100c1, 0xd7660000, 0x000200c1, 0xbf8a0000, 0x7da206f9, 0x81060000, 0xd5480014,
    0x02151101, 0x362402ff, 0x000000ff, 0x160028a0, 0xd5430015, 0x040224ff, 0x00000060, 0xdbfc0000,
    0x02000015, 0x162c2890, 0xd5430016, 0x045a24ff, 0x00000030, 0xbf8cc07f, 0xe0781000, 0x02020216,
    0x7da82881, 0xbe94047e, 0xb06a0060, 0x162602f9, 0x0086066a, 0xd8d80008, 0x06000013, 0xd8dc1008,
    0x07000013, 0xbf8cc07f, 0x7e300306, 0x7e320306, 0x7e340306, 0x7e360306, 0x34142484, 0xbefe0414,
    0xbe94047e, 0xbefe0414, 0xe0781000, 0x0403180a, 0x4a162481, 0x16161690, 0x4c1616ff, 0x00008000,
    0xe0741000, 0x0202070b, 0xbf810000,
};

alignas(256) constexpr std::array<std::uint32_t, 29> LocalCodeStride24{
    0xd5480004, 0x02090502, 0xd5480005, 0x02090102, 0xd5480006, 0x02110902, 0xd5480007, 0x02111102,
    0x7e080d04, 0x7e0a0d05, 0x7e0c0d06, 0x7e0e0d07, 0x10100cff, 0x3f19999a, 0x061010ff, 0xbf666666,
    0x3e1008ff, 0x3e800000, 0xd54b0009, 0x03c5e105, 0x16140698, 0xd9340000, 0x0000080a, 0xd8340008,
    0x0000070a, 0xd8340010, 0x0000070a, 0xbf8cc07f, 0xbefd2106,
};

alignas(256) constexpr std::array<std::uint32_t, 51> HullNextInput{
    0xd7650000, 0x000100c1, 0xd7660000, 0x000200c1, 0xbf8a0000, 0x7da206f9, 0x81060000, 0xd5480014,
    0x02151101, 0x362402ff, 0x000000ff, 0x16002898, 0xd5430015, 0x040224ff, 0x00000060, 0xdbfc0018,
    0x02000015, 0x162c2890, 0xd5430016, 0x045a24ff, 0x00000030, 0xbf8cc07f, 0xe0781000, 0x02020216,
    0x7da82881, 0xbe94047e, 0xb06a0060, 0x162602f9, 0x0086066a, 0xd8d80008, 0x06000013, 0xd8dc1008,
    0x07000013, 0xbf8cc07f, 0x7e300306, 0x7e320306, 0x7e340306, 0x7e360306, 0x34142484, 0xbefe0414,
    0xbe94047e, 0xbefe0414, 0xe0781000, 0x0403180a, 0x4a162481, 0x16161690, 0x4c1616ff, 0x00008000,
    0xe0741000, 0x0202070b, 0xbf810000,
};

alignas(256) constexpr std::array<std::uint32_t, 30> DomainCode{
    0xf8000941, 0x00000000, 0x34020e84, 0x16020283, 0xe0341000, 0x04030a01, 0xe0341010, 0x04030c01,
    0xe0341020, 0x04030e01, 0xbf8c3f70, 0x08040af2, 0x08040d02, 0x10201505, 0x3e201906, 0x3e201d02,
    0x10221705, 0x3e221b06, 0x3e221f02, 0xd5510012, 0x040a0d05, 0x102424ff, 0x40400000, 0x7e260280,
    0x7e2802f2, 0xf80008cf, 0x14131110, 0xf800020f, 0x14131214, 0xbf810000,
};

alignas(256) constexpr std::array<std::uint32_t, 37> DomainComputedPoint{
    0xf8000941, 0x00000000, 0x34020e84, 0x16020283, 0xe0341000, 0x04030a01, 0xbf8c3f70, 0x7c0214ff,
    0x7f7fffff, 0xd5010002, 0x01a90280, 0xd7460002, 0x04050902, 0xe0341010, 0x04030c02, 0xe0341020,
    0x04030e02, 0xbf8c3f70, 0x08040af2, 0x08040d02, 0x10201505, 0x3e201906, 0x3e201d02, 0x10221705,
    0x3e221b06, 0x3e221f02, 0xd5510012, 0x040a0d05, 0x102424ff, 0x40400000, 0x7e260280, 0x7e2802f2,
    0xf80008cf, 0x14131110, 0xf800020f, 0x14131214, 0xbf810000,
};

alignas(256) constexpr std::array<std::uint32_t, 31> DomainLoweredInterior{
    0xf8000941, 0x00000000, 0x34020e84, 0x16020283, 0xe0341000, 0x04030a01, 0xe0341010, 0x04030c01,
    0xe0341020, 0x04030e01, 0xbf8c3f70, 0x08040af2, 0x08040d02, 0x10201505, 0x3e201906, 0x3e201d02,
    0x10221705, 0x3e221b06, 0x3e221f02, 0xd5510012, 0x040a0d05, 0x102424ff, 0x40400000, 0x08222511,
    0x7e260280, 0x7e2802f2, 0xf80008cf, 0x14131110, 0xf800020f, 0x14131214, 0xbf810000,
};

alignas(256) constexpr std::array<std::uint32_t, 34> DomainRelativeData{
    0xf8000941, 0x00000000, 0x34020e84, 0x16020283, 0x4a2a0e81, 0x7e2a0d15, 0x102a2aff, 0x3e000000,
    0xe0341000, 0x04030a01, 0xe0341010, 0x04030c01, 0xe0341020, 0x04030e01, 0xbf8c3f70, 0x08040af2,
    0x08040d02, 0x10201505, 0x3e201906, 0x3e201d02, 0x10221705, 0x3e221b06, 0x3e221f02, 0xd5510012,
    0x040a0d05, 0x102424ff, 0x40400000, 0x7e260280, 0x7e2802f2, 0xf80008cf, 0x14131110, 0xf800020f,
    0x14151214, 0xbf810000,
};

alignas(256) constexpr std::array<std::uint32_t, 35> DomainCompareBranch{
    0xf8000941, 0x00000000, 0xd4040014, 0x00020a80, 0xbeea0414, 0xbf870001, 0xbf800000, 0x34020e84,
    0x16020283, 0xe0341000, 0x04030a01, 0xe0341010, 0x04030c01, 0xe0341020, 0x04030e01, 0xbf8c3f70,
    0x08040af2, 0x08040d02, 0x10201505, 0x3e201906, 0x3e201d02, 0x10221705, 0x3e221b06, 0x3e221f02,
    0xd5510012, 0x040a0d05, 0x102424ff, 0x40400000, 0x7e260280, 0x7e2802f2, 0xf80008cf, 0x14131110,
    0xf800020f, 0x14131214, 0xbf810000,
};

alignas(256) constexpr std::array<std::uint32_t, 29> LocalIndexAsData{
    0xd5480004, 0x02090502, 0xd5480005, 0x02090102, 0xd5480006, 0x02110902, 0xd5480007, 0x02111102,
    0x7e080d04, 0x7e0a0d05, 0x7e0c0d06, 0x7e0e0d07, 0x10100cff, 0x3f19999a, 0x061010ff, 0xbf666666,
    0x3e1008ff, 0x3e800000, 0xd54b0009, 0x03c5e105, 0x161406a0, 0xd9340000, 0x0000080a, 0xd8340008,
    0x0000030a, 0xd8340010, 0x0000070a, 0xbf8cc07f, 0xbefd2106,
};

alignas(256) constexpr std::array<std::uint32_t, 7> LocalInactiveIndex{
    0x7e108704, 0x7da80b02, 0x160e06a8, 0x7e060302, 0xd9340020, 0x00000207, 0xbe802006,
};

alignas(256) constexpr std::array<std::uint32_t, 8> LocalRestoredIndex{
    0x7e108704, 0x160e06a8, 0x7da80b02, 0x7e060302, 0xbefe04c1, 0xd9340020, 0x00000207, 0xbe802006,
};

alignas(256) constexpr std::array<std::uint32_t, 7> LocalObservedWindow{
    0xbefc0383, 0x7da80b02, 0x160e06a8, 0x7e108704, 0xd9340020, 0x00000807, 0xbe802006,
};

alignas(256) constexpr std::array<std::uint32_t, 9> LocalNegatedConjunction{
    0x7d880b02, 0x160e06a8, 0x02100e80, 0x7da20d02, 0x87fe6a7e, 0xbefe087e, 0xd9340020, 0x00000807, 0xbe802006,
};

alignas(256) constexpr std::array<std::uint32_t, 50> HullDescendingRecords{
    0xd7650000, 0x000100c1, 0xd7660000, 0x000200c1, 0xbf8a0000, 0x7da206f9, 0x81060000, 0xd5480014,
    0x02151101, 0x362402ff, 0x000000ff, 0x160028a0, 0xd5430015, 0x040224ff, 0x00000060, 0xdbfc0000,
    0x02000015, 0x162c2890, 0xd5430016, 0x045a24ff, 0x00000030, 0xbf8cc07f, 0xe0781000, 0x02020216,
    0x7da82881, 0xbe94047e, 0xb06a0060, 0x162602f9, 0x0086066a, 0xd8d80008, 0x06000013, 0xd8dc1008,
    0x07000013, 0xbf8cc07f, 0x7e300306, 0x7e320306, 0x7e340306, 0x7e360306, 0x34142484, 0xbefe0414,
    0xe0781000, 0x0403180a, 0x4a162481, 0x161616ff, 0x000000d0, 0x4c1616ff, 0x00008000, 0xe0741000,
    0x0202070b, 0xbf810000,
};

alignas(256) constexpr std::array<std::uint32_t, 52> HullPatchConstant{
    0xd7650000, 0x000100c1, 0xd7660000, 0x000200c1, 0xbf8a0000, 0x7da206f9, 0x81060000, 0xd5480014,
    0x02151101, 0x362402ff, 0x000000ff, 0x160028a0, 0xd5430015, 0x040224ff, 0x00000060, 0xdbfc0000,
    0x02000015, 0x162c2890, 0xd5430016, 0x045a24ff, 0x00000030, 0xbf8cc07f, 0xe0781000, 0x02020216,
    0x7da82881, 0xbe94047e, 0xb06a0060, 0x162602f9, 0x0086066a, 0xd8d80008, 0x06000013, 0xd8dc1008,
    0x07000013, 0xbf8cc07f, 0x7e300306, 0x7e320306, 0x7e340306, 0x7e360306, 0x34142484, 0xbefe0414,
    0xe0781000, 0x0403180a, 0x4a162481, 0x161616ff, 0x000000d0, 0x4c1616ff, 0x00008000, 0xe0741000,
    0x0202070b, 0xe0701064, 0x0202060b, 0xbf810000,
};

alignas(256) constexpr std::array<std::uint32_t, 54> HullRelativeData{
    0xd7650000, 0x000100c1, 0xd7660000, 0x000200c1, 0xbf8a0000, 0x7da206f9, 0x81060000, 0xd5480014,
    0x02151101, 0x362402ff, 0x000000ff, 0x160028a0, 0xd5430015, 0x040224ff, 0x00000060, 0xdbfc0000,
    0x02000015, 0x162c2890, 0xd5430016, 0x045a24ff, 0x00000030, 0xbf8cc07f, 0xe0781000, 0x02020216,
    0x7da82881, 0xbe94047e, 0xb06a0060, 0x162602f9, 0x0086066a, 0xd8d80008, 0x06000013, 0xd8dc1008,
    0x07000013, 0xbf8cc07f, 0x7e300306, 0x7e320306, 0x7e340306, 0x7e360306, 0x34142484, 0xbefe0414,
    0xe0781000, 0x0403180a, 0x4a162481, 0x161616ff, 0x000000d0, 0x4c1616ff, 0x00008000, 0xe0741000,
    0x0202070b, 0x4a182481, 0x7e180d0c, 0xe0701064, 0x02020c0b, 0xbf810000,
};

alignas(256) constexpr std::array<std::uint32_t, 46> HullUnguardedFactors{
    0xd7650000, 0x000100c1, 0xd7660000, 0x000200c1, 0xbf8a0000, 0x7da206f9, 0x81060000, 0xd5480014,
    0x02151101, 0x362402ff, 0x000000ff, 0x160028a0, 0xd5430015, 0x040224ff, 0x00000060, 0xdbfc0000,
    0x02000015, 0x162c2890, 0xd5430016, 0x045a24ff, 0x00000030, 0xbf8cc07f, 0xe0781000, 0x02020216,
    0xb06a0060, 0x162602f9, 0x0086066a, 0xd8d80008, 0x06000013, 0xd8dc1008, 0x07000013, 0xbf8cc07f,
    0x7e300306, 0x7e320306, 0x7e340306, 0x7e360306, 0x34142484, 0xe0781000, 0x0403180a, 0x4a162481,
    0x16161690, 0x4c1616ff, 0x00008000, 0xe0741000, 0x0202070b, 0xbf810000,
};

alignas(256) constexpr std::array<std::uint32_t, 50> HullDynamicIndex{
    0xd7650000, 0x000100c1, 0xd7660000, 0x000200c1, 0xbf8a0000, 0x7da206f9, 0x81060000, 0xd5480014,
    0x02151101, 0x362402ff, 0x000000ff, 0x26002882, 0x34000085, 0xd5430015, 0x040224ff, 0x00000060,
    0xdbfc0000, 0x02000015, 0x162c2890, 0xd5430016, 0x045a24ff, 0x00000030, 0xbf8cc07f, 0xe0781000,
    0x02020216, 0x7da82881, 0xbe94047e, 0xb06a0060, 0x162602f9, 0x0086066a, 0xd8d80008, 0x06000013,
    0xd8dc1008, 0x07000013, 0xbf8cc07f, 0x7e300306, 0x7e320306, 0x7e340306, 0x7e360306, 0x34142484,
    0xbefe0414, 0xe0781000, 0x0403180a, 0x4a162481, 0x16161690, 0x4c1616ff, 0x00008000, 0xe0741000,
    0x0202070b, 0xbf810000,
};

alignas(256) constexpr std::array<std::uint32_t, 50> HullHalfSlotIndex{
    0xd7650000, 0x000100c1, 0xd7660000, 0x000200c1, 0xbf8a0000, 0x7da206f9, 0x81060000, 0xd5480014,
    0x02151101, 0x362402ff, 0x000000ff, 0x26002882, 0x34000084, 0xd5430015, 0x040224ff, 0x00000060,
    0xdbfc0000, 0x02000015, 0x162c2890, 0xd5430016, 0x045a24ff, 0x00000030, 0xbf8cc07f, 0xe0781000,
    0x02020216, 0x7da82881, 0xbe94047e, 0xb06a0060, 0x162602f9, 0x0086066a, 0xd8d80008, 0x06000013,
    0xd8dc1008, 0x07000013, 0xbf8cc07f, 0x7e300306, 0x7e320306, 0x7e340306, 0x7e360306, 0x34142484,
    0xbefe0414, 0xe0781000, 0x0403180a, 0x4a162481, 0x16161690, 0x4c1616ff, 0x00008000, 0xe0741000,
    0x0202070b, 0xbf810000,
};

alignas(256) constexpr std::array<std::uint32_t, 47> HullSquaredAddress{
    0xd7650000, 0x000100c1, 0xd7660000, 0x000200c1, 0xbf8a0000, 0x7da206f9, 0x81060000, 0xd5480014,
    0x02151101, 0x362402ff, 0x000000ff, 0x16002914, 0xd5430015, 0x040224ff, 0x00000060, 0xdbfc0000,
    0x02000015, 0x162c2890, 0xd5430016, 0x045a24ff, 0x00000030, 0xbf8cc07f, 0xe0781000, 0x02020216,
    0x7da82881, 0xb06a0060, 0x162602f9, 0x0086066a, 0xd8d80008, 0x06000013, 0xd8dc1008, 0x07000013,
    0xbf8cc07f, 0x7e300306, 0x7e320306, 0x7e340306, 0x7e360306, 0x34142484, 0xe0781000, 0x0403180a,
    0x4a162481, 0x16161690, 0x4c1616ff, 0x00008000, 0xe0741000, 0x0202070b, 0xbf810000,
};

alignas(256) constexpr std::array<std::uint32_t, 49> HullAndSaveexec{
    0xd7650000, 0x000100c1, 0xd7660000, 0x000200c1, 0xbf8a0000, 0x7da206f9, 0x81060000, 0xd5480014,
    0x02151101, 0x362402ff, 0x000000ff, 0x160028a0, 0xd5430015, 0x040224ff, 0x00000060, 0xdbfc0000,
    0x02000015, 0x162c2890, 0xd5430016, 0x045a24ff, 0x00000030, 0xbf8cc07f, 0x7d8a2887, 0xbe94246a,
    0xe0781000, 0x02020216, 0x7da82881, 0xb06a0060, 0x162602f9, 0x0086066a, 0xd8d80008, 0x06000013,
    0xd8dc1008, 0x07000013, 0xbf8cc07f, 0x7e300306, 0x7e320306, 0x7e340306, 0x7e360306, 0x34142484,
    0xe0781000, 0x0403180a, 0x4a162481, 0x16161690, 0x4c1616ff, 0x00008000, 0xe0741000, 0x0202070b,
    0xbf810000,
};

alignas(256) constexpr std::array<std::uint32_t, 57> HullSaveexecLoop{
    0xd7650000, 0x000100c1, 0xd7660000, 0x000200c1, 0xbf8a0000, 0x7da206f9, 0x81060000, 0xd5480014,
    0x02151101, 0x362402ff, 0x000000ff, 0x160028a0, 0xd5430015, 0x040224ff, 0x00000060, 0xdbfc0000,
    0x02000015, 0x162c2890, 0xd5430016, 0x045a24ff, 0x00000030, 0xbf8cc07f, 0xbe980382, 0xbe9a0480,
    0x7d882881, 0xbe96246a, 0x889a7e1a, 0xbefe0416, 0x80988118, 0xbf078018, 0xbf85fff9, 0xe0781000,
    0x02020216, 0x7d882881, 0xbe94246a, 0xb06a0060, 0x162602f9, 0x0086066a, 0xd8d80008, 0x06000013,
    0xd8dc1008, 0x07000013, 0xbf8cc07f, 0x7e300306, 0x7e320306, 0x7e340306, 0x7e360306, 0x34142484,
    0xe0781000, 0x0403180a, 0x4a162481, 0x16161690, 0x4c1616ff, 0x00008000, 0xe0741000, 0x0202070b,
    0xbf810000,
};

alignas(256) constexpr std::array<std::uint32_t, 50> HullLaneCount{
    0xd7650000, 0x000100c1, 0xd7660000, 0x000200c1, 0xbf8a0000, 0x7da206f9, 0x81060000, 0xd5480014,
    0x02151101, 0x362402ff, 0x000000ff, 0x160028a0, 0xd5430015, 0x040224ff, 0x00000060, 0xdbfc0000,
    0x02000015, 0x162c2890, 0xd5430016, 0x045a24ff, 0x00000030, 0xbf8cc07f, 0xe0781000, 0x02020216,
    0x7da82881, 0xbe98107e, 0xbe94047e, 0xb06a0060, 0x162602f9, 0x0086066a, 0xd8d80008, 0x06000013,
    0xd8dc1008, 0x07000013, 0xbf8cc07f, 0x7e300218, 0x7e320306, 0x7e340306, 0x7e360306, 0x34142484,
    0xbefe0414, 0xe0781000, 0x0403180a, 0x4a162481, 0x16161690, 0x4c1616ff, 0x00008000, 0xe0741000,
    0x0202070b, 0xbf810000,
};

alignas(256) constexpr std::array<std::uint32_t, 54> HullBdaScale{
    0xd7650000, 0x000100c1, 0xd7660000, 0x000200c1, 0xbf8a0000, 0x7da206f9, 0x81060000, 0xd5480014,
    0x02151101, 0x362402ff, 0x000000ff, 0x160028a0, 0xd5430015, 0x040224ff, 0x00000060, 0xdbfc0000,
    0x02000015, 0x162c2890, 0xd5430016, 0x045a24ff, 0x00000030, 0xbf8cc07f, 0xe0781000, 0x02020216,
    0x7da82881, 0xbe94047e, 0xb06a0060, 0x162602f9, 0x0086066a, 0xd8d80008, 0x06000013, 0xd8dc1008,
    0x07000013, 0xbf8cc07f, 0x7e180280, 0xdc308000, 0x0d06000c, 0xbf8c3f70, 0x100c1b06, 0x7e300306,
    0x7e320306, 0x7e340306, 0x7e360306, 0x34142484, 0xbefe0414, 0xe0781000, 0x0403180a, 0x4a162481,
    0x16161690, 0x4c1616ff, 0x00008000, 0xe0741000, 0x0202070b, 0xbf810000,
};

alignas(256) constexpr std::array<std::uint32_t, 53> HullBdaScaleWithoutBarrier{
    0xd7650000, 0x000100c1, 0xd7660000, 0x000200c1, 0x7da206f9, 0x81060000, 0xd5480014, 0x02151101,
    0x362402ff, 0x000000ff, 0x160028a0, 0xd5430015, 0x040224ff, 0x00000060, 0xdbfc0000, 0x02000015,
    0x162c2890, 0xd5430016, 0x045a24ff, 0x00000030, 0xbf8cc07f, 0xe0781000, 0x02020216, 0x7da82881,
    0xbe94047e, 0xb06a0060, 0x162602f9, 0x0086066a, 0xd8d80008, 0x06000013, 0xd8dc1008, 0x07000013,
    0xbf8cc07f, 0x7e180280, 0xdc308000, 0x0d06000c, 0xbf8c3f70, 0x100c1b06, 0x7e300306, 0x7e320306,
    0x7e340306, 0x7e360306, 0x34142484, 0xbefe0414, 0xe0781000, 0x0403180a, 0x4a162481, 0x16161690,
    0x4c1616ff, 0x00008000, 0xe0741000, 0x0202070b, 0xbf810000,
};

alignas(256) constexpr std::array<std::uint32_t, 39> DomainPatchRead{
    0xf8000941, 0x00000000, 0x34020e84, 0x16020283, 0xe0341000, 0x04030a01, 0xe0341010, 0x04030c01,
    0xe0341020, 0x04030e01, 0x4a060e81, 0x160606ff, 0x000000d0, 0x4c0606ff, 0x00008000, 0xe0301064,
    0x04031503, 0xbf8c3f70, 0x08040af2, 0x08040d02, 0x10201505, 0x3e201906, 0x3e201d02, 0x10221705,
    0x3e221b06, 0x3e221f02, 0xd5510012, 0x040a0d05, 0x102424ff, 0x40400000, 0x102a2aff, 0x3e000000,
    0x7e260280, 0x7e2802f2, 0xf80008cf, 0x14131110, 0xf800020f, 0x14151214, 0xbf810000,
};

alignas(256) constexpr std::array<std::uint32_t, 42> DomainNeighbourRecord{
    0xf8000941, 0x00000000, 0x34020e84, 0x16020283, 0xe0341000, 0x04030a01, 0xe0341010, 0x04030c01,
    0xe0341020, 0x04030e01, 0x4a060e81, 0x160606ff, 0x000000d0, 0x4c0606ff, 0x00008000, 0xe0301064,
    0x04031503, 0xe0301134, 0x04031603, 0xbf8c3f70, 0x062a2b16, 0x08040af2, 0x08040d02, 0x10201505,
    0x3e201906, 0x3e201d02, 0x10221705, 0x3e221b06, 0x3e221f02, 0xd5510012, 0x040a0d05, 0x102424ff,
    0x40400000, 0x102a2aff, 0x3e000000, 0x7e260280, 0x7e2802f2, 0xf80008cf, 0x14131110, 0xf800020f,
    0x14151214, 0xbf810000,
};

alignas(256) constexpr std::array<std::uint32_t, 42> DomainFixedPatchRead{
    0xf8000941, 0x00000000, 0x34020e84, 0x16020283, 0xe0341000, 0x04030a01, 0xe0341010, 0x04030c01,
    0xe0341020, 0x04030e01, 0x4a060e81, 0x160606ff, 0x000000d0, 0x4c0606ff, 0x00008000, 0xe0301064,
    0x04031503, 0xe0300064, 0x04031600, 0xbf8c3f70, 0x062a2b16, 0x08040af2, 0x08040d02, 0x10201505,
    0x3e201906, 0x3e201d02, 0x10221705, 0x3e221b06, 0x3e221f02, 0xd5510012, 0x040a0d05, 0x102424ff,
    0x40400000, 0x102a2aff, 0x3e000000, 0x7e260280, 0x7e2802f2, 0xf80008cf, 0x14131110, 0xf800020f,
    0x14151214, 0xbf810000,
};

alignas(256) constexpr std::array<std::uint32_t, 42> DomainNeighbourPoint{
    0xf8000941, 0x00000000, 0x34020e84, 0x16020283, 0xe0341000, 0x04030a01, 0xe0341010, 0x04030c01,
    0xe0341020, 0x04030e01, 0x4a060e81, 0x160606ff, 0x000000d0, 0x4c0606ff, 0x00008000, 0xe0301064,
    0x04031503, 0xe0301030, 0x04031601, 0xbf8c3f70, 0x062a2b16, 0x08040af2, 0x08040d02, 0x10201505,
    0x3e201906, 0x3e201d02, 0x10221705, 0x3e221b06, 0x3e221f02, 0xd5510012, 0x040a0d05, 0x102424ff,
    0x40400000, 0x102a2aff, 0x3e000000, 0x7e260280, 0x7e2802f2, 0xf80008cf, 0x14131110, 0xf800020f,
    0x14151214, 0xbf810000,
};

alignas(256) constexpr std::array<std::uint32_t, 32> DomainCulledPrimitive{
    0x380000ff, 0x80000000, 0xf8000941, 0x00000000, 0x34020e84, 0x16020283, 0xe0341000, 0x04030a01,
    0xe0341010, 0x04030c01, 0xe0341020, 0x04030e01, 0xbf8c3f70, 0x08040af2, 0x08040d02, 0x10201505,
    0x3e201906, 0x3e201d02, 0x10221705, 0x3e221b06, 0x3e221f02, 0xd5510012, 0x040a0d05, 0x102424ff,
    0x40400000, 0x7e260280, 0x7e2802f2, 0xf80008cf, 0x14131110, 0xf800020f, 0x14131214, 0xbf810000,
};

alignas(256) constexpr std::array<std::uint32_t, 41> QuadHullCode{
    0xd7650000, 0x000100c1, 0xd7660000, 0x000200c1, 0xbf8a0000, 0x7da206f9, 0x81060000, 0xd5480014,
    0x02151101, 0x362402ff, 0x000000ff, 0x160028a0, 0xd5430015, 0x040224ff, 0x00000080, 0xdbfc0000,
    0x02000015, 0x162c2890, 0xd5430016, 0x045a24ff, 0x00000040, 0xbf8cc07f, 0xe0781000, 0x02020216,
    0x7da82881, 0xb06a0080, 0x162602f9, 0x0086066a, 0xd8d80008, 0x06000013, 0xbf8cc07f, 0x7e300306,
    0x7e320306, 0x7e340306, 0x7e360306, 0x16142498, 0xe0781000, 0x0403180a, 0xe0741010, 0x0403180a,
    0xbf810000,
};

alignas(256) constexpr std::array<std::uint32_t, 38> QuadDomainCode{
    0xf8000941, 0x00000000, 0x34020e86, 0xe0341000, 0x04030a01, 0xe0341010, 0x04030c01, 0xe0341020,
    0x04030e01, 0xe0341030, 0x04031001, 0xbf8c3f70, 0x08040af2, 0x08060cf2, 0x10241502, 0x3e241905,
    0x10261702, 0x3e261b05, 0x10281d05, 0x3e282102, 0x102a1f05, 0x3e2a2302, 0x102c2503, 0x3e2c2906,
    0x102e2703, 0x3e2e2b06, 0xd5510018, 0x040a0d05, 0xd5510018, 0x04620718, 0x103030f4, 0x7e320280,
    0x7e3402f2, 0xf80008cf, 0x1a191716, 0xf800020f, 0x1a19181a, 0xbf810000,
};

alignas(256) constexpr std::array<std::uint32_t, 87> MergedHullCode{
    0xbefe04c1, 0xd7650000, 0x000100c1, 0xd7660000, 0x000200c1, 0x7da206f9, 0x80060000, 0xbf88001c,
    0xd5480004, 0x02090502, 0xd5480005, 0x02090102, 0xd5480006, 0x02110902, 0xd5480007, 0x02111102,
    0x7e080d04, 0x7e0a0d05, 0x7e0c0d06, 0x7e0e0d07, 0x10100cff, 0x3f19999a, 0x061010ff, 0xbf666666,
    0x3e1008ff, 0x3e800000, 0xd54b0009, 0x03c5e105, 0x161406a0, 0xd9340000, 0x0000080a, 0xd8340008,
    0x0000070a, 0xd8340010, 0x0000070a, 0xbf8cc07f, 0xbefe04c1, 0xbf8cc07f, 0xd7650000, 0x000100c1,
    0xd7660000, 0x000200c1, 0xbf8a0000, 0x7da206f9, 0x81060000, 0xd5480014, 0x02151101, 0x362402ff,
    0x000000ff, 0x160028a0, 0xd5430015, 0x040224ff, 0x00000060, 0xdbfc0000, 0x02000015, 0x162c2890,
    0xd5430016, 0x045a24ff, 0x00000030, 0xbf8cc07f, 0xe0781000, 0x02020216, 0x7da82881, 0xbe94047e,
    0xb06a0060, 0x162602f9, 0x0086066a, 0xd8d80048, 0x06000013, 0xd8dc1008, 0x07000013, 0xbf8cc07f,
    0x7e300306, 0x7e320306, 0x7e340306, 0x7e360306, 0x34142484, 0xbefe0414, 0xe0781000, 0x0403180a,
    0x4a162481, 0x16161690, 0x4c1616ff, 0x00008000, 0xe0741000, 0x0202070b, 0xbf810000,
};

alignas(256) constexpr std::array<std::uint32_t, 87> MergedHullRegisters{
    0xbefe04c1, 0xd7650000, 0x000100c1, 0xd7660000, 0x000200c1, 0x7da206f9, 0x80060000, 0xbf88001c,
    0xd5480004, 0x02090502, 0xd5480005, 0x02090102, 0xd5480006, 0x02110902, 0xd5480007, 0x02111102,
    0x7e080d04, 0x7e0a0d05, 0x7e0c0d06, 0x7e0e0d07, 0x10100cff, 0x3f19999a, 0x061010ff, 0xbf666666,
    0x3e1008ff, 0x3e800000, 0xd54b0009, 0x03c5e105, 0x161406a0, 0xd9340000, 0x0000080a, 0xd8340008,
    0x0000070a, 0xd8340010, 0x0000070a, 0xbf8cc07f, 0xbefe04c1, 0xbf8cc07f, 0xd7650000, 0x000100c1,
    0xd7660000, 0x000200c1, 0xbf8a0000, 0x7da206f9, 0x81060000, 0xd5480014, 0x02151101, 0x362402ff,
    0x000000ff, 0x160028a0, 0xd5430015, 0x040224ff, 0x00000060, 0xdbfc0000, 0x02000015, 0x162c2890,
    0xd5430016, 0x045a24ff, 0x00000030, 0xbf8cc07f, 0xe0741000, 0x02020816, 0x7da82881, 0xbe94047e,
    0xb06a0060, 0x162602f9, 0x0086066a, 0xd8d80048, 0x06000013, 0xd8dc1008, 0x07000013, 0xbf8cc07f,
    0x7e300306, 0x7e320306, 0x7e340306, 0x7e360306, 0x34142484, 0xbefe0414, 0xe0781000, 0x0403180a,
    0x4a162481, 0x16161690, 0x4c1616ff, 0x00008000, 0xe0741000, 0x0202070b, 0xbf810000,
};

alignas(256) constexpr std::array<std::uint32_t, 87> MergedHullForeignRecord{
    0xbefe04c1, 0xd7650000, 0x000100c1, 0xd7660000, 0x000200c1, 0x7da206f9, 0x80060000, 0xbf88001c,
    0xd5480004, 0x02090502, 0xd5480005, 0x02090102, 0xd5480006, 0x02110902, 0xd5480007, 0x02111102,
    0x7e080d04, 0x7e0a0d05, 0x7e0c0d06, 0x7e0e0d07, 0x10100cff, 0x3f19999a, 0x061010ff, 0xbf666666,
    0x3e1008ff, 0x3e800000, 0xd54b0009, 0x03c5e105, 0x161406a0, 0xd9340020, 0x0000080a, 0xd8340008,
    0x0000070a, 0xd8340010, 0x0000070a, 0xbf8cc07f, 0xbefe04c1, 0xbf8cc07f, 0xd7650000, 0x000100c1,
    0xd7660000, 0x000200c1, 0xbf8a0000, 0x7da206f9, 0x81060000, 0xd5480014, 0x02151101, 0x362402ff,
    0x000000ff, 0x160028a0, 0xd5430015, 0x040224ff, 0x00000060, 0xdbfc0000, 0x02000015, 0x162c2890,
    0xd5430016, 0x045a24ff, 0x00000030, 0xbf8cc07f, 0xe0781000, 0x02020216, 0x7da82881, 0xbe94047e,
    0xb06a0060, 0x162602f9, 0x0086066a, 0xd8d80048, 0x06000013, 0xd8dc1008, 0x07000013, 0xbf8cc07f,
    0x7e300306, 0x7e320306, 0x7e340306, 0x7e360306, 0x34142484, 0xbefe0414, 0xe0781000, 0x0403180a,
    0x4a162481, 0x16161690, 0x4c1616ff, 0x00008000, 0xe0741000, 0x0202070b, 0xbf810000,
};

alignas(256) constexpr std::array<std::uint32_t, 88> MergedHullDivergentBarrier{
    0xbefe04c1, 0xd7650000, 0x000100c1, 0xd7660000, 0x000200c1, 0x7da206f9, 0x80060000, 0xbf88001d,
    0xd5480004, 0x02090502, 0xd5480005, 0x02090102, 0xd5480006, 0x02110902, 0xd5480007, 0x02111102,
    0x7e080d04, 0x7e0a0d05, 0x7e0c0d06, 0x7e0e0d07, 0x10100cff, 0x3f19999a, 0x061010ff, 0xbf666666,
    0x3e1008ff, 0x3e800000, 0xd54b0009, 0x03c5e105, 0x161406a0, 0xd9340000, 0x0000080a, 0xd8340008,
    0x0000070a, 0xd8340010, 0x0000070a, 0xbf8a0000, 0xbf8cc07f, 0xbefe04c1, 0xbf8cc07f, 0xd7650000,
    0x000100c1, 0xd7660000, 0x000200c1, 0xbf8a0000, 0x7da206f9, 0x81060000, 0xd5480014, 0x02151101,
    0x362402ff, 0x000000ff, 0x160028a0, 0xd5430015, 0x040224ff, 0x00000060, 0xdbfc0000, 0x02000015,
    0x162c2890, 0xd5430016, 0x045a24ff, 0x00000030, 0xbf8cc07f, 0xe0781000, 0x02020216, 0x7da82881,
    0xbe94047e, 0xb06a0060, 0x162602f9, 0x0086066a, 0xd8d80048, 0x06000013, 0xd8dc1008, 0x07000013,
    0xbf8cc07f, 0x7e300306, 0x7e320306, 0x7e340306, 0x7e360306, 0x34142484, 0xbefe0414, 0xe0781000,
    0x0403180a, 0x4a162481, 0x16161690, 0x4c1616ff, 0x00008000, 0xe0741000, 0x0202070b, 0xbf810000,
};

alignas(256) constexpr std::array<std::uint32_t, 87> MergedHullOutsidePatch{
    0xbefe04c1, 0xd7650000, 0x000100c1, 0xd7660000, 0x000200c1, 0x7da206f9, 0x80060000, 0xbf88001c,
    0xd5480004, 0x02090502, 0xd5480005, 0x02090102, 0xd5480006, 0x02110902, 0xd5480007, 0x02111102,
    0x7e080d04, 0x7e0a0d05, 0x7e0c0d06, 0x7e0e0d07, 0x10100cff, 0x3f19999a, 0x061010ff, 0xbf666666,
    0x3e1008ff, 0x3e800000, 0xd54b0009, 0x03c5e105, 0x161406a0, 0xd9340000, 0x0000080a, 0xd8340008,
    0x0000070a, 0xd8340010, 0x0000070a, 0xbf8cc07f, 0xbefe04c1, 0xbf8cc07f, 0xd7650000, 0x000100c1,
    0xd7660000, 0x000200c1, 0xbf8a0000, 0x7da206f9, 0x81060000, 0xd5480014, 0x02151101, 0x362402ff,
    0x000000ff, 0x160028a0, 0xd5430015, 0x040224ff, 0x00000060, 0xdbfc0000, 0x02000015, 0x162c2890,
    0xd5430016, 0x045a24ff, 0x00000030, 0xbf8cc07f, 0xe0781000, 0x02020216, 0x7da82881, 0xbe94047e,
    0xb06a0060, 0x162602f9, 0x0086066a, 0xd8d80060, 0x06000013, 0xd8dc1008, 0x07000013, 0xbf8cc07f,
    0x7e300306, 0x7e320306, 0x7e340306, 0x7e360306, 0x34142484, 0xbefe0414, 0xe0781000, 0x0403180a,
    0x4a162481, 0x16161690, 0x4c1616ff, 0x00008000, 0xe0741000, 0x0202070b, 0xbf810000,
};

alignas(256) constexpr std::array<std::uint32_t, 87> MergedHullReadLane{
    0xbefe04c1, 0xd7650000, 0x000100c1, 0xd7660000, 0x000200c1, 0x7da206f9, 0x80060000, 0xbf88001c,
    0xd5480004, 0x02090502, 0xd5480005, 0x02090102, 0xd5480006, 0x02110902, 0xd5480007, 0x02111102,
    0x7e080d04, 0x7e0a0d05, 0x7e0c0d06, 0x7e0e0d07, 0x10100cff, 0x3f19999a, 0x061010ff, 0xbf666666,
    0x3e1008ff, 0x3e800000, 0xd54b0009, 0x03c5e105, 0x161406a0, 0xd9340000, 0x0000080a, 0xd8340008,
    0x0000070a, 0xd8340010, 0x0000070a, 0xbf8cc07f, 0xbefe04c1, 0xbf8cc07f, 0xd7650000, 0x000100c1,
    0xd7660000, 0x000200c1, 0xbf8a0000, 0x7da206f9, 0x81060000, 0xd5480014, 0x02151101, 0x362402ff,
    0x000000ff, 0x160028a0, 0xd5430015, 0x040224ff, 0x00000060, 0xdbfc0000, 0x02000015, 0x162c2890,
    0xd5430016, 0x045a24ff, 0x00000030, 0xbf8cc07f, 0xe0781000, 0x02020216, 0x7da82881, 0xbe94047e,
    0xb06a0060, 0x162602f9, 0x0086066a, 0xd8d80048, 0x06000013, 0xd8dc1008, 0x07000013, 0xbf8cc07f,
    0xd7600010, 0x02010106, 0x7e300210, 0x7e360306, 0x34142484, 0xbefe0414, 0xe0781000, 0x0403180a,
    0x4a162481, 0x16161690, 0x4c1616ff, 0x00008000, 0xe0741000, 0x0202070b, 0xbf810000,
};

alignas(256) constexpr std::array<std::uint32_t, 7> PixelCode{
    0xc8020002, 0xc8060102, 0xc80a0202, 0xc80e0302, 0xf800180f, 0x03020100, 0xbf810000,
};

using ShaderRecompiler::TessellationConfiguration;

constexpr TessellationConfiguration Patches{3u, 3u, 1u, 2u, 2u};
constexpr TessellationConfiguration MergedPatches{3u, 3u, 1u, 2u, 2u, 256u, 0u, false, 0u, false, true};
constexpr std::array<std::uint32_t, 3> PatchLevels{1u, 0u, 5u};

constexpr std::uint16_t Corner(std::uint32_t patch, std::uint32_t x, std::uint32_t y) {
    return static_cast<std::uint16_t>((PatchLevels[patch] << 8u) | (patch << 4u) | (x << 2u) | y);
}

alignas(256) constexpr std::array<std::uint16_t, 9> Indices{
    Corner(0, 0, 0), Corner(0, 2, 0), Corner(0, 1, 2),
    Corner(1, 0, 0), Corner(1, 2, 0), Corner(1, 1, 2),
    Corner(2, 0, 0), Corner(2, 2, 0), Corner(2, 1, 2),
};

alignas(256) constexpr std::array<std::uint16_t, 24> LatePatchIndices{
    Corner(1, 0, 0), Corner(1, 2, 0), Corner(1, 1, 2),
    Corner(1, 0, 0), Corner(1, 2, 0), Corner(1, 1, 2),
    Corner(1, 0, 0), Corner(1, 2, 0), Corner(1, 1, 2),
    Corner(1, 0, 0), Corner(1, 2, 0), Corner(1, 1, 2),
    Corner(1, 0, 0), Corner(1, 2, 0), Corner(1, 1, 2),
    Corner(1, 0, 0), Corner(1, 2, 0), Corner(1, 1, 2),
    Corner(2, 0, 0), Corner(2, 2, 0), Corner(2, 1, 2),
    Corner(0, 0, 0), Corner(0, 2, 0), Corner(0, 1, 2),
};

alignas(256) constexpr std::array<std::uint16_t, 12> QuadIndices{
    Corner(0, 0, 0), Corner(0, 2, 0), Corner(0, 2, 2), Corner(0, 0, 2),
    Corner(1, 0, 0), Corner(1, 2, 0), Corner(1, 2, 2), Corner(1, 0, 2),
    Corner(2, 0, 0), Corner(2, 2, 0), Corner(2, 2, 2), Corner(2, 0, 2),
};

struct PatchDraw {
    TessellationConfiguration configuration;
    std::span<const std::uint16_t> indices;
    float minLevel = 1.0f;
    float maxLevel = 64.0f;
    std::uint32_t instances = 1;
    std::uint32_t firstInstance = 0;
};

alignas(256) constexpr std::array<std::uint16_t, 12> NextInputIndices{
    Corner(0, 0, 0), Corner(0, 0, 0), Corner(0, 2, 0), Corner(0, 1, 2),
    Corner(1, 0, 0), Corner(1, 0, 0), Corner(1, 2, 0), Corner(1, 1, 2),
    Corner(2, 0, 0), Corner(2, 0, 0), Corner(2, 2, 0), Corner(2, 1, 2),
};

const PatchDraw Triangles{Patches, Indices};
const PatchDraw NextInputTriangles{{4u, 3u, 1u, 2u, 2u}, NextInputIndices};
const PatchDraw MergedTriangles{MergedPatches, Indices};

struct Stages {
    ShaderRecompiler::RecompileResult local;
    ShaderRecompiler::RecompileResult hull;
    ShaderRecompiler::RecompileResult domain;
    ShaderRecompiler::RecompileResult fragment;
};

ShaderRecompiler::RecompileResult Compile(const ShaderRecompiler::SpirvTarget& target, ShaderStage stage, std::span<const std::uint32_t> code, std::uint32_t& pushCursor, const PatchDraw& patches = Triangles) {
    static const std::array<std::uint32_t, 8> domainUserData{};
    const std::array<ShaderRecompiler::MemoryRegion, 1> memory{{{reinterpret_cast<std::uintptr_t>(code.data()), std::as_bytes(code)}}};
    const bool hull = stage == ShaderStage::TessellationControl;
    const auto userData = hull ? std::span<const std::uint32_t>(HullUserData) : stage == ShaderStage::TessellationEvaluation ? std::span<const std::uint32_t>(domainUserData) : std::span<const std::uint32_t>();
    std::optional<ShaderRecompiler::ShaderPixelStageInfo> pixel;
    std::optional<ShaderRecompiler::ShaderVertexStageInfo> vertex;
    std::optional<ShaderRecompiler::GraphicsCompileContext> graphics;
    if (stage == ShaderStage::Fragment) {
        ShaderRecompiler::ShaderPixelStageInfo info{};
        info.interpolatorCount = 1;
        info.interpolatorSettings[0] = 0x400u;
        info.targetOutputMode[0] = 9;
        info.targetExportMapping.fill(0xe4u);
        pixel = info;
    } else {
        vertex = ShaderRecompiler::ShaderVertexStageInfo{};
        graphics = ShaderRecompiler::GraphicsCompileContext{hull ? 0u : 8u, {}, std::nullopt, patches.configuration, {reinterpret_cast<std::uintptr_t>(patches.indices.data()), static_cast<std::uint32_t>(patches.indices.size()), 2, patches.instances}};
    }
    auto layout = stage == ShaderStage::Fragment ? PixelPushLayout(pushCursor, target) : ShaderRecompiler::BindingLayout{0, 0, pushCursor, AgcDriver::Graphics::PipelinePushSlotBytes - pushCursor};
    if (layout.pushConstantOffsetBytes < AgcDriver::Graphics::PipelinePushSlotBytes) layout.pushConstantSizeBytes = ShaderRecompiler::TessellationLevelPushOffsetBytes - layout.pushConstantOffsetBytes;
    const ShaderRecompiler::RecompileRequest request{
        {stage, reinterpret_cast<std::uintptr_t>(code.data()), code, 0, {}},
        {64, hull ? 0u : 8u, userData, std::nullopt, pixel, vertex, memory},
        target,
        layout,
        graphics
    };
    auto result = ShaderRecompiler::Recompile(request);
    pushCursor += static_cast<std::uint32_t>(result.pushConstants.size());
    return result;
}

void ExpectRefusal(const ShaderRecompiler::SpirvTarget& target, ShaderStage stage, std::span<const std::uint32_t> code, const std::string& reason, const PatchDraw& patches = Triangles) {
    std::uint32_t cursor = 0;
    try {
        static_cast<void>(Compile(target, stage, code, cursor, patches));
    } catch (const std::exception& error) {
        Require(std::string(error.what()).find(reason) != std::string::npos, "the refusal named another reason: " + std::string(error.what()));
        return;
    }
    Require(false, "a tessellation program was accepted although " + reason);
}

void RequireCompareBranches(const ShaderRecompiler::SpirvTarget& target) {
    std::uint32_t cursor = 0;
    Require(!Compile(target, ShaderStage::TessellationEvaluation, DomainCompareBranch, cursor).spirv.empty(), "a domain program branching on a compare copied to VCC was not compiled");
    Require(!Compile(target, ShaderStage::TessellationControl, HullCompareBranch, cursor).spirv.empty(), "a hull program branching on a compare copied to VCC was not compiled");
}

std::array<std::uint8_t, 4> PixelAt(std::uint32_t x, std::uint32_t y) {
    const auto offset = (static_cast<std::size_t>(y) * Width + x) * 4u;
    return {std::to_integer<std::uint8_t>(Pixels[offset]), std::to_integer<std::uint8_t>(Pixels[offset + 1]), std::to_integer<std::uint8_t>(Pixels[offset + 2]), std::to_integer<std::uint8_t>(Pixels[offset + 3])};
}

std::string PixelText(const std::array<std::uint8_t, 4>& pixel) {
    char text[64];
    std::snprintf(text, sizeof(text), "(%u, %u, %u, %u)", pixel[0], pixel[1], pixel[2], pixel[3]);
    return text;
}

std::uint32_t Column(std::uint32_t patch, float offset) {
    return static_cast<std::uint32_t>((-0.9f + 0.6f * static_cast<float>(patch) + offset + 1.0f) * 0.5f * Width);
}

std::uint32_t Row(float y) {
    return static_cast<std::uint32_t>((1.0f - y) * 0.5f * Height);
}

void Draw(AgcDriver::VulkanDevice& device, const Stages& stages, VkCullModeFlags cull, VkFrontFace front, const PatchDraw& patches = Triangles) {
    for (std::size_t i = 0; i < Pixels.size(); i += 4) {
        for (std::size_t c = 0; c < 4; ++c) Pixels[i + c] = std::byte{Background[c]};
    }
    std::uint32_t cursor = 0;
    const auto offset = [&](const ShaderRecompiler::RecompileResult& result) {
        const auto at = result.pushConstants.empty() ? 0u : cursor;
        cursor += static_cast<std::uint32_t>(result.pushConstants.size());
        return at;
    };
    const std::array<AgcDriver::Graphics::CompiledShader, 4> shaders{{
        {ShaderStage::Local, &stages.local, offset(stages.local)},
        {ShaderStage::TessellationControl, &stages.hull, offset(stages.hull)},
        {ShaderStage::TessellationEvaluation, &stages.domain, offset(stages.domain)},
        {ShaderStage::Fragment, &stages.fragment, PixelPushOffset(cursor, device.Target())},
    }};
    AgcDriver::Graphics::State state{};
    state.stages = {AgcDriver::Graphics::ShaderPath::Tessellation, 0x0200210du, 64, 64, std::nullopt, patches.configuration, patches.minLevel, patches.maxLevel};
    state.color = {reinterpret_cast<std::uintptr_t>(Pixels.data()), {Width, Height}, VK_FORMAT_R8G8B8A8_UNORM, Pixels.size(), 0xe4u};
    state.colors = {state.color};
    state.hasColorTarget = true;
    state.renderExtent = {Width, Height};
    state.topology = VK_PRIMITIVE_TOPOLOGY_PATCH_LIST;
    state.viewport = {0, static_cast<float>(Height), static_cast<float>(Width), -static_cast<float>(Height), 0, 1};
    state.negativeOneToOne = false;
    state.scissor = {{0, 0}, {Width, Height}};
    state.cullMode = cull;
    state.frontFace = front;
    state.blend.colorWriteMask = 15;
    state.blends = {state.blend};
    state.blendConstants = {};
    device.Draw(state, {reinterpret_cast<std::uintptr_t>(patches.indices.data()), static_cast<std::uint32_t>(patches.indices.size()), 2, patches.instances, 0, true, 0, patches.firstInstance}, shaders);
    device.WaitIdle();
}

std::uint32_t CoveredPixels(std::uint32_t patch, bool interior) {
    std::uint32_t count = 0;
    for (std::uint32_t y = Row(0.5f); y <= Row(-0.5f) && y < Height; ++y) {
        for (std::uint32_t x = Column(patch, 0.0f); x <= Column(patch, 0.5f) && x < Width; ++x) {
            const auto pixel = PixelAt(x, y);
            if (pixel == Background) continue;
            Require(pixel[0] == 255 && pixel[2] == 0 && pixel[3] == 255, "patch " + std::to_string(patch) + " pixel (" + std::to_string(x) + ", " + std::to_string(y) + ") is " + PixelText(pixel));
            if (!interior || pixel[1] > 32) ++count;
        }
    }
    return count;
}

void RequirePatches(AgcDriver::VulkanDevice& device, const ShaderRecompiler::SpirvTarget& target, std::span<const std::uint32_t> hull, const std::string& program, const PatchDraw& patches = Triangles, std::span<const std::uint32_t> local = LocalCode, std::span<const std::uint32_t> domain = DomainCode) {
    std::uint32_t cursor = 0;
    const Stages stages{
        Compile(target, ShaderStage::Local, local, cursor, patches),
        Compile(target, ShaderStage::TessellationControl, hull, cursor, patches),
        Compile(target, ShaderStage::TessellationEvaluation, domain, cursor, patches),
        Compile(target, ShaderStage::Fragment, PixelCode, cursor, patches),
    };
    const auto center = Row(-0.5f + 1.0f / 3.0f);
    for (const auto cull : {VK_CULL_MODE_NONE, VK_CULL_MODE_BACK_BIT}) {
        Draw(device, stages, cull, VK_FRONT_FACE_COUNTER_CLOCKWISE, patches);
        const std::string name = program + (cull == VK_CULL_MODE_NONE ? ", unculled" : ", back-face culled");
        Require(CoveredPixels(0, false) > 100 && CoveredPixels(0, true) == 0, name + ": the level-1 patch was not drawn as one flat triangle, centre " + PixelText(PixelAt(Column(0, 0.25f), center)));
        Require(CoveredPixels(1, false) == 0, name + ": the patch with level 0 was not culled");
        Require(CoveredPixels(2, false) > 100 && PixelAt(Column(2, 0.25f), center)[1] > 32, name + ": the level-5 patch has no interior domain points at its centre, " + PixelText(PixelAt(Column(2, 0.25f), center)));
        Require(PixelAt(0, 0) == Background && PixelAt(Width - 1, Height - 1) == Background, name + ": the corners changed");
    }
    Draw(device, stages, VK_CULL_MODE_BACK_BIT, VK_FRONT_FACE_CLOCKWISE, patches);
    Require(CoveredPixels(0, false) == 0 && CoveredPixels(2, false) == 0, program + ": tessellated triangles did not keep the control points' winding");
}

Stages CompileTriangles(const ShaderRecompiler::SpirvTarget& target, const PatchDraw& patches) {
    std::uint32_t cursor = 0;
    return {
        Compile(target, ShaderStage::Local, LocalCode, cursor, patches),
        Compile(target, ShaderStage::TessellationControl, HullCode, cursor, patches),
        Compile(target, ShaderStage::TessellationEvaluation, DomainCode, cursor, patches),
        Compile(target, ShaderStage::Fragment, PixelCode, cursor, patches),
    };
}

void RequireCounterClockwise(AgcDriver::VulkanDevice& device, const ShaderRecompiler::SpirvTarget& target) {
    const PatchDraw patches{{3u, 3u, 1u, 2u, 3u}, Indices};
    const auto stages = CompileTriangles(target, patches);
    Draw(device, stages, VK_CULL_MODE_BACK_BIT, VK_FRONT_FACE_COUNTER_CLOCKWISE, patches);
    Require(CoveredPixels(0, false) == 0 && CoveredPixels(2, false) == 0, "counter-clockwise output kept the winding of clockwise output");
    Draw(device, stages, VK_CULL_MODE_BACK_BIT, VK_FRONT_FACE_CLOCKWISE, patches);
    Require(CoveredPixels(0, false) > 100 && CoveredPixels(2, false) > 100, "counter-clockwise output did not reverse the control points' winding");
}

bool HasInstruction(std::span<const std::uint32_t> words, spv::Op opcode, std::uint32_t length, std::uint32_t operand, std::uint32_t value) {
    for (std::size_t cursor = 5; cursor < words.size();) {
        const auto count = words[cursor] >> 16u;
        Require(count != 0 && cursor + count <= words.size(), "malformed SPIR-V while scanning a tessellation module");
        if ((words[cursor] & 0xffffu) == opcode && count == length && words[cursor + operand] == value) return true;
        cursor += count;
    }
    return false;
}

bool HasSpacing(const ShaderRecompiler::RecompileResult& domain, spv::ExecutionMode spacing) {
    return HasInstruction(domain.spirv.Words(), spv::OpExecutionMode, 3u, 2u, static_cast<std::uint32_t>(spacing));
}

bool HasLevelBound(const ShaderRecompiler::RecompileResult& hull, float bound) {
    return HasInstruction(hull.spirv.Words(), spv::OpConstant, 4u, 3u, std::bit_cast<std::uint32_t>(bound));
}

void RequireLevelClamps(AgcDriver::VulkanDevice& device, const ShaderRecompiler::SpirvTarget& target) {
    const auto center = Row(-0.5f + 1.0f / 3.0f);
    const auto stages = CompileTriangles(target, Triangles);
    Require(HasSpacing(stages.domain, spv::ExecutionModeSpacingFractionalOdd) && !HasSpacing(stages.domain, spv::ExecutionModeSpacingEqual), "fractional-odd partitioning did not compile to fractional-odd spacing");
    Require(HasLevelBound(stages.hull, 63.0f) && !HasLevelBound(stages.hull, 64.0f), "fractional-odd partitioning did not bound its tessellation factors at 63");
    const PatchDraw flattened{Patches, Indices, 1.0f, 1.0f};
    Draw(device, stages, VK_CULL_MODE_NONE, VK_FRONT_FACE_COUNTER_CLOCKWISE, flattened);
    Require(CoveredPixels(2, false) > 100 && CoveredPixels(2, true) == 0, "VGT_HOS_MAX_TESS_LEVEL 1 did not flatten the level-5 patch, centre " + PixelText(PixelAt(Column(2, 0.25f), center)));
    Require(CoveredPixels(1, false) == 0, "VGT_HOS_MAX_TESS_LEVEL 1 kept the patch with level 0");
    Draw(device, stages, VK_CULL_MODE_NONE, VK_FRONT_FACE_COUNTER_CLOCKWISE, Triangles);
    Require(CoveredPixels(2, false) > 100 && PixelAt(Column(2, 0.25f), center)[1] > 32, "the same hull kept the previous draw's VGT_HOS_MAX_TESS_LEVEL 1 for a draw with level 64, centre " + PixelText(PixelAt(Column(2, 0.25f), center)));
    const PatchDraw raised{Patches, Indices, 5.0f, 64.0f};
    Draw(device, stages, VK_CULL_MODE_NONE, VK_FRONT_FACE_COUNTER_CLOCKWISE, raised);
    Require(PixelAt(Column(0, 0.25f), center)[1] > 32, "VGT_HOS_MIN_TESS_LEVEL 5 did not tessellate the level-1 patch, centre " + PixelText(PixelAt(Column(0, 0.25f), center)));
    Require(CoveredPixels(1, false) == 0, "VGT_HOS_MIN_TESS_LEVEL 5 raised the patch with level 0 instead of culling it");
}

const PatchDraw RecordPatches{{3u, 3u, 1u, 2u, 2u, 63u}, Indices};

void RequirePatchConstants(AgcDriver::VulkanDevice& device, const ShaderRecompiler::SpirvTarget& target) {
    std::uint32_t cursor = 0;
    const Stages stages{
        Compile(target, ShaderStage::Local, LocalCode, cursor, RecordPatches),
        Compile(target, ShaderStage::TessellationControl, HullPatchConstant, cursor, RecordPatches),
        Compile(target, ShaderStage::TessellationEvaluation, DomainPatchRead, cursor, RecordPatches),
        Compile(target, ShaderStage::Fragment, PixelCode, cursor, RecordPatches),
    };
    Draw(device, stages, VK_CULL_MODE_NONE, VK_FRONT_FACE_COUNTER_CLOCKWISE, RecordPatches);
    const auto center = Row(-0.5f + 1.0f / 3.0f);
    for (const auto& [patch, blue] : {std::pair{0u, 32}, std::pair{2u, 159}}) {
        const auto pixel = PixelAt(Column(patch, 0.25f), center);
        Require(pixel[0] == 255 && std::abs(pixel[2] - blue) <= 1, "patch " + std::to_string(patch) + " did not take its blue channel from the patch constant its hull wrote, " + PixelText(pixel));
    }
    Require(PixelAt(Column(1, 0.25f), center) == Background, "the patch with level 0 was not culled");
}

void RequireIntegerPartitioning(AgcDriver::VulkanDevice& device, const ShaderRecompiler::SpirvTarget& target) {
    const auto center = Row(-0.5f + 1.0f / 3.0f);
    const PatchDraw patches{{3u, 3u, 1u, 0u, 2u}, Indices};
    const auto stages = CompileTriangles(target, patches);
    Require(HasSpacing(stages.domain, spv::ExecutionModeSpacingEqual) && !HasSpacing(stages.domain, spv::ExecutionModeSpacingFractionalOdd), "integer partitioning did not compile to equal spacing");
    Require(HasLevelBound(stages.hull, 64.0f) && !HasLevelBound(stages.hull, 63.0f), "integer partitioning did not bound its tessellation factors at 64");
    Draw(device, stages, VK_CULL_MODE_NONE, VK_FRONT_FACE_COUNTER_CLOCKWISE, patches);
    Require(CoveredPixels(0, false) > 100 && CoveredPixels(0, true) == 0, "integer partitioning: the level-1 patch was not drawn as one flat triangle, centre " + PixelText(PixelAt(Column(0, 0.25f), center)));
    Require(CoveredPixels(1, false) == 0, "integer partitioning: the patch with level 0 was not culled");
    Require(CoveredPixels(2, false) > 100 && PixelAt(Column(2, 0.25f), center)[1] > 32, "integer partitioning: the level-5 patch has no interior domain points at its centre, " + PixelText(PixelAt(Column(2, 0.25f), center)));
}

void RequireRelativePatches(AgcDriver::VulkanDevice& device, const ShaderRecompiler::SpirvTarget& target) {
    const auto center = Row(-0.5f + 1.0f / 3.0f);
    for (const auto& [hull, domain, name] : {std::tuple{std::span<const std::uint32_t>(HullRelativeData), std::span<const std::uint32_t>(DomainPatchRead), "hull"}, std::tuple{std::span<const std::uint32_t>(HullCode), std::span<const std::uint32_t>(DomainRelativeData), "domain"}}) {
        for (const auto& [patches, blue] : {std::pair{PatchDraw{{3u, 3u, 1u, 2u, 2u, 3u, 3u, true}, Indices}, 96}, std::pair{PatchDraw{{3u, 3u, 1u, 2u, 2u, 2u, 4u, true}, Indices}, 32}, std::pair{PatchDraw{{3u, 3u, 1u, 2u, 2u, 3u, 4u, true}, LatePatchIndices}, 96}, std::pair{PatchDraw{{3u, 3u, 1u, 2u, 2u, 3u, 2u, true}, Indices}, 32}}) {
            std::uint32_t cursor = 0;
            const Stages stages{
                Compile(target, ShaderStage::Local, LocalCode, cursor, patches),
                Compile(target, ShaderStage::TessellationControl, hull, cursor, patches),
                Compile(target, ShaderStage::TessellationEvaluation, domain, cursor, patches),
                Compile(target, ShaderStage::Fragment, PixelCode, cursor, patches),
            };
            Draw(device, stages, VK_CULL_MODE_NONE, VK_FRONT_FACE_COUNTER_CLOCKWISE, patches);
            for (const auto& [patch, expected] : {std::pair{0u, 32}, std::pair{2u, blue}}) {
                const auto pixel = PixelAt(Column(patch, 0.25f), center);
                Require(pixel[0] == 255 && std::abs(pixel[2] - expected) <= 1, "the patch in column " + std::to_string(patch) + " with " + std::to_string(patches.configuration.patchesPerGroup) + " patches per group and " + std::to_string(patches.configuration.primitiveGroupSize) + " primitives per group did not take its blue channel from the " + name + " program's relative patch index, " + PixelText(pixel));
            }
            Require(PixelAt(Column(1, 0.25f), center) == Background, "the patch with level 0 was not culled");
        }
    }
}

void RequireInstancedRelativePatches(AgcDriver::VulkanDevice& device, const ShaderRecompiler::SpirvTarget& target) {
    const auto center = Row(-0.5f + 1.0f / 3.0f);
    TessellationConfiguration restarting{3u, 3u, 1u, 2u, 2u, 4u, 8u};
    restarting.breakAtInstance = true;
    for (const auto& [patches, first, last] : {std::tuple{PatchDraw{{3u, 3u, 1u, 2u, 2u, 4u, 8u}, Indices, 1.0f, 64.0f, 2u, 1u}, 128, 64}, std::tuple{PatchDraw{{3u, 3u, 1u, 2u, 2u, 3u, 3u}, Indices, 1.0f, 64.0f, 2u, 1u}, 32, 96}, std::tuple{PatchDraw{restarting, Indices, 1.0f, 64.0f, 2u, 1u}, 32, 96}}) {
        std::uint32_t cursor = 0;
        const Stages stages{
            Compile(target, ShaderStage::Local, LocalCode, cursor, patches),
            Compile(target, ShaderStage::TessellationControl, HullRelativeData, cursor, patches),
            Compile(target, ShaderStage::TessellationEvaluation, DomainPatchRead, cursor, patches),
            Compile(target, ShaderStage::Fragment, PixelCode, cursor, patches),
        };
        Draw(device, stages, VK_CULL_MODE_NONE, VK_FRONT_FACE_COUNTER_CLOCKWISE, patches);
        for (const auto& [patch, expected] : {std::pair{0u, first}, std::pair{2u, last}}) {
            const auto pixel = PixelAt(Column(patch, 0.25f), center);
            Require(pixel[0] == 255 && std::abs(pixel[2] - expected) <= 1, "the patch in column " + std::to_string(patch) + " of an instanced draw with " + std::to_string(patches.configuration.patchesPerGroup) + " patches per group and " + std::to_string(patches.configuration.primitiveGroupSize) + " primitives per group" + (patches.configuration.breakAtInstance ? " and BREAK_WAVE_AT_EOI" : "") + " did not take its blue channel from the hull's relative patch index across instances, " + PixelText(pixel));
        }
    }
    const PatchDraw shared{{3u, 3u, 1u, 2u, 2u, 4u, 8u}, Indices, 1.0f, 64.0f, 2u, 1u};
    ExpectRefusal(target, ShaderStage::TessellationEvaluation, DomainRelativeData, "domain program uses its relative patch index as data in an instanced draw", shared);
    std::vector<std::uint32_t> capabilities(target.supportedCapabilities.begin(), target.supportedCapabilities.end());
    std::erase(capabilities, static_cast<std::uint32_t>(spv::CapabilityDrawParameters));
    auto withoutDrawParameters = target;
    withoutDrawParameters.supportedCapabilities = capabilities;
    ExpectRefusal(withoutDrawParameters, ShaderStage::TessellationControl, HullRelativeData, "shaderDrawParameters", shared);
}

class GuestBlock {
public:
    GuestBlock() {
#ifdef _WIN32
        block = static_cast<std::uint8_t*>(VirtualAlloc(nullptr, 2 * GuestBytes, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE));
#else
        block = static_cast<std::uint8_t*>(std::aligned_alloc(GuestBytes, 2 * GuestBytes));
#endif
        Require(block != nullptr, "tessellation: cannot allocate the guest block");
        std::memset(block, 0, 2 * GuestBytes);
        GuestAllocations::Mutation().Add(block, GuestBytes, true, true, true);
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
    std::uint64_t Address() const { return reinterpret_cast<std::uintptr_t>(block); }

private:
    std::uint8_t* block = nullptr;
};

void RequireHullBdaReads(AgcDriver::VulkanDevice& device, const ShaderRecompiler::SpirvTarget& target) {
    GuestBlock guest;
    const float scale = 1.0f;
    std::memcpy(guest.Data(), &scale, sizeof(scale));
    const auto address = [](std::uint64_t value) {
        HullUserData[6] = static_cast<std::uint32_t>(value);
        HullUserData[7] = static_cast<std::uint32_t>(value >> 32u);
    };
    const bool llvmpipe = device.DeviceName().starts_with("llvmpipe");
    for (const auto& [hull, program] : {std::pair{std::span<const std::uint32_t>(HullBdaScale), std::string("hull program scaling its levels by a BDA read after s_barrier")}, std::pair{std::span<const std::uint32_t>(HullBdaScaleWithoutBarrier), std::string("hull program scaling its levels by a BDA read without s_barrier")}}) {
        address(guest.Address());
        RequirePatches(device, target, hull, program);
        if (llvmpipe) {
            std::puts(("skipped the unmapped read of the " + program + ": llvmpipe runs tessellation-control loads that the check of the unmapped address skips").c_str());
            continue;
        }
        address(guest.Address() + GuestBytes);
        std::uint32_t cursor = 0;
        const Stages stages{
            Compile(target, ShaderStage::Local, LocalCode, cursor),
            Compile(target, ShaderStage::TessellationControl, hull, cursor),
            Compile(target, ShaderStage::TessellationEvaluation, DomainCode, cursor),
            Compile(target, ShaderStage::Fragment, PixelCode, cursor),
        };
        std::string failure;
        try {
            Draw(device, stages, VK_CULL_MODE_NONE, VK_FRONT_FACE_COUNTER_CLOCKWISE);
        } catch (const std::exception& error) {
            failure = error.what();
        }
        device.WaitIdle();
        Require(failure.find("BDA access failed") != std::string::npos && failure.find("stage=7 reason=1") != std::string::npos, program + ": the unmapped read was reported as \"" + failure + "\", not as a tessellation-control BDA fault");
    }
    address(0u);
}

Stages CompileLoweredInterior(const ShaderRecompiler::SpirvTarget& target, const PatchDraw& patches) {
    std::uint32_t cursor = 0;
    return {
        Compile(target, ShaderStage::Local, LocalCode, cursor, patches),
        Compile(target, ShaderStage::TessellationControl, HullCode, cursor, patches),
        Compile(target, ShaderStage::TessellationEvaluation, DomainLoweredInterior, cursor, patches),
        Compile(target, ShaderStage::Fragment, PixelCode, cursor, patches),
    };
}

void RequireFractionalEvenPartitioning(AgcDriver::VulkanDevice& device, const ShaderRecompiler::SpirvTarget& target) {
    const auto below = Row(-0.75f);
    const PatchDraw integer{{3u, 3u, 1u, 0u, 2u}, Indices};
    Draw(device, CompileLoweredInterior(target, integer), VK_CULL_MODE_NONE, VK_FRONT_FACE_COUNTER_CLOCKWISE, integer);
    Require(CoveredPixels(0, false) > 100 && PixelAt(Column(0, 0.25f), below) == Background, "integer partitioning: the level-1 patch has a domain point inside it, " + PixelText(PixelAt(Column(0, 0.25f), below)));
    Require(PixelAt(Column(2, 0.25f), below) != Background, "integer partitioning: the level-5 patch has no domain point inside it");
    const PatchDraw patches{{3u, 3u, 1u, 3u, 2u}, Indices};
    const auto stages = CompileLoweredInterior(target, patches);
    Require(HasSpacing(stages.domain, spv::ExecutionModeSpacingFractionalEven) && !HasSpacing(stages.domain, spv::ExecutionModeSpacingFractionalOdd) && !HasSpacing(stages.domain, spv::ExecutionModeSpacingEqual), "fractional-even partitioning did not compile to fractional-even spacing");
    Require(HasLevelBound(stages.hull, 64.0f) && !HasLevelBound(stages.hull, 63.0f), "fractional-even partitioning did not bound its tessellation factors at 64");
    Draw(device, stages, VK_CULL_MODE_NONE, VK_FRONT_FACE_COUNTER_CLOCKWISE, patches);
    Require(CoveredPixels(0, false) > 100 && PixelAt(Column(0, 0.25f), below) != Background, "fractional-even partitioning: the level-1 patch was not raised to level 2 with a domain point inside it");
    Require(CoveredPixels(1, false) == 0 && PixelAt(Column(1, 0.25f), below) == Background, "fractional-even partitioning: the patch with level 0 was not culled");
    Require(CoveredPixels(2, false) > 100 && PixelAt(Column(2, 0.25f), below) != Background, "fractional-even partitioning: the level-5 patch has no domain point inside it");
}

void RequireQuads(AgcDriver::VulkanDevice& device, const ShaderRecompiler::SpirvTarget& target) {
    const auto center = Row(0.0f);
    std::array<bool, 2> frontFacing{};
    for (const std::uint32_t topology : {2u, 3u}) {
        const PatchDraw patches{{4u, 4u, 2u, 2u, topology}, QuadIndices, 1.0f, 9.0f};
        std::uint32_t cursor = 0;
        const Stages stages{
            Compile(target, ShaderStage::Local, LocalCode, cursor, patches),
            Compile(target, ShaderStage::TessellationControl, QuadHullCode, cursor, patches),
            Compile(target, ShaderStage::TessellationEvaluation, QuadDomainCode, cursor, patches),
            Compile(target, ShaderStage::Fragment, PixelCode, cursor, patches),
        };
        const std::string name = topology == 2u ? "clockwise quad patches" : "counter-clockwise quad patches";
        Draw(device, stages, VK_CULL_MODE_NONE, VK_FRONT_FACE_COUNTER_CLOCKWISE, patches);
        Require(CoveredPixels(0, false) > 2000 && CoveredPixels(0, true) == 0, name + ": the level-1 patch was not drawn as one flat quad, " + std::to_string(CoveredPixels(0, false)) + " pixels");
        Require(PixelAt(Column(0, 0.02f), Row(0.45f)) != Background && PixelAt(Column(0, 0.48f), Row(0.45f)) != Background, name + ": the level-1 patch misses its upper corners");
        Require(CoveredPixels(1, false) == 0, name + ": the patch with level 0 was not culled");
        Require(CoveredPixels(2, false) > 2000 && PixelAt(Column(2, 0.25f), center)[1] > 32, name + ": the level-5 patch has no interior domain points at its centre, " + PixelText(PixelAt(Column(2, 0.25f), center)));
        Draw(device, stages, VK_CULL_MODE_BACK_BIT, VK_FRONT_FACE_COUNTER_CLOCKWISE, patches);
        frontFacing[topology - 2u] = CoveredPixels(0, false) > 2000;
        Require(frontFacing[topology - 2u] == (CoveredPixels(2, false) > 2000) && (frontFacing[topology - 2u] || CoveredPixels(0, false) == 0), name + ": back-face culling kept part of a patch");
    }
    Require(frontFacing[0] != frontFacing[1], "clockwise and counter-clockwise quad patches have the same winding");
}

struct RegisteredStage {
    Shader shader{};
    std::array<ShaderRegister, 6> registers{};
    std::array<ShaderRegister, 4> context{};
    ShaderSpecialRegs specials{};
    ShaderUserData users{};
};

template<std::size_t TCount>
void Describe(RegisteredStage& stage, const std::array<std::uint32_t, TCount>& code, std::uint8_t type, std::initializer_list<ShaderRegister> registers, std::initializer_list<ShaderRegister> context, std::uint32_t routing) {
    stage.shader.file_header = 0x34333231u;
    stage.shader.version = 0x18u;
    stage.shader.header_size = sizeof(RegisteredStage);
    stage.shader.shader_size = sizeof(code);
    stage.shader.code = code.data();
    stage.shader.type = type;
    stage.shader.user_data = &stage.users;
    std::copy(registers.begin(), registers.end(), stage.registers.begin());
    stage.shader.sh_registers = stage.registers.data();
    stage.shader.num_sh_registers = static_cast<std::uint8_t>(registers.size());
    std::copy(context.begin(), context.end(), stage.context.begin());
    stage.shader.cx_registers = stage.context.data();
    stage.shader.num_cx_registers = static_cast<std::uint8_t>(context.size());
    stage.specials.ge_cntl = {0x25bu, 0u};
    stage.specials.vgt_shader_stages_en = {0x2d5u, routing};
    stage.specials.vgt_gs_out_prim_type = {0x29bu, 0u};
    stage.specials.ge_user_vgpr_en = {0x262u, 0u};
    stage.shader.specials = &stage.specials;
}

void RequireRegisteredStages() {
    const auto low = [](const auto& code) { return static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(code.data()) >> 8u); };
    const auto high = [](const auto& code) { return static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(code.data()) >> 40u); };
    RegisteredStage local;
    RegisteredStage hull;
    RegisteredStage domain;
    Describe(local, LocalCode, 5u, {{0x10au, 0u}, {0x10bu, 0u}}, {}, 0u);
    Describe(hull, HullCode, 7u, {{0x148u, 0u}, {0x149u, 0u}, {0x10au, 0u}, {0x10bu, 16u}, {0x108u, low(HullCode)}, {0x109u, high(HullCode)}}, {{0x2dbu, 0x40049u}, {0x286u, 0x42800000u}, {0x287u, 0x3f800000u}, {0x2d6u, 0xc355u}}, 0x105u);
    Describe(domain, DomainCode, 2u, {{0xc8u, low(DomainCode)}, {0xc9u, high(DomainCode)}, {0x8au, 0u}, {0x8bu, 16u}}, {}, 0x02002008u);
    for (const auto* stage : {&local, &hull, &domain}) AgcDriverRegisterShader_nid_postfix(&stage->shader);
    const std::array<const Shader*, 2> stages{&hull.shader, &domain.shader};
    const std::array<ShaderRegister, 2> context{{{0x2d5u, 0x0200210du}, {0x29bu, 0u}}};
    const std::array<ShaderRegister, 3> primitive{{{0x25bu, 0u}, {0x262u, 0u}, {0x242u, 9u}}};
    AgcDriverResolveGraphicsStagesAbi_nid_postfix(stages, context, primitive);
    AgcDriverShutdown_nid_postfix();
}

ShaderRecompiler::RecompileResult CompileHull(const ShaderRecompiler::SpirvTarget& target, std::span<const std::uint32_t> code, const ShaderRecompiler::TessellationConfiguration& configuration) {
    static const std::array<std::uint32_t, 16> userData{0x1000u};
    const std::array<ShaderRecompiler::MemoryRegion, 1> memory{{{reinterpret_cast<std::uintptr_t>(code.data()), std::as_bytes(code)}}};
    const ShaderRecompiler::RecompileRequest request{
        {ShaderStage::TessellationControl, reinterpret_cast<std::uintptr_t>(code.data()), code, 0, {}},
        {64, 0u, userData, std::nullopt, std::nullopt, ShaderRecompiler::ShaderVertexStageInfo{}, memory},
        target,
        ShaderRecompiler::BindingLayout{0, 0, 0, AgcDriver::Graphics::PipelinePushSlotBytes},
        ShaderRecompiler::GraphicsCompileContext{0u, {}, std::nullopt, configuration, {reinterpret_cast<std::uintptr_t>(Indices.data()), static_cast<std::uint32_t>(Indices.size()), 2, 1}},
    };
    return ShaderRecompiler::Recompile(request);
}

void RequireSingleControlPointHulls(const ShaderRecompiler::SpirvTarget& target) {
    Require(!CompileHull(target, HullSingleInput, {1u, 3u, 1u, 2u, 2u}).spirv.empty(), "a hull that reads only the one input control point of its patch was not compiled");
    Require(!CompileHull(target, HullSingleOutput, {1u, 1u, 1u, 2u, 2u}).spirv.empty(), "a hull with one output control point and unguarded patch outputs was not compiled");
}

void RequireMergedHullWithoutSubgroups(const ShaderRecompiler::SpirvTarget& target) {
    std::uint32_t cursor = 0;
    const auto hull = Compile(target, ShaderStage::TessellationControl, MergedHullCode, cursor, MergedTriangles);
    const auto words = hull.spirv.Words();
    for (std::size_t at = 5; at + 1u < words.size(); at += words[at] >> 16u) {
        Require((words[at] >> 16u) != 0u, "the merged hull program compiled to a SPIR-V instruction without words");
        Require((words[at] & 0xffffu) != spv::OpCapability || words[at + 1u] != spv::CapabilityGroupNonUniform, "the hull stage of a merged hull program declares subgroup operations, which a device without tessellation control subgroups rejects");
    }
}

void RequireMergedHullConstantIndices(const ShaderRecompiler::SpirvTarget& target) {
    std::uint32_t cursor = 0;
    const auto hull = Compile(target, ShaderStage::TessellationControl, MergedHullCode, cursor, MergedTriangles);
    const auto words = hull.spirv.Words();
    std::unordered_set<std::uint32_t> constants;
    std::unordered_set<std::uint32_t> folded;
    for (std::size_t at = 5; at + 1u < words.size(); at += words[at] >> 16u) {
        const auto count = words[at] >> 16u;
        Require(count != 0u && at + count <= words.size(), "the merged hull program compiled to a malformed SPIR-V instruction");
        const auto opcode = words[at] & 0xffffu;
        if (opcode == spv::OpConstant && count >= 3u) constants.insert(words[at + 2u]);
        const bool integer = opcode == spv::OpIAdd || opcode == spv::OpIMul || opcode == spv::OpUDiv || opcode == spv::OpUMod || opcode == spv::OpShiftRightLogical || opcode == spv::OpBitwiseAnd;
        const auto operand = [&](std::uint32_t id) { return constants.contains(id) || folded.contains(id); };
        if (integer && count == 5u && operand(words[at + 3u]) && operand(words[at + 4u])) folded.insert(words[at + 2u]);
        if (opcode == spv::OpAccessChain) {
            for (std::uint32_t index = 4u; index < count; ++index) {
                Require(!folded.contains(words[at + index]), "the merged hull program indexes a tessellation attribute with arithmetic on constants instead of a constant, which AMD's Windows driver reads wrongly");
            }
        }
    }
}

void RequireRegisteredMergedStages() {
    const auto low = [](const auto& code) { return static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(code.data()) >> 8u); };
    const auto high = [](const auto& code) { return static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(code.data()) >> 40u); };
    RegisteredStage merged;
    RegisteredStage domain;
    Describe(merged, MergedHullCode, 3u, {{0x148u, low(MergedHullCode)}, {0x149u, high(MergedHullCode)}, {0x10au, 0u}, {0x10bu, 16u}}, {{0x2dbu, 0x40049u}, {0x286u, 0x42800000u}, {0x287u, 0x3f800000u}, {0x2d6u, 0xc355u}}, 0x105u);
    Describe(domain, DomainCode, 2u, {{0xc8u, low(DomainCode)}, {0xc9u, high(DomainCode)}, {0x8au, 0u}, {0x8bu, 16u}}, {}, 0x02002008u);
    for (const auto* stage : {&merged, &domain}) AgcDriverRegisterShader_nid_postfix(&stage->shader);
    const std::array<const Shader*, 2> stages{&merged.shader, &domain.shader};
    const std::array<ShaderRegister, 2> context{{{0x2d5u, 0x0200210du}, {0x29bu, 0u}}};
    const std::array<ShaderRegister, 3> primitive{{{0x25bu, 0u}, {0x262u, 0u}, {0x242u, 9u}}};
    AgcDriverResolveGraphicsStagesAbi_nid_postfix(stages, context, primitive);
    AgcDriverResolveGraphicsStagesAbi_nid_postfix(stages, context, primitive);
}

}

int main() {
    try {
        auto device = OpenVulkanTestDevice();
        if (!device) return VulkanTestSkipped;
        const auto target = device->Target();
        if (!target.tessellation.has_value()) {
            std::puts("tessellation: skipped, the device has no tessellation shaders");
            return VulkanTestSkipped;
        }
        RequireSingleControlPointHulls(target);
        ExpectRefusal(target, ShaderStage::TessellationControl, HullUnguardedFactors, "not written by control point 0 alone");
        ExpectRefusal(target, ShaderStage::TessellationControl, HullSquaredAddress, "not a whole number of control points");
        ExpectRefusal(target, ShaderStage::TessellationControl, HullHalfSlotIndex, "not a whole number of control points");
        ExpectRefusal(target, ShaderStage::TessellationControl, HullLaneCount, "lane mask");
        ExpectRefusal(target, ShaderStage::TessellationEvaluation, DomainNeighbourRecord, "more than one patch-constant record", RecordPatches);
        ExpectRefusal(target, ShaderStage::TessellationEvaluation, DomainFixedPatchRead, "not indexed by its relative patch", RecordPatches);
        ExpectRefusal(target, ShaderStage::TessellationEvaluation, DomainNeighbourPoint, "neither its patch's control points nor a patch-constant record", RecordPatches);
        ExpectRefusal(target, ShaderStage::TessellationEvaluation, DomainCulledPrimitive, "passthrough primitive");
        ExpectRefusal(target, ShaderStage::Local, LocalIndexAsData, "outside a tessellation ring or LDS address");
        {
            std::uint32_t cursor = 0;
            Require(!Compile(target, ShaderStage::Local, LocalInactiveIndex, cursor).spirv.empty(), "a local program whose relative index survives only on inactive lanes was not compiled");
        }
        ExpectRefusal(target, ShaderStage::Local, LocalRestoredIndex, "outside a tessellation ring or LDS address");
        ExpectRefusal(target, ShaderStage::Local, LocalObservedWindow, "outside a tessellation ring or LDS address");
        ExpectRefusal(target, ShaderStage::Local, LocalNegatedConjunction, "outside a tessellation ring or LDS address");
        ExpectRefusal(target, ShaderStage::TessellationControl, HullRelativeData, "GE_CNTL.PRIM_GRP_SIZE is 0", RecordPatches);
        ExpectRefusal(target, ShaderStage::TessellationEvaluation, DomainRelativeData, "GE_CNTL.PRIM_GRP_SIZE is 0", RecordPatches);
        ExpectRefusal(target, ShaderStage::TessellationControl, HullCode, "outside its patch's quad factors", {{3u, 3u, 2u, 2u, 2u}, Indices});
        ExpectRefusal(target, ShaderStage::TessellationControl, HullDescendingRecords, "not affine");
        ExpectRefusal(target, ShaderStage::TessellationControl, HullDescendingRecords, "not affine", {{3u, 3u, 1u, 2u, 2u, 158u}, Indices});
        ExpectRefusal(target, ShaderStage::TessellationControl, HullDescendingRecords, "outside its control points and patch constants", {{3u, 3u, 1u, 2u, 2u, 129u}, Indices});

        RequirePatches(*device, target, HullCode, "hull program");
        RequirePatches(*device, target, HullRestoredExecSaved, "hull program saving its restored exec");
        RequirePatches(*device, target, HullNextInput, "hull program reading the next of 4 input control points of 24 bytes", NextInputTriangles, LocalCodeStride24);
        RequirePatches(*device, target, HullDynamicIndex, "hull program reading a control point at a computed index");
        RequirePatches(*device, target, HullCode, "domain program reading control points at a computed index", Triangles, LocalCode, DomainComputedPoint);
        RequirePatches(*device, target, HullAndSaveexec, "hull program narrowing exec with s_and_saveexec_b64");
        RequirePatches(*device, target, HullSaveexecLoop, "hull program restoring exec through loop phis");
        RequirePatches(*device, target, HullDescendingRecords, "hull program with descending patch records in 128-patch groups", {{3u, 3u, 1u, 2u, 2u, 128u}, Indices});
        ExpectRefusal(target, ShaderStage::TessellationControl, MergedHullCode, "more input than output control points", {{4u, 3u, 1u, 2u, 2u, 256u, 0u, false, 0u, false, true}, Indices});
        ExpectRefusal(target, ShaderStage::TessellationControl, MergedHullForeignRecord, "outside its own control point's record", MergedTriangles);
        ExpectRefusal(target, ShaderStage::TessellationControl, MergedHullDivergentBarrier, "s_barrier that some invocations do not reach", MergedTriangles);
        ExpectRefusal(target, ShaderStage::TessellationControl, MergedHullOutsidePatch, "reads LDS outside its patch's control point records", MergedTriangles);
        ExpectRefusal(target, ShaderStage::TessellationControl, MergedHullReadLane, "another lane's register", MergedTriangles);
        RequireMergedHullWithoutSubgroups(target);
        RequireMergedHullConstantIndices(target);
        RequirePatches(*device, target, MergedHullCode, "merged hull program", MergedTriangles, MergedHullCode);
        RequirePatches(*device, target, MergedHullRegisters, "merged hull program storing its local part's registers", MergedTriangles, MergedHullRegisters);
        RequireCounterClockwise(*device, target);
        RequireLevelClamps(*device, target);
        RequireIntegerPartitioning(*device, target);
        RequireFractionalEvenPartitioning(*device, target);
        RequireQuads(*device, target);
        RequirePatchConstants(*device, target);
        RequireRelativePatches(*device, target);
        RequireInstancedRelativePatches(*device, target);
        RequireHullBdaReads(*device, target);
        RequireCompareBranches(target);
        device.reset();
        RequireRegisteredMergedStages();
        RequireRegisteredStages();
        std::puts("Tessellation tests passed");
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
