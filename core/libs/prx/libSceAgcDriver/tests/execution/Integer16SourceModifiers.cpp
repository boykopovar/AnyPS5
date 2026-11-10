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

constexpr std::uint32_t Threads = 32;
constexpr std::uint32_t Inputs = 4;
constexpr std::uint32_t Results = 16;
constexpr std::uint32_t Rows = 16;
constexpr std::uint32_t Cases = 16;
constexpr std::uint32_t Preset = 0xa5a5a5a5u;
alignas(256) std::array<std::uint32_t, Threads * Inputs> Input{};
alignas(256) std::array<std::uint32_t, Threads * Results> Output{};

alignas(256) constexpr std::array<std::uint32_t, 62> DppCode{
    0x34020084u, 0x34060086u, 0xe0381000u, 0x80000401u, 0xbf8c3f70u, 0x7e140307u, 0x7e14a0fau, 0xff18e404u, 0x7e160307u,
    0x7e16a0fau, 0xff28e404u, 0x7e180307u, 0x7e18a0fau, 0xff38e404u, 0x7e1a0307u, 0x7e1aa2fau, 0xff18e404u, 0x7e1c0307u,
    0x7e1ca2fau, 0xff28e404u, 0x7e1e0307u, 0x7e1ea2fau, 0xff38e404u, 0x7e200307u, 0x7e20a0fau, 0xff190104u, 0x7e220307u,
    0x7e22a2fau, 0xff290104u, 0x7e240307u, 0x7e24a2fau, 0xff110104u, 0x7e260307u, 0x7e26a0fau, 0xff390104u, 0x7e280307u,
    0x76280afau, 0xff48e406u, 0x7e2a0307u, 0x762a0afau, 0xff88e406u, 0x7e2c0307u, 0x762c0afau, 0xffc8e406u, 0x7e2e0307u,
    0x762e0afau, 0xff490106u, 0x7e300307u, 0x76300afau, 0xff890106u, 0x7e320307u, 0x7e32a0fau, 0xff290104u, 0xe0781000u,
    0x80010a03u, 0xe0781010u, 0x80010e03u, 0xe0781020u, 0x80011203u, 0xe0781030u, 0x80011603u, 0xbf810000u,
};

