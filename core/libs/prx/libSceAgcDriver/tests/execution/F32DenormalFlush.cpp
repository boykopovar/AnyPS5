#include "prx/libSceAgcDriver/Execution/include/VulkanDevice.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Draw.hpp"
#include "Recompiler.hpp"
#include "VulkanTestDevice.hpp"
#include <algorithm>
#include <array>
#include <bit>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <iostream>
#include <optional>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

using AgcDriver::Graphics::Require;
using ShaderRecompiler::ShaderStage;

constexpr std::uint32_t Threads = 32;
constexpr std::uint32_t Inputs = 4;
constexpr std::uint32_t Results = 19;
constexpr std::uint32_t TinyResults = 16;
alignas(256) std::array<std::uint32_t, Threads * Inputs> Input{};
alignas(256) std::array<std::uint32_t, Threads * Results> Output{};

alignas(256) constexpr std::array<std::uint32_t, 46> Code{
    0x34020084u, 0x160600ffu, 0x0000004cu, 0xe0381000u, 0x80000401u, 0xbf8c3f70u, 0x06140b04u, 0x08160905u,
    0x10180b04u, 0x0e1a0b04u, 0xd54b000eu, 0x041a0b04u, 0x7e1e0306u, 0x561e0b04u, 0x1e200b04u, 0xd5540011u,
    0x041a0b04u, 0xd5570012u, 0x041a0b04u, 0x7e264904u, 0x7e284505u, 0x7e2a8104u, 0x7c020b04u, 0xd5010016u,
    0x01a90280u, 0x7e2e1b04u, 0x7e307f05u, 0xd5080119u, 0x42020b04u, 0xd55f001au, 0x041a0b04u, 0xd550001bu,
    0x041a0b04u, 0xd550001cu, 0x03ca0b04u, 0xe0781000u, 0x80010a03u, 0xe0781010u, 0x80010e03u, 0xe0781020u,
    0x80011203u, 0xe0781030u, 0x80011603u, 0xe07c1040u, 0x80011a03u, 0xbf810000u,
};

