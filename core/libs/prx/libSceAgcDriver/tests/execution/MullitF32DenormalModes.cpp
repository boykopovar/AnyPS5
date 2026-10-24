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

constexpr std::uint32_t Threads = 64;
constexpr std::uint32_t Inputs = 4;
constexpr std::uint32_t ConstantForms = 4;
alignas(256) std::array<std::uint32_t, Threads * Inputs> Input{};
alignas(256) std::array<std::uint32_t, Threads * ConstantForms> Output{};

alignas(256) constexpr std::array<std::uint32_t, 11> MullitCode{
    0x34020084u, 0x34040082u, 0xe0381000u, 0x80000401u, 0xbf8c3f70u, 0x7e140280u, 0xd550000au, 0x041a0b04u,
    0xe0701000u, 0x80010a02u, 0xbf810000u,
};

alignas(256) constexpr std::array<std::uint32_t, 17> ConstantMullitCode{
    0x34020084u, 0x34040084u, 0xe0381000u, 0x80000401u, 0xbf8c3f70u, 0xd550000au, 0x041a08f2u, 0xd550000bu,
    0x0419e904u, 0xd550000cu, 0x041a0a81u, 0xd550000du, 0x041a08ffu, 0x3fc00000u, 0xe0781000u, 0x80010a02u,
    0xbf810000u,
};

constexpr const char* ConstantNames[ConstantForms] = {"v_mullit_f32 1.0, v4, v6", "v_mullit_f32 v4, 2.0, v6", "v_mullit_f32 0x1, v5, v6", "v_mullit_f32 0x3fc00000, v4, v6"};

struct Vector {
    std::uint32_t lhs;
    std::uint32_t rhs;
    std::uint32_t limit;
    std::uint32_t expected[8];
};

constexpr Vector Vectors[] = {
    {0x00000000u, 0x00000000u, 0x00000001u, {0xff7fffffu, 0xff7fffffu, 0x00000000u, 0x00000000u, 0xff7fffffu, 0xff7fffffu, 0x00000000u, 0x00000000u}},
    {0x00000000u, 0x00800000u, 0x00000001u, {0xff7fffffu, 0xff7fffffu, 0x00000000u, 0x00000000u, 0xff7fffffu, 0xff7fffffu, 0x00000000u, 0x00000000u}},
    {0x00000001u, 0x80000001u, 0x00800000u, {0x00000000u, 0x00000000u, 0x80000000u, 0x80000000u, 0x00000000u, 0x00000000u, 0x80000000u, 0x80000000u}},
    {0x00000001u, 0x3f800000u, 0x00000001u, {0xff7fffffu, 0xff7fffffu, 0x00000000u, 0x00000000u, 0xff7fffffu, 0xff7fffffu, 0x00000001u, 0x00000001u}},
    {0x00000001u, 0x3f800000u, 0x00800000u, {0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000001u, 0x00000001u}},
    {0x00000001u, 0xbf800000u, 0x00800000u, {0x00000000u, 0x00000000u, 0x80000000u, 0x80000000u, 0x00000000u, 0x00000000u, 0x80000001u, 0x80000001u}},
    {0x00000001u, 0x7f7fffffu, 0x00800000u, {0x00000000u, 0x00000000u, 0x34ffffffu, 0x34ffffffu, 0x00000000u, 0x00000000u, 0x34ffffffu, 0x34ffffffu}},
    {0x00800000u, 0x00000000u, 0x00000001u, {0xff7fffffu, 0xff7fffffu, 0x00000000u, 0x00000000u, 0xff7fffffu, 0xff7fffffu, 0x00000000u, 0x00000000u}},
    {0x00800000u, 0x80000001u, 0x00800000u, {0x00000000u, 0x00000000u, 0x80000000u, 0x80000000u, 0x00000000u, 0x00000000u, 0x80000000u, 0x80000000u}},
    {0x00800000u, 0x00800000u, 0x00000001u, {0xff7fffffu, 0xff7fffffu, 0x00000000u, 0x00000000u, 0xff7fffffu, 0xff7fffffu, 0x00000000u, 0x00000000u}},
    {0x00800000u, 0x3f000000u, 0x00000001u, {0xff7fffffu, 0xff7fffffu, 0x00000000u, 0x00000000u, 0xff7fffffu, 0xff7fffffu, 0x00400000u, 0x00400000u}},
    {0x00800000u, 0x3f000000u, 0x00800000u, {0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00400000u, 0x00400000u, 0x00400000u, 0x00400000u}},
    {0x3f000000u, 0x007fffffu, 0x00000001u, {0xff7fffffu, 0xff7fffffu, 0x00000000u, 0x00000000u, 0xff7fffffu, 0xff7fffffu, 0x00400000u, 0x00400000u}},
    {0x3f000000u, 0x007fffffu, 0x00800000u, {0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00400000u, 0x00400000u}},
    {0x3f000000u, 0x807fffffu, 0x00800000u, {0x00000000u, 0x00000000u, 0x80000000u, 0x80000000u, 0x00000000u, 0x00000000u, 0x80400000u, 0x80400000u}},
    {0x7f800001u, 0x00000001u, 0x00000001u, {0xff7fffffu, 0xff7fffffu, 0x7f800001u, 0x7fc00001u, 0xff7fffffu, 0xff7fffffu, 0x7f800001u, 0x7fc00001u}},
    {0x7f800001u, 0x00000001u, 0x00800000u, {0x00000000u, 0x00000000u, 0x7f800001u, 0x7fc00001u, 0x00000000u, 0x00000000u, 0x7f800001u, 0x7fc00001u}},
    {0x7f800001u, 0x00800000u, 0x00000001u, {0xff7fffffu, 0xff7fffffu, 0x7f800001u, 0x7fc00001u, 0xff7fffffu, 0xff7fffffu, 0x7f800001u, 0x7fc00001u}},
    {0x7f800001u, 0x00800000u, 0x00800000u, {0x7f800001u, 0x7fc00001u, 0x7f800001u, 0x7fc00001u, 0x7f800001u, 0x7fc00001u, 0x7f800001u, 0x7fc00001u}},
    {0x3f800000u, 0x40000000u, 0x3f800000u, {0x40000000u, 0x40000000u, 0x40000000u, 0x40000000u, 0x40000000u, 0x40000000u, 0x40000000u, 0x40000000u}},
    {0x3f800000u, 0xff7fffffu, 0x3f800000u, {0xff7fffffu, 0xff7fffffu, 0xff7fffffu, 0xff7fffffu, 0xff7fffffu, 0xff7fffffu, 0xff7fffffu, 0xff7fffffu}},
    {0x3f800000u, 0x3f800000u, 0x80000000u, {0xff7fffffu, 0xff7fffffu, 0xff7fffffu, 0xff7fffffu, 0xff7fffffu, 0xff7fffffu, 0xff7fffffu, 0xff7fffffu}},
    {0x7f7fffffu, 0x7f7fffffu, 0x3f800000u, {0x7f800000u, 0x7f800000u, 0x7f800000u, 0x7f800000u, 0x7f800000u, 0x7f800000u, 0x7f800000u, 0x7f800000u}},
};

