#include "prx/libSceAgcDriver/Execution/include/VulkanDevice.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Draw.hpp"
#include "Recompiler.hpp"
#include "VulkanTestDevice.hpp"
#include <algorithm>
#include <array>
#include <cstdint>
#include <cstdio>
#include <iostream>
#include <optional>
#include <span>
#include <string>
#include <vector>

namespace {

using AgcDriver::Graphics::Require;
using ShaderRecompiler::ShaderStage;

constexpr std::uint32_t Rows = 32;
constexpr std::uint32_t Lanes = 64;
constexpr std::uint32_t Inputs = 4;
constexpr std::uint32_t Stride = 8;
constexpr std::uint32_t Columns = 4;
alignas(256) std::array<std::uint32_t, Lanes * Inputs> Input{};
alignas(256) std::array<std::uint32_t, Lanes * Stride> Output{};
std::vector<std::string> Mismatches;

// v_pk_min_f16 v10, v4, v5; v_pk_max_f16 v11, v4, v5;
// v_pk_max_f16 v12, v4, v6 op_sel:[1,0] op_sel_hi:[0,1] neg_lo:[1,0] neg_hi:[0,1] clamp; v_pk_min_f16 v13, v5, v7 neg_lo:[0,1] clamp
alignas(256) constexpr std::array<std::uint32_t, 16> Code{
    0x34020084u, 0x34060085u, 0xe0381000u, 0x80000401u, 0xbf8c3f70u,
    0xcc11400au, 0x18020b04u,
    0xcc12400bu, 0x18020b04u,
    0xcc12ca0cu, 0x30020d04u,
    0xcc11c00du, 0x58020f05u,
    0xe0781000u, 0x80010a03u, 0xbf810000u,
};

constexpr const char* Names[Columns] = {
    "v_pk_min_f16 v10, v4, v5",
    "v_pk_max_f16 v11, v4, v5",
    "v_pk_max_f16 v12, v4, v6 op_sel:[1,0] op_sel_hi:[0,1] neg_lo:[1,0] neg_hi:[0,1] clamp",
    "v_pk_min_f16 v13, v5, v7 neg_lo:[0,1] clamp",
};

// v4..v7 per lane, signed zeros, NaN payloads, infinities, denormals and values above 1.0 in both halves; lanes 32..63 repeat them.
constexpr std::uint32_t RowData[Rows][Inputs] = {
    {0x80000000u, 0x80000000u, 0xfe427e01u, 0xfc427c01u},
    {0x7e018000u, 0xfe010000u, 0x3800bc00u, 0xfc000000u},
    {0xfc427c00u, 0x40000400u, 0x40000400u, 0x35554000u},
    {0x03fffc00u, 0x80004000u, 0xc0003c00u, 0x0000c000u},
    {0x84007e01u, 0x00013555u, 0x83ff7c00u, 0x3c00fe42u},
    {0x3c01fe42u, 0xbc007e01u, 0x80008400u, 0x03ff3555u},
    {0x7bff7c01u, 0x35558001u, 0x3c008000u, 0x7c013c01u},
    {0x3555fc42u, 0x7c013c00u, 0x7e010400u, 0x7c0003ffu},
    {0x7c000001u, 0x7c000001u, 0x40000000u, 0xc0007e01u},
    {0xfe428001u, 0x7e420001u, 0xfc4283ffu, 0x4000c000u},
    {0x000103ffu, 0x7e017c01u, 0x7e017c01u, 0x8400bc00u},
    {0x83ff83ffu, 0x83ff83ffu, 0x03ff03ffu, 0x03ff03ffu},
    {0x3c000400u, 0x38003c01u, 0x0000c000u, 0xfe42fc00u},
    {0x40008400u, 0x7c00c000u, 0x84008001u, 0x8000fbffu},
    {0xfbff3c00u, 0x8001fc00u, 0xfc00fbffu, 0xfbff3c00u},
    {0x0000bc00u, 0x3c010001u, 0x3c010001u, 0x3c010001u},
    {0xfc003c01u, 0xfc003c01u, 0x7c017bffu, 0x04007c00u},
    {0x7c014000u, 0xfc01c000u, 0x7bfffc42u, 0x00017bffu},
    {0x80013800u, 0x3c000000u, 0x3c000000u, 0x7e018400u},
    {0x04007bffu, 0xc000fe42u, 0x35557c01u, 0x40007e42u},
    {0xbc00fbffu, 0xfe4203ffu, 0x04004000u, 0x7bff8000u},
    {0x3800c000u, 0x0400bc00u, 0x7c00fe42u, 0xbc003800u},
    {0xc0003555u, 0x7bfffbffu, 0xbc003c01u, 0x83ff0400u},
    {0x80000000u, 0xfc007c00u, 0xfe427e01u, 0xfc427c01u},
    {0x7e018000u, 0x7e018000u, 0x3800bc00u, 0xfc000000u},
    {0xfc427c00u, 0x7c42fc00u, 0x0001fc00u, 0x35554000u},
    {0x03fffc00u, 0x80004000u, 0x80004000u, 0x380083ffu},
    {0x84007e01u, 0x00013555u, 0x83ff7c00u, 0x8001b555u},
    {0x3c01fe42u, 0xbc007e01u, 0x80008400u, 0x03ff3555u},
    {0x7bff7c01u, 0x35558001u, 0x3c008000u, 0x7c013c01u},
    {0x3555fc42u, 0x7c013c00u, 0x7e010400u, 0x7c0003ffu},
    {0x7c000001u, 0x84007bffu, 0x40000000u, 0xc0007e01u},
};

struct Group {
    std::uint32_t ieee;
    std::uint32_t dx10;
    std::uint32_t denorm16;
    std::array<std::array<std::uint32_t, Columns>, Rows> rows;
};

// Expected v10..v13 per row, the same in every f16 round mode.
const std::array<Group, 8> Expected{{
    {0, 0, 3, {{
        {0x80000000u, 0x80000000u, 0x00000000u, 0x00000000u},
        {0x7e018000u, 0x7e010000u, 0x00000000u, 0x00000000u},
        {0x40000400u, 0x40007c00u, 0x3c000400u, 0x35550000u},
        {0x8000fc00u, 0x03ff4000u, 0x3c003c00u, 0x00003c00u},
        {0x84003555u, 0x00013555u, 0x03ff3c00u, 0x00013555u},
        {0xbc00fe42u, 0x3c01fe42u, 0x00000000u, 0x00000000u},
        {0x35558001u, 0x7bff8001u, 0x00000000u, 0x35550000u},
        {0x35553c00u, 0x35553c00u, 0xfc420400u, 0x3c000000u},
        {0x7c000001u, 0x7c000001u, 0x00010000u, 0x00000001u},
        {0xfe428001u, 0xfe420001u, 0x00000000u, 0x3c000001u},
        {0x000103ffu, 0x000103ffu, 0x03ff0000u, 0x00003c00u},
        {0x83ff83ffu, 0x83ff83ffu, 0x000003ffu, 0x00000000u},
        {0x38000400u, 0x3c003c01u, 0x04000000u, 0x38003c00u},
        {0x4000c000u, 0x7c008400u, 0x04000000u, 0x00000000u},
        {0xfbfffc00u, 0x80013c00u, 0x3c003c00u, 0x00000000u},
        {0x0000bc00u, 0x3c010001u, 0x00000001u, 0x3c000000u},
        {0xfc003c01u, 0xfc003c01u, 0x3c003c00u, 0x00000000u},
        {0x7c01c000u, 0x7c014000u, 0x3c00fc01u, 0x00010000u},
        {0x80010000u, 0x3c003800u, 0x38000001u, 0x3c000000u},
        {0xc0007bffu, 0x04007bffu, 0x3c000000u, 0x0000fe42u},
        {0xbc00fbffu, 0xbc0003ffu, 0x00003c00u, 0x3c000000u},
        {0x0400c000u, 0x3800bc00u, 0x00000000u, 0x00000000u},
        {0xc000fbffu, 0x7bff3555u, 0x3c003c00u, 0x00000000u},
        {0xfc000000u, 0x80007c00u, 0x00000000u, 0x00003c00u},
        {0x7e018000u, 0x7e018000u, 0x00000000u, 0x00000000u},
        {0xfc42fc00u, 0xfc427c00u, 0x3c000000u, 0x35550000u},
        {0x8000fc00u, 0x03ff4000u, 0x00003c00u, 0x000003ffu},
        {0x84003555u, 0x00013555u, 0x03ff3c00u, 0x00003555u},
        {0xbc00fe42u, 0x3c01fe42u, 0x00000000u, 0x00000000u},
        {0x35558001u, 0x7bff8001u, 0x00000000u, 0x35550000u},
        {0x35553c00u, 0x35553c00u, 0xfc420400u, 0x3c000000u},
        {0x84000001u, 0x7c007bffu, 0x00010000u, 0x00003c00u},
    }}},
    {0, 0, 0, {{
        {0x80000000u, 0x80000000u, 0x00000000u, 0x00000000u},
        {0x7e018000u, 0x7e010000u, 0x00000000u, 0x00000000u},
        {0x40000400u, 0x40007c00u, 0x3c000400u, 0x35550000u},
        {0x8000fc00u, 0x00004000u, 0x3c003c00u, 0x00003c00u},
        {0x84003555u, 0x00003555u, 0x00003c00u, 0x00003555u},
        {0xbc00fe42u, 0x3c01fe42u, 0x00000000u, 0x00000000u},
        {0x35558000u, 0x7bff8000u, 0x00000000u, 0x35550000u},
        {0x35553c00u, 0x35553c00u, 0xfc420400u, 0x3c000000u},
        {0x7c000000u, 0x7c000000u, 0x00000000u, 0x00000000u},
        {0xfe428000u, 0xfe420000u, 0x00000000u, 0x3c000000u},
        {0x00000000u, 0x00000000u, 0x00000000u, 0x00003c00u},
        {0x80008000u, 0x80008000u, 0x00000000u, 0x00000000u},
        {0x38000400u, 0x3c003c01u, 0x04000000u, 0x38003c00u},
        {0x4000c000u, 0x7c008400u, 0x04000000u, 0x00000000u},
        {0xfbfffc00u, 0x80003c00u, 0x3c003c00u, 0x00000000u},
        {0x0000bc00u, 0x3c010000u, 0x00000000u, 0x3c000000u},
        {0xfc003c01u, 0xfc003c01u, 0x3c003c00u, 0x00000000u},
        {0x7c01c000u, 0x7c014000u, 0x3c00fc01u, 0x00000000u},
        {0x80000000u, 0x3c003800u, 0x38000000u, 0x3c000000u},
        {0xc0007bffu, 0x04007bffu, 0x3c000000u, 0x0000fe42u},
        {0xbc00fbffu, 0xbc000000u, 0x00003c00u, 0x3c000000u},
        {0x0400c000u, 0x3800bc00u, 0x00000000u, 0x00000000u},
        {0xc000fbffu, 0x7bff3555u, 0x3c003c00u, 0x00000000u},
        {0xfc000000u, 0x80007c00u, 0x00000000u, 0x00003c00u},
        {0x7e018000u, 0x7e018000u, 0x00000000u, 0x00000000u},
        {0xfc42fc00u, 0xfc427c00u, 0x3c000000u, 0x35550000u},
        {0x8000fc00u, 0x00004000u, 0x00003c00u, 0x00000000u},
        {0x84003555u, 0x00003555u, 0x00003c00u, 0x00003555u},
        {0xbc00fe42u, 0x3c01fe42u, 0x00000000u, 0x00000000u},
        {0x35558000u, 0x7bff8000u, 0x00000000u, 0x35550000u},
        {0x35553c00u, 0x35553c00u, 0xfc420400u, 0x3c000000u},
        {0x84000000u, 0x7c007bffu, 0x00000000u, 0x00003c00u},
    }}},
    {0, 1, 3, {{
        {0x80000000u, 0x80000000u, 0x00000000u, 0x00000000u},
        {0x7e018000u, 0x7e010000u, 0x00000000u, 0x00000000u},
        {0x40000400u, 0x40007c00u, 0x3c000400u, 0x35550000u},
        {0x8000fc00u, 0x03ff4000u, 0x3c003c00u, 0x00003c00u},
        {0x84003555u, 0x00013555u, 0x03ff3c00u, 0x00013555u},
        {0xbc00fe42u, 0x3c01fe42u, 0x00000000u, 0x00000000u},
        {0x35558001u, 0x7bff8001u, 0x00000000u, 0x35550000u},
        {0x35553c00u, 0x35553c00u, 0x00000400u, 0x3c000000u},
        {0x7c000001u, 0x7c000001u, 0x00010000u, 0x00000001u},
        {0xfe428001u, 0xfe420001u, 0x00000000u, 0x3c000001u},
        {0x000103ffu, 0x000103ffu, 0x03ff0000u, 0x00003c00u},
        {0x83ff83ffu, 0x83ff83ffu, 0x000003ffu, 0x00000000u},
        {0x38000400u, 0x3c003c01u, 0x04000000u, 0x38003c00u},
        {0x4000c000u, 0x7c008400u, 0x04000000u, 0x00000000u},
        {0xfbfffc00u, 0x80013c00u, 0x3c003c00u, 0x00000000u},
        {0x0000bc00u, 0x3c010001u, 0x00000001u, 0x3c000000u},
        {0xfc003c01u, 0xfc003c01u, 0x3c003c00u, 0x00000000u},
        {0x7c01c000u, 0x7c014000u, 0x3c000000u, 0x00010000u},
        {0x80010000u, 0x3c003800u, 0x38000001u, 0x3c000000u},
        {0xc0007bffu, 0x04007bffu, 0x3c000000u, 0x00000000u},
        {0xbc00fbffu, 0xbc0003ffu, 0x00003c00u, 0x3c000000u},
        {0x0400c000u, 0x3800bc00u, 0x00000000u, 0x00000000u},
        {0xc000fbffu, 0x7bff3555u, 0x3c003c00u, 0x00000000u},
        {0xfc000000u, 0x80007c00u, 0x00000000u, 0x00003c00u},
        {0x7e018000u, 0x7e018000u, 0x00000000u, 0x00000000u},
        {0xfc42fc00u, 0xfc427c00u, 0x3c000000u, 0x35550000u},
        {0x8000fc00u, 0x03ff4000u, 0x00003c00u, 0x000003ffu},
        {0x84003555u, 0x00013555u, 0x03ff3c00u, 0x00003555u},
        {0xbc00fe42u, 0x3c01fe42u, 0x00000000u, 0x00000000u},
        {0x35558001u, 0x7bff8001u, 0x00000000u, 0x35550000u},
        {0x35553c00u, 0x35553c00u, 0x00000400u, 0x3c000000u},
        {0x84000001u, 0x7c007bffu, 0x00010000u, 0x00003c00u},
    }}},
    {0, 1, 0, {{
        {0x80000000u, 0x80000000u, 0x00000000u, 0x00000000u},
        {0x7e018000u, 0x7e010000u, 0x00000000u, 0x00000000u},
        {0x40000400u, 0x40007c00u, 0x3c000400u, 0x35550000u},
        {0x8000fc00u, 0x00004000u, 0x3c003c00u, 0x00003c00u},
        {0x84003555u, 0x00003555u, 0x00003c00u, 0x00003555u},
        {0xbc00fe42u, 0x3c01fe42u, 0x00000000u, 0x00000000u},
        {0x35558000u, 0x7bff8000u, 0x00000000u, 0x35550000u},
        {0x35553c00u, 0x35553c00u, 0x00000400u, 0x3c000000u},
        {0x7c000000u, 0x7c000000u, 0x00000000u, 0x00000000u},
        {0xfe428000u, 0xfe420000u, 0x00000000u, 0x3c000000u},
        {0x00000000u, 0x00000000u, 0x00000000u, 0x00003c00u},
        {0x80008000u, 0x80008000u, 0x00000000u, 0x00000000u},
        {0x38000400u, 0x3c003c01u, 0x04000000u, 0x38003c00u},
        {0x4000c000u, 0x7c008400u, 0x04000000u, 0x00000000u},
        {0xfbfffc00u, 0x80003c00u, 0x3c003c00u, 0x00000000u},
        {0x0000bc00u, 0x3c010000u, 0x00000000u, 0x3c000000u},
        {0xfc003c01u, 0xfc003c01u, 0x3c003c00u, 0x00000000u},
        {0x7c01c000u, 0x7c014000u, 0x3c000000u, 0x00000000u},
        {0x80000000u, 0x3c003800u, 0x38000000u, 0x3c000000u},
        {0xc0007bffu, 0x04007bffu, 0x3c000000u, 0x00000000u},
        {0xbc00fbffu, 0xbc000000u, 0x00003c00u, 0x3c000000u},
        {0x0400c000u, 0x3800bc00u, 0x00000000u, 0x00000000u},
        {0xc000fbffu, 0x7bff3555u, 0x3c003c00u, 0x00000000u},
        {0xfc000000u, 0x80007c00u, 0x00000000u, 0x00003c00u},
        {0x7e018000u, 0x7e018000u, 0x00000000u, 0x00000000u},
        {0xfc42fc00u, 0xfc427c00u, 0x3c000000u, 0x35550000u},
        {0x8000fc00u, 0x00004000u, 0x00003c00u, 0x00000000u},
        {0x84003555u, 0x00003555u, 0x00003c00u, 0x00003555u},
        {0xbc00fe42u, 0x3c01fe42u, 0x00000000u, 0x00000000u},
        {0x35558000u, 0x7bff8000u, 0x00000000u, 0x35550000u},
        {0x35553c00u, 0x35553c00u, 0x00000400u, 0x3c000000u},
        {0x84000000u, 0x7c007bffu, 0x00000000u, 0x00003c00u},
    }}},
    {1, 0, 3, {{
        {0x80000000u, 0x80000000u, 0x00000000u, 0xfe42fe01u},
        {0x7e018000u, 0x7e010000u, 0x00000000u, 0x00000000u},
        {0xfe420400u, 0xfe427c00u, 0x3c007e42u, 0x35550000u},
        {0x8000fc00u, 0x03ff4000u, 0x3c003c00u, 0x00003c00u},
        {0x84003555u, 0x00013555u, 0x03ff3c00u, 0x00013555u},
        {0xbc00fe42u, 0x3c01fe42u, 0x00000000u, 0x00000000u},
        {0x35557e01u, 0x7bff7e01u, 0x7e010000u, 0x7e010000u},
        {0x7e01fe42u, 0x7e01fe42u, 0xfe420400u, 0x7e010000u},
        {0x7c000001u, 0x7c000001u, 0x00010000u, 0x00000001u},
        {0xfe428001u, 0xfe420001u, 0x7e420000u, 0x3c000001u},
        {0x00017e01u, 0x00017e01u, 0x03ff7e01u, 0x00007e01u},
        {0x83ff83ffu, 0x83ff83ffu, 0x000003ffu, 0x00000000u},
        {0x38000400u, 0x3c003c01u, 0x04000000u, 0x38003c00u},
        {0x4000c000u, 0x7c008400u, 0x04000000u, 0x00000000u},
        {0xfbfffc00u, 0x80013c00u, 0x3c003c00u, 0x00000000u},
        {0x0000bc00u, 0x3c010001u, 0x00000001u, 0x3c000000u},
        {0xfc003c01u, 0xfc003c01u, 0xfe013c00u, 0x00000000u},
        {0x7e01c000u, 0x7e014000u, 0x3c00fe01u, 0xfe010000u},
        {0x80010000u, 0x3c003800u, 0x38000001u, 0x3c000000u},
        {0xc0007bffu, 0x04007bffu, 0x3c007e01u, 0x0000fe42u},
        {0xbc00fbffu, 0xbc0003ffu, 0x00003c00u, 0x3c000000u},
        {0x0400c000u, 0x3800bc00u, 0x00000000u, 0x00000000u},
        {0xc000fbffu, 0x7bff3555u, 0x3c003c00u, 0x00000000u},
        {0xfc000000u, 0x80007c00u, 0x00000000u, 0xfe42fe01u},
        {0x7e018000u, 0x7e018000u, 0x00000000u, 0x00000000u},
        {0xfe42fc00u, 0xfe427c00u, 0x3c007e42u, 0x7e420000u},
        {0x8000fc00u, 0x03ff4000u, 0x00003c00u, 0x000003ffu},
        {0x84003555u, 0x00013555u, 0x03ff3c00u, 0x00003555u},
        {0xbc00fe42u, 0x3c01fe42u, 0x00000000u, 0x00000000u},
        {0x35557e01u, 0x7bff7e01u, 0x7e010000u, 0x7e010000u},
        {0x7e01fe42u, 0x7e01fe42u, 0xfe420400u, 0x7e010000u},
        {0x84000001u, 0x7c007bffu, 0x00010000u, 0x00003c00u},
    }}},
    {1, 0, 0, {{
        {0x80000000u, 0x80000000u, 0x00000000u, 0xfe42fe01u},
        {0x7e018000u, 0x7e010000u, 0x00000000u, 0x00000000u},
        {0xfe420400u, 0xfe427c00u, 0x3c007e42u, 0x35550000u},
        {0x8000fc00u, 0x00004000u, 0x3c003c00u, 0x00003c00u},
        {0x84003555u, 0x00003555u, 0x00003c00u, 0x00003555u},
        {0xbc00fe42u, 0x3c01fe42u, 0x00000000u, 0x00000000u},
        {0x35557e01u, 0x7bff7e01u, 0x7e010000u, 0x7e010000u},
        {0x7e01fe42u, 0x7e01fe42u, 0xfe420400u, 0x7e010000u},
        {0x7c000000u, 0x7c000000u, 0x00000000u, 0x00000000u},
        {0xfe428000u, 0xfe420000u, 0x7e420000u, 0x3c000000u},
        {0x00007e01u, 0x00007e01u, 0x00007e01u, 0x00007e01u},
        {0x80008000u, 0x80008000u, 0x00000000u, 0x00000000u},
        {0x38000400u, 0x3c003c01u, 0x04000000u, 0x38003c00u},
        {0x4000c000u, 0x7c008400u, 0x04000000u, 0x00000000u},
        {0xfbfffc00u, 0x80003c00u, 0x3c003c00u, 0x00000000u},
        {0x0000bc00u, 0x3c010000u, 0x00000000u, 0x3c000000u},
        {0xfc003c01u, 0xfc003c01u, 0xfe013c00u, 0x00000000u},
        {0x7e01c000u, 0x7e014000u, 0x3c00fe01u, 0xfe010000u},
        {0x80000000u, 0x3c003800u, 0x38000000u, 0x3c000000u},
        {0xc0007bffu, 0x04007bffu, 0x3c007e01u, 0x0000fe42u},
        {0xbc00fbffu, 0xbc000000u, 0x00003c00u, 0x3c000000u},
        {0x0400c000u, 0x3800bc00u, 0x00000000u, 0x00000000u},
        {0xc000fbffu, 0x7bff3555u, 0x3c003c00u, 0x00000000u},
        {0xfc000000u, 0x80007c00u, 0x00000000u, 0xfe42fe01u},
        {0x7e018000u, 0x7e018000u, 0x00000000u, 0x00000000u},
        {0xfe42fc00u, 0xfe427c00u, 0x3c007e42u, 0x7e420000u},
        {0x8000fc00u, 0x00004000u, 0x00003c00u, 0x00000000u},
        {0x84003555u, 0x00003555u, 0x00003c00u, 0x00003555u},
        {0xbc00fe42u, 0x3c01fe42u, 0x00000000u, 0x00000000u},
        {0x35557e01u, 0x7bff7e01u, 0x7e010000u, 0x7e010000u},
        {0x7e01fe42u, 0x7e01fe42u, 0xfe420400u, 0x7e010000u},
        {0x84000000u, 0x7c007bffu, 0x00000000u, 0x00003c00u},
    }}},
    {1, 1, 3, {{
        {0x80000000u, 0x80000000u, 0x00000000u, 0x00000000u},
        {0x7e018000u, 0x7e010000u, 0x00000000u, 0x00000000u},
        {0xfe420400u, 0xfe427c00u, 0x3c000000u, 0x35550000u},
        {0x8000fc00u, 0x03ff4000u, 0x3c003c00u, 0x00003c00u},
        {0x84003555u, 0x00013555u, 0x03ff3c00u, 0x00013555u},
        {0xbc00fe42u, 0x3c01fe42u, 0x00000000u, 0x00000000u},
        {0x35557e01u, 0x7bff7e01u, 0x00000000u, 0x00000000u},
        {0x7e01fe42u, 0x7e01fe42u, 0x00000400u, 0x00000000u},
        {0x7c000001u, 0x7c000001u, 0x00010000u, 0x00000001u},
        {0xfe428001u, 0xfe420001u, 0x00000000u, 0x3c000001u},
        {0x00017e01u, 0x00017e01u, 0x03ff0000u, 0x00000000u},
        {0x83ff83ffu, 0x83ff83ffu, 0x000003ffu, 0x00000000u},
        {0x38000400u, 0x3c003c01u, 0x04000000u, 0x38003c00u},
        {0x4000c000u, 0x7c008400u, 0x04000000u, 0x00000000u},
        {0xfbfffc00u, 0x80013c00u, 0x3c003c00u, 0x00000000u},
        {0x0000bc00u, 0x3c010001u, 0x00000001u, 0x3c000000u},
        {0xfc003c01u, 0xfc003c01u, 0x00003c00u, 0x00000000u},
        {0x7e01c000u, 0x7e014000u, 0x3c000000u, 0x00000000u},
        {0x80010000u, 0x3c003800u, 0x38000001u, 0x3c000000u},
        {0xc0007bffu, 0x04007bffu, 0x3c000000u, 0x00000000u},
        {0xbc00fbffu, 0xbc0003ffu, 0x00003c00u, 0x3c000000u},
        {0x0400c000u, 0x3800bc00u, 0x00000000u, 0x00000000u},
        {0xc000fbffu, 0x7bff3555u, 0x3c003c00u, 0x00000000u},
        {0xfc000000u, 0x80007c00u, 0x00000000u, 0x00000000u},
        {0x7e018000u, 0x7e018000u, 0x00000000u, 0x00000000u},
        {0xfe42fc00u, 0xfe427c00u, 0x3c000000u, 0x00000000u},
        {0x8000fc00u, 0x03ff4000u, 0x00003c00u, 0x000003ffu},
        {0x84003555u, 0x00013555u, 0x03ff3c00u, 0x00003555u},
        {0xbc00fe42u, 0x3c01fe42u, 0x00000000u, 0x00000000u},
        {0x35557e01u, 0x7bff7e01u, 0x00000000u, 0x00000000u},
        {0x7e01fe42u, 0x7e01fe42u, 0x00000400u, 0x00000000u},
        {0x84000001u, 0x7c007bffu, 0x00010000u, 0x00003c00u},
    }}},
    {1, 1, 0, {{
        {0x80000000u, 0x80000000u, 0x00000000u, 0x00000000u},
        {0x7e018000u, 0x7e010000u, 0x00000000u, 0x00000000u},
        {0xfe420400u, 0xfe427c00u, 0x3c000000u, 0x35550000u},
        {0x8000fc00u, 0x00004000u, 0x3c003c00u, 0x00003c00u},
        {0x84003555u, 0x00003555u, 0x00003c00u, 0x00003555u},
        {0xbc00fe42u, 0x3c01fe42u, 0x00000000u, 0x00000000u},
        {0x35557e01u, 0x7bff7e01u, 0x00000000u, 0x00000000u},
        {0x7e01fe42u, 0x7e01fe42u, 0x00000400u, 0x00000000u},
        {0x7c000000u, 0x7c000000u, 0x00000000u, 0x00000000u},
        {0xfe428000u, 0xfe420000u, 0x00000000u, 0x3c000000u},
        {0x00007e01u, 0x00007e01u, 0x00000000u, 0x00000000u},
        {0x80008000u, 0x80008000u, 0x00000000u, 0x00000000u},
        {0x38000400u, 0x3c003c01u, 0x04000000u, 0x38003c00u},
        {0x4000c000u, 0x7c008400u, 0x04000000u, 0x00000000u},
        {0xfbfffc00u, 0x80003c00u, 0x3c003c00u, 0x00000000u},
        {0x0000bc00u, 0x3c010000u, 0x00000000u, 0x3c000000u},
        {0xfc003c01u, 0xfc003c01u, 0x00003c00u, 0x00000000u},
        {0x7e01c000u, 0x7e014000u, 0x3c000000u, 0x00000000u},
        {0x80000000u, 0x3c003800u, 0x38000000u, 0x3c000000u},
        {0xc0007bffu, 0x04007bffu, 0x3c000000u, 0x00000000u},
        {0xbc00fbffu, 0xbc000000u, 0x00003c00u, 0x3c000000u},
        {0x0400c000u, 0x3800bc00u, 0x00000000u, 0x00000000u},
        {0xc000fbffu, 0x7bff3555u, 0x3c003c00u, 0x00000000u},
        {0xfc000000u, 0x80007c00u, 0x00000000u, 0x00000000u},
        {0x7e018000u, 0x7e018000u, 0x00000000u, 0x00000000u},
        {0xfe42fc00u, 0xfe427c00u, 0x3c000000u, 0x00000000u},
        {0x8000fc00u, 0x00004000u, 0x00003c00u, 0x00000000u},
        {0x84003555u, 0x00003555u, 0x00003c00u, 0x00003555u},
        {0xbc00fe42u, 0x3c01fe42u, 0x00000000u, 0x00000000u},
        {0x35557e01u, 0x7bff7e01u, 0x00000000u, 0x00000000u},
        {0x7e01fe42u, 0x7e01fe42u, 0x00000400u, 0x00000000u},
        {0x84000000u, 0x7c007bffu, 0x00000000u, 0x00003c00u},
    }}},
}};

std::array<std::uint32_t, 4> BufferDescriptor(const void* data, std::uint32_t bytes) {
    const auto address = reinterpret_cast<std::uintptr_t>(data);
    return {static_cast<std::uint32_t>(address), static_cast<std::uint32_t>((address >> 32u) & 0xffffu), bytes, 0x31016facu};
}

std::string Hex(std::uint32_t value) {
    char text[16];
    std::snprintf(text, sizeof(text), "0x%08x", value);
    return text;
}

void Run(AgcDriver::VulkanDevice& device, std::uint32_t waveSize, const ShaderRecompiler::SpirvTarget& target, const Group& group, std::uint32_t round16, const std::string& run) {
    for (std::uint32_t lane = 0; lane < Lanes; ++lane) {
        std::copy(std::begin(RowData[lane % Rows]), std::end(RowData[lane % Rows]), Input.begin() + lane * Inputs);
    }
    Output.fill(0xdeadbeefu);
    std::vector<std::uint32_t> userData(8, 0u);
    const auto input = BufferDescriptor(Input.data(), static_cast<std::uint32_t>(Input.size() * 4u));
    const auto output = BufferDescriptor(Output.data(), static_cast<std::uint32_t>(Output.size() * 4u));
    std::copy(input.begin(), input.end(), userData.begin());
    std::copy(output.begin(), output.end(), userData.begin() + 4);
    const std::span<const std::uint32_t> code(Code);
    const std::array<ShaderRecompiler::MemoryRegion, 1> memory{{{reinterpret_cast<std::uintptr_t>(code.data()), std::as_bytes(code)}}};
    const ShaderRecompiler::ShaderComputeStageInfo compute{{waveSize, 1, 1}, 0u, {false, false, false}, false, 1};
    ShaderRecompiler::RecompileRequest request{
        {ShaderStage::Compute, reinterpret_cast<std::uintptr_t>(code.data()), code, 0, {}},
        {waveSize, 0, userData, compute, std::nullopt, std::nullopt, memory},
        target,
        {0, 0, 0, 128}
    };
    request.useCache = false;
    request.context.floatMode = ShaderRecompiler::ShaderFloatMode{0x30u | (round16 << 2u) | (group.denorm16 << 6u), group.dx10 != 0u, group.ieee != 0u, false};
    const auto result = ShaderRecompiler::Recompile(request);
    device.Dispatch(result, 1, 1, 1, {}, reinterpret_cast<std::uintptr_t>(code.data()));
    device.WaitIdle();
    const std::string mode = run + " round16=" + std::to_string(round16) + " ieee=" + std::to_string(group.ieee) + " dx10=" + std::to_string(group.dx10) + " denorm16=" + std::to_string(group.denorm16);
    bool failed = false;
    for (std::uint32_t lane = 0; lane < waveSize; ++lane) {
        for (std::uint32_t column = 0; column < Columns; ++column) {
            const std::uint32_t actual = Output[lane * Stride + column];
            const std::uint32_t wanted = group.rows[lane % Rows][column];
            if (actual == wanted) continue;
            failed = true;
            Mismatches.push_back(mode + " lane " + std::to_string(lane) + " " + Names[column] + " with v4=" + Hex(RowData[lane % Rows][0]) + " v5=" + Hex(RowData[lane % Rows][1]) + " v6=" + Hex(RowData[lane % Rows][2]) + " v7=" + Hex(RowData[lane % Rows][3]) + " is " + Hex(actual) + ", expected " + Hex(wanted));
        }
    }
    if (!failed) return;
    // The measured table in the layout of Expected, ready to paste, one row per input row (lanes 0..31).
    std::cerr << "measured " << mode << ":\n    {" << group.ieee << ", " << group.dx10 << ", " << group.denorm16 << ", {{\n";
    for (std::uint32_t row = 0; row < Rows; ++row) {
        std::cerr << "        {";
        for (std::uint32_t column = 0; column < Columns; ++column) std::cerr << (column != 0u ? ", " : "") << Hex(Output[row * Stride + column]) << 'u';
        std::cerr << "},\n";
    }
    std::cerr << "    }}},\n";
}

}

int main() {
    try {
        const auto device = OpenVulkanTestDevice();
        if (!device) return VulkanTestSkipped;
        if (device->Target().subgroupSize < 32) {
            std::printf("skipped, subgroup size %u cannot hold a wave32\n", device->Target().subgroupSize);
            return VulkanTestSkipped;
        }
        for (const auto& group : Expected) {
            for (std::uint32_t round16 = 0; round16 < 4u; ++round16) {
                Run(*device, 32, device->Target(), group, round16, "wave32");
                Run(*device, 64, device->Target(), group, round16, "wave64");
                Run(*device, 64, device->ComputeTarget(32), group, round16, "wave64 split");
            }
        }
        for (const auto& mismatch : Mismatches) std::cerr << mismatch << '\n';
        Require(Mismatches.empty(), std::to_string(Mismatches.size()) + " packed f16 min/max round mode mismatches");
        std::puts("packed f16 min/max round mode tests passed");
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
