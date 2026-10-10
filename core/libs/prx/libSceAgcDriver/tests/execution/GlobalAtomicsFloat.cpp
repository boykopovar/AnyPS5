#include "prx/libSceAgcDriver/Execution/include/VulkanDevice.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Draw.hpp"
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
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

namespace {

using Testing::Require;
using ShaderRecompiler::ShaderStage;

constexpr std::uint32_t Threads = 64;
constexpr std::uint32_t OpCount = 8;
constexpr std::uint32_t NarrowOps = 3;
constexpr std::uint32_t OpStride = 0x200;
constexpr std::uint32_t Sentinel = 0x05e471e1;
constexpr std::size_t BlockBytes = 65536;
constexpr std::uint8_t Fill = 0xcd;
alignas(256) std::array<std::uint32_t, OpCount * Threads * 4> Input{};
alignas(256) std::array<std::uint32_t, OpCount * Threads * 2> Output{};

alignas(256) constexpr std::array<std::uint32_t, 129> GlobalAtomicsFloatCode{
    0x34020083, 0x34040084, 0x340a0083, 0x7e2802ff, 0x05e471e1, 0x7e2a02ff, 0x05e471e1, 0x4a060280,
    0x4a080480, 0xe0381000, 0x80000a04, 0xbf8c3f70, 0x4a0c0a80, 0xdcf98000, 0x14080a03, 0xbf8c0070,
    0xe0701000, 0x80011406, 0x4a0602ff, 0x00000200, 0x4a0804ff, 0x00000400, 0xe0381000, 0x80000a04,
    0xbf8c3f70, 0x4a0c0aff, 0x00000200, 0xd70f6a1e, 0x02020608, 0x7e3e0209, 0x503e3e80, 0xdcfd0000,
    0x147d0a1e, 0xbf8c0070, 0xe0701000, 0x80011406, 0x4a0602ff, 0x00000400, 0x4a0804ff, 0x00000800,
    0xe0381000, 0x80000a04, 0xbf8c3f70, 0x4a0c0aff, 0x00000400, 0xdd018000, 0x14080a03, 0xbf8c0070,
    0xe0701000, 0x80011406, 0x4a0602ff, 0x00000600, 0x4a0804ff, 0x00000c00, 0xe0381000, 0x80000a04,
    0xbf8c3f70, 0x4a0c0aff, 0x00000600, 0xdd798000, 0x14080a03, 0xbf8c0070, 0xe0741000, 0x80011406,
    0x4a0602ff, 0x00000800, 0x4a0804ff, 0x00001000, 0xe0381000, 0x80000a04, 0xbf8c3f70, 0x4a0c0aff,
    0x00000800, 0xdd7d8000, 0x14080a03, 0xbf8c0070, 0xe0741000, 0x80011406, 0x4a0602ff, 0x00000a00,
    0x4a0804ff, 0x00001400, 0xe0381000, 0x80000a04, 0xbf8c3f70, 0x4a0c0aff, 0x00000a00, 0xd70f6a1e,
    0x02020608, 0x7e3e0209, 0x503e3e80, 0xdd810000, 0x147d0a1e, 0xbf8c0070, 0xe0741000, 0x80011406,
    0x4a0602ff, 0x00000c00, 0x4a0804ff, 0x00001800, 0xe0381000, 0x80000a04, 0xbf8c3f70, 0x4a0c0aff,
    0x00000c00, 0xdd718000, 0x14080a03, 0xbf8c0070, 0xe0741000, 0x80011406, 0x4a0602ff, 0x00000e00,
    0x4a0804ff, 0x00001c00, 0xe0381000, 0x80000a04, 0xbf8c3f70, 0x4a0c0aff, 0x00000e00, 0xd70f6a1e,
    0x02020608, 0x7e3e0209, 0x503e3e80, 0xdd750000, 0x147d0a1e, 0xbf8c0070, 0xe0741000, 0x80011406,
    0xbf810000
};

struct Row {
    std::uint32_t op;
    std::uint64_t memory;
    std::uint64_t data;
    std::uint64_t comparator;
    std::uint64_t final;
};

constexpr std::array<Row, 512> Rows{{
    {0, 0x0ull, 0x0ull, 0x0ull, 0x0ull},
    {0, 0x0ull, 0x80000000ull, 0x80000000ull, 0x80000000ull},
    {0, 0x0ull, 0x3f800000ull, 0xc0200000ull, 0x0ull},
    {0, 0x0ull, 0xbf800000ull, 0xbf800000ull, 0x0ull},
    {0, 0x0ull, 0x3fc00000ull, 0x0ull, 0x3fc00000ull},
    {0, 0x0ull, 0x7f800000ull, 0xffc12345ull, 0x0ull},
    {0, 0x0ull, 0xff800000ull, 0xff800000ull, 0x0ull},
    {0, 0x0ull, 0x7fa00000ull, 0x807fffffull, 0x0ull},
    {0, 0x0ull, 0xff800001ull, 0xff800001ull, 0x0ull},
    {0, 0x0ull, 0x800000ull, 0x800000ull, 0x0ull},
    {0, 0x80000000ull, 0x0ull, 0x80000000ull, 0x0ull},
    {0, 0x80000000ull, 0x80000000ull, 0x0ull, 0x80000000ull},
    {0, 0x80000000ull, 0x3f800000ull, 0x7f800000ull, 0x80000000ull},
    {0, 0x80000000ull, 0xbf800000ull, 0xbf800000ull, 0x80000000ull},
    {0, 0x80000000ull, 0xc0200000ull, 0x0ull, 0xc0200000ull},
    {0, 0x80000000ull, 0x7f800000ull, 0x7fa00000ull, 0x80000000ull},
    {0, 0x80000000ull, 0xff800000ull, 0xff800000ull, 0x80000000ull},
    {0, 0x80000000ull, 0x7fa00000ull, 0x7f7fffffull, 0x80000000ull},
    {0, 0x80000000ull, 0xff800001ull, 0xff800001ull, 0x80000000ull},
    {0, 0x80000000ull, 0x7f7fffffull, 0x3f800000ull, 0x80000000ull},
    {0, 0x80000000ull, 0x800000ull, 0x800000ull, 0x80000000ull},
    {0, 0x3f800000ull, 0x0ull, 0x3f800000ull, 0x0ull},
    {0, 0x3f800000ull, 0x80000000ull, 0xbf800000ull, 0x3f800000ull},
    {0, 0x3f800000ull, 0x3f800000ull, 0xff800000ull, 0x3f800000ull},
    {0, 0x3f800000ull, 0x7f800000ull, 0xff800001ull, 0x3f800000ull},
    {0, 0x3f800000ull, 0x7fa00000ull, 0x800000ull, 0x3f800000ull},
    {0, 0xbf800000ull, 0x0ull, 0xbf800000ull, 0x0ull},
    {0, 0xbf800000ull, 0x80000000ull, 0x3f800000ull, 0xbf800000ull},
    {0, 0xbf800000ull, 0x3f800000ull, 0x7fc00000ull, 0xbf800000ull},
    {0, 0xbf800000ull, 0xbf800000ull, 0xbf800000ull, 0xbf800000ull},
    {0, 0xbf800000ull, 0x7f800000ull, 0x1ull, 0xbf800000ull},
    {0, 0xbf800000ull, 0xff800000ull, 0xff800000ull, 0xbf800000ull},
    {0, 0xbf800000ull, 0x7fa00000ull, 0x0ull, 0xbf800000ull},
    {0, 0xbf800000ull, 0xff800001ull, 0xff800001ull, 0xbf800000ull},
    {0, 0xbf800000ull, 0x7f7fffffull, 0x3fc00000ull, 0xbf800000ull},
    {0, 0xbf800000ull, 0x800000ull, 0x800000ull, 0xbf800000ull},
    {0, 0x3fc00000ull, 0x0ull, 0x3fc00000ull, 0x0ull},
    {0, 0x3fc00000ull, 0x80000000ull, 0xbfc00000ull, 0x3fc00000ull},
    {0, 0x3fc00000ull, 0x3f800000ull, 0xffc12345ull, 0x3fc00000ull},
    {0, 0x3fc00000ull, 0xbf800000ull, 0xbf800000ull, 0x3fc00000ull},
    {0, 0x3fc00000ull, 0x3fc00000ull, 0x3fc00000ull, 0x3fc00000ull},
    {0, 0x3fc00000ull, 0x7f800000ull, 0x807fffffull, 0x3fc00000ull},
    {0, 0x3fc00000ull, 0xff800000ull, 0xff800000ull, 0x3fc00000ull},
    {0, 0x3fc00000ull, 0x7fa00000ull, 0x80000000ull, 0x3fc00000ull},
    {0, 0x3fc00000ull, 0xff800001ull, 0xff800001ull, 0x3fc00000ull},
    {0, 0x3fc00000ull, 0x7f7fffffull, 0xc0200000ull, 0x3fc00000ull},
    {0, 0x3fc00000ull, 0x800000ull, 0x800000ull, 0x3fc00000ull},
    {0, 0xc0200000ull, 0x0ull, 0xc0200000ull, 0x0ull},
    {0, 0xc0200000ull, 0x80000000ull, 0x40200000ull, 0xc0200000ull},
    {0, 0xc0200000ull, 0x3f800000ull, 0x7fa00000ull, 0xc0200000ull},
    {0, 0xc0200000ull, 0xbf800000ull, 0xbf800000ull, 0xc0200000ull},
    {0, 0xc0200000ull, 0x7f800000ull, 0x7f7fffffull, 0xc0200000ull},
    {0, 0xc0200000ull, 0xff800000ull, 0xff800000ull, 0xc0200000ull},
    {0, 0xc0200000ull, 0x7fa00000ull, 0x3f800000ull, 0xc0200000ull},
    {0, 0xc0200000ull, 0xff800001ull, 0xff800001ull, 0xc0200000ull},
    {0, 0xc0200000ull, 0x7f7fffffull, 0x7f800000ull, 0xc0200000ull},
    {0, 0xc0200000ull, 0x800000ull, 0x800000ull, 0xc0200000ull},
    {0, 0x7f800000ull, 0x0ull, 0x7f800000ull, 0x0ull},
    {0, 0x7f800000ull, 0x80000000ull, 0xff800000ull, 0x7f800000ull},
    {0, 0x7f800000ull, 0x3f800000ull, 0xff800001ull, 0x7f800000ull},
    {0, 0x7f800000ull, 0xbf800000ull, 0xbf800000ull, 0x7f800000ull},
    {0, 0x7f800000ull, 0x7f800000ull, 0x800000ull, 0x7f800000ull},
    {0, 0xff800000ull, 0x0ull, 0xff800000ull, 0x0ull},
    {0, 0xff800000ull, 0x80000000ull, 0x7f800000ull, 0xff800000ull},
    {1, 0x0ull, 0x0ull, 0x0ull, 0x0ull},
    {1, 0x0ull, 0x80000000ull, 0x80000000ull, 0x80000000ull},
    {1, 0x0ull, 0x3f800000ull, 0xc0200000ull, 0x0ull},
    {1, 0x0ull, 0xbf800000ull, 0xbf800000ull, 0xbf800000ull},
    {1, 0x0ull, 0x3fc00000ull, 0x0ull, 0x0ull},
    {1, 0x0ull, 0xc0200000ull, 0x80000000ull, 0xc0200000ull},
    {1, 0x0ull, 0x7f800000ull, 0xffc12345ull, 0x0ull},
    {1, 0x0ull, 0xff800000ull, 0xff800000ull, 0xff800000ull},
    {1, 0x0ull, 0x7fc00000ull, 0x0ull, 0x0ull},
    {1, 0x0ull, 0xffc12345ull, 0x80000000ull, 0x0ull},
    {1, 0x0ull, 0x7fa00000ull, 0x807fffffull, 0x7fe00000ull},
    {1, 0x0ull, 0xff800001ull, 0xff800001ull, 0xffc00001ull},
    {1, 0x0ull, 0x1ull, 0x0ull, 0x0ull},
    {1, 0x0ull, 0x807fffffull, 0x80000000ull, 0x807fffffull},
    {1, 0x0ull, 0x7f7fffffull, 0x80000000ull, 0x0ull},
    {1, 0x0ull, 0x800000ull, 0x800000ull, 0x0ull},
    {1, 0x80000000ull, 0x0ull, 0x80000000ull, 0x80000000ull},
    {1, 0x80000000ull, 0x80000000ull, 0x0ull, 0x80000000ull},
    {1, 0x80000000ull, 0x3f800000ull, 0x7f800000ull, 0x80000000ull},
    {1, 0x80000000ull, 0xbf800000ull, 0xbf800000ull, 0xbf800000ull},
    {1, 0x80000000ull, 0x3fc00000ull, 0x80000000ull, 0x80000000ull},
    {1, 0x80000000ull, 0xc0200000ull, 0x0ull, 0xc0200000ull},
    {1, 0x80000000ull, 0x7f800000ull, 0x7fa00000ull, 0x80000000ull},
    {1, 0x80000000ull, 0xff800000ull, 0xff800000ull, 0xff800000ull},
    {1, 0x80000000ull, 0x7fc00000ull, 0x80000000ull, 0x80000000ull},
    {1, 0x80000000ull, 0xffc12345ull, 0x0ull, 0x80000000ull},
    {1, 0x80000000ull, 0x7fa00000ull, 0x7f7fffffull, 0x7fe00000ull},
    {1, 0x80000000ull, 0xff800001ull, 0xff800001ull, 0xffc00001ull},
    {1, 0x80000000ull, 0x1ull, 0x80000000ull, 0x80000000ull},
    {1, 0x80000000ull, 0x807fffffull, 0x0ull, 0x807fffffull},
    {1, 0x80000000ull, 0x7f7fffffull, 0x3f800000ull, 0x80000000ull},
    {1, 0x80000000ull, 0x800000ull, 0x800000ull, 0x80000000ull},
    {1, 0x3f800000ull, 0x0ull, 0x3f800000ull, 0x0ull},
    {1, 0x3f800000ull, 0x80000000ull, 0xbf800000ull, 0x80000000ull},
    {1, 0x3f800000ull, 0x3f800000ull, 0xff800000ull, 0x3f800000ull},
    {1, 0x3f800000ull, 0xbf800000ull, 0xbf800000ull, 0xbf800000ull},
    {1, 0x3f800000ull, 0x3fc00000ull, 0x3f800000ull, 0x3f800000ull},
    {1, 0x3f800000ull, 0xc0200000ull, 0xbf800000ull, 0xc0200000ull},
    {1, 0x3f800000ull, 0x7f800000ull, 0xff800001ull, 0x3f800000ull},
    {1, 0x3f800000ull, 0xff800000ull, 0xff800000ull, 0xff800000ull},
    {1, 0x3f800000ull, 0x7fc00000ull, 0x3f800000ull, 0x3f800000ull},
    {1, 0x3f800000ull, 0xffc12345ull, 0xbf800000ull, 0x3f800000ull},
    {1, 0x3f800000ull, 0x7fa00000ull, 0x800000ull, 0x7fe00000ull},
    {1, 0x3f800000ull, 0xff800001ull, 0xff800001ull, 0xffc00001ull},
    {1, 0x3f800000ull, 0x1ull, 0x3f800000ull, 0x1ull},
    {1, 0x3f800000ull, 0x807fffffull, 0xbf800000ull, 0x807fffffull},
    {1, 0x3f800000ull, 0x7f7fffffull, 0xbf800000ull, 0x3f800000ull},
    {1, 0x3f800000ull, 0x800000ull, 0x800000ull, 0x800000ull},
    {1, 0xbf800000ull, 0x0ull, 0xbf800000ull, 0xbf800000ull},
    {1, 0xbf800000ull, 0x80000000ull, 0x3f800000ull, 0xbf800000ull},
    {1, 0xbf800000ull, 0x3f800000ull, 0x7fc00000ull, 0xbf800000ull},
    {1, 0xbf800000ull, 0xbf800000ull, 0xbf800000ull, 0xbf800000ull},
    {1, 0xbf800000ull, 0x3fc00000ull, 0xbf800000ull, 0xbf800000ull},
    {1, 0xbf800000ull, 0xc0200000ull, 0x3f800000ull, 0xc0200000ull},
    {1, 0xbf800000ull, 0x7f800000ull, 0x1ull, 0xbf800000ull},
    {1, 0xbf800000ull, 0xff800000ull, 0xff800000ull, 0xff800000ull},
    {1, 0xbf800000ull, 0x7fc00000ull, 0xbf800000ull, 0xbf800000ull},
    {1, 0xbf800000ull, 0xffc12345ull, 0x3f800000ull, 0xbf800000ull},
    {1, 0xbf800000ull, 0x7fa00000ull, 0x0ull, 0x7fe00000ull},
    {1, 0xbf800000ull, 0xff800001ull, 0xff800001ull, 0xffc00001ull},
    {1, 0xbf800000ull, 0x1ull, 0xbf800000ull, 0xbf800000ull},
    {1, 0xbf800000ull, 0x807fffffull, 0x3f800000ull, 0xbf800000ull},
    {1, 0xbf800000ull, 0x7f7fffffull, 0x3fc00000ull, 0xbf800000ull},
    {1, 0xbf800000ull, 0x800000ull, 0x800000ull, 0xbf800000ull},
    {2, 0x0ull, 0x0ull, 0x0ull, 0x0ull},
    {2, 0x0ull, 0x80000000ull, 0x80000000ull, 0x0ull},
    {2, 0x0ull, 0x3f800000ull, 0xc0200000ull, 0x3f800000ull},
    {2, 0x0ull, 0xbf800000ull, 0xbf800000ull, 0x0ull},
    {2, 0x0ull, 0x3fc00000ull, 0x0ull, 0x3fc00000ull},
    {2, 0x0ull, 0xc0200000ull, 0x80000000ull, 0x0ull},
    {2, 0x0ull, 0x7f800000ull, 0xffc12345ull, 0x7f800000ull},
    {2, 0x0ull, 0xff800000ull, 0xff800000ull, 0x0ull},
    {2, 0x0ull, 0x7fc00000ull, 0x0ull, 0x0ull},
    {2, 0x0ull, 0xffc12345ull, 0x80000000ull, 0x0ull},
    {2, 0x0ull, 0x7fa00000ull, 0x807fffffull, 0x7fe00000ull},
    {2, 0x0ull, 0xff800001ull, 0xff800001ull, 0xffc00001ull},
    {2, 0x0ull, 0x1ull, 0x0ull, 0x1ull},
    {2, 0x0ull, 0x807fffffull, 0x80000000ull, 0x0ull},
    {2, 0x0ull, 0x7f7fffffull, 0x80000000ull, 0x7f7fffffull},
    {2, 0x0ull, 0x800000ull, 0x800000ull, 0x800000ull},
    {2, 0x80000000ull, 0x0ull, 0x80000000ull, 0x0ull},
    {2, 0x80000000ull, 0x80000000ull, 0x0ull, 0x80000000ull},
    {2, 0x80000000ull, 0x3f800000ull, 0x7f800000ull, 0x3f800000ull},
    {2, 0x80000000ull, 0xbf800000ull, 0xbf800000ull, 0x80000000ull},
    {2, 0x80000000ull, 0x3fc00000ull, 0x80000000ull, 0x3fc00000ull},
    {2, 0x80000000ull, 0xc0200000ull, 0x0ull, 0x80000000ull},
    {2, 0x80000000ull, 0x7f800000ull, 0x7fa00000ull, 0x7f800000ull},
    {2, 0x80000000ull, 0xff800000ull, 0xff800000ull, 0x80000000ull},
    {2, 0x80000000ull, 0x7fc00000ull, 0x80000000ull, 0x80000000ull},
    {2, 0x80000000ull, 0xffc12345ull, 0x0ull, 0x80000000ull},
    {2, 0x80000000ull, 0x7fa00000ull, 0x7f7fffffull, 0x7fe00000ull},
    {2, 0x80000000ull, 0xff800001ull, 0xff800001ull, 0xffc00001ull},
    {2, 0x80000000ull, 0x1ull, 0x80000000ull, 0x1ull},
    {2, 0x80000000ull, 0x807fffffull, 0x0ull, 0x80000000ull},
    {2, 0x80000000ull, 0x7f7fffffull, 0x3f800000ull, 0x7f7fffffull},
    {2, 0x80000000ull, 0x800000ull, 0x800000ull, 0x800000ull},
    {2, 0x3f800000ull, 0x0ull, 0x3f800000ull, 0x3f800000ull},
    {2, 0x3f800000ull, 0x80000000ull, 0xbf800000ull, 0x3f800000ull},
    {2, 0x3f800000ull, 0x3f800000ull, 0xff800000ull, 0x3f800000ull},
    {2, 0x3f800000ull, 0xbf800000ull, 0xbf800000ull, 0x3f800000ull},
    {2, 0x3f800000ull, 0x3fc00000ull, 0x3f800000ull, 0x3fc00000ull},
    {2, 0x3f800000ull, 0xc0200000ull, 0xbf800000ull, 0x3f800000ull},
    {2, 0x3f800000ull, 0x7f800000ull, 0xff800001ull, 0x7f800000ull},
    {2, 0x3f800000ull, 0xff800000ull, 0xff800000ull, 0x3f800000ull},
    {2, 0x3f800000ull, 0x7fc00000ull, 0x3f800000ull, 0x3f800000ull},
    {2, 0x3f800000ull, 0xffc12345ull, 0xbf800000ull, 0x3f800000ull},
    {2, 0x3f800000ull, 0x7fa00000ull, 0x800000ull, 0x7fe00000ull},
    {2, 0x3f800000ull, 0xff800001ull, 0xff800001ull, 0xffc00001ull},
    {2, 0x3f800000ull, 0x1ull, 0x3f800000ull, 0x3f800000ull},
    {2, 0x3f800000ull, 0x807fffffull, 0xbf800000ull, 0x3f800000ull},
    {2, 0x3f800000ull, 0x7f7fffffull, 0xbf800000ull, 0x7f7fffffull},
    {2, 0x3f800000ull, 0x800000ull, 0x800000ull, 0x3f800000ull},
    {2, 0xbf800000ull, 0x0ull, 0xbf800000ull, 0x0ull},
    {2, 0xbf800000ull, 0x80000000ull, 0x3f800000ull, 0x80000000ull},
    {2, 0xbf800000ull, 0x3f800000ull, 0x7fc00000ull, 0x3f800000ull},
    {2, 0xbf800000ull, 0xbf800000ull, 0xbf800000ull, 0xbf800000ull},
    {2, 0xbf800000ull, 0x3fc00000ull, 0xbf800000ull, 0x3fc00000ull},
    {2, 0xbf800000ull, 0xc0200000ull, 0x3f800000ull, 0xbf800000ull},
    {2, 0xbf800000ull, 0x7f800000ull, 0x1ull, 0x7f800000ull},
    {2, 0xbf800000ull, 0xff800000ull, 0xff800000ull, 0xbf800000ull},
    {2, 0xbf800000ull, 0x7fc00000ull, 0xbf800000ull, 0xbf800000ull},
    {2, 0xbf800000ull, 0xffc12345ull, 0x3f800000ull, 0xbf800000ull},
    {2, 0xbf800000ull, 0x7fa00000ull, 0x0ull, 0x7fe00000ull},
    {2, 0xbf800000ull, 0xff800001ull, 0xff800001ull, 0xffc00001ull},
    {2, 0xbf800000ull, 0x1ull, 0xbf800000ull, 0x1ull},
    {2, 0xbf800000ull, 0x807fffffull, 0x3f800000ull, 0x807fffffull},
    {2, 0xbf800000ull, 0x7f7fffffull, 0x3fc00000ull, 0x7f7fffffull},
    {2, 0xbf800000ull, 0x800000ull, 0x800000ull, 0x800000ull},
    {3, 0x0ull, 0x0ull, 0x0ull, 0x0ull},
    {3, 0x0ull, 0x8000000000000000ull, 0x8000000000000000ull, 0x8000000000000000ull},
    {3, 0x0ull, 0x3ff0000000000000ull, 0xc004000000000000ull, 0x0ull},
    {3, 0x0ull, 0xbff0000000000000ull, 0xbff0000000000000ull, 0x0ull},
    {3, 0x0ull, 0x3ff8000000000000ull, 0x0ull, 0x3ff8000000000000ull},
    {3, 0x0ull, 0x7ff0000000000000ull, 0xfff8000012345678ull, 0x0ull},
    {3, 0x0ull, 0xfff0000000000000ull, 0xfff0000000000000ull, 0x0ull},
    {3, 0x0ull, 0x7ff4000000000000ull, 0x800fffffffffffffull, 0x0ull},
    {3, 0x0ull, 0xfff0000000000001ull, 0xfff0000000000001ull, 0x0ull},
    {3, 0x0ull, 0x10000000000000ull, 0x10000000000000ull, 0x0ull},
    {3, 0x8000000000000000ull, 0x0ull, 0x8000000000000000ull, 0x0ull},
    {3, 0x8000000000000000ull, 0x8000000000000000ull, 0x0ull, 0x8000000000000000ull},
    {3, 0x8000000000000000ull, 0x3ff0000000000000ull, 0x7ff0000000000000ull, 0x8000000000000000ull},
    {3, 0x8000000000000000ull, 0xbff0000000000000ull, 0xbff0000000000000ull, 0x8000000000000000ull},
    {3, 0x8000000000000000ull, 0xc004000000000000ull, 0x0ull, 0xc004000000000000ull},
    {3, 0x8000000000000000ull, 0x7ff0000000000000ull, 0x7ff4000000000000ull, 0x8000000000000000ull},
    {3, 0x8000000000000000ull, 0xfff0000000000000ull, 0xfff0000000000000ull, 0x8000000000000000ull},
    {3, 0x8000000000000000ull, 0x7ff4000000000000ull, 0x7fefffffffffffffull, 0x8000000000000000ull},
    {3, 0x8000000000000000ull, 0xfff0000000000001ull, 0xfff0000000000001ull, 0x8000000000000000ull},
    {3, 0x8000000000000000ull, 0x7fefffffffffffffull, 0x3ff0000000000000ull, 0x8000000000000000ull},
    {3, 0x8000000000000000ull, 0x10000000000000ull, 0x10000000000000ull, 0x8000000000000000ull},
    {3, 0x3ff0000000000000ull, 0x0ull, 0x3ff0000000000000ull, 0x0ull},
    {3, 0x3ff0000000000000ull, 0x8000000000000000ull, 0xbff0000000000000ull, 0x3ff0000000000000ull},
    {3, 0x3ff0000000000000ull, 0x3ff0000000000000ull, 0xfff0000000000000ull, 0x3ff0000000000000ull},
    {3, 0x3ff0000000000000ull, 0x7ff0000000000000ull, 0xfff0000000000001ull, 0x3ff0000000000000ull},
    {3, 0x3ff0000000000000ull, 0x7ff4000000000000ull, 0x10000000000000ull, 0x3ff0000000000000ull},
    {3, 0xbff0000000000000ull, 0x0ull, 0xbff0000000000000ull, 0x0ull},
    {3, 0xbff0000000000000ull, 0x8000000000000000ull, 0x3ff0000000000000ull, 0xbff0000000000000ull},
    {3, 0xbff0000000000000ull, 0x3ff0000000000000ull, 0x7ff8000000000000ull, 0xbff0000000000000ull},
    {3, 0xbff0000000000000ull, 0xbff0000000000000ull, 0xbff0000000000000ull, 0xbff0000000000000ull},
    {3, 0xbff0000000000000ull, 0x7ff0000000000000ull, 0x1ull, 0xbff0000000000000ull},
    {3, 0xbff0000000000000ull, 0xfff0000000000000ull, 0xfff0000000000000ull, 0xbff0000000000000ull},
    {3, 0xbff0000000000000ull, 0x7ff4000000000000ull, 0x0ull, 0xbff0000000000000ull},
    {3, 0xbff0000000000000ull, 0xfff0000000000001ull, 0xfff0000000000001ull, 0xbff0000000000000ull},
    {3, 0xbff0000000000000ull, 0x7fefffffffffffffull, 0x3ff8000000000000ull, 0xbff0000000000000ull},
    {3, 0xbff0000000000000ull, 0x10000000000000ull, 0x10000000000000ull, 0xbff0000000000000ull},
    {3, 0x3ff8000000000000ull, 0x0ull, 0x3ff8000000000000ull, 0x0ull},
    {3, 0x3ff8000000000000ull, 0x8000000000000000ull, 0xbff8000000000000ull, 0x3ff8000000000000ull},
    {3, 0x3ff8000000000000ull, 0x3ff0000000000000ull, 0xfff8000012345678ull, 0x3ff8000000000000ull},
    {3, 0x3ff8000000000000ull, 0xbff0000000000000ull, 0xbff0000000000000ull, 0x3ff8000000000000ull},
    {3, 0x3ff8000000000000ull, 0x3ff8000000000000ull, 0x3ff8000000000000ull, 0x3ff8000000000000ull},
    {3, 0x3ff8000000000000ull, 0x7ff0000000000000ull, 0x800fffffffffffffull, 0x3ff8000000000000ull},
    {3, 0x3ff8000000000000ull, 0xfff0000000000000ull, 0xfff0000000000000ull, 0x3ff8000000000000ull},
    {3, 0x3ff8000000000000ull, 0x7ff4000000000000ull, 0x8000000000000000ull, 0x3ff8000000000000ull},
    {3, 0x3ff8000000000000ull, 0xfff0000000000001ull, 0xfff0000000000001ull, 0x3ff8000000000000ull},
    {3, 0x3ff8000000000000ull, 0x7fefffffffffffffull, 0xc004000000000000ull, 0x3ff8000000000000ull},
    {3, 0x3ff8000000000000ull, 0x10000000000000ull, 0x10000000000000ull, 0x3ff8000000000000ull},
    {3, 0xc004000000000000ull, 0x0ull, 0xc004000000000000ull, 0x0ull},
    {3, 0xc004000000000000ull, 0x8000000000000000ull, 0x4004000000000000ull, 0xc004000000000000ull},
    {3, 0xc004000000000000ull, 0x3ff0000000000000ull, 0x7ff4000000000000ull, 0xc004000000000000ull},
    {3, 0xc004000000000000ull, 0xbff0000000000000ull, 0xbff0000000000000ull, 0xc004000000000000ull},
    {3, 0xc004000000000000ull, 0x7ff0000000000000ull, 0x7fefffffffffffffull, 0xc004000000000000ull},
    {3, 0xc004000000000000ull, 0xfff0000000000000ull, 0xfff0000000000000ull, 0xc004000000000000ull},
    {3, 0xc004000000000000ull, 0x7ff4000000000000ull, 0x3ff0000000000000ull, 0xc004000000000000ull},
    {3, 0xc004000000000000ull, 0xfff0000000000001ull, 0xfff0000000000001ull, 0xc004000000000000ull},
    {3, 0xc004000000000000ull, 0x7fefffffffffffffull, 0x7ff0000000000000ull, 0xc004000000000000ull},
    {3, 0xc004000000000000ull, 0x10000000000000ull, 0x10000000000000ull, 0xc004000000000000ull},
    {3, 0x7ff0000000000000ull, 0x0ull, 0x7ff0000000000000ull, 0x0ull},
    {3, 0x7ff0000000000000ull, 0x8000000000000000ull, 0xfff0000000000000ull, 0x7ff0000000000000ull},
    {3, 0x7ff0000000000000ull, 0x3ff0000000000000ull, 0xfff0000000000001ull, 0x7ff0000000000000ull},
    {3, 0x7ff0000000000000ull, 0xbff0000000000000ull, 0xbff0000000000000ull, 0x7ff0000000000000ull},
    {3, 0x7ff0000000000000ull, 0x7ff0000000000000ull, 0x10000000000000ull, 0x7ff0000000000000ull},
    {3, 0xfff0000000000000ull, 0x0ull, 0xfff0000000000000ull, 0x0ull},
    {3, 0xfff0000000000000ull, 0x8000000000000000ull, 0x7ff0000000000000ull, 0xfff0000000000000ull},
    {4, 0x0ull, 0x0ull, 0x0ull, 0x0ull},
    {4, 0x0ull, 0x8000000000000000ull, 0x8000000000000000ull, 0x8000000000000000ull},
    {4, 0x0ull, 0x3ff0000000000000ull, 0xc004000000000000ull, 0x0ull},
    {4, 0x0ull, 0xbff0000000000000ull, 0xbff0000000000000ull, 0xbff0000000000000ull},
    {4, 0x0ull, 0x3ff8000000000000ull, 0x0ull, 0x0ull},
    {4, 0x0ull, 0xc004000000000000ull, 0x8000000000000000ull, 0xc004000000000000ull},
    {4, 0x0ull, 0x7ff0000000000000ull, 0xfff8000012345678ull, 0x0ull},
    {4, 0x0ull, 0xfff0000000000000ull, 0xfff0000000000000ull, 0xfff0000000000000ull},
    {4, 0x0ull, 0x7ff8000000000000ull, 0x0ull, 0x0ull},
    {4, 0x0ull, 0xfff8000012345678ull, 0x8000000000000000ull, 0x0ull},
    {4, 0x0ull, 0x7ff4000000000000ull, 0x800fffffffffffffull, 0x7ffc000000000000ull},
    {4, 0x0ull, 0xfff0000000000001ull, 0xfff0000000000001ull, 0xfff8000000000001ull},
    {4, 0x0ull, 0x1ull, 0x0ull, 0x0ull},
    {4, 0x0ull, 0x800fffffffffffffull, 0x8000000000000000ull, 0x800fffffffffffffull},
    {4, 0x0ull, 0x7fefffffffffffffull, 0x8000000000000000ull, 0x0ull},
    {4, 0x0ull, 0x10000000000000ull, 0x10000000000000ull, 0x0ull},
    {4, 0x8000000000000000ull, 0x0ull, 0x8000000000000000ull, 0x8000000000000000ull},
    {4, 0x8000000000000000ull, 0x8000000000000000ull, 0x0ull, 0x8000000000000000ull},
    {4, 0x8000000000000000ull, 0x3ff0000000000000ull, 0x7ff0000000000000ull, 0x8000000000000000ull},
    {4, 0x8000000000000000ull, 0xbff0000000000000ull, 0xbff0000000000000ull, 0xbff0000000000000ull},
    {4, 0x8000000000000000ull, 0x3ff8000000000000ull, 0x8000000000000000ull, 0x8000000000000000ull},
    {4, 0x8000000000000000ull, 0xc004000000000000ull, 0x0ull, 0xc004000000000000ull},
    {4, 0x8000000000000000ull, 0x7ff0000000000000ull, 0x7ff4000000000000ull, 0x8000000000000000ull},
    {4, 0x8000000000000000ull, 0xfff0000000000000ull, 0xfff0000000000000ull, 0xfff0000000000000ull},
    {4, 0x8000000000000000ull, 0x7ff8000000000000ull, 0x8000000000000000ull, 0x8000000000000000ull},
    {4, 0x8000000000000000ull, 0xfff8000012345678ull, 0x0ull, 0x8000000000000000ull},
    {4, 0x8000000000000000ull, 0x7ff4000000000000ull, 0x7fefffffffffffffull, 0x7ffc000000000000ull},
    {4, 0x8000000000000000ull, 0xfff0000000000001ull, 0xfff0000000000001ull, 0xfff8000000000001ull},
    {4, 0x8000000000000000ull, 0x1ull, 0x8000000000000000ull, 0x8000000000000000ull},
    {4, 0x8000000000000000ull, 0x800fffffffffffffull, 0x0ull, 0x800fffffffffffffull},
    {4, 0x8000000000000000ull, 0x7fefffffffffffffull, 0x3ff0000000000000ull, 0x8000000000000000ull},
    {4, 0x8000000000000000ull, 0x10000000000000ull, 0x10000000000000ull, 0x8000000000000000ull},
    {4, 0x3ff0000000000000ull, 0x0ull, 0x3ff0000000000000ull, 0x0ull},
    {4, 0x3ff0000000000000ull, 0x8000000000000000ull, 0xbff0000000000000ull, 0x8000000000000000ull},
    {4, 0x3ff0000000000000ull, 0x3ff0000000000000ull, 0xfff0000000000000ull, 0x3ff0000000000000ull},
    {4, 0x3ff0000000000000ull, 0xbff0000000000000ull, 0xbff0000000000000ull, 0xbff0000000000000ull},
    {4, 0x3ff0000000000000ull, 0x3ff8000000000000ull, 0x3ff0000000000000ull, 0x3ff0000000000000ull},
    {4, 0x3ff0000000000000ull, 0xc004000000000000ull, 0xbff0000000000000ull, 0xc004000000000000ull},
    {4, 0x3ff0000000000000ull, 0x7ff0000000000000ull, 0xfff0000000000001ull, 0x3ff0000000000000ull},
    {4, 0x3ff0000000000000ull, 0xfff0000000000000ull, 0xfff0000000000000ull, 0xfff0000000000000ull},
    {4, 0x3ff0000000000000ull, 0x7ff8000000000000ull, 0x3ff0000000000000ull, 0x3ff0000000000000ull},
    {4, 0x3ff0000000000000ull, 0xfff8000012345678ull, 0xbff0000000000000ull, 0x3ff0000000000000ull},
    {4, 0x3ff0000000000000ull, 0x7ff4000000000000ull, 0x10000000000000ull, 0x7ffc000000000000ull},
    {4, 0x3ff0000000000000ull, 0xfff0000000000001ull, 0xfff0000000000001ull, 0xfff8000000000001ull},
    {4, 0x3ff0000000000000ull, 0x1ull, 0x3ff0000000000000ull, 0x1ull},
    {4, 0x3ff0000000000000ull, 0x800fffffffffffffull, 0xbff0000000000000ull, 0x800fffffffffffffull},
    {4, 0x3ff0000000000000ull, 0x7fefffffffffffffull, 0xbff0000000000000ull, 0x3ff0000000000000ull},
    {4, 0x3ff0000000000000ull, 0x10000000000000ull, 0x10000000000000ull, 0x10000000000000ull},
    {4, 0xbff0000000000000ull, 0x0ull, 0xbff0000000000000ull, 0xbff0000000000000ull},
    {4, 0xbff0000000000000ull, 0x8000000000000000ull, 0x3ff0000000000000ull, 0xbff0000000000000ull},
    {4, 0xbff0000000000000ull, 0x3ff0000000000000ull, 0x7ff8000000000000ull, 0xbff0000000000000ull},
    {4, 0xbff0000000000000ull, 0xbff0000000000000ull, 0xbff0000000000000ull, 0xbff0000000000000ull},
    {4, 0xbff0000000000000ull, 0x3ff8000000000000ull, 0xbff0000000000000ull, 0xbff0000000000000ull},
    {4, 0xbff0000000000000ull, 0xc004000000000000ull, 0x3ff0000000000000ull, 0xc004000000000000ull},
    {4, 0xbff0000000000000ull, 0x7ff0000000000000ull, 0x1ull, 0xbff0000000000000ull},
    {4, 0xbff0000000000000ull, 0xfff0000000000000ull, 0xfff0000000000000ull, 0xfff0000000000000ull},
    {4, 0xbff0000000000000ull, 0x7ff8000000000000ull, 0xbff0000000000000ull, 0xbff0000000000000ull},
    {4, 0xbff0000000000000ull, 0xfff8000012345678ull, 0x3ff0000000000000ull, 0xbff0000000000000ull},
    {4, 0xbff0000000000000ull, 0x7ff4000000000000ull, 0x0ull, 0x7ffc000000000000ull},
    {4, 0xbff0000000000000ull, 0xfff0000000000001ull, 0xfff0000000000001ull, 0xfff8000000000001ull},
    {4, 0xbff0000000000000ull, 0x1ull, 0xbff0000000000000ull, 0xbff0000000000000ull},
    {4, 0xbff0000000000000ull, 0x800fffffffffffffull, 0x3ff0000000000000ull, 0xbff0000000000000ull},
    {4, 0xbff0000000000000ull, 0x7fefffffffffffffull, 0x3ff8000000000000ull, 0xbff0000000000000ull},
    {4, 0xbff0000000000000ull, 0x10000000000000ull, 0x10000000000000ull, 0xbff0000000000000ull},
    {5, 0x0ull, 0x0ull, 0x0ull, 0x0ull},
    {5, 0x0ull, 0x8000000000000000ull, 0x8000000000000000ull, 0x0ull},
    {5, 0x0ull, 0x3ff0000000000000ull, 0xc004000000000000ull, 0x3ff0000000000000ull},
    {5, 0x0ull, 0xbff0000000000000ull, 0xbff0000000000000ull, 0x0ull},
    {5, 0x0ull, 0x3ff8000000000000ull, 0x0ull, 0x3ff8000000000000ull},
    {5, 0x0ull, 0xc004000000000000ull, 0x8000000000000000ull, 0x0ull},
    {5, 0x0ull, 0x7ff0000000000000ull, 0xfff8000012345678ull, 0x7ff0000000000000ull},
    {5, 0x0ull, 0xfff0000000000000ull, 0xfff0000000000000ull, 0x0ull},
    {5, 0x0ull, 0x7ff8000000000000ull, 0x0ull, 0x0ull},
    {5, 0x0ull, 0xfff8000012345678ull, 0x8000000000000000ull, 0x0ull},
    {5, 0x0ull, 0x7ff4000000000000ull, 0x800fffffffffffffull, 0x7ffc000000000000ull},
    {5, 0x0ull, 0xfff0000000000001ull, 0xfff0000000000001ull, 0xfff8000000000001ull},
    {5, 0x0ull, 0x1ull, 0x0ull, 0x1ull},
    {5, 0x0ull, 0x800fffffffffffffull, 0x8000000000000000ull, 0x0ull},
    {5, 0x0ull, 0x7fefffffffffffffull, 0x8000000000000000ull, 0x7fefffffffffffffull},
    {5, 0x0ull, 0x10000000000000ull, 0x10000000000000ull, 0x10000000000000ull},
    {5, 0x8000000000000000ull, 0x0ull, 0x8000000000000000ull, 0x0ull},
    {5, 0x8000000000000000ull, 0x8000000000000000ull, 0x0ull, 0x8000000000000000ull},
    {5, 0x8000000000000000ull, 0x3ff0000000000000ull, 0x7ff0000000000000ull, 0x3ff0000000000000ull},
    {5, 0x8000000000000000ull, 0xbff0000000000000ull, 0xbff0000000000000ull, 0x8000000000000000ull},
    {5, 0x8000000000000000ull, 0x3ff8000000000000ull, 0x8000000000000000ull, 0x3ff8000000000000ull},
    {5, 0x8000000000000000ull, 0xc004000000000000ull, 0x0ull, 0x8000000000000000ull},
    {5, 0x8000000000000000ull, 0x7ff0000000000000ull, 0x7ff4000000000000ull, 0x7ff0000000000000ull},
    {5, 0x8000000000000000ull, 0xfff0000000000000ull, 0xfff0000000000000ull, 0x8000000000000000ull},
    {5, 0x8000000000000000ull, 0x7ff8000000000000ull, 0x8000000000000000ull, 0x8000000000000000ull},
    {5, 0x8000000000000000ull, 0xfff8000012345678ull, 0x0ull, 0x8000000000000000ull},
    {5, 0x8000000000000000ull, 0x7ff4000000000000ull, 0x7fefffffffffffffull, 0x7ffc000000000000ull},
    {5, 0x8000000000000000ull, 0xfff0000000000001ull, 0xfff0000000000001ull, 0xfff8000000000001ull},
    {5, 0x8000000000000000ull, 0x1ull, 0x8000000000000000ull, 0x1ull},
    {5, 0x8000000000000000ull, 0x800fffffffffffffull, 0x0ull, 0x8000000000000000ull},
    {5, 0x8000000000000000ull, 0x7fefffffffffffffull, 0x3ff0000000000000ull, 0x7fefffffffffffffull},
    {5, 0x8000000000000000ull, 0x10000000000000ull, 0x10000000000000ull, 0x10000000000000ull},
    {5, 0x3ff0000000000000ull, 0x0ull, 0x3ff0000000000000ull, 0x3ff0000000000000ull},
    {5, 0x3ff0000000000000ull, 0x8000000000000000ull, 0xbff0000000000000ull, 0x3ff0000000000000ull},
    {5, 0x3ff0000000000000ull, 0x3ff0000000000000ull, 0xfff0000000000000ull, 0x3ff0000000000000ull},
    {5, 0x3ff0000000000000ull, 0xbff0000000000000ull, 0xbff0000000000000ull, 0x3ff0000000000000ull},
    {5, 0x3ff0000000000000ull, 0x3ff8000000000000ull, 0x3ff0000000000000ull, 0x3ff8000000000000ull},
    {5, 0x3ff0000000000000ull, 0xc004000000000000ull, 0xbff0000000000000ull, 0x3ff0000000000000ull},
    {5, 0x3ff0000000000000ull, 0x7ff0000000000000ull, 0xfff0000000000001ull, 0x7ff0000000000000ull},
    {5, 0x3ff0000000000000ull, 0xfff0000000000000ull, 0xfff0000000000000ull, 0x3ff0000000000000ull},
    {5, 0x3ff0000000000000ull, 0x7ff8000000000000ull, 0x3ff0000000000000ull, 0x3ff0000000000000ull},
    {5, 0x3ff0000000000000ull, 0xfff8000012345678ull, 0xbff0000000000000ull, 0x3ff0000000000000ull},
    {5, 0x3ff0000000000000ull, 0x7ff4000000000000ull, 0x10000000000000ull, 0x7ffc000000000000ull},
    {5, 0x3ff0000000000000ull, 0xfff0000000000001ull, 0xfff0000000000001ull, 0xfff8000000000001ull},
    {5, 0x3ff0000000000000ull, 0x1ull, 0x3ff0000000000000ull, 0x3ff0000000000000ull},
    {5, 0x3ff0000000000000ull, 0x800fffffffffffffull, 0xbff0000000000000ull, 0x3ff0000000000000ull},
    {5, 0x3ff0000000000000ull, 0x7fefffffffffffffull, 0xbff0000000000000ull, 0x7fefffffffffffffull},
    {5, 0x3ff0000000000000ull, 0x10000000000000ull, 0x10000000000000ull, 0x3ff0000000000000ull},
    {5, 0xbff0000000000000ull, 0x0ull, 0xbff0000000000000ull, 0x0ull},
    {5, 0xbff0000000000000ull, 0x8000000000000000ull, 0x3ff0000000000000ull, 0x8000000000000000ull},
    {5, 0xbff0000000000000ull, 0x3ff0000000000000ull, 0x7ff8000000000000ull, 0x3ff0000000000000ull},
    {5, 0xbff0000000000000ull, 0xbff0000000000000ull, 0xbff0000000000000ull, 0xbff0000000000000ull},
    {5, 0xbff0000000000000ull, 0x3ff8000000000000ull, 0xbff0000000000000ull, 0x3ff8000000000000ull},
    {5, 0xbff0000000000000ull, 0xc004000000000000ull, 0x3ff0000000000000ull, 0xbff0000000000000ull},
    {5, 0xbff0000000000000ull, 0x7ff0000000000000ull, 0x1ull, 0x7ff0000000000000ull},
    {5, 0xbff0000000000000ull, 0xfff0000000000000ull, 0xfff0000000000000ull, 0xbff0000000000000ull},
    {5, 0xbff0000000000000ull, 0x7ff8000000000000ull, 0xbff0000000000000ull, 0xbff0000000000000ull},
    {5, 0xbff0000000000000ull, 0xfff8000012345678ull, 0x3ff0000000000000ull, 0xbff0000000000000ull},
    {5, 0xbff0000000000000ull, 0x7ff4000000000000ull, 0x0ull, 0x7ffc000000000000ull},
    {5, 0xbff0000000000000ull, 0xfff0000000000001ull, 0xfff0000000000001ull, 0xfff8000000000001ull},
    {5, 0xbff0000000000000ull, 0x1ull, 0xbff0000000000000ull, 0x1ull},
    {5, 0xbff0000000000000ull, 0x800fffffffffffffull, 0x3ff0000000000000ull, 0x800fffffffffffffull},
    {5, 0xbff0000000000000ull, 0x7fefffffffffffffull, 0x3ff8000000000000ull, 0x7fefffffffffffffull},
    {5, 0xbff0000000000000ull, 0x10000000000000ull, 0x10000000000000ull, 0x10000000000000ull},
    {6, 0x0ull, 0x0ull, 0x0ull, 0x0ull},
    {6, 0x0ull, 0x1ull, 0x8000000000000000ull, 0x1ull},
    {6, 0x0ull, 0x7fffffffffffffffull, 0x7fffffffffffffffull, 0x1ull},
    {6, 0x1ull, 0x0ull, 0x1ull, 0x0ull},
    {6, 0x1ull, 0x1ull, 0x8000000000000001ull, 0x0ull},
    {6, 0x1ull, 0x2ull, 0xfffffffffffffffeull, 0x2ull},
    {6, 0x1ull, 0x7fffffffffffffffull, 0x7fffffffffffffffull, 0x2ull},
    {6, 0x7fffffffffffffffull, 0x0ull, 0x7fffffffffffffffull, 0x0ull},
    {6, 0x0ull, 0x2ull, 0xffffffffffffffffull, 0x1ull},
    {6, 0x0ull, 0x8000000000000000ull, 0x0ull, 0x1ull},
    {6, 0x0ull, 0xffffffffffffffffull, 0x8000000000000000ull, 0x1ull},
    {6, 0x0ull, 0xfffffffffffffffeull, 0x100000001ull, 0x1ull},
    {6, 0x0ull, 0xffffffffull, 0xffffffffull, 0x1ull},
    {6, 0x0ull, 0x100000000ull, 0x0ull, 0x1ull},
    {6, 0x0ull, 0x100000001ull, 0x8000000000000000ull, 0x1ull},
    {6, 0x0ull, 0x123456789abcdef0ull, 0x3ull, 0x1ull},
    {6, 0x0ull, 0xfffffffeffffffffull, 0xfffffffeffffffffull, 0x1ull},
    {6, 0x0ull, 0xfffffffeull, 0x0ull, 0x1ull},
    {6, 0x0ull, 0x3ull, 0x8000000000000000ull, 0x1ull},
    {6, 0x0ull, 0x80000000ull, 0x1ull, 0x1ull},
    {6, 0x0ull, 0x7fffffffull, 0x7fffffffull, 0x1ull},
    {6, 0x1ull, 0x8000000000000000ull, 0x1ull, 0x2ull},
    {6, 0x1ull, 0xffffffffffffffffull, 0x8000000000000001ull, 0x2ull},
    {6, 0x1ull, 0xfffffffffffffffeull, 0x123456789abcdef0ull, 0x2ull},
    {6, 0x1ull, 0xffffffffull, 0xffffffffull, 0x2ull},
    {6, 0x1ull, 0x100000000ull, 0x1ull, 0x2ull},
    {6, 0x1ull, 0x100000001ull, 0x8000000000000001ull, 0x2ull},
    {6, 0x1ull, 0x123456789abcdef0ull, 0x80000000ull, 0x2ull},
    {6, 0x1ull, 0xfffffffeffffffffull, 0xfffffffeffffffffull, 0x2ull},
    {6, 0x1ull, 0xfffffffeull, 0x1ull, 0x2ull},
    {6, 0x1ull, 0x3ull, 0x8000000000000001ull, 0x2ull},
    {6, 0x1ull, 0x80000000ull, 0x2ull, 0x2ull},
    {6, 0x1ull, 0x7fffffffull, 0x7fffffffull, 0x2ull},
    {6, 0x2ull, 0x0ull, 0x2ull, 0x0ull},
    {6, 0x2ull, 0x1ull, 0x8000000000000002ull, 0x0ull},
    {6, 0x2ull, 0x2ull, 0xffffffffull, 0x0ull},
    {6, 0x2ull, 0x7fffffffffffffffull, 0x7fffffffffffffffull, 0x3ull},
    {6, 0x2ull, 0x8000000000000000ull, 0x2ull, 0x3ull},
    {6, 0x2ull, 0xffffffffffffffffull, 0x8000000000000002ull, 0x3ull},
    {6, 0x2ull, 0xfffffffffffffffeull, 0xfffffffeffffffffull, 0x3ull},
    {6, 0x2ull, 0xffffffffull, 0xffffffffull, 0x3ull},
    {6, 0x2ull, 0x100000000ull, 0x2ull, 0x3ull},
    {6, 0x2ull, 0x100000001ull, 0x8000000000000002ull, 0x3ull},
    {6, 0x2ull, 0x123456789abcdef0ull, 0x7fffffffull, 0x3ull},
    {6, 0x2ull, 0xfffffffeffffffffull, 0xfffffffeffffffffull, 0x3ull},
    {6, 0x2ull, 0xfffffffeull, 0x2ull, 0x3ull},
    {6, 0x2ull, 0x3ull, 0x8000000000000002ull, 0x3ull},
    {6, 0x2ull, 0x80000000ull, 0x7fffffffffffffffull, 0x3ull},
    {6, 0x2ull, 0x7fffffffull, 0x7fffffffull, 0x3ull},
    {6, 0x7fffffffffffffffull, 0x1ull, 0xffffffffffffffffull, 0x0ull},
    {6, 0x7fffffffffffffffull, 0x2ull, 0x100000000ull, 0x0ull},
    {6, 0x7fffffffffffffffull, 0x7fffffffffffffffull, 0x7fffffffffffffffull, 0x0ull},
    {6, 0x7fffffffffffffffull, 0x8000000000000000ull, 0x7fffffffffffffffull, 0x8000000000000000ull},
    {6, 0x7fffffffffffffffull, 0xffffffffffffffffull, 0xffffffffffffffffull, 0x8000000000000000ull},
    {6, 0x7fffffffffffffffull, 0xfffffffffffffffeull, 0xfffffffeull, 0x8000000000000000ull},
    {6, 0x7fffffffffffffffull, 0xffffffffull, 0xffffffffull, 0x0ull},
    {6, 0x7fffffffffffffffull, 0x100000000ull, 0x7fffffffffffffffull, 0x0ull},
    {6, 0x7fffffffffffffffull, 0x100000001ull, 0xffffffffffffffffull, 0x0ull},
    {6, 0x7fffffffffffffffull, 0x123456789abcdef0ull, 0x0ull, 0x0ull},
    {6, 0x7fffffffffffffffull, 0xfffffffeffffffffull, 0xfffffffeffffffffull, 0x8000000000000000ull},
    {6, 0x7fffffffffffffffull, 0xfffffffeull, 0x7fffffffffffffffull, 0x0ull},
    {6, 0x7fffffffffffffffull, 0x3ull, 0xffffffffffffffffull, 0x0ull},
    {6, 0x7fffffffffffffffull, 0x80000000ull, 0x8000000000000000ull, 0x0ull},
    {6, 0x7fffffffffffffffull, 0x7fffffffull, 0x7fffffffull, 0x0ull},
    {7, 0x0ull, 0x0ull, 0x0ull, 0x0ull},
    {7, 0x0ull, 0x1ull, 0x8000000000000000ull, 0x1ull},
    {7, 0x0ull, 0x7fffffffffffffffull, 0x7fffffffffffffffull, 0x7fffffffffffffffull},
    {7, 0x1ull, 0x0ull, 0x1ull, 0x0ull},
    {7, 0x1ull, 0x1ull, 0x8000000000000001ull, 0x0ull},
    {7, 0x1ull, 0x2ull, 0xfffffffffffffffeull, 0x0ull},
    {7, 0x1ull, 0x7fffffffffffffffull, 0x7fffffffffffffffull, 0x0ull},
    {7, 0x7fffffffffffffffull, 0x0ull, 0x7fffffffffffffffull, 0x0ull},
    {7, 0x0ull, 0x2ull, 0xffffffffffffffffull, 0x2ull},
    {7, 0x0ull, 0x8000000000000000ull, 0x0ull, 0x8000000000000000ull},
    {7, 0x0ull, 0xffffffffffffffffull, 0x8000000000000000ull, 0xffffffffffffffffull},
    {7, 0x0ull, 0xfffffffffffffffeull, 0x100000001ull, 0xfffffffffffffffeull},
    {7, 0x0ull, 0xffffffffull, 0xffffffffull, 0xffffffffull},
    {7, 0x0ull, 0x100000000ull, 0x0ull, 0x100000000ull},
    {7, 0x0ull, 0x100000001ull, 0x8000000000000000ull, 0x100000001ull},
    {7, 0x0ull, 0x123456789abcdef0ull, 0x3ull, 0x123456789abcdef0ull},
    {7, 0x0ull, 0xfffffffeffffffffull, 0xfffffffeffffffffull, 0xfffffffeffffffffull},
    {7, 0x0ull, 0xfffffffeull, 0x0ull, 0xfffffffeull},
    {7, 0x0ull, 0x3ull, 0x8000000000000000ull, 0x3ull},
    {7, 0x0ull, 0x80000000ull, 0x1ull, 0x80000000ull},
    {7, 0x0ull, 0x7fffffffull, 0x7fffffffull, 0x7fffffffull},
    {7, 0x1ull, 0x8000000000000000ull, 0x1ull, 0x0ull},
    {7, 0x1ull, 0xffffffffffffffffull, 0x8000000000000001ull, 0x0ull},
    {7, 0x1ull, 0xfffffffffffffffeull, 0x123456789abcdef0ull, 0x0ull},
    {7, 0x1ull, 0xffffffffull, 0xffffffffull, 0x0ull},
    {7, 0x1ull, 0x100000000ull, 0x1ull, 0x0ull},
    {7, 0x1ull, 0x100000001ull, 0x8000000000000001ull, 0x0ull},
    {7, 0x1ull, 0x123456789abcdef0ull, 0x80000000ull, 0x0ull},
    {7, 0x1ull, 0xfffffffeffffffffull, 0xfffffffeffffffffull, 0x0ull},
    {7, 0x1ull, 0xfffffffeull, 0x1ull, 0x0ull},
    {7, 0x1ull, 0x3ull, 0x8000000000000001ull, 0x0ull},
    {7, 0x1ull, 0x80000000ull, 0x2ull, 0x0ull},
    {7, 0x1ull, 0x7fffffffull, 0x7fffffffull, 0x0ull},
    {7, 0x2ull, 0x0ull, 0x2ull, 0x0ull},
    {7, 0x2ull, 0x1ull, 0x8000000000000002ull, 0x1ull},
    {7, 0x2ull, 0x2ull, 0xffffffffull, 0x1ull},
    {7, 0x2ull, 0x7fffffffffffffffull, 0x7fffffffffffffffull, 0x1ull},
    {7, 0x2ull, 0x8000000000000000ull, 0x2ull, 0x1ull},
    {7, 0x2ull, 0xffffffffffffffffull, 0x8000000000000002ull, 0x1ull},
    {7, 0x2ull, 0xfffffffffffffffeull, 0xfffffffeffffffffull, 0x1ull},
    {7, 0x2ull, 0xffffffffull, 0xffffffffull, 0x1ull},
    {7, 0x2ull, 0x100000000ull, 0x2ull, 0x1ull},
    {7, 0x2ull, 0x100000001ull, 0x8000000000000002ull, 0x1ull},
    {7, 0x2ull, 0x123456789abcdef0ull, 0x7fffffffull, 0x1ull},
    {7, 0x2ull, 0xfffffffeffffffffull, 0xfffffffeffffffffull, 0x1ull},
    {7, 0x2ull, 0xfffffffeull, 0x2ull, 0x1ull},
    {7, 0x2ull, 0x3ull, 0x8000000000000002ull, 0x1ull},
    {7, 0x2ull, 0x80000000ull, 0x7fffffffffffffffull, 0x1ull},
    {7, 0x2ull, 0x7fffffffull, 0x7fffffffull, 0x1ull},
    {7, 0x7fffffffffffffffull, 0x1ull, 0xffffffffffffffffull, 0x1ull},
    {7, 0x7fffffffffffffffull, 0x2ull, 0x100000000ull, 0x2ull},
    {7, 0x7fffffffffffffffull, 0x7fffffffffffffffull, 0x7fffffffffffffffull, 0x7ffffffffffffffeull},
    {7, 0x7fffffffffffffffull, 0x8000000000000000ull, 0x7fffffffffffffffull, 0x7ffffffffffffffeull},
    {7, 0x7fffffffffffffffull, 0xffffffffffffffffull, 0xffffffffffffffffull, 0x7ffffffffffffffeull},
    {7, 0x7fffffffffffffffull, 0xfffffffffffffffeull, 0xfffffffeull, 0x7ffffffffffffffeull},
    {7, 0x7fffffffffffffffull, 0xffffffffull, 0xffffffffull, 0xffffffffull},
    {7, 0x7fffffffffffffffull, 0x100000000ull, 0x7fffffffffffffffull, 0x100000000ull},
    {7, 0x7fffffffffffffffull, 0x100000001ull, 0xffffffffffffffffull, 0x100000001ull},
    {7, 0x7fffffffffffffffull, 0x123456789abcdef0ull, 0x0ull, 0x123456789abcdef0ull},
    {7, 0x7fffffffffffffffull, 0xfffffffeffffffffull, 0xfffffffeffffffffull, 0x7ffffffffffffffeull},
    {7, 0x7fffffffffffffffull, 0xfffffffeull, 0x7fffffffffffffffull, 0xfffffffeull},
    {7, 0x7fffffffffffffffull, 0x3ull, 0xffffffffffffffffull, 0x3ull},
    {7, 0x7fffffffffffffffull, 0x80000000ull, 0x8000000000000000ull, 0x80000000ull},
    {7, 0x7fffffffffffffffull, 0x7fffffffull, 0x7fffffffull, 0x7fffffffull}
}};

class GuestBlock {
public:
    explicit GuestBlock(bool writable) {
#ifdef _WIN32
        block = static_cast<std::uint8_t*>(VirtualAlloc(nullptr, BlockBytes, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE));
#else
        block = static_cast<std::uint8_t*>(std::aligned_alloc(BlockBytes, BlockBytes));
#endif
        Require(block != nullptr, "global float atomics: cannot allocate the guest block");
        GuestAllocations::Mutation().Add(block, BlockBytes, true, writable);
    }