struct ConstantVector {
    std::uint32_t inputs[3];
    std::uint32_t expected[4][ConstantForms];
};

constexpr ConstantVector ConstantVectors[] = {
    {{0x00000001u, 0x00000001u, 0x3f800000u}, {{0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u}, {0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u}, {0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u}, {0x00000001u, 0x00000002u, 0x00000000u, 0x00000002u}}},
    {{0x00000001u, 0x00800001u, 0x3f800000u}, {{0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u}, {0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u}, {0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u}, {0x00000001u, 0x00000002u, 0x00000000u, 0x00000002u}}},
    {{0x00000002u, 0x00000001u, 0x3f800000u}, {{0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u}, {0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u}, {0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u}, {0x00000002u, 0x00000004u, 0x00000000u, 0x00000003u}}},
    {{0x00000002u, 0x00800001u, 0x3f800000u}, {{0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u}, {0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u}, {0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u}, {0x00000002u, 0x00000004u, 0x00000000u, 0x00000003u}}},
    {{0x00000003u, 0x00000001u, 0x3f800000u}, {{0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u}, {0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u}, {0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u}, {0x00000003u, 0x00000006u, 0x00000000u, 0x00000004u}}},
    {{0x00000003u, 0x00800001u, 0x3f800000u}, {{0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u}, {0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u}, {0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u}, {0x00000003u, 0x00000006u, 0x00000000u, 0x00000004u}}},
    {{0x00155555u, 0x00000001u, 0x3f800000u}, {{0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u}, {0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u}, {0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u}, {0x00155555u, 0x002aaaaau, 0x00000000u, 0x00200000u}}},
    {{0x00155555u, 0x00800001u, 0x3f800000u}, {{0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u}, {0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u}, {0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u}, {0x00155555u, 0x002aaaaau, 0x00000000u, 0x00200000u}}},
    {{0x003fffffu, 0x00000001u, 0x3f800000u}, {{0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u}, {0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u}, {0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u}, {0x003fffffu, 0x007ffffeu, 0x00000000u, 0x005ffffeu}}},
    {{0x003fffffu, 0x00800001u, 0x3f800000u}, {{0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u}, {0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u}, {0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u}, {0x003fffffu, 0x007ffffeu, 0x00000000u, 0x005ffffeu}}},
    {{0x00400000u, 0x00000001u, 0x3f800000u}, {{0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u}, {0x00000000u, 0x00800000u, 0x00000000u, 0x00000000u}, {0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u}, {0x00400000u, 0x00800000u, 0x00000000u, 0x00600000u}}},
    {{0x00400000u, 0x00800001u, 0x3f800000u}, {{0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u}, {0x00000000u, 0x00800000u, 0x00000000u, 0x00000000u}, {0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u}, {0x00400000u, 0x00800000u, 0x00000000u, 0x00600000u}}},
    {{0x00400001u, 0x00000001u, 0x3f800000u}, {{0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u}, {0x00000000u, 0x00800002u, 0x00000000u, 0x00000000u}, {0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u}, {0x00400001u, 0x00800002u, 0x00000000u, 0x00600002u}}},
    {{0x00400001u, 0x00800001u, 0x3f800000u}, {{0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u}, {0x00000000u, 0x00800002u, 0x00000000u, 0x00000000u}, {0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u}, {0x00400001u, 0x00800002u, 0x00000000u, 0x00600002u}}},
    {{0x00555555u, 0x00000001u, 0x3f800000u}, {{0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u}, {0x00000000u, 0x00aaaaaau, 0x00000000u, 0x00000000u}, {0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u}, {0x00555555u, 0x00aaaaaau, 0x00000000u, 0x00800000u}}},
    {{0x00555555u, 0x00800001u, 0x3f800000u}, {{0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u}, {0x00000000u, 0x00aaaaaau, 0x00000000u, 0x00000000u}, {0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u}, {0x00555555u, 0x00aaaaaau, 0x00000000u, 0x00800000u}}},
    {{0x007ffffeu, 0x00000001u, 0x3f800000u}, {{0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u}, {0x00000000u, 0x00fffffcu, 0x00000000u, 0x00bffffdu}, {0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u}, {0x007ffffeu, 0x00fffffcu, 0x00000000u, 0x00bffffdu}}},
    {{0x007ffffeu, 0x00800001u, 0x3f800000u}, {{0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u}, {0x00000000u, 0x00fffffcu, 0x00000000u, 0x00bffffdu}, {0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u}, {0x007ffffeu, 0x00fffffcu, 0x00000000u, 0x00bffffdu}}},
    {{0x007fffffu, 0x00000001u, 0x3f800000u}, {{0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u}, {0x00000000u, 0x00fffffeu, 0x00000000u, 0x00bffffeu}, {0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u}, {0x007fffffu, 0x00fffffeu, 0x00000000u, 0x00bffffeu}}},
    {{0x007fffffu, 0x00800001u, 0x3f800000u}, {{0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u}, {0x00000000u, 0x00fffffeu, 0x00000000u, 0x00bffffeu}, {0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u}, {0x007fffffu, 0x00fffffeu, 0x00000000u, 0x00bffffeu}}},
    {{0x80000001u, 0x00000001u, 0x3f800000u}, {{0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u}, {0x80000000u, 0x80000000u, 0x00000000u, 0x80000000u}, {0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u}, {0x80000001u, 0x80000002u, 0x00000000u, 0x80000002u}}},
    {{0x80000001u, 0x00800001u, 0x3f800000u}, {{0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u}, {0x80000000u, 0x80000000u, 0x00000000u, 0x80000000u}, {0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u}, {0x80000001u, 0x80000002u, 0x00000000u, 0x80000002u}}},
    {{0x80000002u, 0x00000001u, 0x3f800000u}, {{0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u}, {0x80000000u, 0x80000000u, 0x00000000u, 0x80000000u}, {0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u}, {0x80000002u, 0x80000004u, 0x00000000u, 0x80000003u}}},
    {{0x80000002u, 0x00800001u, 0x3f800000u}, {{0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u}, {0x80000000u, 0x80000000u, 0x00000000u, 0x80000000u}, {0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u}, {0x80000002u, 0x80000004u, 0x00000000u, 0x80000003u}}},
    {{0x80000003u, 0x00000001u, 0x3f800000u}, {{0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u}, {0x80000000u, 0x80000000u, 0x00000000u, 0x80000000u}, {0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u}, {0x80000003u, 0x80000006u, 0x00000000u, 0x80000004u}}},
    {{0x80000003u, 0x00800001u, 0x3f800000u}, {{0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u}, {0x80000000u, 0x80000000u, 0x00000000u, 0x80000000u}, {0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u}, {0x80000003u, 0x80000006u, 0x00000000u, 0x80000004u}}},
    {{0x80155555u, 0x00000001u, 0x3f800000u}, {{0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u}, {0x80000000u, 0x80000000u, 0x00000000u, 0x80000000u}, {0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u}, {0x80155555u, 0x802aaaaau, 0x00000000u, 0x80200000u}}},
    {{0x80155555u, 0x00800001u, 0x3f800000u}, {{0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u}, {0x80000000u, 0x80000000u, 0x00000000u, 0x80000000u}, {0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u}, {0x80155555u, 0x802aaaaau, 0x00000000u, 0x80200000u}}},
    {{0x803fffffu, 0x00000001u, 0x3f800000u}, {{0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u}, {0x80000000u, 0x80000000u, 0x00000000u, 0x80000000u}, {0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u}, {0x803fffffu, 0x807ffffeu, 0x00000000u, 0x805ffffeu}}},
    {{0x803fffffu, 0x00800001u, 0x3f800000u}, {{0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u}, {0x80000000u, 0x80000000u, 0x00000000u, 0x80000000u}, {0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u}, {0x803fffffu, 0x807ffffeu, 0x00000000u, 0x805ffffeu}}},
    {{0x80400000u, 0x00000001u, 0x3f800000u}, {{0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u}, {0x80000000u, 0x80800000u, 0x00000000u, 0x80000000u}, {0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u}, {0x80400000u, 0x80800000u, 0x00000000u, 0x80600000u}}},
    {{0x80400000u, 0x00800001u, 0x3f800000u}, {{0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u}, {0x80000000u, 0x80800000u, 0x00000000u, 0x80000000u}, {0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u}, {0x80400000u, 0x80800000u, 0x00000000u, 0x80600000u}}},
    {{0x80400001u, 0x00000001u, 0x3f800000u}, {{0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u}, {0x80000000u, 0x80800002u, 0x00000000u, 0x80000000u}, {0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u}, {0x80400001u, 0x80800002u, 0x00000000u, 0x80600002u}}},
    {{0x80400001u, 0x00800001u, 0x3f800000u}, {{0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u}, {0x80000000u, 0x80800002u, 0x00000000u, 0x80000000u}, {0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u}, {0x80400001u, 0x80800002u, 0x00000000u, 0x80600002u}}},
    {{0x80555555u, 0x00000001u, 0x3f800000u}, {{0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u}, {0x80000000u, 0x80aaaaaau, 0x00000000u, 0x80000000u}, {0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u}, {0x80555555u, 0x80aaaaaau, 0x00000000u, 0x80800000u}}},
    {{0x80555555u, 0x00800001u, 0x3f800000u}, {{0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u}, {0x80000000u, 0x80aaaaaau, 0x00000000u, 0x80000000u}, {0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u}, {0x80555555u, 0x80aaaaaau, 0x00000000u, 0x80800000u}}},
    {{0x807ffffeu, 0x00000001u, 0x3f800000u}, {{0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u}, {0x80000000u, 0x80fffffcu, 0x00000000u, 0x80bffffdu}, {0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u}, {0x807ffffeu, 0x80fffffcu, 0x00000000u, 0x80bffffdu}}},
    {{0x807ffffeu, 0x00800001u, 0x3f800000u}, {{0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u}, {0x80000000u, 0x80fffffcu, 0x00000000u, 0x80bffffdu}, {0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u}, {0x807ffffeu, 0x80fffffcu, 0x00000000u, 0x80bffffdu}}},
    {{0x807fffffu, 0x00000001u, 0x3f800000u}, {{0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u}, {0x80000000u, 0x80fffffeu, 0x00000000u, 0x80bffffeu}, {0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u}, {0x807fffffu, 0x80fffffeu, 0x00000000u, 0x80bffffeu}}},
    {{0x807fffffu, 0x00800001u, 0x3f800000u}, {{0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u}, {0x80000000u, 0x80fffffeu, 0x00000000u, 0x80bffffeu}, {0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u}, {0x807fffffu, 0x80fffffeu, 0x00000000u, 0x80bffffeu}}},
};

