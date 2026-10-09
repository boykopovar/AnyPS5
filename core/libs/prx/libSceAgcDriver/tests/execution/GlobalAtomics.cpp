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
#include <iostream>
#include <string>
#include <utility>
#include <vector>

namespace {

using AgcDriver::Graphics::Require;
using ShaderRecompiler::ShaderStage;

constexpr std::uint32_t Threads = 64;
constexpr std::uint32_t RowsPerOp = 16;
constexpr std::uint32_t OpCount = 13;
constexpr std::uint32_t NarrowOps = 13;
constexpr std::uint32_t OpStride = 0x200;
constexpr std::uint32_t Special = OpCount * OpStride;
constexpr std::uint32_t Sentinel = 0x05e471e1;
constexpr std::size_t BlockBytes = 65536;
constexpr std::uint8_t Fill = 0xcd;
alignas(256) std::array<std::uint32_t, OpCount * Threads * 4> Input{};
alignas(256) std::array<std::uint32_t, (OpCount + 2) * Threads * 2> Output{};

alignas(256) constexpr std::array<std::uint32_t, 187> GlobalAtomicsCode{
    0x34020083, 0x34040084, 0x340a0083, 0x7e2802ff, 0x05e471e1, 0x7e2a02ff, 0x05e471e1, 0x4a060280,
    0x4a080480, 0xe0381000, 0x80000a04, 0xbf8c3f70, 0x4a0c0a80, 0xdcc18000, 0x14080a03, 0xbf8c3f70,
    0xe0701000, 0x80011406, 0x4a0602ff, 0x00000200, 0x4a0804ff, 0x00000400, 0xe0381000, 0x80000a04,
    0xbf8c3f70, 0x4a0c0aff, 0x00000200, 0xdcc58000, 0x14080a03, 0xbf8c3f70, 0xe0701000, 0x80011406,
    0x4a0602ff, 0x00000400, 0x4a0804ff, 0x00000800, 0xe0381000, 0x80000a04, 0xbf8c3f70, 0x4a0c0aff,
    0x00000400, 0xdcc98000, 0x14080a03, 0xbf8c3f70, 0xe0701000, 0x80011406, 0x4a0602ff, 0x00000600,
    0x4a0804ff, 0x00000c00, 0xe0381000, 0x80000a04, 0xbf8c3f70, 0x4a0c0aff, 0x00000600, 0xdccd8000,
    0x14080a03, 0xbf8c3f70, 0xe0701000, 0x80011406, 0x4a0602ff, 0x00000800, 0x4a0804ff, 0x00001000,
    0xe0381000, 0x80000a04, 0xbf8c3f70, 0x4a0c0aff, 0x00000800, 0xdcd58000, 0x14080a03, 0xbf8c3f70,
    0xe0701000, 0x80011406, 0x4a0602ff, 0x00000a00, 0x4a0804ff, 0x00001400, 0xe0381000, 0x80000a04,
    0xbf8c3f70, 0x4a0c0aff, 0x00000a00, 0xdcd98000, 0x14080a03, 0xbf8c3f70, 0xe0701000, 0x80011406,
    0x4a0602ff, 0x00000c00, 0x4a0804ff, 0x00001800, 0xe0381000, 0x80000a04, 0xbf8c3f70, 0x4a0c0aff,
    0x00000c00, 0xdcdd8000, 0x14080a03, 0xbf8c3f70, 0xe0701000, 0x80011406, 0x4a0602ff, 0x00000e00,
    0x4a0804ff, 0x00001c00, 0xe0381000, 0x80000a04, 0xbf8c3f70, 0x4a0c0aff, 0x00000e00, 0xdce18000,
    0x14080a03, 0xbf8c3f70, 0xe0701000, 0x80011406, 0x4a0602ff, 0x00001000, 0x4a0804ff, 0x00002000,
    0xe0381000, 0x80000a04, 0xbf8c3f70, 0x4a0c0aff, 0x00001000, 0xdce58000, 0x14080a03, 0xbf8c3f70,
    0xe0701000, 0x80011406, 0x4a0602ff, 0x00001200, 0x4a0804ff, 0x00002400, 0xe0381000, 0x80000a04,
    0xbf8c3f70, 0x4a0c0aff, 0x00001200, 0xdce98000, 0x14080a03, 0xbf8c3f70, 0xe0701000, 0x80011406,
    0x4a0602ff, 0x00001400, 0x4a0804ff, 0x00002800, 0xe0381000, 0x80000a04, 0xbf8c3f70, 0x4a0c0aff,
    0x00001400, 0xdced8000, 0x14080a03, 0xbf8c3f70, 0xe0701000, 0x80011406, 0x4a0602ff, 0x00001600,
    0x4a0804ff, 0x00002c00, 0xe0381000, 0x80000a04, 0xbf8c3f70, 0x4a0c0aff, 0x00001600, 0xdcf18000,
    0x14080a03, 0xbf8c3f70, 0xe0701000, 0x80011406, 0x4a0602ff, 0x00001800, 0x4a0804ff, 0x00003000,
    0xe0381000, 0x80000a04, 0xbf8c3f70, 0x4a0c0aff, 0x00001800, 0xdcf58000, 0x14080a03, 0xbf8c3f70,
    0xe0701000, 0x80011406, 0xbf810000
};

struct Row {
    std::uint32_t op;
    std::uint64_t memory;
    std::uint64_t data;
    std::uint64_t comparator;
    std::uint64_t returned;
    std::uint64_t final;
};

constexpr std::array<Row, 208> Rows{{
    {0, 0x0ull, 0x1ull, 0x0ull, 0x0ull, 0x1ull},
    {0, 0x1ull, 0x80000000ull, 0xf9677975ull, 0x1ull, 0x80000000ull},
    {0, 0x2ull, 0xffffffffull, 0x2ull, 0x2ull, 0xffffffffull},
    {0, 0x7fffffffull, 0x0ull, 0xfc5039f7ull, 0x7fffffffull, 0x0ull},
    {0, 0x80000000ull, 0x7fffffffull, 0x80000000ull, 0x80000000ull, 0x7fffffffull},
    {0, 0x80000001ull, 0xfffffffeull, 0x1098ad65ull, 0x80000001ull, 0xfffffffeull},
    {0, 0xfffffffeull, 0xdeadbeefull, 0xfffffffeull, 0xfffffffeull, 0xdeadbeefull},
    {0, 0xffffffffull, 0x2ull, 0x19211eull, 0xffffffffull, 0x2ull},
    {0, 0x12345678ull, 0x80000001ull, 0x12345678ull, 0x12345678ull, 0x80000001ull},
    {0, 0xdeadbeefull, 0x12345678ull, 0xcdbc2be9ull, 0xdeadbeefull, 0x12345678ull},
    {0, 0x0ull, 0x1ull, 0x0ull, 0x0ull, 0x1ull},
    {0, 0x1ull, 0x80000000ull, 0xa3639922ull, 0x1ull, 0x80000000ull},
    {0, 0x46e9a894ull, 0xd107eea9ull, 0x46e9a894ull, 0x46e9a894ull, 0xd107eea9ull},
    {0, 0xbc50bcc8ull, 0x58ffc3ecull, 0x4cd661ull, 0xbc50bcc8ull, 0x58ffc3ecull},
    {0, 0xb9093c7eull, 0x665932a4ull, 0xb9093c7eull, 0xb9093c7eull, 0x665932a4ull},
    {0, 0xd88449d9ull, 0xfe358db5ull, 0xba80659full, 0xd88449d9ull, 0xfe358db5ull},
    {1, 0x0ull, 0x1ull, 0x0ull, 0x0ull, 0x1ull},
    {1, 0x1ull, 0x80000000ull, 0x109949f9ull, 0x1ull, 0x1ull},
    {1, 0x2ull, 0xffffffffull, 0x2ull, 0x2ull, 0xffffffffull},
    {1, 0x7fffffffull, 0x0ull, 0x22d348c2ull, 0x7fffffffull, 0x7fffffffull},
    {1, 0x80000000ull, 0x7fffffffull, 0x80000000ull, 0x80000000ull, 0x7fffffffull},
    {1, 0x80000001ull, 0xfffffffeull, 0x42872e08ull, 0x80000001ull, 0x80000001ull},
    {1, 0xfffffffeull, 0xdeadbeefull, 0xfffffffeull, 0xfffffffeull, 0xdeadbeefull},
    {1, 0xffffffffull, 0x2ull, 0x675c2152ull, 0xffffffffull, 0xffffffffull},
    {1, 0x12345678ull, 0x80000001ull, 0x12345678ull, 0x12345678ull, 0x80000001ull},
    {1, 0xdeadbeefull, 0x12345678ull, 0x2f1ed38ull, 0xdeadbeefull, 0xdeadbeefull},
    {1, 0x0ull, 0x1ull, 0x0ull, 0x0ull, 0x1ull},
    {1, 0x1ull, 0x80000000ull, 0xd6be8bc1ull, 0x1ull, 0x1ull},
    {1, 0x283d5502ull, 0xfc565b67ull, 0x283d5502ull, 0x283d5502ull, 0xfc565b67ull},
    {1, 0xe503313aull, 0x1722ca72ull, 0xdd44c959ull, 0xe503313aull, 0xe503313aull},
    {1, 0x2a841e2ull, 0x52a450e2ull, 0x2a841e2ull, 0x2a841e2ull, 0x52a450e2ull},
    {1, 0xcc440ac9ull, 0x46313f70ull, 0xee4a57bbull, 0xcc440ac9ull, 0xcc440ac9ull},
    {2, 0x0ull, 0x1ull, 0x0ull, 0x0ull, 0x1ull},
    {2, 0x1ull, 0x80000000ull, 0x452f2ffcull, 0x1ull, 0x80000001ull},
    {2, 0x2ull, 0xffffffffull, 0x2ull, 0x2ull, 0x1ull},
    {2, 0x7fffffffull, 0x0ull, 0xf5091963ull, 0x7fffffffull, 0x7fffffffull},
    {2, 0x80000000ull, 0x7fffffffull, 0x80000000ull, 0x80000000ull, 0xffffffffull},
    {2, 0x80000001ull, 0xfffffffeull, 0xeb165c4ull, 0x80000001ull, 0x7fffffffull},
    {2, 0xfffffffeull, 0xdeadbeefull, 0xfffffffeull, 0xfffffffeull, 0xdeadbeedull},
    {2, 0xffffffffull, 0x2ull, 0x1a438e52ull, 0xffffffffull, 0x1ull},
    {2, 0x12345678ull, 0x80000001ull, 0x12345678ull, 0x12345678ull, 0x92345679ull},
    {2, 0xdeadbeefull, 0x12345678ull, 0x64638c03ull, 0xdeadbeefull, 0xf0e21567ull},
    {2, 0x0ull, 0x1ull, 0x0ull, 0x0ull, 0x1ull},
    {2, 0x1ull, 0x80000000ull, 0xe6d2996dull, 0x1ull, 0x80000001ull},
    {2, 0xa9bdd75aull, 0x70734273ull, 0xa9bdd75aull, 0xa9bdd75aull, 0x1a3119cdull},
    {2, 0x8aa692d0ull, 0x74d3a033ull, 0x5a7fd942ull, 0x8aa692d0ull, 0xff7a3303ull},
    {2, 0x201fe536ull, 0xf8ddb196ull, 0x201fe536ull, 0x201fe536ull, 0x18fd96ccull},
    {2, 0x15f79627ull, 0xb2346d27ull, 0x8a6b642dull, 0x15f79627ull, 0xc82c034eull},
    {3, 0x0ull, 0x1ull, 0x0ull, 0x0ull, 0xffffffffull},
    {3, 0x1ull, 0x80000000ull, 0x69639719ull, 0x1ull, 0x80000001ull},
    {3, 0x2ull, 0xffffffffull, 0x2ull, 0x2ull, 0x3ull},
    {3, 0x7fffffffull, 0x0ull, 0x89e34c92ull, 0x7fffffffull, 0x7fffffffull},
    {3, 0x80000000ull, 0x7fffffffull, 0x80000000ull, 0x80000000ull, 0x1ull},
    {3, 0x80000001ull, 0xfffffffeull, 0x73229920ull, 0x80000001ull, 0x80000003ull},
    {3, 0xfffffffeull, 0xdeadbeefull, 0xfffffffeull, 0xfffffffeull, 0x2152410full},
    {3, 0xffffffffull, 0x2ull, 0x66310986ull, 0xffffffffull, 0xfffffffdull},
    {3, 0x12345678ull, 0x80000001ull, 0x12345678ull, 0x12345678ull, 0x92345677ull},
    {3, 0xdeadbeefull, 0x12345678ull, 0x8fa4a357ull, 0xdeadbeefull, 0xcc796877ull},
    {3, 0x0ull, 0x1ull, 0x0ull, 0x0ull, 0xffffffffull},
    {3, 0x1ull, 0x80000000ull, 0x64bdad88ull, 0x1ull, 0x80000001ull},
    {3, 0xd5b148e7ull, 0x91d4c68ull, 0xd5b148e7ull, 0xd5b148e7ull, 0xcc93fc7full},
    {3, 0xb6cd96e4ull, 0xa145e36full, 0x6efd9be0ull, 0xb6cd96e4ull, 0x1587b375ull},
    {3, 0xf123997cull, 0x12d3d18aull, 0xf123997cull, 0xf123997cull, 0xde4fc7f2ull},
    {3, 0x62bfff89ull, 0x2106fea7ull, 0x22db0452ull, 0x62bfff89ull, 0x41b900e2ull},
    {4, 0x0ull, 0x1ull, 0x0ull, 0x0ull, 0x0ull},
    {4, 0x1ull, 0x80000000ull, 0xdbbb69abull, 0x1ull, 0x80000000ull},
    {4, 0x2ull, 0xffffffffull, 0x2ull, 0x2ull, 0xffffffffull},
    {4, 0x7fffffffull, 0x0ull, 0xdffce4e3ull, 0x7fffffffull, 0x0ull},
    {4, 0x80000000ull, 0x7fffffffull, 0x80000000ull, 0x80000000ull, 0x80000000ull},
    {4, 0x80000001ull, 0xfffffffeull, 0x77ac152aull, 0x80000001ull, 0x80000001ull},
    {4, 0xfffffffeull, 0xdeadbeefull, 0xfffffffeull, 0xfffffffeull, 0xdeadbeefull},
    {4, 0xffffffffull, 0x2ull, 0x24880f60ull, 0xffffffffull, 0xffffffffull},
    {4, 0x12345678ull, 0x80000001ull, 0x12345678ull, 0x12345678ull, 0x80000001ull},
    {4, 0xdeadbeefull, 0x12345678ull, 0x6adc5e6full, 0xdeadbeefull, 0xdeadbeefull},
    {4, 0x0ull, 0x1ull, 0x0ull, 0x0ull, 0x0ull},
    {4, 0x1ull, 0x80000000ull, 0x5c5e447eull, 0x1ull, 0x80000000ull},
    {4, 0xbc193f27ull, 0x5b2c838cull, 0xbc193f27ull, 0xbc193f27ull, 0xbc193f27ull},
    {4, 0xa69c5ce5ull, 0xbf61575eull, 0x341a640bull, 0xa69c5ce5ull, 0xa69c5ce5ull},
    {4, 0x1836851cull, 0x12fef585ull, 0x1836851cull, 0x1836851cull, 0x12fef585ull},
    {4, 0xaa5c538dull, 0x618e48b9ull, 0x56002b6cull, 0xaa5c538dull, 0xaa5c538dull},
    {5, 0x0ull, 0x1ull, 0x0ull, 0x0ull, 0x0ull},
    {5, 0x1ull, 0x80000000ull, 0x21d3075eull, 0x1ull, 0x1ull},
    {5, 0x2ull, 0xffffffffull, 0x2ull, 0x2ull, 0x2ull},
    {5, 0x7fffffffull, 0x0ull, 0x16b60422ull, 0x7fffffffull, 0x0ull},
    {5, 0x80000000ull, 0x7fffffffull, 0x80000000ull, 0x80000000ull, 0x7fffffffull},
    {5, 0x80000001ull, 0xfffffffeull, 0xfd31cfd7ull, 0x80000001ull, 0x80000001ull},
    {5, 0xfffffffeull, 0xdeadbeefull, 0xfffffffeull, 0xfffffffeull, 0xdeadbeefull},
    {5, 0xffffffffull, 0x2ull, 0x6c5db104ull, 0xffffffffull, 0x2ull},
    {5, 0x12345678ull, 0x80000001ull, 0x12345678ull, 0x12345678ull, 0x12345678ull},
    {5, 0xdeadbeefull, 0x12345678ull, 0xca2b6cbeull, 0xdeadbeefull, 0x12345678ull},
    {5, 0x0ull, 0x1ull, 0x0ull, 0x0ull, 0x0ull},
    {5, 0x1ull, 0x80000000ull, 0xdc32d1aaull, 0x1ull, 0x1ull},
    {5, 0xfd11be0eull, 0x30eaba72ull, 0xfd11be0eull, 0xfd11be0eull, 0x30eaba72ull},
    {5, 0x4f696046ull, 0xed71a68aull, 0x59f1d18dull, 0x4f696046ull, 0x4f696046ull},
    {5, 0x2e192541ull, 0x1f6c7f26ull, 0x2e192541ull, 0x2e192541ull, 0x1f6c7f26ull},
    {5, 0x7a1ff8aeull, 0xdae2a240ull, 0xd604dd68ull, 0x7a1ff8aeull, 0x7a1ff8aeull},
    {6, 0x0ull, 0x1ull, 0x0ull, 0x0ull, 0x1ull},
    {6, 0x1ull, 0x80000000ull, 0xadf8324cull, 0x1ull, 0x1ull},
    {6, 0x2ull, 0xffffffffull, 0x2ull, 0x2ull, 0x2ull},
    {6, 0x7fffffffull, 0x0ull, 0x9f75da08ull, 0x7fffffffull, 0x7fffffffull},
    {6, 0x80000000ull, 0x7fffffffull, 0x80000000ull, 0x80000000ull, 0x7fffffffull},
    {6, 0x80000001ull, 0xfffffffeull, 0xcd48b9bdull, 0x80000001ull, 0xfffffffeull},
    {6, 0xfffffffeull, 0xdeadbeefull, 0xfffffffeull, 0xfffffffeull, 0xfffffffeull},
    {6, 0xffffffffull, 0x2ull, 0x40591809ull, 0xffffffffull, 0x2ull},
    {6, 0x12345678ull, 0x80000001ull, 0x12345678ull, 0x12345678ull, 0x12345678ull},
    {6, 0xdeadbeefull, 0x12345678ull, 0x951a2694ull, 0xdeadbeefull, 0x12345678ull},
    {6, 0x0ull, 0x1ull, 0x0ull, 0x0ull, 0x1ull},
    {6, 0x1ull, 0x80000000ull, 0x55ff16b5ull, 0x1ull, 0x1ull},
    {6, 0xadb9fc15ull, 0x9263cee2ull, 0xadb9fc15ull, 0xadb9fc15ull, 0xadb9fc15ull},
    {6, 0xa055a0b2ull, 0xe06373c3ull, 0x406ffc43ull, 0xa055a0b2ull, 0xe06373c3ull},
    {6, 0xe442a859ull, 0xbcdeb639ull, 0xe442a859ull, 0xe442a859ull, 0xe442a859ull},
    {6, 0x238c6bf4ull, 0xfe8fedc6ull, 0x771aaf91ull, 0x238c6bf4ull, 0x238c6bf4ull},
    {7, 0x0ull, 0x1ull, 0x0ull, 0x0ull, 0x1ull},
    {7, 0x1ull, 0x80000000ull, 0x237244c9ull, 0x1ull, 0x80000000ull},
    {7, 0x2ull, 0xffffffffull, 0x2ull, 0x2ull, 0xffffffffull},
    {7, 0x7fffffffull, 0x0ull, 0xf62e5e4full, 0x7fffffffull, 0x7fffffffull},
    {7, 0x80000000ull, 0x7fffffffull, 0x80000000ull, 0x80000000ull, 0x80000000ull},
    {7, 0x80000001ull, 0xfffffffeull, 0x90bfe553ull, 0x80000001ull, 0xfffffffeull},
    {7, 0xfffffffeull, 0xdeadbeefull, 0xfffffffeull, 0xfffffffeull, 0xfffffffeull},
    {7, 0xffffffffull, 0x2ull, 0x44b4cc99ull, 0xffffffffull, 0xffffffffull},
    {7, 0x12345678ull, 0x80000001ull, 0x12345678ull, 0x12345678ull, 0x80000001ull},
    {7, 0xdeadbeefull, 0x12345678ull, 0x99faa521ull, 0xdeadbeefull, 0xdeadbeefull},
    {7, 0x0ull, 0x1ull, 0x0ull, 0x0ull, 0x1ull},
    {7, 0x1ull, 0x80000000ull, 0x5136e276ull, 0x1ull, 0x80000000ull},
    {7, 0x64a7abb4ull, 0x146d75bcull, 0x64a7abb4ull, 0x64a7abb4ull, 0x64a7abb4ull},
    {7, 0x2ae5498eull, 0x63a37ae1ull, 0xde40e42bull, 0x2ae5498eull, 0x63a37ae1ull},
    {7, 0x99206560ull, 0x2a0494fdull, 0x99206560ull, 0x99206560ull, 0x99206560ull},
    {7, 0x634e77acull, 0xbfe9ef2bull, 0x70824c54ull, 0x634e77acull, 0xbfe9ef2bull},
    {8, 0x0ull, 0x1ull, 0x0ull, 0x0ull, 0x0ull},
    {8, 0x1ull, 0x80000000ull, 0x5b03f923ull, 0x1ull, 0x0ull},
    {8, 0x2ull, 0xffffffffull, 0x2ull, 0x2ull, 0x2ull},
    {8, 0x7fffffffull, 0x0ull, 0x32c25127ull, 0x7fffffffull, 0x0ull},
    {8, 0x80000000ull, 0x7fffffffull, 0x80000000ull, 0x80000000ull, 0x0ull},
    {8, 0x80000001ull, 0xfffffffeull, 0x1197fd88ull, 0x80000001ull, 0x80000000ull},
    {8, 0xfffffffeull, 0xdeadbeefull, 0xfffffffeull, 0xfffffffeull, 0xdeadbeeeull},
    {8, 0xffffffffull, 0x2ull, 0x7aa29c55ull, 0xffffffffull, 0x2ull},
    {8, 0x12345678ull, 0x80000001ull, 0x12345678ull, 0x12345678ull, 0x0ull},
    {8, 0xdeadbeefull, 0x12345678ull, 0x4dc23c90ull, 0xdeadbeefull, 0x12241668ull},
    {8, 0x0ull, 0x1ull, 0x0ull, 0x0ull, 0x0ull},
    {8, 0x1ull, 0x80000000ull, 0xfa62c0f8ull, 0x1ull, 0x0ull},
    {8, 0xfd7537d1ull, 0x72643b5aull, 0xfd7537d1ull, 0xfd7537d1ull, 0x70643350ull},
    {8, 0xd8dd1ff5ull, 0x8007f94cull, 0x1c7c7b29ull, 0xd8dd1ff5ull, 0x80051944ull},
    {8, 0xadaa9f69ull, 0x542db28dull, 0xadaa9f69ull, 0xadaa9f69ull, 0x4289209ull},
    {8, 0xeba8ad6dull, 0xaf3f49e4ull, 0xbf7eef4eull, 0xeba8ad6dull, 0xab280964ull},
    {9, 0x0ull, 0x1ull, 0x0ull, 0x0ull, 0x1ull},
    {9, 0x1ull, 0x80000000ull, 0xd2dc3da8ull, 0x1ull, 0x80000001ull},
    {9, 0x2ull, 0xffffffffull, 0x2ull, 0x2ull, 0xffffffffull},
    {9, 0x7fffffffull, 0x0ull, 0xcbcf2e9cull, 0x7fffffffull, 0x7fffffffull},
    {9, 0x80000000ull, 0x7fffffffull, 0x80000000ull, 0x80000000ull, 0xffffffffull},
    {9, 0x80000001ull, 0xfffffffeull, 0xf1616b9dull, 0x80000001ull, 0xffffffffull},
    {9, 0xfffffffeull, 0xdeadbeefull, 0xfffffffeull, 0xfffffffeull, 0xffffffffull},
    {9, 0xffffffffull, 0x2ull, 0xaf3ff23eull, 0xffffffffull, 0xffffffffull},
    {9, 0x12345678ull, 0x80000001ull, 0x12345678ull, 0x12345678ull, 0x92345679ull},
    {9, 0xdeadbeefull, 0x12345678ull, 0xd2b04560ull, 0xdeadbeefull, 0xdebdfeffull},
    {9, 0x0ull, 0x1ull, 0x0ull, 0x0ull, 0x1ull},
    {9, 0x1ull, 0x80000000ull, 0xef07f743ull, 0x1ull, 0x80000001ull},
    {9, 0x24c74e5ull, 0x5084dec1ull, 0x24c74e5ull, 0x24c74e5ull, 0x52ccfee5ull},
    {9, 0x8c6705a2ull, 0x1e525d48ull, 0xab05ab28ull, 0x8c6705a2ull, 0x9e775deaull},
    {9, 0xa36a05e5ull, 0x3a793efcull, 0xa36a05e5ull, 0xa36a05e5ull, 0xbb7b3ffdull},
    {9, 0xd5f31646ull, 0xa296c3e7ull, 0x213632ddull, 0xd5f31646ull, 0xf7f7d7e7ull},
    {10, 0x0ull, 0x1ull, 0x0ull, 0x0ull, 0x1ull},
    {10, 0x1ull, 0x80000000ull, 0x11e1dbaaull, 0x1ull, 0x80000001ull},
    {10, 0x2ull, 0xffffffffull, 0x2ull, 0x2ull, 0xfffffffdull},
    {10, 0x7fffffffull, 0x0ull, 0x2cd60e1aull, 0x7fffffffull, 0x7fffffffull},
    {10, 0x80000000ull, 0x7fffffffull, 0x80000000ull, 0x80000000ull, 0xffffffffull},
    {10, 0x80000001ull, 0xfffffffeull, 0x1f025ad0ull, 0x80000001ull, 0x7fffffffull},
    {10, 0xfffffffeull, 0xdeadbeefull, 0xfffffffeull, 0xfffffffeull, 0x21524111ull},
    {10, 0xffffffffull, 0x2ull, 0x5d9eb5b7ull, 0xffffffffull, 0xfffffffdull},
    {10, 0x12345678ull, 0x80000001ull, 0x12345678ull, 0x12345678ull, 0x92345679ull},
    {10, 0xdeadbeefull, 0x12345678ull, 0xf167a808ull, 0xdeadbeefull, 0xcc99e897ull},
    {10, 0x0ull, 0x1ull, 0x0ull, 0x0ull, 0x1ull},
    {10, 0x1ull, 0x80000000ull, 0x9f4e6575ull, 0x1ull, 0x80000001ull},
    {10, 0x8bd3aa1cull, 0x44b01dcdull, 0x8bd3aa1cull, 0x8bd3aa1cull, 0xcf63b7d1ull},
    {10, 0xc2c143f7ull, 0x51bc9443ull, 0xd65e2124ull, 0xc2c143f7ull, 0x937dd7b4ull},
    {10, 0xb0cb286eull, 0xec5194a4ull, 0xb0cb286eull, 0xb0cb286eull, 0x5c9abccaull},
    {10, 0xe17a51ecull, 0xe0bfc82aull, 0x44dde368ull, 0xe17a51ecull, 0x1c599c6ull},
    {11, 0x0ull, 0x1ull, 0x0ull, 0x0ull, 0x1ull},
    {11, 0x1ull, 0x80000000ull, 0x20d071afull, 0x1ull, 0x2ull},
    {11, 0x2ull, 0xffffffffull, 0x2ull, 0x2ull, 0x3ull},
    {11, 0x7fffffffull, 0x0ull, 0x7a347eebull, 0x7fffffffull, 0x0ull},
    {11, 0x80000000ull, 0x7fffffffull, 0x80000000ull, 0x80000000ull, 0x0ull},
    {11, 0x80000001ull, 0xfffffffeull, 0x56e21801ull, 0x80000001ull, 0x80000002ull},
    {11, 0xfffffffeull, 0xdeadbeefull, 0xfffffffeull, 0xfffffffeull, 0x0ull},
    {11, 0xffffffffull, 0x2ull, 0x7a390ac1ull, 0xffffffffull, 0x0ull},
    {11, 0x12345678ull, 0x80000001ull, 0x12345678ull, 0x12345678ull, 0x12345679ull},
    {11, 0xdeadbeefull, 0x12345678ull, 0xe457cd1full, 0xdeadbeefull, 0x0ull},
    {11, 0x0ull, 0x1ull, 0x0ull, 0x0ull, 0x1ull},
    {11, 0x1ull, 0x80000000ull, 0x460a51e2ull, 0x1ull, 0x2ull},
    {11, 0xe3202a2aull, 0xe3202a2bull, 0xe3202a2aull, 0xe3202a2aull, 0xe3202a2bull},
    {11, 0x9ec2949ull, 0x9ec2949ull, 0x947613e8ull, 0x9ec2949ull, 0x0ull},
    {11, 0x8f8118afull, 0x8f8118b0ull, 0x8f8118afull, 0x8f8118afull, 0x8f8118b0ull},
    {11, 0x761757b6ull, 0x761757b6ull, 0x8be15c75ull, 0x761757b6ull, 0x0ull},
    {12, 0x0ull, 0x1ull, 0x0ull, 0x0ull, 0x1ull},
    {12, 0x1ull, 0x80000000ull, 0x7c5a8c15ull, 0x1ull, 0x0ull},
    {12, 0x2ull, 0xffffffffull, 0x2ull, 0x2ull, 0x1ull},
    {12, 0x7fffffffull, 0x0ull, 0x29769614ull, 0x7fffffffull, 0x0ull},
    {12, 0x80000000ull, 0x7fffffffull, 0x80000000ull, 0x80000000ull, 0x7fffffffull},
    {12, 0x80000001ull, 0xfffffffeull, 0x537a54d3ull, 0x80000001ull, 0x80000000ull},
    {12, 0xfffffffeull, 0xdeadbeefull, 0xfffffffeull, 0xfffffffeull, 0xdeadbeefull},
    {12, 0xffffffffull, 0x2ull, 0x6add45d9ull, 0xffffffffull, 0x2ull},
    {12, 0x12345678ull, 0x80000001ull, 0x12345678ull, 0x12345678ull, 0x12345677ull},
    {12, 0xdeadbeefull, 0x12345678ull, 0x978c0812ull, 0xdeadbeefull, 0x12345678ull},
    {12, 0x0ull, 0x1ull, 0x0ull, 0x0ull, 0x1ull},
    {12, 0x1ull, 0x80000000ull, 0xf67e5e26ull, 0x1ull, 0x0ull},
    {12, 0xebea6cf1ull, 0xebea6cf2ull, 0xebea6cf1ull, 0xebea6cf1ull, 0xebea6cf0ull},
    {12, 0xf9a9360ull, 0xf9a9360ull, 0xf85b81cdull, 0xf9a9360ull, 0xf9a935full},
    {12, 0x6e723d5cull, 0x6e723d5dull, 0x6e723d5cull, 0x6e723d5cull, 0x6e723d5bull},
    {12, 0x1f609b4full, 0x1f609b4full, 0x69d177f8ull, 0x1f609b4full, 0x1f609b4eull},
}};

class GuestBlock {
public:
    explicit GuestBlock(bool writable) {
#ifdef _WIN32
        block = static_cast<std::uint8_t*>(VirtualAlloc(nullptr, BlockBytes, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE));
#else
        block = static_cast<std::uint8_t*>(std::aligned_alloc(BlockBytes, BlockBytes));
#endif
        Require(block != nullptr, "global atomics: cannot allocate the guest block");
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
    return Rows.at(op * RowsPerOp + tid % RowsPerOp);
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
    const std::span<const std::uint32_t> code(GlobalAtomicsCode);
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
    const auto wave = "global atomics: wave" + std::to_string(waveSize) + " ";
    for (std::uint32_t op = 0; op < OpCount; ++op) {
        for (std::uint32_t tid = 0; tid < Threads; ++tid) {
            const auto& row = RowOf(op, tid);
            const auto* words = &Output[(op * Threads + tid) * 2u];
            const auto returned = OpBytes(op) == 4u ? static_cast<std::uint64_t>(words[0]) : (static_cast<std::uint64_t>(words[1]) << 32u) | words[0];
            Require(returned == row.returned, wave + "op " + std::to_string(op) + " lane " + std::to_string(tid) + " returned " + Hex(returned) + ", expected " + Hex(row.returned));
        }
    }
    const auto expected = Expected();
    for (std::uint32_t offset = 0; offset < BlockBytes; ++offset) {
        const auto actual = guest.Data()[offset];
        Require(actual == expected[offset], wave + "byte " + std::to_string(offset) + " is " + std::to_string(actual) + ", expected " + std::to_string(expected[offset]));
    }
}


}

int main() {
    try {
        const auto device = OpenVulkanTestDevice();
        if (!device) return VulkanTestSkipped;
        GuestBlock writable(true);
        RunAtomics(*device, writable, 32);
        std::puts("global atomics tests passed");
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
