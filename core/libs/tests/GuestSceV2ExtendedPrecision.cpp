#include "prx/libc/include/Api.hpp"
#include "prx/libc/include/GuestErrno.hpp"

#include <array>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <limits>

namespace {

template<typename TBits>
struct Expected {
    const char* label;
    TBits words[8];
    int count;
    int error;
};

constexpr struct { int exceptions; int error; } RaiseCases[] = {{0, 0}, {1, 33}, {2, 0}, {4, 33}, {5, 33}, {8, 34}, {16, 34}, {24, 34}, {32, 0}, {33, 33}, {63, 33}};

constexpr Expected<std::uint64_t> DoubleCases[] = {
    {"setw v0 n1", {0x3fd5555555555555, 0x401c000000000000, 0x401c000000000000, 0x401c000000000000, 0x401c000000000000, 0x401c000000000000}, 6, 0},
    {"getw v0 n1", {0x3fd5555555555555}, 1, 0},
    {"setw v0 n2", {0x3fd5555550000000, 0x3e35555554000000, 0x401c000000000000, 0x401c000000000000, 0x401c000000000000, 0x401c000000000000}, 6, 0},
    {"getw v0 n2", {0x3fd5555555555555}, 1, 0},
    {"setw v0 n3", {0x3fd5555550000000, 0x3e35555550000000, 0x3c90000000000000, 0x401c000000000000, 0x401c000000000000, 0x401c000000000000}, 6, 0},
    {"getw v0 n3", {0x3fd5555555555555}, 1, 0},
    {"setw v0 n4", {0x3fd5555550000000, 0x3e35555550000000, 0x3c90000000000000, 0x0, 0x401c000000000000, 0x401c000000000000}, 6, 0},
    {"getw v0 n4", {0x3fd5555555555555}, 1, 0},
    {"setw v1 n1", {0xc00921fb54442d18, 0x401c000000000000, 0x401c000000000000, 0x401c000000000000, 0x401c000000000000, 0x401c000000000000}, 6, 0},
    {"getw v1 n1", {0xc00921fb54442d18}, 1, 0},
    {"setw v1 n2", {0xc00921fb50000000, 0xbe6110b460000000, 0x401c000000000000, 0x401c000000000000, 0x401c000000000000, 0x401c000000000000}, 6, 0},
    {"getw v1 n2", {0xc00921fb54442d18}, 1, 0},
    {"setw v1 n3", {0xc00921fb50000000, 0xbe6110b460000000, 0x0, 0x401c000000000000, 0x401c000000000000, 0x401c000000000000}, 6, 0},
    {"getw v1 n3", {0xc00921fb54442d18}, 1, 0},
    {"setw v1 n4", {0xc00921fb50000000, 0xbe6110b460000000, 0x0, 0x401c000000000000, 0x401c000000000000, 0x401c000000000000}, 6, 0},
    {"getw v1 n4", {0xc00921fb54442d18}, 1, 0},
    {"setw v2 n1", {0x7fefffffffffffff, 0x401c000000000000, 0x401c000000000000, 0x401c000000000000, 0x401c000000000000, 0x401c000000000000}, 6, 0},
    {"getw v2 n1", {0x7fefffffffffffff}, 1, 0},
    {"setw v2 n2", {0x7feffffff8000000, 0x7e4ffffffc000000, 0x401c000000000000, 0x401c000000000000, 0x401c000000000000, 0x401c000000000000}, 6, 0},
    {"getw v2 n2", {0x7fefffffffffffff}, 1, 0},
    {"setw v2 n3", {0x7feffffff8000000, 0x7e4ffffff8000000, 0x7ca0000000000000, 0x401c000000000000, 0x401c000000000000, 0x401c000000000000}, 6, 0},
    {"getw v2 n3", {0x7fefffffffffffff}, 1, 0},
    {"setw v2 n4", {0x7feffffff8000000, 0x7e4ffffff8000000, 0x7ca0000000000000, 0x0, 0x401c000000000000, 0x401c000000000000}, 6, 0},
    {"getw v2 n4", {0x7fefffffffffffff}, 1, 0},
    {"setw v3 n1", {0x1, 0x401c000000000000, 0x401c000000000000, 0x401c000000000000, 0x401c000000000000, 0x401c000000000000}, 6, 0},
    {"getw v3 n1", {0x1}, 1, 0},
    {"setw v3 n2", {0x1, 0x0, 0x401c000000000000, 0x401c000000000000, 0x401c000000000000, 0x401c000000000000}, 6, 0},
    {"getw v3 n2", {0x1}, 1, 0},
    {"setw v3 n3", {0x1, 0x0, 0x0, 0x401c000000000000, 0x401c000000000000, 0x401c000000000000}, 6, 0},
    {"getw v3 n3", {0x1}, 1, 0},
    {"setw v3 n4", {0x1, 0x0, 0x0, 0x401c000000000000, 0x401c000000000000, 0x401c000000000000}, 6, 0},
    {"getw v3 n4", {0x1}, 1, 0},
    {"setw v4 n1", {0x0, 0x401c000000000000, 0x401c000000000000, 0x401c000000000000, 0x401c000000000000, 0x401c000000000000}, 6, 0},
    {"getw v4 n1", {0x0}, 1, 0},
    {"setw v4 n2", {0x0, 0x401c000000000000, 0x401c000000000000, 0x401c000000000000, 0x401c000000000000, 0x401c000000000000}, 6, 0},
    {"getw v4 n2", {0x0}, 1, 0},
    {"setw v4 n3", {0x0, 0x401c000000000000, 0x401c000000000000, 0x401c000000000000, 0x401c000000000000, 0x401c000000000000}, 6, 0},
    {"getw v4 n3", {0x0}, 1, 0},
    {"setw v4 n4", {0x0, 0x401c000000000000, 0x401c000000000000, 0x401c000000000000, 0x401c000000000000, 0x401c000000000000}, 6, 0},
    {"getw v4 n4", {0x0}, 1, 0},
    {"setw v5 n1", {0x8000000000000000, 0x401c000000000000, 0x401c000000000000, 0x401c000000000000, 0x401c000000000000, 0x401c000000000000}, 6, 0},
    {"getw v5 n1", {0x8000000000000000}, 1, 0},
    {"setw v5 n2", {0x8000000000000000, 0x401c000000000000, 0x401c000000000000, 0x401c000000000000, 0x401c000000000000, 0x401c000000000000}, 6, 0},
    {"getw v5 n2", {0x8000000000000000}, 1, 0},
    {"setw v5 n3", {0x8000000000000000, 0x401c000000000000, 0x401c000000000000, 0x401c000000000000, 0x401c000000000000, 0x401c000000000000}, 6, 0},
    {"getw v5 n3", {0x8000000000000000}, 1, 0},
    {"setw v5 n4", {0x8000000000000000, 0x401c000000000000, 0x401c000000000000, 0x401c000000000000, 0x401c000000000000, 0x401c000000000000}, 6, 0},
    {"getw v5 n4", {0x8000000000000000}, 1, 0},
    {"setw v6 n1", {0x7ff0000000000000, 0x401c000000000000, 0x401c000000000000, 0x401c000000000000, 0x401c000000000000, 0x401c000000000000}, 6, 0},
    {"getw v6 n1", {0x7ff0000000000000}, 1, 0},
    {"setw v6 n2", {0x7ff0000000000000, 0x0, 0x401c000000000000, 0x401c000000000000, 0x401c000000000000, 0x401c000000000000}, 6, 0},
    {"getw v6 n2", {0x7ff0000000000000}, 1, 0},
    {"setw v6 n3", {0x7ff0000000000000, 0x0, 0x401c000000000000, 0x401c000000000000, 0x401c000000000000, 0x401c000000000000}, 6, 0},
    {"getw v6 n3", {0x7ff0000000000000}, 1, 0},
    {"setw v6 n4", {0x7ff0000000000000, 0x0, 0x401c000000000000, 0x401c000000000000, 0x401c000000000000, 0x401c000000000000}, 6, 0},
    {"getw v6 n4", {0x7ff0000000000000}, 1, 0},
    {"setw v7 n1", {0x7ff8000000000000, 0x401c000000000000, 0x401c000000000000, 0x401c000000000000, 0x401c000000000000, 0x401c000000000000}, 6, 0},
    {"getw v7 n1", {0x7ff8000000000000}, 1, 0},
    {"setw v7 n2", {0x7ff8000000000000, 0x0, 0x401c000000000000, 0x401c000000000000, 0x401c000000000000, 0x401c000000000000}, 6, 0},
    {"getw v7 n2", {0x7ff8000000000000}, 1, 0},
    {"setw v7 n3", {0x7ff8000000000000, 0x0, 0x401c000000000000, 0x401c000000000000, 0x401c000000000000, 0x401c000000000000}, 6, 0},
    {"getw v7 n3", {0x7ff8000000000000}, 1, 0},
    {"setw v7 n4", {0x7ff8000000000000, 0x0, 0x401c000000000000, 0x401c000000000000, 0x401c000000000000, 0x401c000000000000}, 6, 0},
    {"getw v7 n4", {0x7ff8000000000000}, 1, 0},
    {"setn i0", {0x0, 0x401c000000000000, 0x401c000000000000, 0x401c000000000000, 0x401c000000000000, 0x401c000000000000}, 6, 0},
    {"setn i1", {0x3ff0000000000000, 0x0, 0x0, 0x401c000000000000, 0x401c000000000000, 0x401c000000000000}, 6, 0},
    {"setn i2", {0xbff0000000000000, 0x0, 0x0, 0x401c000000000000, 0x401c000000000000, 0x401c000000000000}, 6, 0},
    {"setn i3", {0x42dc122180000000, 0x412bbef200000000, 0x0, 0x401c000000000000, 0x401c000000000000, 0x401c000000000000}, 6, 0},
    {"setn i4", {0xc3e0000000000000, 0x0, 0x0, 0x401c000000000000, 0x401c000000000000, 0x401c000000000000}, 6, 0},
    {"setn i5", {0x43e0000000000000, 0x0, 0x0, 0x401c000000000000, 0x401c000000000000, 0x401c000000000000}, 6, 0},
    {"addh a0", {0x3fd5555550000000, 0x3e35555550000000, 0x3c9000bce0000000, 0x3af4219248000000, 0x0, 0x0}, 6, 0},
    {"addh inf a0", {0x7ff0000000000000, 0x0, 0x0, 0x0, 0x0, 0x0}, 6, 0},
    {"addh a1", {0xbe35555550000000, 0x3e35555550000000, 0x0, 0x0, 0x0, 0x0}, 6, 0},
    {"addh inf a1", {0x7ff0000000000000, 0x0, 0x0, 0x0, 0x0, 0x0}, 6, 0},
    {"addh a2", {0x4415af1d78000000, 0x4246b18800000000, 0x3fd5555550000000, 0x3e35555550000000, 0x0, 0x0}, 6, 0},
    {"addh inf a2", {0x7ff0000000000000, 0x0, 0x0, 0x0, 0x0, 0x0}, 6, 0},
    {"addh a3", {0x7ff0000000000000, 0x3e35555550000000, 0x3c90000000000000, 0x0, 0x0, 0x0}, 6, 0},
    {"addh inf a3", {0x7ff8000000000000, 0x0, 0x0, 0x0, 0x0, 0x0}, 6, 33},
    {"addh a4", {0x3fd5555550000000, 0x3e35555550000000, 0x3c90000000000000, 0x0, 0x0, 0x0}, 6, 0},
    {"addh inf a4", {0x7ff0000000000000, 0x0, 0x0, 0x0, 0x0, 0x0}, 6, 0},
    {"addh a5", {0x3fd5555550000000, 0x3e35555550000000, 0x3c90000000000000, 0x1, 0x0, 0x0}, 6, 0},
    {"addh inf a5", {0x7ff0000000000000, 0x0, 0x0, 0x0, 0x0, 0x0}, 6, 0},
    {"mulh f0", {0x3feffffff8000000, 0x3e4ffffff8000000, 0x3ca8000000000000, 0x0, 0x0, 0x0}, 6, 0},
    {"mulh f1", {0x399b0b0ff8000000, 0x37fa3eb8a8000000, 0x362bdfa550000000, 0x348d785400000000, 0x0, 0x0}, 6, 0},
    {"mulh f2", {0x7ff8000000000000, 0x0, 0x3c90000000000000, 0x0, 0x0, 0x0}, 6, 33},
    {"mulh f3", {0x0, 0x3e35555550000000, 0x3c90000000000000, 0x0, 0x0, 0x0}, 6, 0},
    {"mulh f4", {0xbfa1111110000000, 0xbde1111110000000, 0xba999999a0000000, 0x0, 0x0, 0x0}, 6, 0},
    {"mulh f5", {0x7fd5555550000000, 0x7e35555548000000, 0x7c9ffffff8000000, 0x7aeffffffc000000, 0x0, 0x0}, 6, 0},
    {"addh across a part boundary", {0x43ed8a07d8000000, 0x423f44a9b0000000, 0xbf92456178000000, 0x3dc5714d80000000, 0x0, 0x0}, 6, 0},
    {"addh shift -28", {0x3fd5555558000000, 0x3df5555540000000, 0x0, 0x0, 0x0, 0x0}, 6, 0},
    {"addh shift -27", {0x3fd5555558000000, 0x3e2aaaaaa8000000, 0x0, 0x0, 0x0, 0x0}, 6, 0},
    {"addh shift -26", {0x3fd5555560000000, 0x3e15555550000000, 0x0, 0x0, 0x0, 0x0}, 6, 0},
    {"addh shift -25", {0x3fd5555568000000, 0x3e35555550000000, 0x3c90000000000000, 0x0, 0x0, 0x0}, 6, 0},
    {"addh shift -24", {0x3fd5555580000000, 0x3e35555550000000, 0x3c90000000000000, 0x0, 0x0, 0x0}, 6, 0},
    {"addh shift 24", {0x4168000008000000, 0x3fb5555550000000, 0x3e15555550000000, 0x0, 0x0, 0x0}, 6, 0},
    {"addh shift 25", {0x4178000000000000, 0x3fd5555550000000, 0x3e35555550000000, 0x3c90000000000000, 0x0, 0x0}, 6, 0},
    {"addh shift 26", {0x4188000000000000, 0x3fd5555550000000, 0x3e35555550000000, 0x3c90000000000000, 0x0, 0x0}, 6, 0},
    {"addh shift 27", {0x4198000000000000, 0x3fd5555550000000, 0x3e35555550000000, 0x3c90000000000000, 0x0, 0x0}, 6, 0},
    {"addh shift 28", {0x41a8000000000000, 0x3fd5555550000000, 0x3e35555550000000, 0x3c90000000000000, 0x0, 0x0}, 6, 0},
    {"mulh two NaNs", {0x7ff8000000000456, 0x0, 0x3c90000000000000, 0x0, 0x0, 0x0}, 6, 33},
    {"addx", {0x3fcdddddd8000000, 0x3e27777770000000, 0x3c80000000000000, 0x0, 0x0, 0x0}, 6, 0},
    {"mulx", {0xbfa1111110000000, 0xbde1111110000000, 0xbc21111110000000, 0xba61111100000000, 0x0, 0x0}, 6, 0},
    {"mulx scratch", {0x3fd5555550000000, 0x3e35555550000000, 0x3c90000000000000, 0x0, 0xbde1111110000000, 0xbc45555550000000, 0xbaa1111110000000, 0x0}, 8, 0},
    {"ldexpx e10", {0x4075555550000000, 0x3ed5555550000000, 0x3d30000000000000, 0x0, 0x0, 0x0}, 6, 0},
    {"ldexpx e-2000", {0x0, 0x3e35555550000000, 0x3c90000000000000, 0x0, 0x0, 0x0}, 6, 34},
    {"ldexpx e2000", {0x7ff0000000000000, 0x7ff0000000000000, 0x7ff0000000000000, 0x0, 0x0, 0x0}, 6, 34},
};

constexpr Expected<std::uint32_t> FloatCases[] = {
    {"setw v0 n1", {0x3eaaaaab, 0x40e00000, 0x40e00000, 0x40e00000, 0x40e00000, 0x40e00000}, 6, 0},
    {"getw v0 n1", {0x3eaaaaab}, 1, 0},
    {"setw v0 n2", {0x3eaaa000, 0x38aab000, 0x40e00000, 0x40e00000, 0x40e00000, 0x40e00000}, 6, 0},
    {"getw v0 n2", {0x3eaaaaab}, 1, 0},
    {"setw v0 n3", {0x3eaaa000, 0x38aab000, 0x0, 0x40e00000, 0x40e00000, 0x40e00000}, 6, 0},
    {"getw v0 n3", {0x3eaaaaab}, 1, 0},
    {"setw v0 n4", {0x3eaaa000, 0x38aab000, 0x0, 0x40e00000, 0x40e00000, 0x40e00000}, 6, 0},
    {"getw v0 n4", {0x3eaaaaab}, 1, 0},
    {"setw v1 n1", {0xc0490fdb, 0x40e00000, 0x40e00000, 0x40e00000, 0x40e00000, 0x40e00000}, 6, 0},
    {"getw v1 n1", {0xc0490fdb}, 1, 0},
    {"setw v1 n2", {0xc0490000, 0xba7db000, 0x40e00000, 0x40e00000, 0x40e00000, 0x40e00000}, 6, 0},
    {"getw v1 n2", {0xc0490fdb}, 1, 0},
    {"setw v1 n3", {0xc0490000, 0xba7db000, 0x0, 0x40e00000, 0x40e00000, 0x40e00000}, 6, 0},
    {"getw v1 n3", {0xc0490fdb}, 1, 0},
    {"setw v1 n4", {0xc0490000, 0xba7db000, 0x0, 0x40e00000, 0x40e00000, 0x40e00000}, 6, 0},
    {"getw v1 n4", {0xc0490fdb}, 1, 0},
    {"setw v2 n1", {0x7f7fffff, 0x40e00000, 0x40e00000, 0x40e00000, 0x40e00000, 0x40e00000}, 6, 0},
    {"getw v2 n1", {0x7f7fffff}, 1, 0},
    {"setw v2 n2", {0x7f7ff000, 0x797ff000, 0x40e00000, 0x40e00000, 0x40e00000, 0x40e00000}, 6, 0},
    {"getw v2 n2", {0x7f7fffff}, 1, 0},
    {"setw v2 n3", {0x7f7ff000, 0x797ff000, 0x0, 0x40e00000, 0x40e00000, 0x40e00000}, 6, 0},
    {"getw v2 n3", {0x7f7fffff}, 1, 0},
    {"setw v2 n4", {0x7f7ff000, 0x797ff000, 0x0, 0x40e00000, 0x40e00000, 0x40e00000}, 6, 0},
    {"getw v2 n4", {0x7f7fffff}, 1, 0},
    {"setw v3 n1", {0x1, 0x40e00000, 0x40e00000, 0x40e00000, 0x40e00000, 0x40e00000}, 6, 0},
    {"getw v3 n1", {0x1}, 1, 0},
    {"setw v3 n2", {0x1, 0x0, 0x40e00000, 0x40e00000, 0x40e00000, 0x40e00000}, 6, 0},
    {"getw v3 n2", {0x1}, 1, 0},
    {"setw v3 n3", {0x1, 0x0, 0x0, 0x40e00000, 0x40e00000, 0x40e00000}, 6, 0},
    {"getw v3 n3", {0x1}, 1, 0},
    {"setw v3 n4", {0x1, 0x0, 0x0, 0x40e00000, 0x40e00000, 0x40e00000}, 6, 0},
    {"getw v3 n4", {0x1}, 1, 0},
    {"setw v4 n1", {0x0, 0x40e00000, 0x40e00000, 0x40e00000, 0x40e00000, 0x40e00000}, 6, 0},
    {"getw v4 n1", {0x0}, 1, 0},
    {"setw v4 n2", {0x0, 0x40e00000, 0x40e00000, 0x40e00000, 0x40e00000, 0x40e00000}, 6, 0},
    {"getw v4 n2", {0x0}, 1, 0},
    {"setw v4 n3", {0x0, 0x40e00000, 0x40e00000, 0x40e00000, 0x40e00000, 0x40e00000}, 6, 0},
    {"getw v4 n3", {0x0}, 1, 0},
    {"setw v4 n4", {0x0, 0x40e00000, 0x40e00000, 0x40e00000, 0x40e00000, 0x40e00000}, 6, 0},
    {"getw v4 n4", {0x0}, 1, 0},
    {"setw v5 n1", {0x80000000, 0x40e00000, 0x40e00000, 0x40e00000, 0x40e00000, 0x40e00000}, 6, 0},
    {"getw v5 n1", {0x80000000}, 1, 0},
    {"setw v5 n2", {0x80000000, 0x40e00000, 0x40e00000, 0x40e00000, 0x40e00000, 0x40e00000}, 6, 0},
    {"getw v5 n2", {0x80000000}, 1, 0},
    {"setw v5 n3", {0x80000000, 0x40e00000, 0x40e00000, 0x40e00000, 0x40e00000, 0x40e00000}, 6, 0},
    {"getw v5 n3", {0x80000000}, 1, 0},
    {"setw v5 n4", {0x80000000, 0x40e00000, 0x40e00000, 0x40e00000, 0x40e00000, 0x40e00000}, 6, 0},
    {"getw v5 n4", {0x80000000}, 1, 0},
    {"setw v6 n1", {0x7f800000, 0x40e00000, 0x40e00000, 0x40e00000, 0x40e00000, 0x40e00000}, 6, 0},
    {"getw v6 n1", {0x7f800000}, 1, 0},
    {"setw v6 n2", {0x7f800000, 0x0, 0x40e00000, 0x40e00000, 0x40e00000, 0x40e00000}, 6, 0},
    {"getw v6 n2", {0x7f800000}, 1, 0},
    {"setw v6 n3", {0x7f800000, 0x0, 0x40e00000, 0x40e00000, 0x40e00000, 0x40e00000}, 6, 0},
    {"getw v6 n3", {0x7f800000}, 1, 0},
    {"setw v6 n4", {0x7f800000, 0x0, 0x40e00000, 0x40e00000, 0x40e00000, 0x40e00000}, 6, 0},
    {"getw v6 n4", {0x7f800000}, 1, 0},
    {"setw v7 n1", {0x7fc00000, 0x40e00000, 0x40e00000, 0x40e00000, 0x40e00000, 0x40e00000}, 6, 0},
    {"getw v7 n1", {0x7fc00000}, 1, 0},
    {"setw v7 n2", {0x7fc00000, 0x0, 0x40e00000, 0x40e00000, 0x40e00000, 0x40e00000}, 6, 0},
    {"getw v7 n2", {0x7fc00000}, 1, 0},
    {"setw v7 n3", {0x7fc00000, 0x0, 0x40e00000, 0x40e00000, 0x40e00000, 0x40e00000}, 6, 0},
    {"getw v7 n3", {0x7fc00000}, 1, 0},
    {"setw v7 n4", {0x7fc00000, 0x0, 0x40e00000, 0x40e00000, 0x40e00000, 0x40e00000}, 6, 0},
    {"getw v7 n4", {0x7fc00000}, 1, 0},
    {"setn i0", {0x0, 0x40e00000, 0x40e00000, 0x40e00000, 0x40e00000, 0x40e00000}, 6, 0},
    {"setn i1", {0x3f800000, 0x0, 0x40e00000, 0x40e00000, 0x40e00000, 0x40e00000}, 6, 0},
    {"setn i2", {0xbf800000, 0x0, 0x40e00000, 0x40e00000, 0x40e00000, 0x40e00000}, 6, 0},
    {"setn i3", {0x56e09000, 0x4f060000, 0x48b92000, 0x42240000, 0x40e00000, 0x40e00000}, 6, 0},
    {"setn i4", {0xdefff000, 0xd8fff000, 0xd2938000, 0xc5b58000, 0x40e00000, 0x40e00000}, 6, 0},
    {"setn i5", {0x5efff000, 0x58fff000, 0x52938000, 0x45b57800, 0x40e00000, 0x40e00000}, 6, 0},
    {"addh a0", {0x3eaaa000, 0x38aab000, 0x1e3ce508, 0x0, 0x0, 0x0}, 6, 0},
    {"addh inf a0", {0x7f800000, 0x0, 0x0, 0x0, 0x0, 0x0}, 6, 0},
    {"addh a1", {0x0, 0x0, 0x0, 0x0, 0x0, 0x0}, 6, 0},
    {"addh inf a1", {0x7f800000, 0x0, 0x0, 0x0, 0x0, 0x0}, 6, 0},
    {"addh a2", {0x60ad7000, 0x5a8ec000, 0x3eaaa000, 0x38aab000, 0x0, 0x0}, 6, 0},
    {"addh inf a2", {0x7f800000, 0x0, 0x0, 0x0, 0x0, 0x0}, 6, 0},
    {"addh a3", {0x7f800000, 0x38aab000, 0x0, 0x0, 0x0, 0x0}, 6, 0},
    {"addh inf a3", {0x7fc00000, 0x0, 0x0, 0x0, 0x0, 0x0}, 6, 33},
    {"addh a4", {0x3eaaa000, 0x38aab000, 0x0, 0x0, 0x0, 0x0}, 6, 0},
    {"addh inf a4", {0x7f800000, 0x0, 0x0, 0x0, 0x0, 0x0}, 6, 0},
    {"addh a5", {0x3eaaa000, 0x38aab000, 0x1, 0x0, 0x0, 0x0}, 6, 0},
    {"addh inf a5", {0x7f800000, 0x0, 0x0, 0x0, 0x0, 0x0}, 6, 0},
    {"mulh f0", {0x3f800000, 0x33000000, 0x0, 0x0, 0x0, 0x0}, 6, 0},
    {"mulh f1", {0xcd85000, 0x687f000, 0xf43000, 0x0, 0x0, 0x0}, 6, 0},
    {"mulh f2", {0x7fc00000, 0x0, 0x0, 0x0, 0x0, 0x0}, 6, 33},
    {"mulh f3", {0x0, 0x38aab000, 0x0, 0x0, 0x0, 0x0}, 6, 0},
    {"mulh f4", {0xbd088000, 0xb7088000, 0xb14cd000, 0x0, 0x0, 0x0}, 6, 0},
    {"mulh f5", {0x7eaaa000, 0x78aa9000, 0x72fff000, 0x0, 0x0, 0x0}, 6, 0},
    {"addh shift -14", {0x3eaab000, 0x36ab0000, 0x0, 0x0, 0x0, 0x0}, 6, 0},
    {"addh shift -13", {0x3eaab000, 0x38556000, 0x0, 0x0, 0x0, 0x0}, 6, 0},
    {"addh shift -12", {0x3eaac000, 0x37aac000, 0x0, 0x0, 0x0, 0x0}, 6, 0},
    {"addh shift -11", {0x3eaad000, 0x38aab000, 0x0, 0x0, 0x0, 0x0}, 6, 0},
    {"addh shift -10", {0x3eab0000, 0x38aab000, 0x0, 0x0, 0x0, 0x0}, 6, 0},
    {"addh shift 10", {0x44401000, 0x3daaa000, 0x37aac000, 0x0, 0x0, 0x0}, 6, 0},
    {"addh shift 11", {0x44c00000, 0x3eaaa000, 0x38aab000, 0x0, 0x0, 0x0}, 6, 0},
    {"addh shift 12", {0x45400000, 0x3eaaa000, 0x38aab000, 0x0, 0x0, 0x0}, 6, 0},
    {"addh shift 13", {0x45c00000, 0x3eaaa000, 0x38aab000, 0x0, 0x0, 0x0}, 6, 0},
    {"addh shift 14", {0x46400000, 0x3eaaa000, 0x38aab000, 0x0, 0x0, 0x0}, 6, 0},
    {"mulh two NaNs", {0x7fc00456, 0x0, 0x0, 0x0, 0x0, 0x0}, 6, 33},
    {"addx", {0x3e6ef000, 0xb6884000, 0x0, 0x0, 0x0, 0x0}, 6, 0},
    {"mulx", {0xbd088000, 0xb7088000, 0xb16ee000, 0xab6ef000, 0x0, 0x0}, 6, 0},
    {"mulx scratch", {0x3eaaa000, 0x38aab000, 0x0, 0x0, 0xb7088000, 0xb12aa000, 0xab6ef000, 0x0}, 8, 0},
    {"ldexpx e10", {0x43aaa000, 0x3daab000, 0x0, 0x0, 0x0, 0x0}, 6, 0},
    {"ldexpx e-2000", {0x0, 0x38aab000, 0x0, 0x0, 0x0, 0x0}, 6, 0},
    {"ldexpx e2000", {0x7f800000, 0x7f800000, 0x0, 0x0, 0x0, 0x0}, 6, 0},
};

void Require(bool condition, const char* label, const char* message) {
    if (condition) return;
    std::fprintf(stderr, "sce v2 extended precision: %s: %s\n", label, message);
    std::abort();
}

template<typename T>
struct Family;

template<>
struct Family<double> {
    using Bits = std::uint64_t;
    static constexpr const auto& Cases = DoubleCases;
    static double* SetWide(double* p, int n, double x) { return _ZNSt8__sce_v28_Xp_setwEPdid_nid_postfix(p, n, x); }
    static double GetWide(const double* p, int n) { return _ZNSt8__sce_v28_Xp_getwEPKdi_nid_postfix(p, n); }
    static double* SetInteger(double* p, int n, std::int64_t value) { return _ZNSt8__sce_v28_Xp_setnEPdil_nid_postfix(p, n, value); }
    static double* Move(double* p, int n, const double* q) { return _ZNSt8__sce_v28_Xp_movxEPdiPKd_nid_postfix(p, n, q); }
    static double* AddHalf(double* p, int n, double x) { return _ZNSt8__sce_v28_Xp_addhEPdid_nid_postfix(p, n, x); }
    static double* AddWide(double* p, int n, const double* q, int m) { return _ZNSt8__sce_v28_Xp_addxEPdiPKdi_nid_postfix(p, n, q, m); }
    static double* MultiplyHalf(double* p, int n, double x) { return _ZNSt8__sce_v28_Xp_mulhEPdid_nid_postfix(p, n, x); }
    static double* MultiplyWide(double* p, int n, const double* q, int m, double* scratch) { return _ZNSt8__sce_v28_Xp_mulxEPdiPKdiS0__nid_postfix(p, n, q, m, scratch); }
    static double* ScaleWide(double* p, int n, int exponent) { return _ZNSt8__sce_v210_Xp_ldexpxEPdii_nid_postfix(p, n, exponent); }
};

template<>
struct Family<float> {
    using Bits = std::uint32_t;
    static constexpr const auto& Cases = FloatCases;
    static float* SetWide(float* p, int n, float x) { return _ZNSt8__sce_v29_FXp_setwEPfif_nid_postfix(p, n, x); }
    static float GetWide(const float* p, int n) { return _ZNSt8__sce_v29_FXp_getwEPKfi_nid_postfix(p, n); }
    static float* SetInteger(float* p, int n, std::int64_t value) { return _ZNSt8__sce_v29_FXp_setnEPfil_nid_postfix(p, n, value); }
    static float* Move(float* p, int n, const float* q) { return _ZNSt8__sce_v29_FXp_movxEPfiPKf_nid_postfix(p, n, q); }
    static float* AddHalf(float* p, int n, float x) { return _ZNSt8__sce_v29_FXp_addhEPfif_nid_postfix(p, n, x); }
    static float* AddWide(float* p, int n, const float* q, int m) { return _ZNSt8__sce_v29_FXp_addxEPfiPKfi_nid_postfix(p, n, q, m); }
    static float* MultiplyHalf(float* p, int n, float x) { return _ZNSt8__sce_v29_FXp_mulhEPfif_nid_postfix(p, n, x); }
    static float* MultiplyWide(float* p, int n, const float* q, int m, float* scratch) { return _ZNSt8__sce_v29_FXp_mulxEPfiPKfiS0__nid_postfix(p, n, q, m, scratch); }
    static float* ScaleWide(float* p, int n, int exponent) { return _ZNSt8__sce_v211_FXp_ldexpxEPfii_nid_postfix(p, n, exponent); }
};

template<typename T>
T QuietNan(unsigned payload) {
    using Bits = typename Family<T>::Bits;
    const Bits bits = sizeof(T) == 8 ? Bits(0x7ff8000000000000ULL | payload) : Bits(0x7fc00000u | payload);
    T value;
    std::memcpy(&value, &bits, sizeof(value));
    return value;
}

template<typename T>
class Replay {
public:
    void Check(const T* p, int count) {
        using Bits = typename Family<T>::Bits;
        const auto& cases = Family<T>::Cases;
        Require(next < std::size(cases), "end of table", "more results than expected");
        const auto& expected = cases[next++];
        Require(expected.count == count, expected.label, "word count");
        for (int i = 0; i < count; ++i) {
            Bits bits;
            std::memcpy(&bits, &p[i], sizeof(bits));
            Require(bits == expected.words[i], expected.label, "result bits");
        }
        Require(*__error_nid_postfix() == expected.error, expected.label, "errno");
    }