constexpr std::uint32_t Rows[32][4] = {
    {0x00000001u, 0x3f800000u, 0x00000000u, 0x00000000u},
    {0x80400000u, 0x3f800000u, 0x00800000u, 0x00000000u},
    {0x3f800000u, 0x00012345u, 0x80000001u, 0x00000000u},
    {0x007fffffu, 0x007fffffu, 0x3f800000u, 0x00000000u},
    {0x3f7fffffu, 0x00800000u, 0x00000000u, 0x00000000u},
    {0xbf7fffffu, 0x00800000u, 0x80000000u, 0x00000000u},
    {0x3f7fffffu, 0x00800001u, 0x00000001u, 0x00000000u},
    {0x3f000000u, 0x01000000u, 0x80000001u, 0x00000000u},
    {0x00800000u, 0x80800001u, 0x00800000u, 0x00000000u},
    {0x00800001u, 0x00800000u, 0x3f800000u, 0x00000000u},
    {0x00000000u, 0x80000001u, 0x00000001u, 0x00000000u},
    {0x80000000u, 0x00000001u, 0x807fffffu, 0x00000000u},
    {0x7f800000u, 0x00000001u, 0x3f800000u, 0x00000000u},
    {0x00000001u, 0xff800000u, 0x00000000u, 0x00000000u},
    {0x7fc00000u, 0x00000001u, 0x00000001u, 0x00000000u},
    {0x40000000u, 0x00400000u, 0x80800000u, 0x00000000u},
    {0x3f800000u, 0x3f800000u, 0x80000001u, 0x00000000u},
    {0xbf800000u, 0x00000001u, 0x3f000000u, 0x00000000u},
    {0x34000000u, 0x34000000u, 0x00000000u, 0x00000000u},
    {0x1f800000u, 0x1f800000u, 0x00000000u, 0x00000000u},
    {0x1fffffffu, 0x1fffffffu, 0x00800000u, 0x00000000u},
    {0x20000000u, 0x1f000000u, 0x00000000u, 0x00000000u},
    {0x20000000u, 0x1f7fffffu, 0x00000000u, 0x00000000u},
    {0x20000001u, 0x1f7fffffu, 0x00000000u, 0x00000000u},
    {0xa0000000u, 0x1f7fffffu, 0x80000000u, 0x00000000u},
    {0x3f800000u, 0xbf800000u, 0x00000001u, 0x00000000u},
    {0x00800000u, 0x3f000000u, 0x00000000u, 0x00000000u},
    {0x00800000u, 0x3f7fffffu, 0x00000001u, 0x00000000u},
    {0x41200000u, 0x00000003u, 0x3dcccccdu, 0x00000000u},
    {0xc0490fdbu, 0x80000002u, 0x40490fdbu, 0x00000000u},
    {0x80000001u, 0x80000001u, 0x80000001u, 0x00000000u},
    {0x00000001u, 0x00000001u, 0x00000001u, 0x00000000u}
};
constexpr std::uint32_t Expected[32][19] = {
    {0x3f800000u, 0x3f800000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x3f800000u, 0x00000000u, 0x00000000u, 0x3f800000u, 0x00000000u, 0x00000001u, 0x00000000u, 0x00000001u, 0x80000000u, 0x00000000u, 0xff7fffffu, 0x00000000u},
    {0x3f800000u, 0x3f800000u, 0x80000000u, 0x00000000u, 0x00800000u, 0x00800000u, 0x80000000u, 0x3f800000u, 0x00800000u, 0x80000000u, 0x3f800000u, 0x80000000u, 0x00000001u, 0x00000000u, 0x00000001u, 0x80000000u, 0x00400000u, 0x00000000u, 0x00000000u},
    {0x3f800000u, 0xbf800000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x3f800000u, 0x00000000u, 0x3f800000u, 0x00000000u, 0x3f000000u, 0x00000000u, 0x00000001u, 0x00000000u, 0x80000000u, 0xffc00000u, 0xff7fffffu, 0x00000000u},
    {0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x3f800000u, 0x3f800000u, 0x00000000u, 0x3f800000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x80000000u, 0x7f800000u, 0x00000000u, 0x00000000u},
    {0x3f7fffffu, 0xbf7fffffu, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00800000u, 0x3f7fffffu, 0x00800000u, 0x00000000u, 0x3f800000u, 0x3f7fffffu, 0x00000000u, 0x00000000u, 0xffffff83u, 0x80000000u, 0x00000000u, 0xff7fffffu, 0x00000000u},
    {0xbf7fffffu, 0x3f7fffffu, 0x80000000u, 0x80000000u, 0x80000000u, 0x80000000u, 0xbf7fffffu, 0x00800000u, 0x80000000u, 0xbf800000u, 0x3f800000u, 0xbf7fffffu, 0x00000001u, 0xffffffffu, 0xffffff83u, 0x80000000u, 0x80000000u, 0xff7fffffu, 0x80000000u},
    {0x3f7fffffu, 0xbf7fffffu, 0x00800000u, 0x00800000u, 0x00800000u, 0x00800000u, 0x00800001u, 0x3f7fffffu, 0x00800001u, 0x00000000u, 0x3f800000u, 0x3f7fffffu, 0x00000000u, 0x00000000u, 0xffffff83u, 0x80800000u, 0x00000000u, 0xff7fffffu, 0x00800000u},
    {0x3f000000u, 0xbf000000u, 0x00800000u, 0x00800000u, 0x00800000u, 0x00800000u, 0x01000000u, 0x3f000000u, 0x01000000u, 0x00000000u, 0x3f800000u, 0x3f000000u, 0x00000000u, 0x00000000u, 0xffffff84u, 0x80800000u, 0x80000000u, 0xff7fffffu, 0x00800000u},
    {0x80000000u, 0x81000000u, 0x80000000u, 0x80000000u, 0x00800000u, 0x00800000u, 0x80800001u, 0x00800000u, 0x00800000u, 0x00000000u, 0x80000000u, 0x3f000000u, 0x00000000u, 0x00000000u, 0xffffff83u, 0x00000000u, 0x80800000u, 0x80000000u, 0x80000000u},
    {0x01000000u, 0x80000000u, 0x00000000u, 0x00000000u, 0x3f800000u, 0x3f800000u, 0x00800000u, 0x3f800000u, 0x00800001u, 0x00000000u, 0x3f800000u, 0x3f000001u, 0x00000000u, 0x00000000u, 0xffffff83u, 0x80000000u, 0x00800001u, 0x00000000u, 0x00000000u},
    {0x00000000u, 0x80000000u, 0x80000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x80000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x80000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0xffc00000u, 0xff7fffffu, 0x00000000u},
    {0x00000000u, 0x00000000u, 0x80000000u, 0x00000000u, 0x80000000u, 0x80000000u, 0x80000000u, 0x00000000u, 0x80000000u, 0x80000000u, 0x00000000u, 0x80000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x80000000u, 0xffc00000u, 0xff7fffffu, 0x00000000u},
    {0x7f800000u, 0xff800000u, 0xffc00000u, 0x00000000u, 0xffc00000u, 0xffc00000u, 0x00000000u, 0x7f800000u, 0x3f800000u, 0x7f800000u, 0x00000000u, 0x7f800000u, 0x00000000u, 0x7fffffffu, 0x00000000u, 0xffc00000u, 0x7f800000u, 0x00000000u, 0x00000000u},
    {0xff800000u, 0xff800000u, 0xffc00000u, 0x00000000u, 0xffc00000u, 0xffc00000u, 0xff800000u, 0x00000000u, 0x00000000u, 0x00000000u, 0xff800000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0xffc00000u, 0x80000000u, 0xff7fffffu, 0xff7fffffu},
    {0x7fc00000u, 0xffc00000u, 0x7fc00000u, 0x00000000u, 0x7fc00000u, 0x7fc00000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x7fc00000u, 0x00000000u, 0x7fc00000u, 0x00000000u, 0x7fffffffu, 0x00000000u, 0x7fc00000u, 0xffc00000u, 0xff7fffffu, 0x00000000u},
    {0x40000000u, 0xc0000000u, 0x00000000u, 0x00000000u, 0x80800000u, 0x80800000u, 0x00000000u, 0x40000000u, 0x00000000u, 0x40000000u, 0x00000000u, 0x3f000000u, 0x00000000u, 0x00000002u, 0x00000000u, 0x80000000u, 0xff800000u, 0xff7fffffu, 0x00000000u},
    {0x40000000u, 0x00000000u, 0x3f800000u, 0x3f800000u, 0x3f800000u, 0x3f800000u, 0x3f800000u, 0x3f800000u, 0x3f800000u, 0x3f800000u, 0x3f800000u, 0x3f000000u, 0x00000000u, 0x00000001u, 0x00000001u, 0xbf800000u, 0x80000000u, 0xff7fffffu, 0x3f800000u},
    {0xbf800000u, 0x3f800000u, 0x80000000u, 0x00000000u, 0x3f000000u, 0x3f000000u, 0xbf800000u, 0x3f000000u, 0x00000000u, 0xbf800000u, 0x00000000u, 0xbf000000u, 0x00000001u, 0xffffffffu, 0x00000000u, 0x80000000u, 0x7f800000u, 0x00000000u, 0x00000000u},
    {0x34800000u, 0x00000000u, 0x28800000u, 0x28800000u, 0x28800000u, 0x28800000u, 0x34000000u, 0x34000000u, 0x34000000u, 0x00000000u, 0x3f800000u, 0x3f000000u, 0x00000000u, 0x00000000u, 0xffffffeau, 0xa8800000u, 0x00000000u, 0xff7fffffu, 0x28800000u},
    {0x20000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x1f800000u, 0x1f800000u, 0x1f800000u, 0x00000000u, 0x3f800000u, 0x3f000000u, 0x00000000u, 0x00000000u, 0xffffffc1u, 0x80000000u, 0x00000000u, 0xff7fffffu, 0x00000000u},
    {0x207fffffu, 0x00000000u, 0x00000000u, 0x00000000u, 0x00ffffffu, 0x00ffffffu, 0x1fffffffu, 0x1fffffffu, 0x1fffffffu, 0x00000000u, 0x3f800000u, 0x3f7fffffu, 0x00000000u, 0x00000000u, 0xffffffc1u, 0x80000000u, 0x1fffffffu, 0x00000000u, 0x00000000u},
    {0x20200000u, 0x9fc00000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x1f000000u, 0x20000000u, 0x1f000000u, 0x00000000u, 0x3f800000u, 0x3f000000u, 0x00000000u, 0x00000000u, 0xffffffc0u, 0x80000000u, 0x00000000u, 0xff7fffffu, 0x00000000u},
    {0x20400000u, 0x9f800000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x1f7fffffu, 0x20000000u, 0x1f7fffffu, 0x00000000u, 0x3f800000u, 0x3f000000u, 0x00000000u, 0x00000000u, 0xffffffc0u, 0x80000000u, 0x00000000u, 0xff7fffffu, 0x00000000u},
    {0x20400001u, 0x9f800002u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x1f7fffffu, 0x20000001u, 0x1f7fffffu, 0x00000000u, 0x3f800000u, 0x3f000001u, 0x00000000u, 0x00000000u, 0xffffffc0u, 0x80000000u, 0x00000000u, 0xff7fffffu, 0x00000000u},
    {0x9f800000u, 0x20400000u, 0x80000000u, 0x80000000u, 0x80000000u, 0x80000000u, 0xa0000000u, 0x1f7fffffu, 0x80000000u, 0xbf800000u, 0x3f800000u, 0xbf000000u, 0x00000001u, 0xffffffffu, 0xffffffc0u, 0x80000000u, 0x80000000u, 0xff7fffffu, 0x80000000u},
    {0x00000000u, 0xc0000000u, 0xbf800000u, 0xbf800000u, 0xbf800000u, 0xbf800000u, 0xbf800000u, 0x3f800000u, 0x00000000u, 0x3f800000u, 0xbf800000u, 0x3f000000u, 0x00000000u, 0x00000001u, 0x00000001u, 0x3f800000u, 0x80000000u, 0xff7fffffu, 0xbf800000u},
    {0x3f000000u, 0x3f000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00800000u, 0x3f000000u, 0x00800000u, 0x00000000u, 0x3f800000u, 0x3f000000u, 0x00000001u, 0x00000000u, 0x00000000u, 0x80000000u, 0x00000000u, 0xff7fffffu, 0x00000000u},
    {0x3f7fffffu, 0x3f7fffffu, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00800000u, 0x3f7fffffu, 0x00800000u, 0x00000000u, 0x3f800000u, 0x3f000000u, 0x00000001u, 0x00000000u, 0x00000000u, 0x80000000u, 0x00000000u, 0xff7fffffu, 0x00000000u},
    {0x41200000u, 0xc1200000u, 0x00000000u, 0x00000000u, 0x3dcccccdu, 0x3dcccccdu, 0x00000000u, 0x41200000u, 0x3dcccccdu, 0x41200000u, 0x00000000u, 0x3f200000u, 0x00000000u, 0x0000000au, 0x00000000u, 0x80000000u, 0x7f800000u, 0x00000000u, 0x00000000u},
    {0xc0490fdbu, 0x40490fdbu, 0x00000000u, 0x00000000u, 0x40490fdbu, 0x40490fdbu, 0xc0490fdbu, 0x40490fdbu, 0x80000000u, 0xc0800000u, 0x80000000u, 0xbf490fdbu, 0x00000001u, 0xfffffffcu, 0x00000000u, 0x00000000u, 0xff800000u, 0x00000000u, 0x00000000u},
    {0x80000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x80000000u, 0x80000000u, 0x80000000u, 0x80000000u, 0x80000000u, 0x80000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0xffc00000u, 0xff7fffffu, 0x00000000u},
    {0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x80000000u, 0xffc00000u, 0xff7fffffu, 0x00000000u}
};
constexpr const char* Names[19] = {
    "v_add_f32 v10, v4, v5",
    "v_sub_f32 v11, v5, v4",
    "v_mul_f32 v12, v4, v5",
    "v_mul_legacy_f32 v13, v4, v5",
    "v_fma_f32 v14, v4, v5, v6",
    "v_fmac_f32 v15, v4, v5",
    "v_min_f32 v16, v4, v5",
    "v_max3_f32 v17, v4, v5, v6",
    "v_med3_f32 v18, v4, v5, v6",
    "v_floor_f32 v19, v4",
    "v_ceil_f32 v20, v5",
    "v_frexp_mant_f32 v21, v4",
    "v_cmp_lt_f32 v4, v5",
    "v_cvt_flr_i32_f32 v23, v4",
    "v_frexp_exp_i32_f32 v24, v5",
    "v_mul_f32 v25, |v4|, -v5",
    "v_div_fixup_f32 v26, v4, v5, v6",
    "v_mullit_f32 v27, v4, v5, v6",
    "v_mullit_f32 v28, v4, v5, 1.0"
};