constexpr std::uint32_t DppExpected[Rows][Cases] = {
    {0xa5a57800u, 0xa5a50000u, 0xa5a57800u, 0xa5a5f800u, 0xa5a50000u, 0xa5a5f800u, 0xa5a57800u, 0xa5a53c00u,
     0xa5a5f800u, 0xa5a57800u, 0xa5a50000u, 0xa5a54000u, 0xa5a50000u, 0xa5a50000u, 0xa5a54900u, 0xa5a53c00u},
    {0xa5a57800u, 0xa5a53c00u, 0xa5a57800u, 0xa5a5f800u, 0xa5a53c00u, 0xa5a5f800u, 0xa5a57c00u, 0xa5a57800u,
     0xa5a5bc00u, 0xa5a57c00u, 0xa5a57c00u, 0xa5a57c00u, 0xa5a54100u, 0xa5a5fc00u, 0xa5a5fc00u, 0xa5a57800u},
    {0xa5a57c00u, 0xa5a57800u, 0xa5a57c00u, 0xa5a5bc00u, 0xa5a57800u, 0xa5a5bc00u, 0xa5a50000u, 0xa5a50000u,
     0xa5a50000u, 0xa5a57800u, 0xa5a5c000u, 0xa5a5c000u, 0xa5a58000u, 0xa5a53955u, 0xa5a53955u, 0xa5a50000u},
    {0xa5a50000u, 0xa5a50000u, 0xa5a57800u, 0xa5a50000u, 0xa5a50000u, 0xa5a5f800u, 0xa5a57800u, 0xa5a57800u,
     0xa5a57800u, 0xa5a57c00u, 0xa5a53155u, 0xa5a57c00u, 0xa5a53155u, 0xa5a50200u, 0xa5a57c00u, 0xa5a57800u},
    {0xa5a57800u, 0xa5a57800u, 0xa5a57c00u, 0xa5a57800u, 0xa5a57800u, 0xa5a5bc00u, 0xa5a53c00u, 0xa5a53c00u,
     0xa5a53c00u, 0xa5a57800u, 0xa5a50000u, 0xa5a51000u, 0xa5a50000u, 0xa5a50000u, 0xa5a57c00u, 0xa5a53c00u},
    {0xa5a53c00u, 0xa5a53c00u, 0xa5a57800u, 0xa5a53c00u, 0xa5a53c00u, 0xa5a5f800u, 0xa5a57800u, 0xa5a50000u,
     0xa5a5f800u, 0xa5a57800u, 0xa5a57c00u, 0xa5a57c00u, 0xa5a573ffu, 0xa5a57c00u, 0xa5a57c00u, 0xa5a50000u},
    {0xa5a57800u, 0xa5a50000u, 0xa5a57800u, 0xa5a5f800u, 0xa5a50000u, 0xa5a5f800u, 0xa5a57800u, 0xa5a53c00u,
     0xa5a5f800u, 0xa5a57800u, 0xa5a50000u, 0xa5a50004u, 0xa5a50000u, 0xa5a58000u, 0xa5a5c800u, 0xa5a53c00u},
    {0xa5a57800u, 0xa5a53c00u, 0xa5a57800u, 0xa5a5f800u, 0xa5a53c00u, 0xa5a5f800u, 0xa5a57c00u, 0xa5a57800u,
     0xa5a5bc00u, 0xa5a57c00u, 0xa5a5c000u, 0xa5a5c000u, 0xa5a58000u, 0xa5a53c01u, 0xa5a53c01u, 0xa5a57800u},
    {0xa5a57c00u, 0xa5a57800u, 0xa5a57c00u, 0xa5a5bc00u, 0xa5a57800u, 0xa5a5bc00u, 0xa5a50000u, 0xa5a50000u,
     0xa5a50000u, 0xa5a57800u, 0xa5a50000u, 0xa5a54001u, 0xa5a50000u, 0xa5a50000u, 0xa5a55a40u, 0xa5a50000u},
    {0xa5a50000u, 0xa5a50000u, 0xa5a57800u, 0xa5a50000u, 0xa5a50000u, 0xa5a5f800u, 0xa5a57804u, 0xa5a55800u,
     0xa5a5f7f8u, 0xa5a57804u, 0xa5a50000u, 0xa5a55640u, 0xa5a50000u, 0xa5a58000u, 0xa5a58400u, 0xa5a55800u},
    {0xa5a57804u, 0xa5a55800u, 0xa5a57804u, 0xa5a5f7f8u, 0xa5a55800u, 0xa5a5f7f8u, 0xa5a577f8u, 0xa5a577f8u,
     0xa5a577f8u, 0xa5a57bfcu, 0xa5a58400u, 0xa5a58400u, 0xa5a58000u, 0xa5a51000u, 0xa5a51000u, 0xa5a577f8u},
    {0xa5a577f8u, 0xa5a577f8u, 0xa5a57bfcu, 0xa5a577f8u, 0xa5a577f8u, 0xa5a5d800u, 0xa5a57c00u, 0xa5a57800u,
     0xa5a5c000u, 0xa5a57c00u, 0xa5a50000u, 0xa5a52400u, 0xa5a50000u, 0xa5a50000u, 0xa5a57400u, 0xa5a57800u},
    {0xa5a57c00u, 0xa5a57800u, 0xa5a57c00u, 0xa5a5c000u, 0xa5a57800u, 0xa5a5c000u, 0xa5a57400u, 0xa5a57400u,
     0xa5a57400u, 0xa5a57a00u, 0xa5a57c00u, 0xa5a57c00u, 0xa5a50000u, 0xa5a5fc00u, 0xa5a5fc00u, 0xa5a57400u},
    {0xa5a57400u, 0xa5a57400u, 0xa5a57a00u, 0xa5a57400u, 0xa5a57400u, 0xa5a5f400u, 0xa5a53c00u, 0xa5a53c00u,
     0xa5a53c00u, 0xa5a57800u, 0xa5a58000u, 0xa5a5fc00u, 0xa5a58000u, 0xa5a50000u, 0xa5a57c00u, 0xa5a53c00u},
    {0xa5a53c00u, 0xa5a53c00u, 0xa5a57800u, 0xa5a53c00u, 0xa5a53c00u, 0xa5a5f800u, 0xa5a57808u, 0xa5a55bf8u,
     0xa5a5f7f0u, 0xa5a57808u, 0xa5a57c00u, 0xa5a57c00u, 0xa5a50000u, 0xa5a57c00u, 0xa5a57c00u, 0xa5a55bf8u},
    {0xa5a57808u, 0xa5a55bf8u, 0xa5a57808u, 0xa5a5f7f0u, 0xa5a55bf8u, 0xa5a5f7f0u, 0xa5a57800u, 0xa5a50000u,
     0xa5a5a5a5u, 0xa5a57800u, 0xa5a50c00u, 0xa5a50c00u, 0xa5a50000u, 0xa5a50000u, 0xa5a50000u, 0xa5a50000u},
};