static_assert(std::size(Vectors) <= Threads && std::size(ConstantVectors) <= Threads);

std::array<std::uint32_t, 4> BufferDescriptor(const void* data, std::uint32_t bytes) {
    const auto address = reinterpret_cast<std::uintptr_t>(data);
    return {static_cast<std::uint32_t>(address), static_cast<std::uint32_t>((address >> 32u) & 0xffffu), bytes, 0x31016facu};
}

std::string Hex(std::uint32_t value) {
    char text[16];
    std::snprintf(text, sizeof(text), "0x%08x", value);
    return text;
}

bool KeepsDenormals(const AgcDriver::VulkanDevice& device) {
    const auto target = device.Target();
    return target.float32DenormPreserve && !target.float32FlushToZero;
}

bool Run(AgcDriver::VulkanDevice& device, std::span<const std::uint32_t> code, std::uint32_t denorm32, bool ieee) {
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
    request.context.floatMode = ShaderRecompiler::ShaderFloatMode{(denorm32 << 4u) | (3u << 6u), true, ieee, false};
    std::optional<decltype(ShaderRecompiler::Recompile(request))> result;
    try {
        result.emplace(ShaderRecompiler::Recompile(request));
    } catch (const std::exception& error) {
        const std::string message = error.what();
        Require(!KeepsDenormals(device) && (denorm32 == 1u || denorm32 == 2u) && message.find("needs DenormPreserve 32") != std::string::npos,
                "v_mullit_f32 denorm32 " + std::to_string(denorm32) + ": " + message);
        return false;
    }
    device.Dispatch(*result, 1, 1, 1, {}, reinterpret_cast<std::uintptr_t>(code.data()));
    device.WaitIdle();
    return true;
}