alignas(256) constexpr std::array<std::uint32_t, 30> TinyCode{
    0x34020084u, 0x34040086u, 0xe0381000u, 0x80000401u, 0xbf8c3f70u, 0x10140b04u, 0x0e160b04u, 0xd54b000cu,
    0x041a0b04u, 0x7e1a0306u, 0x561a0b04u, 0x581c0d04u, 0x3f7ffffeu, 0x5a1e0b04u, 0x80000000u, 0xcc200010u,
    0x041a0b04u, 0xd5080011u, 0x22020b04u, 0xd5400012u, 0x041a0b04u, 0x7e260306u, 0x0c260b04u, 0xe0781000u,
    0x80010a02u, 0xe0781010u, 0x80010e02u, 0xe0741020u, 0x80011202u, 0xbf810000u,
};

constexpr std::uint32_t TinyRows[32][4] = {
    {0x00800001u, 0x3f7ffffeu, 0x00000000u, 0x00000000u},
    {0x80800001u, 0x3f7ffffeu, 0x80000000u, 0x00000000u},
    {0x3f7ffffeu, 0x00800001u, 0x00000000u, 0x00000000u},
    {0x008003e6u, 0x3f7ff834u, 0x00000000u, 0x00000000u},
    {0x00f80000u, 0x3f042108u, 0x80000000u, 0x00000000u},
    {0x80f80000u, 0x3f042108u, 0x00000000u, 0x00000000u},
    {0x00800f95u, 0x3f7fe0d9u, 0x00000000u, 0x00000000u},
    {0x00800001u, 0x3f7ffffdu, 0x00000000u, 0x00000000u},
    {0x00ffffffu, 0x3f000000u, 0x00000000u, 0x00000000u},
    {0x008005a8u, 0x3f7ff4b0u, 0x00000000u, 0x00000000u},
    {0x00f7ffffu, 0x3f042108u, 0x00000000u, 0x00000000u},
    {0x00ffffffu, 0x3effffffu, 0x00000000u, 0x00000000u},
    {0x008005a9u, 0x3f7ff4aeu, 0x00000000u, 0x00000000u},
    {0x00800001u, 0x3f7ffffeu, 0x00000001u, 0x00000000u},
    {0x007fffffu, 0x3f800001u, 0x00000000u, 0x00000000u},
    {0x00800000u, 0x3f7fffffu, 0x00000000u, 0x00000000u},
    {0x80800001u, 0x3f7ffffeu, 0x80000000u, 0x00000000u},
    {0x00800001u, 0x3f7ffffeu, 0x00000000u, 0x00000000u},
    {0xbf7ffffeu, 0x00800001u, 0x80000000u, 0x00000000u},
    {0x808003e6u, 0x3f7ff834u, 0x80000000u, 0x00000000u},
    {0x80f80000u, 0x3f042108u, 0x00000000u, 0x00000000u},
    {0x00f80000u, 0x3f042108u, 0x80000000u, 0x00000000u},
    {0x80800f95u, 0x3f7fe0d9u, 0x80000000u, 0x00000000u},
    {0x80800001u, 0x3f7ffffdu, 0x80000000u, 0x00000000u},
    {0x80ffffffu, 0x3f000000u, 0x80000000u, 0x00000000u},
    {0x808005a8u, 0x3f7ff4b0u, 0x80000000u, 0x00000000u},
    {0x80f7ffffu, 0x3f042108u, 0x80000000u, 0x00000000u},
    {0x80ffffffu, 0x3effffffu, 0x80000000u, 0x00000000u},
    {0x808005a9u, 0x3f7ff4aeu, 0x80000000u, 0x00000000u},
    {0x80800001u, 0x3f7ffffeu, 0x80000001u, 0x00000000u},
    {0x807fffffu, 0x3f800001u, 0x80000000u, 0x00000000u},
    {0x80800000u, 0x3f7fffffu, 0x80000000u, 0x00000000u}
};
constexpr std::uint32_t TinyExpected[32][8] = {
    {0x00800000u, 0x00800000u, 0x00800000u, 0x00800000u, 0x00800000u, 0x00800000u, 0x00800000u, 0x80800000u},
    {0x80800000u, 0x80800000u, 0x80800000u, 0x80800000u, 0x80800000u, 0x80800000u, 0x80800000u, 0x00800000u},
    {0x00800000u, 0x00800000u, 0x00800000u, 0x00800000u, 0x3f7ffffcu, 0x00800000u, 0x00800000u, 0x80800000u},
    {0x00800000u, 0x00800000u, 0x00800000u, 0x00800000u, 0x008003e5u, 0x00800000u, 0x00800000u, 0x80800000u},
    {0x00800000u, 0x00800000u, 0x00800000u, 0x00800000u, 0x00f7fffeu, 0x00800000u, 0x00800000u, 0x80800000u},
    {0x80800000u, 0x80800000u, 0x80800000u, 0x80800000u, 0x80f7fffeu, 0x80800000u, 0x80800000u, 0x00800000u},
    {0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00800f94u, 0x00000000u, 0x00000000u, 0x80000000u},
    {0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00800000u, 0x00000000u, 0x00000000u, 0x80000000u},
    {0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00fffffdu, 0x00000000u, 0x00000000u, 0x80000000u},
    {0x00800000u, 0x00800000u, 0x00800000u, 0x00800000u, 0x008005a7u, 0x00800000u, 0x00800000u, 0x80800000u},
    {0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00f7fffdu, 0x00000000u, 0x00000000u, 0x80000000u},
    {0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00fffffdu, 0x00000000u, 0x00000000u, 0x80000000u},
    {0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x008005a8u, 0x00000000u, 0x00000000u, 0x80000000u},
    {0x00800000u, 0x00800000u, 0x00800000u, 0x00800000u, 0x00800000u, 0x00800000u, 0x00800000u, 0x80800000u},
    {0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x80000000u},
    {0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x80000000u},
    {0x80800000u, 0x80800000u, 0x80800000u, 0x80800000u, 0x80800000u, 0x80800000u, 0x80800000u, 0x00800000u},
    {0x00800000u, 0x00800000u, 0x00800000u, 0x00800000u, 0x00800000u, 0x00800000u, 0x00800000u, 0x80800000u},
    {0x80800000u, 0x80800000u, 0x80800000u, 0x80800000u, 0xbf7ffffcu, 0x80800000u, 0x80800000u, 0x00800000u},
    {0x80800000u, 0x80800000u, 0x80800000u, 0x80800000u, 0x808003e5u, 0x80800000u, 0x80800000u, 0x00800000u},
    {0x80800000u, 0x80800000u, 0x80800000u, 0x80800000u, 0x80f7fffeu, 0x80800000u, 0x80800000u, 0x00800000u},
    {0x00800000u, 0x00800000u, 0x00800000u, 0x00800000u, 0x00f7fffeu, 0x00800000u, 0x00800000u, 0x80800000u},
    {0x80000000u, 0x80000000u, 0x80000000u, 0x80000000u, 0x80800f94u, 0x80000000u, 0x80000000u, 0x00000000u},
    {0x80000000u, 0x80000000u, 0x80000000u, 0x80000000u, 0x80800000u, 0x80000000u, 0x80000000u, 0x00000000u},
    {0x80000000u, 0x80000000u, 0x80000000u, 0x80000000u, 0x80fffffdu, 0x80000000u, 0x80000000u, 0x00000000u},
    {0x80800000u, 0x80800000u, 0x80800000u, 0x80800000u, 0x808005a7u, 0x80800000u, 0x80800000u, 0x00800000u},
    {0x80000000u, 0x80000000u, 0x80000000u, 0x80000000u, 0x80f7fffdu, 0x80000000u, 0x80000000u, 0x00000000u},
    {0x80000000u, 0x80000000u, 0x80000000u, 0x80000000u, 0x80fffffdu, 0x80000000u, 0x80000000u, 0x00000000u},
    {0x80000000u, 0x80000000u, 0x80000000u, 0x80000000u, 0x808005a8u, 0x80000000u, 0x80000000u, 0x00000000u},
    {0x80800000u, 0x80800000u, 0x80800000u, 0x80800000u, 0x80800000u, 0x80800000u, 0x80800000u, 0x00800000u},
    {0x80000000u, 0x00000000u, 0x80000000u, 0x80000000u, 0x80000000u, 0x80000000u, 0x80000000u, 0x00000000u},
    {0x80000000u, 0x80000000u, 0x80000000u, 0x80000000u, 0x80000000u, 0x80000000u, 0x80000000u, 0x00000000u}
};
constexpr std::uint32_t TinyDirected[3][32][2] = {
    {
        {0x00800000u, 0x80000000u}, {0x80000000u, 0x00800000u}, {0x00800000u, 0x80000000u}, {0x00800000u, 0x80000000u},
        {0x00800000u, 0x80000000u}, {0x80000000u, 0x00800000u}, {0x00800000u, 0x80000000u}, {0x00000000u, 0x80000000u},
        {0x00000000u, 0x80000000u}, {0x00800000u, 0x80000000u}, {0x00000000u, 0x80000000u}, {0x00000000u, 0x80000000u},
        {0x00800000u, 0x80000000u}, {0x00800000u, 0x80000000u}, {0x00000000u, 0x80000000u}, {0x00000000u, 0x80000000u},
        {0x80000000u, 0x00800000u}, {0x00800000u, 0x80000000u}, {0x80000000u, 0x00800000u}, {0x80000000u, 0x00800000u},
        {0x80000000u, 0x00800000u}, {0x00800000u, 0x80000000u}, {0x80000000u, 0x00800000u}, {0x80000000u, 0x00000000u},
        {0x80000000u, 0x00000000u}, {0x80000000u, 0x00800000u}, {0x80000000u, 0x00000000u}, {0x80000000u, 0x00000000u},
        {0x80000000u, 0x00800000u}, {0x80000000u, 0x00800000u}, {0x80000000u, 0x00000000u}, {0x80000000u, 0x00000000u}
    },
    {
        {0x00000000u, 0x80800000u}, {0x80800000u, 0x00000000u}, {0x00000000u, 0x80800000u}, {0x00000000u, 0x80800000u},
        {0x00000000u, 0x80800000u}, {0x80800000u, 0x00000000u}, {0x00000000u, 0x80800000u}, {0x00000000u, 0x80000000u},
        {0x00000000u, 0x80000000u}, {0x00000000u, 0x80800000u}, {0x00000000u, 0x80000000u}, {0x00000000u, 0x80000000u},
        {0x00000000u, 0x80800000u}, {0x00000000u, 0x80800000u}, {0x00000000u, 0x80000000u}, {0x00000000u, 0x80000000u},
        {0x80800000u, 0x00000000u}, {0x00000000u, 0x80800000u}, {0x80800000u, 0x00000000u}, {0x80800000u, 0x00000000u},
        {0x80800000u, 0x00000000u}, {0x00000000u, 0x80800000u}, {0x80800000u, 0x00000000u}, {0x80000000u, 0x00000000u},
        {0x80000000u, 0x00000000u}, {0x80800000u, 0x00000000u}, {0x80000000u, 0x00000000u}, {0x80000000u, 0x00000000u},
        {0x80800000u, 0x00000000u}, {0x80800000u, 0x00000000u}, {0x80000000u, 0x00000000u}, {0x80000000u, 0x00000000u}
    },
    {
        {0x00000000u, 0x80000000u}, {0x80000000u, 0x00000000u}, {0x00000000u, 0x80000000u}, {0x00000000u, 0x80000000u},
        {0x00000000u, 0x80000000u}, {0x80000000u, 0x00000000u}, {0x00000000u, 0x80000000u}, {0x00000000u, 0x80000000u},
        {0x00000000u, 0x80000000u}, {0x00000000u, 0x80000000u}, {0x00000000u, 0x80000000u}, {0x00000000u, 0x80000000u},
        {0x00000000u, 0x80000000u}, {0x00000000u, 0x80000000u}, {0x00000000u, 0x80000000u}, {0x00000000u, 0x80000000u},
        {0x80000000u, 0x00000000u}, {0x00000000u, 0x80000000u}, {0x80000000u, 0x00000000u}, {0x80000000u, 0x00000000u},
        {0x80000000u, 0x00000000u}, {0x00000000u, 0x80000000u}, {0x80000000u, 0x00000000u}, {0x80000000u, 0x00000000u},
        {0x80000000u, 0x00000000u}, {0x80000000u, 0x00000000u}, {0x80000000u, 0x00000000u}, {0x80000000u, 0x00000000u},
        {0x80000000u, 0x00000000u}, {0x80000000u, 0x00000000u}, {0x80000000u, 0x00000000u}, {0x80000000u, 0x00000000u}
    }
};
constexpr std::uint32_t TinyLegacyExpected[32] = {
    0x00000000u, 0x80000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u,
    0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u,
    0x80000000u, 0x00000000u, 0x80000000u, 0x80000000u, 0x00000000u, 0x00000000u, 0x80000000u, 0x80000000u,
    0x80000000u, 0x80000000u, 0x80000000u, 0x80000000u, 0x80000000u, 0x80000000u, 0x00000000u, 0x80000000u
};
constexpr const char* TinyLegacyNames[2] = {
    "v_mad_legacy_f32 v18, v4, v5, v6",
    "v_mac_legacy_f32 v19, v4, v5"
};
constexpr const char* TinyNames[8] = {
    "v_mul_f32 v10, v4, v5",
    "v_mul_legacy_f32 v11, v4, v5",
    "v_fma_f32 v12, v4, v5, v6",
    "v_fmac_f32 v13, v4, v5",
    "v_fmamk_f32 v14, v4, 0x3f7ffffe, v6",
    "v_fmaak_f32 v15, v4, v5, 0x80000000",
    "v_fma_mix_f32 v16, v4, v5, v6",
    "v_mul_f32 v17, -v4, v5"
};

