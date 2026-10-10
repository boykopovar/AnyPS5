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
#include <utility>
#include <vector>

namespace {

using Testing::Require;
using ShaderRecompiler::ShaderStage;

constexpr std::uint32_t Threads = 64;
constexpr std::uint32_t RowsPerOp = 16;
constexpr std::uint32_t OpCount = 11;
constexpr std::uint32_t NarrowOps = 0;
constexpr std::uint32_t OpStride = 0x200;
constexpr std::uint32_t Special = OpCount * OpStride;
constexpr std::uint32_t Sentinel = 0x05e471e1;
constexpr std::size_t BlockBytes = 65536;
constexpr std::uint8_t Fill = 0xcd;
alignas(256) std::array<std::uint32_t, OpCount * Threads * 4> Input{};
alignas(256) std::array<std::uint32_t, (OpCount + 2) * Threads * 2> Output{};

alignas(256) constexpr std::array<std::uint32_t, 159> GlobalAtomicsCode{
    0x34020083, 0x34040084, 0x340a0083, 0x7e2802ff, 0x05e471e1, 0x7e2a02ff, 0x05e471e1, 0x4a060280,
    0x4a080480, 0xe0381000, 0x80000a04, 0xbf8c3f70, 0x4a0c0a80, 0xdd418000, 0x14080a03, 0xbf8c3f70,
    0xe0741000, 0x80011406, 0x4a0602ff, 0x00000200, 0x4a0804ff, 0x00000400, 0xe0381000, 0x80000a04,
    0xbf8c3f70, 0x4a0c0aff, 0x00000200, 0xdd458000, 0x14080a03, 0xbf8c3f70, 0xe0741000, 0x80011406,
    0x4a0602ff, 0x00000400, 0x4a0804ff, 0x00000800, 0xe0381000, 0x80000a04, 0xbf8c3f70, 0x4a0c0aff,
    0x00000400, 0xdd498000, 0x14080a03, 0xbf8c3f70, 0xe0741000, 0x80011406, 0x4a0602ff, 0x00000600,
    0x4a0804ff, 0x00000c00, 0xe0381000, 0x80000a04, 0xbf8c3f70, 0x4a0c0aff, 0x00000600, 0xdd4d8000,
    0x14080a03, 0xbf8c3f70, 0xe0741000, 0x80011406, 0x4a0602ff, 0x00000800, 0x4a0804ff, 0x00001000,
    0xe0381000, 0x80000a04, 0xbf8c3f70, 0x4a0c0aff, 0x00000800, 0xdd558000, 0x14080a03, 0xbf8c3f70,
    0xe0741000, 0x80011406, 0x4a0602ff, 0x00000a00, 0x4a0804ff, 0x00001400, 0xe0381000, 0x80000a04,
    0xbf8c3f70, 0x4a0c0aff, 0x00000a00, 0xdd598000, 0x14080a03, 0xbf8c3f70, 0xe0741000, 0x80011406,
    0x4a0602ff, 0x00000c00, 0x4a0804ff, 0x00001800, 0xe0381000, 0x80000a04, 0xbf8c3f70, 0x4a0c0aff,
    0x00000c00, 0xdd5d8000, 0x14080a03, 0xbf8c3f70, 0xe0741000, 0x80011406, 0x4a0602ff, 0x00000e00,
    0x4a0804ff, 0x00001c00, 0xe0381000, 0x80000a04, 0xbf8c3f70, 0x4a0c0aff, 0x00000e00, 0xdd618000,
    0x14080a03, 0xbf8c3f70, 0xe0741000, 0x80011406, 0x4a0602ff, 0x00001000, 0x4a0804ff, 0x00002000,
    0xe0381000, 0x80000a04, 0xbf8c3f70, 0x4a0c0aff, 0x00001000, 0xdd658000, 0x14080a03, 0xbf8c3f70,
    0xe0741000, 0x80011406, 0x4a0602ff, 0x00001200, 0x4a0804ff, 0x00002400, 0xe0381000, 0x80000a04,
    0xbf8c3f70, 0x4a0c0aff, 0x00001200, 0xdd698000, 0x14080a03, 0xbf8c3f70, 0xe0741000, 0x80011406,
    0x4a0602ff, 0x00001400, 0x4a0804ff, 0x00002800, 0xe0381000, 0x80000a04, 0xbf8c3f70, 0x4a0c0aff,
    0x00001400, 0xdd6d8000, 0x14080a03, 0xbf8c3f70, 0xe0741000, 0x80011406, 0xbf810000
};

struct Row {
    std::uint32_t op;
    std::uint64_t memory;
    std::uint64_t data;
    std::uint64_t comparator;
    std::uint64_t returned;
    std::uint64_t final;
};

constexpr std::array<Row, 176> Rows{{
    {0, 0x0ull, 0x7fffffffffffffffull, 0x0ull, 0x0ull, 0x7fffffffffffffffull},
    {0, 0x1ull, 0xfffffffeffffffffull, 0x3ull, 0x1ull, 0xfffffffeffffffffull},
    {0, 0x7fffffffffffffffull, 0x8000000000000000ull, 0x7fffffffffffffffull, 0x7fffffffffffffffull, 0x8000000000000000ull},
    {0, 0x8000000000000000ull, 0x123456789abcdef0ull, 0xb210ee8d643f54e6ull, 0x8000000000000000ull, 0x123456789abcdef0ull},
    {0, 0xffffffffffffffffull, 0xffffffffffffffffull, 0xffffffffffffffffull, 0xffffffffffffffffull, 0xffffffffffffffffull},
    {0, 0xffffffffull, 0x0ull, 0xffffffdfull, 0xffffffffull, 0x0ull},
    {0, 0x100000000ull, 0xffffffffull, 0x100000000ull, 0x100000000ull, 0xffffffffull},
    {0, 0xfffffffeffffffffull, 0x1ull, 0xed5fb21be38948d7ull, 0xfffffffeffffffffull, 0x1ull},
    {0, 0x123456789abcdef0ull, 0x100000000ull, 0x123456789abcdef0ull, 0x123456789abcdef0ull, 0x100000000ull},
    {0, 0x0ull, 0x7fffffffffffffffull, 0x200ull, 0x0ull, 0x7fffffffffffffffull},
    {0, 0x1ull, 0xfffffffeffffffffull, 0x1ull, 0x1ull, 0xfffffffeffffffffull},
    {0, 0x7fffffffffffffffull, 0x8000000000000000ull, 0xb346daefa27ab170ull, 0x7fffffffffffffffull, 0x8000000000000000ull},
    {0, 0x53bb9629497dfb3full, 0xa4abfed10ad2a242ull, 0x53bb9629497dfb3full, 0x53bb9629497dfb3full, 0xa4abfed10ad2a242ull},
    {0, 0x6c12816965391baaull, 0xce7cd7b44b45eaf3ull, 0x6c12816967391baaull, 0x6c12816965391baaull, 0xce7cd7b44b45eaf3ull},
    {0, 0xdba996ccffffffffull, 0x10bbe64100000001ull, 0xdba996ccffffffffull, 0xdba996ccffffffffull, 0x10bbe64100000001ull},
    {0, 0x5a7c138cffffffffull, 0x9c15fed00000001ull, 0x5a7c138cdfffffffull, 0x5a7c138cffffffffull, 0x9c15fed00000001ull},
    {1, 0x0ull, 0x7fffffffffffffffull, 0x0ull, 0x0ull, 0x7fffffffffffffffull},
    {1, 0x1ull, 0xfffffffeffffffffull, 0x3ull, 0x1ull, 0x1ull},
    {1, 0x7fffffffffffffffull, 0x8000000000000000ull, 0x7fffffffffffffffull, 0x7fffffffffffffffull, 0x8000000000000000ull},
    {1, 0x8000000000000000ull, 0x123456789abcdef0ull, 0x166c48c6e9c1b01cull, 0x8000000000000000ull, 0x8000000000000000ull},
    {1, 0xffffffffffffffffull, 0xffffffffffffffffull, 0xffffffffffffffffull, 0xffffffffffffffffull, 0xffffffffffffffffull},
    {1, 0xffffffffull, 0x0ull, 0xffffffdfull, 0xffffffffull, 0xffffffffull},
    {1, 0x100000000ull, 0xffffffffull, 0x100000000ull, 0x100000000ull, 0xffffffffull},
    {1, 0xfffffffeffffffffull, 0x1ull, 0x866b08e67ea9d02cull, 0xfffffffeffffffffull, 0xfffffffeffffffffull},
    {1, 0x123456789abcdef0ull, 0x100000000ull, 0x123456789abcdef0ull, 0x123456789abcdef0ull, 0x100000000ull},
    {1, 0x0ull, 0x7fffffffffffffffull, 0x200ull, 0x0ull, 0x0ull},
    {1, 0x1ull, 0xfffffffeffffffffull, 0x1ull, 0x1ull, 0xfffffffeffffffffull},
    {1, 0x7fffffffffffffffull, 0x8000000000000000ull, 0xa54604e5b9b1311full, 0x7fffffffffffffffull, 0x7fffffffffffffffull},
    {1, 0xb9a56d03ead2890full, 0x242f2f3b525af861ull, 0xb9a56d03ead2890full, 0xb9a56d03ead2890full, 0x242f2f3b525af861ull},
    {1, 0x65c783e7e26c0130ull, 0x19a0106761c2f186ull, 0x65c783e7e06c0130ull, 0x65c783e7e26c0130ull, 0x65c783e7e26c0130ull},
    {1, 0x7bebc240ffffffffull, 0x4da1ba4b00000001ull, 0x7bebc240ffffffffull, 0x7bebc240ffffffffull, 0x4da1ba4b00000001ull},
    {1, 0x57e3536bffffffffull, 0xd8d033bd00000001ull, 0x57e3536bdfffffffull, 0x57e3536bffffffffull, 0x57e3536bffffffffull},
    {2, 0x0ull, 0x7fffffffffffffffull, 0x0ull, 0x0ull, 0x7fffffffffffffffull},
    {2, 0x1ull, 0xfffffffeffffffffull, 0x3ull, 0x1ull, 0xffffffff00000000ull},
    {2, 0x7fffffffffffffffull, 0x8000000000000000ull, 0x7fffffffffffffffull, 0x7fffffffffffffffull, 0xffffffffffffffffull},
    {2, 0x8000000000000000ull, 0x123456789abcdef0ull, 0xd4db9193123f6ebaull, 0x8000000000000000ull, 0x923456789abcdef0ull},
    {2, 0xffffffffffffffffull, 0xffffffffffffffffull, 0xffffffffffffffffull, 0xffffffffffffffffull, 0xfffffffffffffffeull},
    {2, 0xffffffffull, 0x0ull, 0xffffffdfull, 0xffffffffull, 0xffffffffull},
    {2, 0x100000000ull, 0xffffffffull, 0x100000000ull, 0x100000000ull, 0x1ffffffffull},
    {2, 0xfffffffeffffffffull, 0x1ull, 0x25fd502c82facbc0ull, 0xfffffffeffffffffull, 0xffffffff00000000ull},
    {2, 0x123456789abcdef0ull, 0x100000000ull, 0x123456789abcdef0ull, 0x123456789abcdef0ull, 0x123456799abcdef0ull},
    {2, 0x0ull, 0x7fffffffffffffffull, 0x200ull, 0x0ull, 0x7fffffffffffffffull},
    {2, 0x1ull, 0xfffffffeffffffffull, 0x1ull, 0x1ull, 0xffffffff00000000ull},
    {2, 0x7fffffffffffffffull, 0x8000000000000000ull, 0xf9124f0e4aae24c5ull, 0x7fffffffffffffffull, 0xffffffffffffffffull},
    {2, 0xc3e86469baffc1dull, 0xc22f0e9d6d69338dull, 0xc3e86469baffc1dull, 0xc3e86469baffc1dull, 0xce6d94e409192faaull},
    {2, 0xe499e6544c91e53ull, 0x4647904bf55b8053ull, 0xe499e6546c91e53ull, 0xe499e6544c91e53ull, 0x54912eb13a249ea6ull},
    {2, 0x7a4a48e2ffffffffull, 0x7ccc4b5300000001ull, 0x7a4a48e2ffffffffull, 0x7a4a48e2ffffffffull, 0xf716943600000000ull},
    {2, 0xa925179dffffffffull, 0xb2b1ac1800000001ull, 0xa925179ddfffffffull, 0xa925179dffffffffull, 0x5bd6c3b600000000ull},
    {3, 0x0ull, 0x7fffffffffffffffull, 0x0ull, 0x0ull, 0x8000000000000001ull},
    {3, 0x1ull, 0xfffffffeffffffffull, 0x3ull, 0x1ull, 0x100000002ull},
    {3, 0x7fffffffffffffffull, 0x8000000000000000ull, 0x7fffffffffffffffull, 0x7fffffffffffffffull, 0xffffffffffffffffull},
    {3, 0x8000000000000000ull, 0x123456789abcdef0ull, 0x6835bf86a066a97eull, 0x8000000000000000ull, 0x6dcba98765432110ull},
    {3, 0xffffffffffffffffull, 0xffffffffffffffffull, 0xffffffffffffffffull, 0xffffffffffffffffull, 0x0ull},
    {3, 0xffffffffull, 0x0ull, 0xffffffdfull, 0xffffffffull, 0xffffffffull},
    {3, 0x100000000ull, 0xffffffffull, 0x100000000ull, 0x100000000ull, 0x1ull},
    {3, 0xfffffffeffffffffull, 0x1ull, 0xbcbca7d9810b83c1ull, 0xfffffffeffffffffull, 0xfffffffefffffffeull},
    {3, 0x123456789abcdef0ull, 0x100000000ull, 0x123456789abcdef0ull, 0x123456789abcdef0ull, 0x123456779abcdef0ull},
    {3, 0x0ull, 0x7fffffffffffffffull, 0x200ull, 0x0ull, 0x8000000000000001ull},
    {3, 0x1ull, 0xfffffffeffffffffull, 0x1ull, 0x1ull, 0x100000002ull},
    {3, 0x7fffffffffffffffull, 0x8000000000000000ull, 0x663c553a9a16e35ull, 0x7fffffffffffffffull, 0xffffffffffffffffull},
    {3, 0x63fbf4e8e453a4c5ull, 0xb7a15652045c43f6ull, 0x63fbf4e8e453a4c5ull, 0x63fbf4e8e453a4c5ull, 0xac5a9e96dff760cfull},
    {3, 0x1603b25645bdd8aaull, 0x1f87b96dd545e1f1ull, 0x1603b25647bdd8aaull, 0x1603b25645bdd8aaull, 0xf67bf8e87077f6b9ull},
    {3, 0x757fd168ffffffffull, 0x2356488500000001ull, 0x757fd168ffffffffull, 0x757fd168ffffffffull, 0x522988e3fffffffeull},
    {3, 0x752b2a35ffffffffull, 0x869e617c00000001ull, 0x752b2a35dfffffffull, 0x752b2a35ffffffffull, 0xee8cc8b9fffffffeull},
    {4, 0x0ull, 0x7fffffffffffffffull, 0x0ull, 0x0ull, 0x0ull},
    {4, 0x1ull, 0xfffffffeffffffffull, 0x3ull, 0x1ull, 0xfffffffeffffffffull},
    {4, 0x7fffffffffffffffull, 0x8000000000000000ull, 0x7fffffffffffffffull, 0x7fffffffffffffffull, 0x8000000000000000ull},
    {4, 0x8000000000000000ull, 0x123456789abcdef0ull, 0xd286c038892fd9d4ull, 0x8000000000000000ull, 0x8000000000000000ull},
    {4, 0xffffffffffffffffull, 0xffffffffffffffffull, 0xffffffffffffffffull, 0xffffffffffffffffull, 0xffffffffffffffffull},
    {4, 0xffffffffull, 0x0ull, 0xffffffdfull, 0xffffffffull, 0x0ull},
    {4, 0x100000000ull, 0xffffffffull, 0x100000000ull, 0x100000000ull, 0xffffffffull},
    {4, 0xfffffffeffffffffull, 0x1ull, 0x80b34c5cb5907e40ull, 0xfffffffeffffffffull, 0xfffffffeffffffffull},
    {4, 0x123456789abcdef0ull, 0x100000000ull, 0x123456789abcdef0ull, 0x123456789abcdef0ull, 0x100000000ull},
    {4, 0x0ull, 0x7fffffffffffffffull, 0x200ull, 0x0ull, 0x0ull},
    {4, 0x1ull, 0xfffffffeffffffffull, 0x1ull, 0x1ull, 0xfffffffeffffffffull},
    {4, 0x7fffffffffffffffull, 0x8000000000000000ull, 0xe7cd1f2260293767ull, 0x7fffffffffffffffull, 0x8000000000000000ull},
    {4, 0x8372abbda96ba8eeull, 0x5e71be928ba29363ull, 0x8372abbda96ba8eeull, 0x8372abbda96ba8eeull, 0x8372abbda96ba8eeull},
    {4, 0xce77c3cdcc677910ull, 0xdf44f6f934647f0eull, 0xce77c3cdce677910ull, 0xce77c3cdcc677910ull, 0xce77c3cdcc677910ull},
    {4, 0x9b3edbb1ffffffffull, 0xf4f16cf100000001ull, 0x9b3edbb1ffffffffull, 0x9b3edbb1ffffffffull, 0x9b3edbb1ffffffffull},
    {4, 0xadf3fbf0ffffffffull, 0x8b29592900000001ull, 0xadf3fbf0dfffffffull, 0xadf3fbf0ffffffffull, 0x8b29592900000001ull},
    {5, 0x0ull, 0x7fffffffffffffffull, 0x0ull, 0x0ull, 0x0ull},
    {5, 0x1ull, 0xfffffffeffffffffull, 0x3ull, 0x1ull, 0x1ull},
    {5, 0x7fffffffffffffffull, 0x8000000000000000ull, 0x7fffffffffffffffull, 0x7fffffffffffffffull, 0x7fffffffffffffffull},
    {5, 0x8000000000000000ull, 0x123456789abcdef0ull, 0xff63cf82d01938e9ull, 0x8000000000000000ull, 0x123456789abcdef0ull},
    {5, 0xffffffffffffffffull, 0xffffffffffffffffull, 0xffffffffffffffffull, 0xffffffffffffffffull, 0xffffffffffffffffull},
    {5, 0xffffffffull, 0x0ull, 0xffffffdfull, 0xffffffffull, 0x0ull},
    {5, 0x100000000ull, 0xffffffffull, 0x100000000ull, 0x100000000ull, 0xffffffffull},
    {5, 0xfffffffeffffffffull, 0x1ull, 0x2a88358605f1254ull, 0xfffffffeffffffffull, 0x1ull},
    {5, 0x123456789abcdef0ull, 0x100000000ull, 0x123456789abcdef0ull, 0x123456789abcdef0ull, 0x100000000ull},
    {5, 0x0ull, 0x7fffffffffffffffull, 0x200ull, 0x0ull, 0x0ull},
    {5, 0x1ull, 0xfffffffeffffffffull, 0x1ull, 0x1ull, 0x1ull},
    {5, 0x7fffffffffffffffull, 0x8000000000000000ull, 0xf078418740b4f6d3ull, 0x7fffffffffffffffull, 0x7fffffffffffffffull},
    {5, 0x8d4a240ceafb92f2ull, 0xd789c424b5282b1dull, 0x8d4a240ceafb92f2ull, 0x8d4a240ceafb92f2ull, 0x8d4a240ceafb92f2ull},
    {5, 0x64b3620ed7a9434full, 0x930b9894c3741be0ull, 0x64b3620ed5a9434full, 0x64b3620ed7a9434full, 0x64b3620ed7a9434full},
    {5, 0x62a12f63ffffffffull, 0x4a1df58400000001ull, 0x62a12f63ffffffffull, 0x62a12f63ffffffffull, 0x4a1df58400000001ull},
    {5, 0xd0826a0cffffffffull, 0x14fe08aa00000001ull, 0xd0826a0cdfffffffull, 0xd0826a0cffffffffull, 0x14fe08aa00000001ull},
    {6, 0x0ull, 0x7fffffffffffffffull, 0x0ull, 0x0ull, 0x7fffffffffffffffull},
    {6, 0x1ull, 0xfffffffeffffffffull, 0x3ull, 0x1ull, 0x1ull},
    {6, 0x7fffffffffffffffull, 0x8000000000000000ull, 0x7fffffffffffffffull, 0x7fffffffffffffffull, 0x7fffffffffffffffull},
    {6, 0x8000000000000000ull, 0x123456789abcdef0ull, 0x35f6b71611838017ull, 0x8000000000000000ull, 0x123456789abcdef0ull},
    {6, 0xffffffffffffffffull, 0xffffffffffffffffull, 0xffffffffffffffffull, 0xffffffffffffffffull, 0xffffffffffffffffull},
    {6, 0xffffffffull, 0x0ull, 0xffffffdfull, 0xffffffffull, 0xffffffffull},
    {6, 0x100000000ull, 0xffffffffull, 0x100000000ull, 0x100000000ull, 0x100000000ull},
    {6, 0xfffffffeffffffffull, 0x1ull, 0x884dcfec36f43da6ull, 0xfffffffeffffffffull, 0x1ull},
    {6, 0x123456789abcdef0ull, 0x100000000ull, 0x123456789abcdef0ull, 0x123456789abcdef0ull, 0x123456789abcdef0ull},
    {6, 0x0ull, 0x7fffffffffffffffull, 0x200ull, 0x0ull, 0x7fffffffffffffffull},
    {6, 0x1ull, 0xfffffffeffffffffull, 0x1ull, 0x1ull, 0x1ull},
    {6, 0x7fffffffffffffffull, 0x8000000000000000ull, 0x97e21514d87b299dull, 0x7fffffffffffffffull, 0x7fffffffffffffffull},
    {6, 0x7ae9f54bc19e56ddull, 0x4a6ac7aeb830ed76ull, 0x7ae9f54bc19e56ddull, 0x7ae9f54bc19e56ddull, 0x7ae9f54bc19e56ddull},
    {6, 0x5b1c8aa1eb9a258cull, 0xf4a0ea8b3badac72ull, 0x5b1c8aa1e99a258cull, 0x5b1c8aa1eb9a258cull, 0x5b1c8aa1eb9a258cull},
    {6, 0x869f35deffffffffull, 0xdeffa68100000001ull, 0x869f35deffffffffull, 0x869f35deffffffffull, 0xdeffa68100000001ull},
    {6, 0x5752b4dfffffffffull, 0x2a51ee7e00000001ull, 0x5752b4dfdfffffffull, 0x5752b4dfffffffffull, 0x5752b4dfffffffffull},
    {7, 0x0ull, 0x7fffffffffffffffull, 0x0ull, 0x0ull, 0x7fffffffffffffffull},
    {7, 0x1ull, 0xfffffffeffffffffull, 0x3ull, 0x1ull, 0xfffffffeffffffffull},
    {7, 0x7fffffffffffffffull, 0x8000000000000000ull, 0x7fffffffffffffffull, 0x7fffffffffffffffull, 0x8000000000000000ull},
    {7, 0x8000000000000000ull, 0x123456789abcdef0ull, 0xd316f470e2d8993bull, 0x8000000000000000ull, 0x8000000000000000ull},
    {7, 0xffffffffffffffffull, 0xffffffffffffffffull, 0xffffffffffffffffull, 0xffffffffffffffffull, 0xffffffffffffffffull},
    {7, 0xffffffffull, 0x0ull, 0xffffffdfull, 0xffffffffull, 0xffffffffull},
    {7, 0x100000000ull, 0xffffffffull, 0x100000000ull, 0x100000000ull, 0x100000000ull},
    {7, 0xfffffffeffffffffull, 0x1ull, 0xfc2b7ace1072e7b9ull, 0xfffffffeffffffffull, 0xfffffffeffffffffull},
    {7, 0x123456789abcdef0ull, 0x100000000ull, 0x123456789abcdef0ull, 0x123456789abcdef0ull, 0x123456789abcdef0ull},
    {7, 0x0ull, 0x7fffffffffffffffull, 0x200ull, 0x0ull, 0x7fffffffffffffffull},
    {7, 0x1ull, 0xfffffffeffffffffull, 0x1ull, 0x1ull, 0xfffffffeffffffffull},
    {7, 0x7fffffffffffffffull, 0x8000000000000000ull, 0x142c1e683d47a123ull, 0x7fffffffffffffffull, 0x8000000000000000ull},
    {7, 0x5ea0e74f0d30cb76ull, 0xecd62560f1407a13ull, 0x5ea0e74f0d30cb76ull, 0x5ea0e74f0d30cb76ull, 0xecd62560f1407a13ull},
    {7, 0x6f8664b99db31c8bull, 0x77417f9d1c387d00ull, 0x6f8664b99fb31c8bull, 0x6f8664b99db31c8bull, 0x77417f9d1c387d00ull},
    {7, 0x1e0ce078ffffffffull, 0xc130cf3c00000001ull, 0x1e0ce078ffffffffull, 0x1e0ce078ffffffffull, 0xc130cf3c00000001ull},
    {7, 0xd160d889ffffffffull, 0x2b456f6800000001ull, 0xd160d889dfffffffull, 0xd160d889ffffffffull, 0xd160d889ffffffffull},
    {8, 0x0ull, 0x7fffffffffffffffull, 0x0ull, 0x0ull, 0x0ull},
    {8, 0x1ull, 0xfffffffeffffffffull, 0x3ull, 0x1ull, 0x1ull},
    {8, 0x7fffffffffffffffull, 0x8000000000000000ull, 0x7fffffffffffffffull, 0x7fffffffffffffffull, 0x0ull},
    {8, 0x8000000000000000ull, 0x123456789abcdef0ull, 0x691beca72ac31615ull, 0x8000000000000000ull, 0x0ull},
    {8, 0xffffffffffffffffull, 0xffffffffffffffffull, 0xffffffffffffffffull, 0xffffffffffffffffull, 0xffffffffffffffffull},
    {8, 0xffffffffull, 0x0ull, 0xffffffdfull, 0xffffffffull, 0x0ull},
    {8, 0x100000000ull, 0xffffffffull, 0x100000000ull, 0x100000000ull, 0x0ull},
    {8, 0xfffffffeffffffffull, 0x1ull, 0xc89594109911783aull, 0xfffffffeffffffffull, 0x1ull},
    {8, 0x123456789abcdef0ull, 0x100000000ull, 0x123456789abcdef0ull, 0x123456789abcdef0ull, 0x0ull},
    {8, 0x0ull, 0x7fffffffffffffffull, 0x200ull, 0x0ull, 0x0ull},
    {8, 0x1ull, 0xfffffffeffffffffull, 0x1ull, 0x1ull, 0x1ull},
    {8, 0x7fffffffffffffffull, 0x8000000000000000ull, 0xa69bfe93d6efb4ddull, 0x7fffffffffffffffull, 0x0ull},
    {8, 0xd5e7cc579d21963aull, 0x93826a1ee8ab2ed1ull, 0xd5e7cc579d21963aull, 0xd5e7cc579d21963aull, 0x9182481688210610ull},
    {8, 0x657c382c4e1cfcddull, 0x837a0dc63ac35f32ull, 0x657c382c4c1cfcddull, 0x657c382c4e1cfcddull, 0x17808040a005c10ull},
    {8, 0x7552c284ffffffffull, 0x995c2cf200000001ull, 0x7552c284ffffffffull, 0x7552c284ffffffffull, 0x1150008000000001ull},
    {8, 0xe20d4f8dffffffffull, 0x4864cdbc00000001ull, 0xe20d4f8ddfffffffull, 0xe20d4f8dffffffffull, 0x40044d8c00000001ull},
    {9, 0x0ull, 0x7fffffffffffffffull, 0x0ull, 0x0ull, 0x7fffffffffffffffull},
    {9, 0x1ull, 0xfffffffeffffffffull, 0x3ull, 0x1ull, 0xfffffffeffffffffull},
    {9, 0x7fffffffffffffffull, 0x8000000000000000ull, 0x7fffffffffffffffull, 0x7fffffffffffffffull, 0xffffffffffffffffull},
    {9, 0x8000000000000000ull, 0x123456789abcdef0ull, 0xbad3dfdbb007d4ecull, 0x8000000000000000ull, 0x923456789abcdef0ull},
    {9, 0xffffffffffffffffull, 0xffffffffffffffffull, 0xffffffffffffffffull, 0xffffffffffffffffull, 0xffffffffffffffffull},
    {9, 0xffffffffull, 0x0ull, 0xffffffdfull, 0xffffffffull, 0xffffffffull},
    {9, 0x100000000ull, 0xffffffffull, 0x100000000ull, 0x100000000ull, 0x1ffffffffull},
    {9, 0xfffffffeffffffffull, 0x1ull, 0xedecea3c6fa0d23full, 0xfffffffeffffffffull, 0xfffffffeffffffffull},
    {9, 0x123456789abcdef0ull, 0x100000000ull, 0x123456789abcdef0ull, 0x123456789abcdef0ull, 0x123456799abcdef0ull},
    {9, 0x0ull, 0x7fffffffffffffffull, 0x200ull, 0x0ull, 0x7fffffffffffffffull},
    {9, 0x1ull, 0xfffffffeffffffffull, 0x1ull, 0x1ull, 0xfffffffeffffffffull},
    {9, 0x7fffffffffffffffull, 0x8000000000000000ull, 0xf7926675f45d1971ull, 0x7fffffffffffffffull, 0xffffffffffffffffull},
    {9, 0x360443a82569e20cull, 0x3800d9ff899231faull, 0x360443a82569e20cull, 0x360443a82569e20cull, 0x3e04dbffadfbf3feull},
    {9, 0xc265df77a6c71a0dull, 0x5e94d6ccc50557f8ull, 0xc265df77a4c71a0dull, 0xc265df77a6c71a0dull, 0xdef5dfffe7c75ffdull},
    {9, 0x41a90d6bffffffffull, 0x4bb3092400000001ull, 0x41a90d6bffffffffull, 0x41a90d6bffffffffull, 0x4bbb0d6fffffffffull},
    {9, 0xc54e9269ffffffffull, 0xf536f34000000001ull, 0xc54e9269dfffffffull, 0xc54e9269ffffffffull, 0xf57ef369ffffffffull},
    {10, 0x0ull, 0x7fffffffffffffffull, 0x0ull, 0x0ull, 0x7fffffffffffffffull},
    {10, 0x1ull, 0xfffffffeffffffffull, 0x3ull, 0x1ull, 0xfffffffefffffffeull},
    {10, 0x7fffffffffffffffull, 0x8000000000000000ull, 0x7fffffffffffffffull, 0x7fffffffffffffffull, 0xffffffffffffffffull},
    {10, 0x8000000000000000ull, 0x123456789abcdef0ull, 0xe3b411de678ee5c0ull, 0x8000000000000000ull, 0x923456789abcdef0ull},
    {10, 0xffffffffffffffffull, 0xffffffffffffffffull, 0xffffffffffffffffull, 0xffffffffffffffffull, 0x0ull},
    {10, 0xffffffffull, 0x0ull, 0xffffffdfull, 0xffffffffull, 0xffffffffull},
    {10, 0x100000000ull, 0xffffffffull, 0x100000000ull, 0x100000000ull, 0x1ffffffffull},
    {10, 0xfffffffeffffffffull, 0x1ull, 0xe4e171d83c299defull, 0xfffffffeffffffffull, 0xfffffffefffffffeull},
    {10, 0x123456789abcdef0ull, 0x100000000ull, 0x123456789abcdef0ull, 0x123456789abcdef0ull, 0x123456799abcdef0ull},
    {10, 0x0ull, 0x7fffffffffffffffull, 0x200ull, 0x0ull, 0x7fffffffffffffffull},
    {10, 0x1ull, 0xfffffffeffffffffull, 0x1ull, 0x1ull, 0xfffffffefffffffeull},
    {10, 0x7fffffffffffffffull, 0x8000000000000000ull, 0xaa23249b400d9ebdull, 0x7fffffffffffffffull, 0xffffffffffffffffull},
    {10, 0xc1f06114a97ad060ull, 0x679f7fe64c950a15ull, 0xc1f06114a97ad060ull, 0xc1f06114a97ad060ull, 0xa66f1ef2e5efda75ull},
    {10, 0x59bdb13042bcfe48ull, 0x1669010b3ea903bbull, 0x59bdb13040bcfe48ull, 0x59bdb13042bcfe48ull, 0x4fd4b03b7c15fdf3ull},
    {10, 0x701d03a9ffffffffull, 0xf96a081600000001ull, 0x701d03a9ffffffffull, 0x701d03a9ffffffffull, 0x89770bbffffffffeull},
    {10, 0x9b817862ffffffffull, 0x3c3e50fe00000001ull, 0x9b817862dfffffffull, 0x9b817862ffffffffull, 0xa7bf289cfffffffeull},
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

const Testing::Case wave32Atomics{"GlobalAtomics64_Wave32QwordOperations_ReturnPreviousAndStoreResult", [] {
    auto& device = SharedVulkanTestDevice();
    SkipUnlessCapability(device.Target(), spv::CapabilityInt64Atomics, "the device has no shaderBufferInt64Atomics");
    GuestBlock writable(true);
    RunAtomics(device, writable, 32);
}};

} // namespace