alignas(256) constexpr std::array<std::uint32_t, 46> SdwaCode{
    0x34020084u, 0x34060086u, 0xe0381000u, 0x80000401u, 0xbf8c3f70u, 0x7e14a0f9u, 0x00160604u, 0x7e16a0f9u, 0x00240604u,
    0x7e18a0f9u, 0x00350604u, 0x7e1aa0f9u, 0x00180604u, 0x7e1ca2f9u, 0x001d0604u, 0x7e1ea2f9u, 0x00100604u, 0x7e20a2f9u,
    0x00280604u, 0x7e22a2f9u, 0x00360604u, 0x7e24a2f9u, 0x002c0604u, 0x7e26a0f9u, 0x002d0604u, 0x76280af9u, 0x14040606u,
    0x762a0af9u, 0x2d040606u, 0x762c0af9u, 0x18040606u, 0x762e0af9u, 0x36040606u, 0x76300af9u, 0x10040606u, 0x76320af9u,
    0x15040606u, 0xe0781000u, 0x80010a03u, 0xe0781010u, 0x80010e03u, 0xe0781020u, 0x80011203u, 0xe0781030u, 0x80011603u,
    0xbf810000u,
};

constexpr std::uint32_t SdwaExpected[Rows][Cases] = {
    {0x00007800u, 0x00000000u, 0x00007800u, 0x00007800u, 0x0000f800u, 0x0000f800u, 0x00000000u, 0x0000f800u,
     0x00000000u, 0x00000000u, 0x00000000u, 0x00003c00u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u},
    {0x00007800u, 0x00003c00u, 0x00007800u, 0x00007800u, 0x0000f800u, 0x0000f800u, 0x00003c00u, 0x0000f800u,
     0x00003c00u, 0x00000000u, 0x00007c00u, 0x00004500u, 0x00007c00u, 0x00004100u, 0x00000000u, 0x00000000u},
    {0x00007c00u, 0x00007800u, 0x00007800u, 0x00007800u, 0x0000f800u, 0x0000f7f0u, 0x00007800u, 0x0000bc00u,
     0x00007800u, 0x00000000u, 0x0000c000u, 0x0000bc00u, 0x00008000u, 0x00008000u, 0x00008000u, 0x00008000u},
    {0x00000000u, 0x00000000u, 0x00007800u, 0x00007800u, 0x0000f800u, 0x0000f800u, 0x00000000u, 0x0000f800u,
     0x00000000u, 0x00000000u, 0x00003155u, 0x00003555u, 0x00007c00u, 0x00003155u, 0x00000000u, 0x00000000u},
    {0x00007800u, 0x00007800u, 0x00007800u, 0x00007800u, 0x0000f800u, 0x0000f7f0u, 0x00007800u, 0x0000bc00u,
     0x00007800u, 0x00000000u, 0x00000000u, 0x00000400u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u},
    {0x00003c00u, 0x00003c00u, 0x00007800u, 0x00007800u, 0x0000f800u, 0x0000f800u, 0x00003c00u, 0x0000f800u,
     0x00003c00u, 0x00000000u, 0x00007c00u, 0x00007bffu, 0x00007c00u, 0x000073ffu, 0x00000000u, 0x00000000u},
    {0x00007800u, 0x00000000u, 0x0000795eu, 0x00007800u, 0x0000717au, 0x0000f800u, 0x00000000u, 0x0000f800u,
     0x00000000u, 0x0000717au, 0x00000000u, 0x00000001u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000001u},
    {0x00007800u, 0x00003c00u, 0x00007892u, 0x00007800u, 0x0000f6ddu, 0x0000f800u, 0x00003c00u, 0x0000f800u,
     0x00003c00u, 0x00006c8du, 0x0000c000u, 0x0000fc00u, 0x00008000u, 0x00008000u, 0x00008000u, 0x00008000u},
    {0x00007c00u, 0x00007800u, 0x00007af5u, 0x00007800u, 0x000075ebu, 0x0000f7f0u, 0x00007800u, 0x0000bc00u,
     0x00007800u, 0x000075ebu, 0x00000000u, 0x00007c00u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00007c00u},
    {0x00000000u, 0x00000000u, 0x00007aabu, 0x00007800u, 0x0000f155u, 0x0000f800u, 0x00000000u, 0x0000f800u,
     0x00000000u, 0x00007555u, 0x00000000u, 0x00005640u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u},
    {0x00007804u, 0x00005800u, 0x00007800u, 0x000077f8u, 0x0000f800u, 0x0000f7f8u, 0x000077f8u, 0x0000f7f8u,
     0x00005800u, 0x00000000u, 0x00008400u, 0x00008400u, 0x00008000u, 0x00008000u, 0x00008000u, 0x00008000u},
    {0x000077f8u, 0x000077f8u, 0x00007800u, 0x000077f8u, 0x0000f800u, 0x0000f7f8u, 0x000077f8u, 0x0000d800u,
     0x000077f8u, 0x00000000u, 0x00000000u, 0x00007c00u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000c00u},
    {0x00007c00u, 0x00007800u, 0x00007800u, 0x00007800u, 0x00003c00u, 0x0000f7f0u, 0x00007800u, 0x0000c000u,
     0x00007800u, 0x00003c00u, 0x00007c00u, 0x00006000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u},
    {0x00007400u, 0x00007400u, 0x00007892u, 0x00007800u, 0x0000f6ddu, 0x0000f800u, 0x00000000u, 0x0000f400u,
     0x00007400u, 0x00006c8du, 0x00008000u, 0x0000b800u, 0x0000fc00u, 0x00008000u, 0x00008000u, 0x00008000u},
    {0x00003c00u, 0x00003c00u, 0x00007c00u, 0x00007800u, 0x0000bc00u, 0x0000f800u, 0x00003c00u, 0x0000f800u,
     0x00003c00u, 0x00007800u, 0x00007c00u, 0x00003266u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u},
    {0x00007808u, 0x00005bf8u, 0x00007800u, 0x00007800u, 0x0000f800u, 0x0000f7f0u, 0x00007800u, 0x0000f7f0u,
     0x00005bf8u, 0x00000000u, 0x00000c00u, 0x00007c00u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00007c00u},
};