alignas(256) constexpr std::array<std::uint32_t, 28> MixedTinyCode{
    0x34020084u, 0x34040086u, 0xe0381000u, 0x80000401u, 0xbf8c3f70u, 0xcc20000au, 0x141a0b04u, 0xcc20000bu,
    0x0c1a0905u, 0xcc20100cu, 0x141a0b04u, 0xcc20080du, 0x0c1a0905u, 0xcc20400eu, 0x141e0b04u, 0xcc20400fu,
    0x0c1e0905u, 0x7e1002ffu, 0x3f7ffffeu, 0xcc204010u, 0x041e1104u, 0xcc206011u, 0x041e1104u, 0xe0781000u,
    0x80010a02u, 0xe0781010u, 0x80010e02u, 0xbf810000u,
};

constexpr std::uint32_t MixedTinyRows[Threads][Inputs] = {
    {0x00801002u, 0x00003bffu, 0x00000000u, 0x00000000u},
    {0x80801002u, 0x00003bffu, 0x80000000u, 0x80008000u},
    {0x00801000u, 0x3bff3bffu, 0x00000000u, 0x00000000u},
    {0x00801001u, 0x3bff3bffu, 0x00000000u, 0x00000000u},
    {0x00801002u, 0x3bff3bffu, 0x00000000u, 0x00000000u},
    {0x00801003u, 0x3bff3bffu, 0x00000000u, 0x00000000u},
    {0x80801000u, 0x3bff3bffu, 0x80000000u, 0x80008000u},
    {0x80801001u, 0x3bff3bffu, 0x80000000u, 0x80008000u},
    {0x80801002u, 0x3bff3bffu, 0x80000000u, 0x80008000u},
    {0x80801003u, 0x3bff3bffu, 0x80000000u, 0x80008000u},
    {0x00801000u, 0xbbffbbffu, 0x80000000u, 0x80008000u},
    {0x00801001u, 0xbbffbbffu, 0x80000000u, 0x80008000u},
    {0x00801002u, 0xbbffbbffu, 0x80000000u, 0x80008000u},
    {0x00801003u, 0xbbffbbffu, 0x80000000u, 0x80008000u},
    {0x00800001u, 0x3bff3bffu, 0x00000000u, 0x00000000u},
    {0x80800001u, 0x3bff3bffu, 0x80000000u, 0x80008000u},
};
constexpr std::uint32_t MixedTinyExpected[16][3] = {
    {0x00800000u, 0x00000000u, 0x00801001u},
    {0x80800000u, 0x80000000u, 0x80801001u},
    {0x00000000u, 0x00000000u, 0x00800fffu},
    {0x00000000u, 0x00000000u, 0x00801000u},
    {0x00800000u, 0x00800000u, 0x00801001u},
    {0x00800001u, 0x00800001u, 0x00801002u},
    {0x80000000u, 0x80000000u, 0x80800fffu},
    {0x80000000u, 0x80000000u, 0x80801000u},
    {0x80800000u, 0x80800000u, 0x80801001u},
    {0x80800001u, 0x80800001u, 0x80801002u},
    {0x80000000u, 0x80000000u, 0x00800fffu},
    {0x80000000u, 0x80000000u, 0x00801000u},
    {0x80800000u, 0x80800000u, 0x00801001u},
    {0x80800001u, 0x80800001u, 0x00801002u},
    {0x00000000u, 0x00000000u, 0x00800000u},
    {0x80000000u, 0x80000000u, 0x80800000u},
};
constexpr std::uint32_t MixedTinyDirected[3][16][2] = {
    {
        {0x00800000u, 0x00000000u},
        {0x80000000u, 0x80000000u},
        {0x00000000u, 0x00000000u},
        {0x00000000u, 0x00000000u},
        {0x00800000u, 0x00800000u},
        {0x00800001u, 0x00800001u},
        {0x80000000u, 0x80000000u},
        {0x80000000u, 0x80000000u},
        {0x80000000u, 0x80000000u},
        {0x80800000u, 0x80800000u},
        {0x80000000u, 0x80000000u},
        {0x80000000u, 0x80000000u},
        {0x80000000u, 0x80000000u},
        {0x80800000u, 0x80800000u},
        {0x00000000u, 0x00000000u},
        {0x80000000u, 0x80000000u},
    },
    {
        {0x00000000u, 0x00000000u},
        {0x80800000u, 0x80000000u},
        {0x00000000u, 0x00000000u},
        {0x00000000u, 0x00000000u},
        {0x00000000u, 0x00000000u},
        {0x00800000u, 0x00800000u},
        {0x80000000u, 0x80000000u},
        {0x80000000u, 0x80000000u},
        {0x80800000u, 0x80800000u},
        {0x80800001u, 0x80800001u},
        {0x80000000u, 0x80000000u},
        {0x80000000u, 0x80000000u},
        {0x80800000u, 0x80800000u},
        {0x80800001u, 0x80800001u},
        {0x00000000u, 0x00000000u},
        {0x80000000u, 0x80000000u},
    },
    {
        {0x00000000u, 0x00000000u},
        {0x80000000u, 0x80000000u},
        {0x00000000u, 0x00000000u},
        {0x00000000u, 0x00000000u},
        {0x00000000u, 0x00000000u},
        {0x00800000u, 0x00800000u},
        {0x80000000u, 0x80000000u},
        {0x80000000u, 0x80000000u},
        {0x80000000u, 0x80000000u},
        {0x80800000u, 0x80800000u},
        {0x80000000u, 0x80000000u},
        {0x80000000u, 0x80000000u},
        {0x80000000u, 0x80000000u},
        {0x80800000u, 0x80800000u},
        {0x00000000u, 0x00000000u},
        {0x80000000u, 0x80000000u},
    },
};
constexpr std::uint32_t MixedAddendDirected[3][2] = {
    {0x00800000u, 0x80000000u},
    {0x00000000u, 0x80800000u},
    {0x00000000u, 0x80000000u},
};
constexpr const char* MixedTinyNames[8] = {
    "v_fma_mix_f32 v10, v4, v5, v6 op_sel_hi:[0,1,0]",
    "v_fma_mix_f32 v11, v5, v4, v6 op_sel_hi:[1,0,0]",
    "v_fma_mix_f32 v12, v4, v5, v6 op_sel:[0,1,0] op_sel_hi:[0,1,0]",
    "v_fma_mix_f32 v13, v5, v4, v6 op_sel:[1,0,0] op_sel_hi:[1,0,0]",
    "v_fma_mix_f32 v14, v4, v5, v7 op_sel_hi:[0,1,1]",
    "v_fma_mix_f32 v15, v5, v4, v7 op_sel_hi:[1,0,1]",
    "v_fma_mix_f32 v16, v4, v8, v7 op_sel_hi:[0,0,1]",
    "v_fma_mix_f32 v17, v4, v8, v7 op_sel:[0,0,1] op_sel_hi:[0,0,1]",
};