void CheckRegisters(std::uint32_t denorm32, bool ieee) {
    for (std::uint32_t row = 0; row < std::size(Vectors); ++row) {
        const auto& vector = Vectors[row];
        const auto actual = Output[row];
        const auto expected = vector.expected[denorm32 * 2u + (ieee ? 1u : 0u)];
        Require(actual == expected, "v_mullit_f32 denorm32 " + std::to_string(denorm32) + (ieee ? " ieee" : " no ieee") + " inputs " + Hex(vector.lhs) + " " + Hex(vector.rhs) + " " +
                                        Hex(vector.limit) + " is " + Hex(actual) + ", expected " + Hex(expected));
    }
}

void CheckConstants(std::uint32_t denorm32, bool ieee) {
    for (std::uint32_t row = 0; row < std::size(ConstantVectors); ++row) {
        const auto& vector = ConstantVectors[row];
        for (std::uint32_t form = 0; form < ConstantForms; ++form) {
            const auto actual = Output[row * ConstantForms + form];
            const auto expected = vector.expected[denorm32][form];
            Require(actual == expected, std::string(ConstantNames[form]) + " denorm32 " + std::to_string(denorm32) + (ieee ? " ieee" : " no ieee") + " inputs " + Hex(vector.inputs[0]) + " " +
                                            Hex(vector.inputs[1]) + " " + Hex(vector.inputs[2]) + " is " + Hex(actual) + ", expected " + Hex(expected));
        }
    }
}

}