alignas(256) constexpr std::array<std::uint32_t, 78> CompareCode{
    0x34020084u, 0x34060086u, 0xe0381000u, 0x80000401u, 0xbf8c3f70u, 0x7d120af9u, 0x04140004u, 0xd501000au, 0x01a90280u,
    0x7d120af9u, 0x04240004u, 0xd501000bu, 0x01a90280u, 0x7d120af9u, 0x1d0d0004u, 0xd501000cu, 0x01a90280u, 0x7d120af9u,
    0x28080004u, 0xd501000du, 0x01a90280u, 0x7d120af9u, 0x06360004u, 0xd501000eu, 0x01a90280u, 0x7d120af9u, 0x34040004u,
    0xd501000fu, 0x01a90280u, 0x7d520af9u, 0x04140004u, 0xd5010010u, 0x01a90280u, 0x7d520af9u, 0x0d2d0004u, 0xd5010011u,
    0x01a90280u, 0x7d520af9u, 0x16060004u, 0xd5010012u, 0x01a90280u, 0x7d520af9u, 0x38080004u, 0xd5010013u, 0x01a90280u,
    0x7d520af9u, 0x24040004u, 0xd5010014u, 0x01a90280u, 0x7d120af9u, 0x06160004u, 0xd5010015u, 0x01a90280u, 0x7d520af9u,
    0x04340004u, 0xd5010016u, 0x01a90280u, 0x7d120af9u, 0x08180004u, 0xd5010017u, 0x01a90280u, 0x7d520af9u, 0x1d0d0004u,
    0xd5010018u, 0x01a90280u, 0x7d120af9u, 0x24040004u, 0xd5010019u, 0x01a90280u, 0xe0781000u, 0x80010a03u, 0xe0781010u,
    0x80010e03u, 0xe0781020u, 0x80011203u, 0xe0781030u, 0x80011603u, 0xbf810000u,
};