std::array<std::uint32_t, 4> BufferDescriptor(const void* data, std::uint32_t bytes) {
    const auto address = reinterpret_cast<std::uintptr_t>(data);
    return {static_cast<std::uint32_t>(address), static_cast<std::uint32_t>((address >> 32u) & 0xffffu), bytes, 0x31016facu};
}

std::string Hex(std::uint32_t value) {
    char text[16];
    std::snprintf(text, sizeof(text), "0x%08x", value);
    return text;
}

void Expect(std::uint32_t tid, std::uint32_t actual, std::uint32_t expected, const char* name) {
    Require(actual == expected, std::string("f32 denormal flush: lane ") + std::to_string(tid) + " " + name + " is " + Hex(actual) + ", expected " + Hex(expected));
}

bool IsNan(std::uint32_t value) {
    return (value & 0x7fffffffu) > 0x7f800000u;
}

void Run(AgcDriver::VulkanDevice& device, const std::optional<ShaderRecompiler::ShaderFloatMode>& floatMode, std::span<const std::uint32_t> code = Code, const std::uint32_t (*rows)[Inputs] = Rows) {
    for (std::uint32_t tid = 0; tid < Threads; ++tid) std::copy(std::begin(rows[tid]), std::end(rows[tid]), &Input[tid * Inputs]);
    Output.fill(0xdeadbeefu);
    std::vector<std::uint32_t> userData(8, 0u);
    const auto input = BufferDescriptor(Input.data(), static_cast<std::uint32_t>(Input.size() * 4u));
    const auto output = BufferDescriptor(Output.data(), static_cast<std::uint32_t>(Output.size() * 4u));
    std::copy(input.begin(), input.end(), userData.begin());
    std::copy(output.begin(), output.end(), userData.begin() + 4);
    const std::array<ShaderRecompiler::MemoryRegion, 1> memory{{{reinterpret_cast<std::uintptr_t>(code.data()), std::as_bytes(code)}}};
    const ShaderRecompiler::ShaderComputeStageInfo compute{{Threads, 1, 1}, 0u, {false, false, false}, false, 1};
    ShaderRecompiler::RecompileRequest request{
        {ShaderStage::Compute, reinterpret_cast<std::uintptr_t>(code.data()), code, 0, {}},
        {32, 0, userData, compute, std::nullopt, std::nullopt, memory},
        device.Target(),
        {0, 0, 0, 128}
    };
    request.useCache = false;
    request.context.floatMode = floatMode;
    const auto result = ShaderRecompiler::Recompile(request);
    device.Dispatch(result, 1, 1, 1, {}, reinterpret_cast<std::uintptr_t>(code.data()));
    device.WaitIdle();
}