    ~GuestBlock() {
        GuestAllocations::Mutation().Remove(block);
    }

    GuestBlock(const GuestBlock&) = delete;
    GuestBlock& operator=(const GuestBlock&) = delete;

    std::uint8_t* Data() { return block; }
    const std::uint8_t* Data() const { return block; }

private:
    std::uint8_t* block = nullptr;
};

void Put(std::vector<std::uint8_t>& image, std::uint32_t offset, std::uint64_t value, std::uint32_t bytes) {
    for (std::uint32_t byte = 0; byte < bytes; ++byte) image.at(offset + byte) = static_cast<std::uint8_t>(value >> (byte * 8u));
}

std::array<std::uint32_t, 4> BufferDescriptor(const void* data, std::uint32_t bytes) {
    const auto address = reinterpret_cast<std::uintptr_t>(data);
    return {static_cast<std::uint32_t>(address), static_cast<std::uint32_t>((address >> 32u) & 0xffffu), bytes, 0x31016facu};
}

std::string Hex(std::uint64_t value) {
    char text[24];
    std::snprintf(text, sizeof(text), "0x%llx", static_cast<unsigned long long>(value));
    return text;
}

const Row& RowOf(std::uint32_t op, std::uint32_t tid) {
    return Rows.at(op * Threads + tid);
}

std::uint32_t OpBytes(std::uint32_t op) {
    return op < NarrowOps ? 4u : 8u;
}

std::vector<std::uint8_t> Initial() {
    std::vector<std::uint8_t> image(BlockBytes, Fill);
    for (std::uint32_t op = 0; op < OpCount; ++op) {
        for (std::uint32_t tid = 0; tid < Threads; ++tid) {
            Put(image, op * OpStride + tid * 8u, RowOf(op, tid).memory, OpBytes(op));
        }
    }
    return image;
}

std::vector<std::uint8_t> Expected() {
    auto image = Initial();
    for (std::uint32_t op = 0; op < OpCount; ++op) {
        for (std::uint32_t tid = 0; tid < Threads; ++tid) {
            Put(image, op * OpStride + tid * 8u, RowOf(op, tid).final, OpBytes(op));
        }
    }
    return image;
}

void FillInput() {
    for (std::uint32_t op = 0; op < OpCount; ++op) {
        for (std::uint32_t tid = 0; tid < Threads; ++tid) {
            const auto& row = RowOf(op, tid);
            auto* words = &Input[(op * Threads + tid) * 4u];
            if (OpBytes(op) == 4u) {
                words[0] = static_cast<std::uint32_t>(row.data);
                words[1] = static_cast<std::uint32_t>(row.comparator);
                words[2] = 0u;
                words[3] = 0u;
            } else {
                words[0] = static_cast<std::uint32_t>(row.data);
                words[1] = static_cast<std::uint32_t>(row.data >> 32u);
                words[2] = static_cast<std::uint32_t>(row.comparator);
                words[3] = static_cast<std::uint32_t>(row.comparator >> 32u);
            }
        }
    }
}

void Dispatch(AgcDriver::VulkanDevice& device, std::uint32_t waveSize, const std::uint8_t* base) {
    const auto address = static_cast<std::uint64_t>(reinterpret_cast<std::uintptr_t>(base));
    std::vector<std::uint32_t> userData(10, 0u);
    const auto input = BufferDescriptor(Input.data(), static_cast<std::uint32_t>(Input.size() * 4u));
    const auto output = BufferDescriptor(Output.data(), static_cast<std::uint32_t>(Output.size() * 4u));
    std::copy(input.begin(), input.end(), userData.begin());
    std::copy(output.begin(), output.end(), userData.begin() + 4);
    userData[8] = static_cast<std::uint32_t>(address);
    userData[9] = static_cast<std::uint32_t>(address >> 32u);
    const std::span<const std::uint32_t> code(GlobalAtomicsFloatCode);
    const std::array<ShaderRecompiler::MemoryRegion, 1> memory{{{reinterpret_cast<std::uintptr_t>(code.data()), std::as_bytes(code)}}};
    const ShaderRecompiler::ShaderComputeStageInfo compute{{Threads, 1, 1}, 0, {false, false, false}, false, 1};
    ShaderRecompiler::RecompileRequest request{
        {ShaderStage::Compute, reinterpret_cast<std::uintptr_t>(code.data()), code, 0, {}},
        {waveSize, 0, userData, compute, std::nullopt, std::nullopt, memory},
        device.Target(),
        {0, 0, 0, 128}
    };
    request.useCache = false;
    const auto result = ShaderRecompiler::Recompile(request);
    device.Dispatch(result, 1, 1, 1, {}, reinterpret_cast<std::uintptr_t>(code.data()));
    device.WaitIdle();
}

void RunAtomics(AgcDriver::VulkanDevice& device, GuestBlock& guest, std::uint32_t waveSize) {
    const auto initial = Initial();
    std::memcpy(guest.Data(), initial.data(), BlockBytes);
    FillInput();
    Output.fill(Sentinel);
    Dispatch(device, waveSize, guest.Data());
    const auto wave = "global float atomics: wave" + std::to_string(waveSize) + " ";
    for (std::uint32_t op = 0; op < OpCount; ++op) {
        for (std::uint32_t tid = 0; tid < Threads; ++tid) {
            const auto& row = RowOf(op, tid);
            const auto* words = &Output[(op * Threads + tid) * 2u];
            const auto returned = OpBytes(op) == 4u ? static_cast<std::uint64_t>(words[0]) : (static_cast<std::uint64_t>(words[1]) << 32u) | words[0];
            Require(returned == row.memory, wave + "op " + std::to_string(op) + " lane " + std::to_string(tid) + " returned " + Hex(returned) + ", expected " + Hex(row.memory));
        }
    }
    const auto expected = Expected();
    for (std::uint32_t op = 0; op < OpCount; ++op) {
        for (std::uint32_t tid = 0; tid < Threads; ++tid) {
            const auto offset = op * OpStride + tid * 8u;
            std::uint64_t actual = 0;
            std::memcpy(&actual, guest.Data() + offset, OpBytes(op));
            const auto& row = RowOf(op, tid);
            Require(actual == row.final, wave + "op " + std::to_string(op) + " lane " + std::to_string(tid) + " memory " + Hex(row.memory) + " data " + Hex(row.data) + " comparator " + Hex(row.comparator) + " is " + Hex(actual) + ", expected " + Hex(row.final));
        }
    }
    for (std::uint32_t offset = 0; offset < BlockBytes; ++offset) {
        const auto actual = guest.Data()[offset];
        Require(actual == expected[offset], wave + "byte " + std::to_string(offset) + " is " + std::to_string(actual) + ", expected " + std::to_string(expected[offset]));
    }
}

void RunReadOnly(AgcDriver::VulkanDevice& device, GuestBlock& guest) {
    const auto initial = Initial();
    std::memcpy(guest.Data(), initial.data(), BlockBytes);
    FillInput();
    Dispatch(device, 32, guest.Data());
    for (std::uint32_t offset = 0; offset < BlockBytes; ++offset) {
        Require(guest.Data()[offset] == initial[offset], "global float atomics: an atomic into a read-only range changed byte " + std::to_string(offset));
    }
}

const Testing::Case wave32Atomics{"GlobalAtomicsFloat_Wave32_ReturnPreviousAndStoreResult", [] {
    auto& device = SharedVulkanTestDevice();
    SkipUnlessCapability(device.Target(), spv::CapabilityInt64Atomics, "the device has no shaderBufferInt64Atomics");
    GuestBlock writable(true);
    RunAtomics(device, writable, 32);
}};

const Testing::Case wave64Atomics{"GlobalAtomicsFloat_Wave64_ReturnPreviousAndStoreResult", [] {
    auto& device = SharedVulkanTestDevice();
    SkipUnlessCapability(device.Target(), spv::CapabilityInt64Atomics, "the device has no shaderBufferInt64Atomics");
    GuestBlock writable(true);
    RunAtomics(device, writable, 64);
}};

const Testing::Case readOnlyRange{"GlobalAtomicsFloat_ReadOnlyRange_IsLeftUnchanged", [] {
    auto& device = SharedVulkanTestDevice();
    SkipUnlessCapability(device.Target(), spv::CapabilityInt64Atomics, "the device has no shaderBufferInt64Atomics");
    GuestBlock readOnly(false);
    RunReadOnly(device, readOnly);
}};

} // namespace