constexpr std::uint32_t CompareExpected[Rows][Cases] = {
    {0x00000001u, 0x00000001u, 0x00000000u, 0x00000001u, 0x00000001u, 0x00000000u, 0x00000000u, 0x00000000u,
     0x00000001u, 0x00000001u, 0x00000001u, 0x00000001u, 0x00000000u, 0x00000001u, 0x00000001u, 0x00000001u},
    {0x00000001u, 0x00000000u, 0x00000000u, 0x00000001u, 0x00000001u, 0x00000000u, 0x00000001u, 0x00000000u,
     0x00000001u, 0x00000001u, 0x00000001u, 0x00000001u, 0x00000001u, 0x00000001u, 0x00000001u, 0x00000001u},
    {0x00000000u, 0x00000000u, 0x00000000u, 0x00000001u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u,
     0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000001u, 0x00000000u},
    {0x00000001u, 0x00000001u, 0x00000000u, 0x00000001u, 0x00000001u, 0x00000001u, 0x00000001u, 0x00000000u,
     0x00000001u, 0x00000001u, 0x00000000u, 0x00000001u, 0x00000000u, 0x00000001u, 0x00000001u, 0x00000001u},
    {0x00000000u, 0x00000000u, 0x00000000u, 0x00000001u, 0x00000001u, 0x00000000u, 0x00000000u, 0x00000000u,
     0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000001u, 0x00000001u},
    {0x00000000u, 0x00000000u, 0x00000000u, 0x00000001u, 0x00000001u, 0x00000001u, 0x00000001u, 0x00000000u,
     0x00000000u, 0x00000001u, 0x00000000u, 0x00000000u, 0x00000001u, 0x00000001u, 0x00000001u, 0x00000001u},
    {0x00000001u, 0x00000001u, 0x00000001u, 0x00000001u, 0x00000001u, 0x00000000u, 0x00000000u, 0x00000001u,
     0x00000001u, 0x00000001u, 0x00000001u, 0x00000001u, 0x00000000u, 0x00000001u, 0x00000000u, 0x00000001u},
    {0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u,
     0x00000000u, 0x00000001u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000001u, 0x00000001u, 0x00000000u},
    {0x00000001u, 0x00000000u, 0x00000001u, 0x00000001u, 0x00000001u, 0x00000000u, 0x00000000u, 0x00000001u,
     0x00000001u, 0x00000000u, 0x00000000u, 0x00000001u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u},
    {0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000001u, 0x00000000u, 0x00000000u, 0x00000000u,
     0x00000000u, 0x00000001u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000001u, 0x00000001u, 0x00000001u},
    {0x00000000u, 0x00000000u, 0x00000000u, 0x00000001u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u,
     0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000001u, 0x00000000u},
    {0x00000000u, 0x00000000u, 0x00000000u, 0x00000001u, 0x00000001u, 0x00000000u, 0x00000000u, 0x00000001u,
     0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000001u, 0x00000001u},
    {0x00000000u, 0x00000000u, 0x00000000u, 0x00000001u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u,
     0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u},
    {0x00000000u, 0x00000000u, 0x00000000u, 0x00000001u, 0x00000001u, 0x00000000u, 0x00000000u, 0x00000000u,
     0x00000000u, 0x00000001u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000001u, 0x00000001u, 0x00000001u},
    {0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000001u, 0x00000001u, 0x00000001u, 0x00000000u,
     0x00000000u, 0x00000001u, 0x00000000u, 0x00000000u, 0x00000001u, 0x00000001u, 0x00000001u, 0x00000001u},
    {0x00000000u, 0x00000000u, 0x00000001u, 0x00000001u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000001u,
     0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000001u, 0x00000000u},
};