void Check(const char* mode) {
    for (std::uint32_t tid = 0; tid < Threads; ++tid) {
        const std::uint32_t* out = &Output[tid * Results];
        for (std::uint32_t i = 0; i < Results; ++i) {
            const bool floatResult = i < 12u || i >= 15u;
            if (floatResult && IsNan(Expected[tid][i])) {
                Require(IsNan(out[i]), std::string("f32 denormal flush: lane ") + std::to_string(tid) + " " + mode + " " + Names[i] + " is " + Hex(out[i]) + ", expected a NaN");
                continue;
            }
            Expect(tid, out[i], Expected[tid][i], (std::string(mode) + " " + Names[i]).c_str());
        }
    }
}

void CheckTiny(const char* mode) {
    for (std::uint32_t tid = 0; tid < Threads; ++tid) {
        for (std::uint32_t i = 0; i < 8; ++i) Expect(tid, Output[tid * TinyResults + i], TinyExpected[tid][i], (std::string(mode) + " " + TinyNames[i]).c_str());
    }
}

void CheckMixedTiny(const std::string& mode, std::uint32_t round = 0u) {
    for (std::uint32_t tid = 0; tid < std::size(MixedTinyExpected); ++tid) {
        for (std::uint32_t i = 0; i < std::size(MixedTinyNames); ++i) {
            const bool highFactor = i == 2u || i == 3u;
            std::uint32_t expected = MixedTinyExpected[tid][i >= 6u ? 2u : highFactor ? 1u : 0u];
            if (round != 0u) {
                if (i >= 6u) {
                    if (tid < 14u) continue;
                    expected = MixedAddendDirected[round - 1u][tid - 14u];
                } else {
                    if ((MixedTinyRows[tid][0] & 0x7fffffffu) == 0x00801003u) continue;
                    expected = MixedTinyDirected[round - 1u][tid][highFactor ? 1u : 0u];
                }
            }
            Expect(tid, Output[tid * TinyResults + i], expected, (mode + " " + MixedTinyNames[i]).c_str());
        }
    }
}