    void Finish() const { Require(next == std::size(Family<T>::Cases), "end of table", "fewer results than expected"); }

private:
    std::size_t next = 0;
};

template<typename T>
void TestFamily() {
    using F = Family<T>;
    Replay<T> replay;
    const T third = T(1) / T(3);
    const T values[] = {third, -T(3.14159265358979323846), std::numeric_limits<T>::max(), std::numeric_limits<T>::denorm_min(), T(0), -T(0),
                        std::numeric_limits<T>::infinity(), std::numeric_limits<T>::quiet_NaN()};
    for (const T value : values) {
        for (int n : {1, 2, 3, 4}) {
            std::array<T, 6> p;
            p.fill(T(7));
            *__error_nid_postfix() = 0;
            Require(F::SetWide(p.data(), n, value) == p.data(), "setw", "returns its array");
            replay.Check(p.data(), 6);
            const T wide = F::GetWide(p.data(), n);
            replay.Check(&wide, 1);
        }
    }
    for (const std::int64_t integer : {std::int64_t{0}, std::int64_t{1}, std::int64_t{-1}, std::int64_t{123456789012345LL}, std::numeric_limits<std::int64_t>::min(), std::numeric_limits<std::int64_t>::max()}) {
        std::array<T, 6> p;
        p.fill(T(7));
        *__error_nid_postfix() = 0;
        F::SetInteger(p.data(), 4, integer);
        replay.Check(p.data(), 6);
    }
    for (const T addend : {T(1e-20), -third, T(1e20), std::numeric_limits<T>::infinity(), T(0), std::numeric_limits<T>::denorm_min()}) {
        std::array<T, 6> p;
        p.fill(T(0));
        F::SetWide(p.data(), 4, third);
        *__error_nid_postfix() = 0;
        F::AddHalf(p.data(), 4, addend);
        replay.Check(p.data(), 6);
        p.fill(T(0));
        F::SetWide(p.data(), 4, std::numeric_limits<T>::infinity());
        *__error_nid_postfix() = 0;
        F::AddHalf(p.data(), 4, -addend);
        replay.Check(p.data(), 6);
    }
    for (const T factor : {T(3), T(1e-30), std::numeric_limits<T>::quiet_NaN(), T(0), -T(0.1), std::numeric_limits<T>::max()}) {
        std::array<T, 6> p;
        p.fill(T(0));
        F::SetWide(p.data(), 4, third);
        *__error_nid_postfix() = 0;
        F::MultiplyHalf(p.data(), 4, factor);
        replay.Check(p.data(), 6);
    }
    if constexpr (sizeof(T) == 8) {
        std::array<T, 6> terms{0x1.d8a07d8p+63, 0x1.f44a9bp+36, 0x1.ff53c7p-32, 0x1.7aeb7b8p-59, 0, 0};
        *__error_nid_postfix() = 0;
        F::AddHalf(terms.data(), 4, -0x1.245617f51c4bp-6);
        replay.Check(terms.data(), 6);
    }
    const int bits = sizeof(T) == 8 ? 26 : 12;
    for (int shift = -bits - 2; shift <= bits + 2; ++shift) {
        if (shift > -bits + 2 && shift < bits - 2) continue;
        std::array<T, 6> terms;
        terms.fill(T(0));
        F::SetWide(terms.data(), 4, third);
        *__error_nid_postfix() = 0;
        F::AddHalf(terms.data(), 4, std::ldexp(T(0.75), shift));
        replay.Check(terms.data(), 6);
    }
    {
        std::array<T, 6> terms;
        terms.fill(T(0));
        F::SetWide(terms.data(), 4, third);
        terms[0] = QuietNan<T>(0x123);
        *__error_nid_postfix() = 0;
        F::MultiplyHalf(terms.data(), 4, QuietNan<T>(0x456));
        replay.Check(terms.data(), 6);
    }
    std::array<T, 6> p, q;
    p.fill(T(0));
    q.fill(T(0));
    F::SetWide(p.data(), 4, third);
    F::SetWide(q.data(), 4, -T(0.1));
    std::array<T, 6> sum = p;
    *__error_nid_postfix() = 0;
    F::AddWide(sum.data(), 4, q.data(), 4);
    replay.Check(sum.data(), 6);
    std::array<T, 6> product = p;
    std::array<T, 12> scratch;
    scratch.fill(T(0));
    *__error_nid_postfix() = 0;
    F::MultiplyWide(product.data(), 4, q.data(), 4, scratch.data());
    replay.Check(product.data(), 6);
    replay.Check(scratch.data(), 8);
    for (int exponent : {10, -2000, 2000}) {
        std::array<T, 6> scaled = p;
        *__error_nid_postfix() = 0;
        F::ScaleWide(scaled.data(), 4, exponent);
        replay.Check(scaled.data(), 6);
    }
    replay.Finish();
    std::array<T, 6> copy;
    copy.fill(T(9));
    Require(F::Move(copy.data(), 3, p.data()) == copy.data(), "movx", "returns its array");
    Require(std::memcmp(copy.data(), p.data(), 3 * sizeof(T)) == 0 && copy[3] == T(9), "movx", "copies exactly n terms");
}

void TestRaise() {
    for (const auto& entry : RaiseCases) {
        *__error_nid_postfix() = 0;
        _ZNSt8__sce_v28_FeraiseEi_nid_postfix(entry.exceptions);
        Require(*__error_nid_postfix() == entry.error, "_Feraise", "errno");
    }
}

}

int main() {
    TestRaise();
    TestFamily<double>();
    TestFamily<float>();
    return 0;
}