constexpr const char* Names[3][Cases] = {
    {
        "v_cvt_f16_u16 dpp neg",
        "v_cvt_f16_u16 dpp abs",
        "v_cvt_f16_u16 dpp neg abs",
        "v_cvt_f16_i16 dpp neg",
        "v_cvt_f16_i16 dpp abs",
        "v_cvt_f16_i16 dpp neg abs",
        "v_cvt_f16_u16 dpp neg row_shl:1 bound_ctrl:1",
        "v_cvt_f16_i16 dpp abs row_shl:1 bound_ctrl:1",
        "v_cvt_f16_i16 dpp neg row_shl:1 bound_ctrl:0",
        "v_cvt_f16_u16 dpp neg abs row_shl:1 bound_ctrl:1",
        "v_ldexp_f16 dpp neg src1",
        "v_ldexp_f16 dpp abs src1",
        "v_ldexp_f16 dpp neg abs src1",
        "v_ldexp_f16 dpp neg src1 row_shl:1 bound_ctrl:1",
        "v_ldexp_f16 dpp abs src1 row_shl:1 bound_ctrl:1",
        "v_cvt_f16_u16 dpp abs row_shl:1 bound_ctrl:1",
    },
    {
        "v_cvt_f16_u16 sdwa DWORD neg",
        "v_cvt_f16_u16 sdwa WORD_0 abs",
        "v_cvt_f16_u16 sdwa WORD_1 neg abs",
        "v_cvt_f16_u16 sdwa sext(BYTE_0) neg",
        "v_cvt_f16_i16 sdwa sext(WORD_1) neg",
        "v_cvt_f16_i16 sdwa BYTE_0 neg",
        "v_cvt_f16_i16 sdwa sext(BYTE_0) abs",
        "v_cvt_f16_i16 sdwa DWORD neg abs",
        "v_cvt_f16_i16 sdwa sext(WORD_0) abs",
        "v_cvt_f16_u16 sdwa sext(WORD_1) abs",
        "v_ldexp_f16 sdwa WORD_0 neg src1",
        "v_ldexp_f16 sdwa sext(WORD_1) abs src1",
        "v_ldexp_f16 sdwa sext(BYTE_0) neg src1",
        "v_ldexp_f16 sdwa DWORD neg abs src1",
        "v_ldexp_f16 sdwa BYTE_0 neg src1",
        "v_ldexp_f16 sdwa WORD_1 neg src1",
    },
    {
        "v_cmp_lt_i16 sdwa WORD_0 neg src0",
        "v_cmp_lt_i16 sdwa WORD_0 abs src0",
        "v_cmp_lt_i16 sdwa sext(WORD_1) neg src1",
        "v_cmp_lt_i16 sdwa sext(BYTE_0) abs src1",
        "v_cmp_lt_i16 sdwa DWORD neg abs src0",
        "v_cmp_lt_i16 sdwa WORD_0 neg abs src1",
        "v_cmp_lt_u16 sdwa WORD_0 neg src0",
        "v_cmp_lt_u16 sdwa sext(WORD_1) abs src0",
        "v_cmp_lt_u16 sdwa DWORD neg src1",
        "v_cmp_lt_u16 sdwa sext(BYTE_0) neg abs src1",
        "v_cmp_lt_u16 sdwa WORD_0 abs src1",
        "v_cmp_lt_i16 sdwa DWORD neg src0",
        "v_cmp_lt_u16 sdwa WORD_0 neg abs src0",
        "v_cmp_lt_i16 sdwa sext(BYTE_0) neg src0",
        "v_cmp_lt_u16 sdwa sext(WORD_1) neg src1",
        "v_cmp_lt_i16 sdwa WORD_0 abs src1",
    },
};