void CheckTinyLegacy(const std::string& mode) {
    for (std::uint32_t tid = 0; tid < Threads; ++tid) {
        for (std::uint32_t i = 0; i < 2; ++i) Expect(tid, Output[tid * TinyResults + 8u + i], TinyLegacyExpected[tid], (mode + " " + TinyLegacyNames[i]).c_str());
    }
}

void CheckTinyDirected(std::uint32_t round) {
    const std::string mode = "round mode " + std::to_string(round) + " ";
    for (std::uint32_t tid = 0; tid < Threads; ++tid) {
        Expect(tid, Output[tid * TinyResults], TinyDirected[round - 1u][tid][0], (mode + TinyNames[0]).c_str());
        Expect(tid, Output[tid * TinyResults + 7u], TinyDirected[round - 1u][tid][1], (mode + TinyNames[7]).c_str());
    }
}

}

int main() {
    try {
        const auto device = OpenVulkanTestDevice();
        if (!device) return VulkanTestSkipped;
        Run(*device, std::nullopt);
        Check("no float mode");
        Run(*device, ShaderRecompiler::ShaderFloatMode{0xc0u, true, false, false});
        Check("IEEE=0 f32 denormals flushed");
        Run(*device, ShaderRecompiler::ShaderFloatMode{0x00u, false, true, false});
        Check("IEEE=1 all denormals flushed");
        Run(*device, std::nullopt, MixedTinyCode, MixedTinyRows);
        CheckMixedTiny("no float mode");
        Run(*device, ShaderRecompiler::ShaderFloatMode{0xc0u, true, false, false}, MixedTinyCode, MixedTinyRows);
        CheckMixedTiny("IEEE=0 mixed sources");
        Run(*device, ShaderRecompiler::ShaderFloatMode{0x00u, false, true, false}, MixedTinyCode, MixedTinyRows);
        CheckMixedTiny("IEEE=1 mixed sources");
        for (std::uint32_t round = 1u; round < 4u; ++round) {
            Run(*device, ShaderRecompiler::ShaderFloatMode{0xc0u | round, true, false, false}, MixedTinyCode, MixedTinyRows);
            CheckMixedTiny("mixed sources round mode " + std::to_string(round), round);
        }
        Run(*device, std::nullopt, TinyCode, TinyRows);
        CheckTinyLegacy("no float mode");
        CheckTiny("no float mode");
        Run(*device, ShaderRecompiler::ShaderFloatMode{0xc0u, true, false, false}, TinyCode, TinyRows);
        CheckTinyLegacy("IEEE=0 f32 denormals flushed");
        CheckTiny("IEEE=0 f32 denormals flushed");
        Run(*device, ShaderRecompiler::ShaderFloatMode{0x00u, false, true, false}, TinyCode, TinyRows);
        CheckTinyLegacy("IEEE=1 all denormals flushed");
        CheckTiny("IEEE=1 all denormals flushed");
        for (std::uint32_t round = 1u; round < 4u; ++round) {
            Run(*device, ShaderRecompiler::ShaderFloatMode{0xc0u | round, true, false, false}, TinyCode, TinyRows);
            CheckTinyLegacy("round mode " + std::to_string(round));
            CheckTinyDirected(round);
        }
        for (const std::uint32_t mode : {0xd0u, 0xe0u}) {
            bool refused = false;
            try {
                Run(*device, ShaderRecompiler::ShaderFloatMode{mode, true, false, false});
            } catch (const std::runtime_error& error) {
                refused = std::string(error.what()).find("f32 denormal mode") != std::string::npos;
            }
            Require(refused, "f32 denormal flush: FLOAT_MODE " + Hex(mode) + " was not refused");
        }
        std::puts("f32 denormal flush tests passed");
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