int main() {
    try {
        const auto device = OpenVulkanTestDevice();
        if (!device) return VulkanTestSkipped;
        const bool keeps = KeepsDenormals(*device);
        std::uint32_t checked = 0u;
        for (std::uint32_t denorm32 = 0u; denorm32 < 4u; ++denorm32) {
            if (denorm32 == 3u && !keeps) continue;
            for (const bool ieee : {false, true}) {
                Input.fill(0u);
                for (std::uint32_t row = 0; row < std::size(Vectors); ++row) {
                    Input[row * Inputs] = Vectors[row].lhs;
                    Input[row * Inputs + 1] = Vectors[row].rhs;
                    Input[row * Inputs + 2] = Vectors[row].limit;
                }
                if (Run(*device, MullitCode, denorm32, ieee)) {
                    CheckRegisters(denorm32, ieee);
                    ++checked;
                }
                Input.fill(0u);
                for (std::uint32_t row = 0; row < std::size(ConstantVectors); ++row) std::copy(std::begin(ConstantVectors[row].inputs), std::end(ConstantVectors[row].inputs), Input.begin() + row * Inputs);
                if (Run(*device, ConstantMullitCode, denorm32, ieee)) {
                    CheckConstants(denorm32, ieee);
                    ++checked;
                }
            }
        }
        std::printf("v_mullit_f32 denormal mode tests passed (%u of 16 runs checked; denormal modes 1 and 2 need DenormPreserve 32, mode 3 is checked only where the device keeps f32 denormals)\n", checked);
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