constexpr std::uint32_t Sources[Rows][3] = {
    {0x00000000u, 0x00000001u, 0x00003c00u},
    {0x00000001u, 0x0000ffffu, 0x00004500u},
    {0x00007fffu, 0x00008001u, 0x0000bc00u},
    {0x00008000u, 0x00007fffu, 0xdead3555u},
    {0x0000ffffu, 0x00000003u, 0x00000400u},
    {0x00008001u, 0x0000fffeu, 0x00007bffu},
    {0xabcd0000u, 0x80000002u, 0x00000001u},
    {0x12340001u, 0x12348000u, 0x1234c000u},
    {0xdead7fffu, 0xffff0001u, 0x00003c01u},
    {0x55558000u, 0x00000000u, 0x00005640u},
    {0x00000080u, 0x00008000u, 0x00008400u},
    {0x0000ff80u, 0x7fff0005u, 0x00001000u},
    {0x80017ffeu, 0x0000c000u, 0xffff6000u},
    {0x1234c000u, 0x00000080u, 0x0000b800u},
    {0x7fff8001u, 0x0001ff00u, 0x00002e66u},
    {0x000000ffu, 0xffff8003u, 0x00000200u},
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

void Run(AgcDriver::VulkanDevice& device, std::span<const std::uint32_t> code) {
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
    request.context.floatMode = ShaderRecompiler::ShaderFloatMode{0xc0u, true, false, false};
    const auto result = ShaderRecompiler::Recompile(request);
    device.Dispatch(result, 1, 1, 1, {}, reinterpret_cast<std::uintptr_t>(code.data()));
    device.WaitIdle();
}

void Check(AgcDriver::VulkanDevice& device, std::span<const std::uint32_t> code, const char* const (&names)[Cases], const std::uint32_t (&expected)[Rows][Cases]) {
    for (std::uint32_t tid = 0; tid < Threads; ++tid) {
        Input[tid * Inputs] = Sources[tid % Rows][0];
        Input[tid * Inputs + 1u] = Sources[tid % Rows][1];
        Input[tid * Inputs + 2u] = Sources[tid % Rows][2];
        Input[tid * Inputs + 3u] = Preset;
    }
    Run(device, code);
    for (std::uint32_t tid = 0; tid < Threads; ++tid) {
        const std::uint32_t row = tid % Rows;
        for (std::uint32_t column = 0; column < Cases; ++column) {
            const std::uint32_t actual = Output[tid * Results + column];
            Require(actual == expected[row][column], std::string("16-bit integer source modifiers: lane ") + std::to_string(tid) + " " + names[column] + " of " +
                Hex(Sources[row][0]) + ", " + Hex(Sources[row][1]) + ", " + Hex(Sources[row][2]) + " is " + Hex(actual) + ", expected " + Hex(expected[row][column]));
        }
    }
}

}

int main() {
    try {
        const auto device = OpenVulkanTestDevice();
        if (!device) return VulkanTestSkipped;
        Check(*device, DppCode, Names[0], DppExpected);
        Check(*device, SdwaCode, Names[1], SdwaExpected);
        Check(*device, CompareCode, Names[2], CompareExpected);
        std::puts("16-bit integer source modifier tests passed");
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
