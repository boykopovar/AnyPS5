#include "prx/libc/include/general/VabiMacros.hpp"
#include <cstdint>
#include <cstdlib>
#include <stdexcept>

using Int128 = __int128;
using Uint128 = unsigned __int128;

extern "C" {
std::int64_t APS5_VABI __ashldi3_nid_postfix(std::int64_t, int);
std::int64_t APS5_VABI __ashrdi3_nid_postfix(std::int64_t, int);
std::int64_t APS5_VABI __lshrdi3_nid_postfix(std::int64_t, int);
Int128 APS5_VABI __ashlti3_nid_postfix(Int128, int);
Int128 APS5_VABI __ashrti3_nid_postfix(Int128, int);
Int128 APS5_VABI __lshrti3_nid_postfix(Int128, int);
int APS5_VABI __clzsi2_nid_postfix(std::uint32_t);
int APS5_VABI __clzdi2_nid_postfix(std::uint64_t);
int APS5_VABI __clzti2_nid_postfix(Uint128);
int APS5_VABI __ctzsi2_nid_postfix(std::uint32_t);
int APS5_VABI __ctzdi2_nid_postfix(std::uint64_t);
int APS5_VABI __ctzti2_nid_postfix(Uint128);
int APS5_VABI __ffsdi2_nid_postfix(std::uint64_t);
int APS5_VABI __ffsti2_nid_postfix(Uint128);
int APS5_VABI __paritysi2_nid_postfix(std::uint32_t);
int APS5_VABI __paritydi2_nid_postfix(std::uint64_t);
int APS5_VABI __parityti2_nid_postfix(Uint128);
int APS5_VABI __popcountsi2_nid_postfix(std::uint32_t);
int APS5_VABI __popcountdi2_nid_postfix(std::uint64_t);
int APS5_VABI __popcountti2_nid_postfix(Uint128);
int APS5_VABI __cmpdi2_nid_postfix(std::int64_t, std::int64_t);
int APS5_VABI __cmpti2_nid_postfix(Int128, Int128);
int APS5_VABI __ucmpdi2_nid_postfix(std::uint64_t, std::uint64_t);
int APS5_VABI __ucmpti2_nid_postfix(Uint128, Uint128);
std::int64_t APS5_VABI __negdi2_nid_postfix(std::int64_t);
Int128 APS5_VABI __negti2_nid_postfix(Int128);
std::int64_t APS5_VABI __muldi3_nid_postfix(std::int64_t, std::int64_t);
Int128 APS5_VABI __multi3_nid_postfix(Int128, Int128);
std::int32_t APS5_VABI __divsi3_nid_postfix(std::int32_t, std::int32_t);
std::int64_t APS5_VABI __divdi3_nid_postfix(std::int64_t, std::int64_t);
std::int32_t APS5_VABI __modsi3_nid_postfix(std::int32_t, std::int32_t);
std::int64_t APS5_VABI __moddi3_nid_postfix(std::int64_t, std::int64_t);
std::uint32_t APS5_VABI __udivsi3_nid_postfix(std::uint32_t, std::uint32_t);
std::uint64_t APS5_VABI __udivdi3_nid_postfix(std::uint64_t, std::uint64_t);
std::uint32_t APS5_VABI __umodsi3_nid_postfix(std::uint32_t, std::uint32_t);
std::uint64_t APS5_VABI __umoddi3_nid_postfix(std::uint64_t, std::uint64_t);
std::int32_t APS5_VABI __divmodsi4_nid_postfix(std::int32_t, std::int32_t, std::int32_t*);
std::int64_t APS5_VABI __divmoddi4_nid_postfix(std::int64_t, std::int64_t, std::int64_t*);
std::uint32_t APS5_VABI __udivmodsi4_nid_postfix(std::uint32_t, std::uint32_t, std::uint32_t*);
std::uint64_t APS5_VABI __udivmoddi4_nid_postfix(std::uint64_t, std::uint64_t, std::uint64_t*);
Uint128 APS5_VABI __udivmodti4_nid_postfix(Uint128, Uint128, Uint128*);
std::int32_t APS5_VABI __mulosi4_nid_postfix(std::int32_t, std::int32_t, int*);
std::int64_t APS5_VABI __mulodi4_nid_postfix(std::int64_t, std::int64_t, int*);
Int128 APS5_VABI __muloti4_nid_postfix(Int128, Int128, int*);
std::int32_t APS5_VABI __mulvsi3_nid_postfix(std::int32_t, std::int32_t);
std::int64_t APS5_VABI __mulvdi3_nid_postfix(std::int64_t, std::int64_t);
Int128 APS5_VABI __mulvti3_nid_postfix(Int128, Int128);
std::int32_t APS5_VABI __addvsi3_nid_postfix(std::int32_t, std::int32_t);
std::int64_t APS5_VABI __addvdi3_nid_postfix(std::int64_t, std::int64_t);
Int128 APS5_VABI __addvti3_nid_postfix(Int128, Int128);
std::int32_t APS5_VABI __subvsi3_nid_postfix(std::int32_t, std::int32_t);
std::int64_t APS5_VABI __subvdi3_nid_postfix(std::int64_t, std::int64_t);
Int128 APS5_VABI __subvti3_nid_postfix(Int128, Int128);
std::int32_t APS5_VABI __negvsi2_nid_postfix(std::int32_t);
std::int64_t APS5_VABI __negvdi2_nid_postfix(std::int64_t);
Int128 APS5_VABI __negvti2_nid_postfix(Int128);
std::int32_t APS5_VABI __absvsi2_nid_postfix(std::int32_t);
std::int64_t APS5_VABI __absvdi2_nid_postfix(std::int64_t);
Int128 APS5_VABI __absvti2_nid_postfix(Int128);
}

static void Require(bool value) { if (!value) std::abort(); }

template<typename TFunction>
static void RequireThrows(TFunction function) {
    try {
        function();
    } catch (const std::runtime_error&) {
        return;
    }
    std::abort();
}

static Uint128 Make(std::uint64_t high, std::uint64_t low) {
    return (static_cast<Uint128>(high) << 64) | low;
}

static constexpr Int128 Int128Max = static_cast<Int128>(~(static_cast<Uint128>(1) << 127));
static constexpr Int128 Int128Min = static_cast<Int128>(static_cast<Uint128>(1) << 127);

static void CheckShifts() {
    Require(__ashldi3_nid_postfix(1, 63) == INT64_MIN);
    Require(__ashrdi3_nid_postfix(INT64_MIN, 63) == -1);
    Require(__lshrdi3_nid_postfix(INT64_MIN, 63) == 1);
    Require(static_cast<Uint128>(__ashlti3_nid_postfix(1, 100)) == Make(1ull << 36, 0));
    Require(__ashrti3_nid_postfix(Int128Min, 127) == -1);
    Require(__lshrti3_nid_postfix(Int128Min, 127) == 1);
    Require(static_cast<Uint128>(__lshrti3_nid_postfix(static_cast<Int128>(Make(0x8000000000000001ull, 0)), 64)) == Make(0, 0x8000000000000001ull));
    Require(__ashldi3_nid_postfix(12345, 0) == 12345);
    Require(__ashrdi3_nid_postfix(-12345, 0) == -12345);
    Require(__lshrdi3_nid_postfix(INT64_MIN, 0) == INT64_MIN);
    Require(__ashlti3_nid_postfix(Int128Max, 0) == Int128Max);
    Require(__ashrti3_nid_postfix(Int128Min, 0) == Int128Min);
    Require(__lshrti3_nid_postfix(Int128Max, 0) == Int128Max);
    Require(__ashldi3_nid_postfix(0, 10) == 0);
    Require(__ashrdi3_nid_postfix(0, 10) == 0);
    Require(__lshrdi3_nid_postfix(0, 10) == 0);
    Require(__ashlti3_nid_postfix(0, 70) == 0);
    Require(__ashrti3_nid_postfix(0, 70) == 0);
    Require(__lshrti3_nid_postfix(0, 70) == 0);
    Require(static_cast<Uint128>(__ashlti3_nid_postfix(1, 64)) == Make(1, 0));
    Require(static_cast<Uint128>(__ashlti3_nid_postfix(1, 127)) == Make(1ull << 63, 0));
    Require(__lshrti3_nid_postfix(static_cast<Int128>(Make(1, 0)), 64) == 1);
    Require(__lshrti3_nid_postfix(Int128Min, 127) == 1);
    Require(__ashrti3_nid_postfix(Int128Min, 64) == static_cast<Int128>(Make(~0ull, 1ull << 63)));
}

static void CheckBitCounts() {
    Require(__clzsi2_nid_postfix(1) == 31 && __clzdi2_nid_postfix(1) == 63);
    Require(__clzti2_nid_postfix(1) == 127 && __clzti2_nid_postfix(Make(1, 0)) == 63);
    Require(__ctzsi2_nid_postfix(0x80000000u) == 31 && __ctzdi2_nid_postfix(1ull << 40) == 40);
    Require(__ctzti2_nid_postfix(Make(1, 0)) == 64 && __ctzti2_nid_postfix(4) == 2);
    Require(__clzdi2_nid_postfix(0) == 32 && __ctzdi2_nid_postfix(0) == 32);
    Require(__clzti2_nid_postfix(0) == 64 && __ctzti2_nid_postfix(0) == 64);
    Require(__ffsdi2_nid_postfix(0) == 0 && __ffsdi2_nid_postfix(8) == 4);
    Require(__ffsti2_nid_postfix(0) == 0 && __ffsti2_nid_postfix(Make(2, 0)) == 66);
    Require(__paritysi2_nid_postfix(7) == 1 && __paritydi2_nid_postfix(3) == 0);
    Require(__parityti2_nid_postfix(Make(1, 1)) == 0 && __parityti2_nid_postfix(Make(1, 0)) == 1);
    Require(__popcountsi2_nid_postfix(0xffffffffu) == 32 && __popcountdi2_nid_postfix(~0ull) == 64);
    Require(__popcountti2_nid_postfix(Make(~0ull, ~0ull)) == 128);
    Require(__clzsi2_nid_postfix(0) == 32);
    Require(__ctzsi2_nid_postfix(0) == 32);
    Require(__popcountsi2_nid_postfix(0) == 0);
    Require(__popcountdi2_nid_postfix(0) == 0);
    Require(__popcountti2_nid_postfix(0) == 0);
    Require(__paritysi2_nid_postfix(0) == 0);
    Require(__paritydi2_nid_postfix(0) == 0);
    Require(__parityti2_nid_postfix(0) == 0);
    Require(__clzsi2_nid_postfix(0x80000000u) == 0);
    Require(__clzdi2_nid_postfix(1ull << 63) == 0);
    Require(__clzti2_nid_postfix(Make(1ull << 63, 0)) == 0);
    Require(__ctzsi2_nid_postfix(1) == 0);
    Require(__ctzdi2_nid_postfix(1) == 0);
    Require(__ctzti2_nid_postfix(1) == 0);
    Require(__ffsdi2_nid_postfix(1) == 1 && __ffsdi2_nid_postfix(1ull << 63) == 64);
    Require(__ffsti2_nid_postfix(1) == 1 && __ffsti2_nid_postfix(Make(1ull << 63, 0)) == 128);
}

static void CheckComparisons() {
    Require(__cmpdi2_nid_postfix(-1, 0) == 0 && __cmpdi2_nid_postfix(5, 5) == 1 && __cmpdi2_nid_postfix(6, 5) == 2);
    Require(__cmpti2_nid_postfix(Int128Min, 0) == 0 && __cmpti2_nid_postfix(Int128Max, 0) == 2);
    Require(__ucmpdi2_nid_postfix(~0ull, 0) == 2 && __ucmpdi2_nid_postfix(0, 1) == 0);
    Require(__ucmpti2_nid_postfix(Make(1, 0), Make(0, ~0ull)) == 2 && __ucmpti2_nid_postfix(3, 3) == 1);
    Require(__cmpdi2_nid_postfix(INT64_MIN, INT64_MIN) == 1 && __cmpdi2_nid_postfix(INT64_MAX, INT64_MAX) == 1);
    Require(__cmpdi2_nid_postfix(INT64_MIN, INT64_MAX) == 0 && __cmpdi2_nid_postfix(INT64_MAX, INT64_MIN) == 2);
    Require(__cmpti2_nid_postfix(Int128Min, Int128Min) == 1 && __cmpti2_nid_postfix(Int128Max, Int128Max) == 1);
    Require(__cmpti2_nid_postfix(Int128Min, Int128Max) == 0 && __cmpti2_nid_postfix(-1, 1) == 0 && __cmpti2_nid_postfix(1, -1) == 2);
    Require(__ucmpdi2_nid_postfix(~0ull, ~0ull) == 1 && __ucmpdi2_nid_postfix(0, 0) == 1);
    Require(__ucmpti2_nid_postfix(0, 0) == 1 && __ucmpti2_nid_postfix(Make(~0ull, ~0ull), Make(~0ull, ~0ull)) == 1);
    Require(__ucmpti2_nid_postfix(0, Make(~0ull, ~0ull)) == 0 && __ucmpti2_nid_postfix(Make(0, ~0ull), Make(1, 0)) == 0);
}

static void CheckArithmetic() {
    Require(__negdi2_nid_postfix(INT64_MIN) == INT64_MIN && __negdi2_nid_postfix(5) == -5);
    Require(__negti2_nid_postfix(Int128Min) == Int128Min && __negti2_nid_postfix(-7) == 7);
    Require(__muldi3_nid_postfix(-3, 7) == -21);
    Require(static_cast<Uint128>(__multi3_nid_postfix(static_cast<Int128>(Make(0, ~0ull)), static_cast<Int128>(Make(0, ~0ull))))
        == Make(0xfffffffffffffffeull, 1));
    Require(__multi3_nid_postfix(-3, static_cast<Int128>(Make(1, 0))) == -static_cast<Int128>(Make(3, 0)));
    Require(__negdi2_nid_postfix(0) == 0 && __negdi2_nid_postfix(1) == -1 && __negdi2_nid_postfix(-1) == 1);
    Require(__negti2_nid_postfix(0) == 0 && __negti2_nid_postfix(1) == -1 && __negti2_nid_postfix(-1) == 1);
    Require(__negti2_nid_postfix(Int128Max) == -Int128Max);
    Require(__muldi3_nid_postfix(0, 100) == 0 && __muldi3_nid_postfix(100, 0) == 0);
    Require(__muldi3_nid_postfix(1, INT64_MIN) == INT64_MIN && __muldi3_nid_postfix(1, INT64_MAX) == INT64_MAX);
    Require(__muldi3_nid_postfix(-1, INT64_MAX) == -INT64_MAX && __muldi3_nid_postfix(-1, INT64_MIN) == INT64_MIN);
    Require(__muldi3_nid_postfix(INT64_MAX, 2) == -2);
    Require(__multi3_nid_postfix(0, Int128Max) == 0 && __multi3_nid_postfix(Int128Min, 0) == 0);
    Require(__multi3_nid_postfix(1, Int128Min) == Int128Min && __multi3_nid_postfix(1, Int128Max) == Int128Max);
    Require(__multi3_nid_postfix(-1, Int128Max) == -Int128Max && __multi3_nid_postfix(-1, Int128Min) == Int128Min);
    Require(__multi3_nid_postfix(Int128Max, 2) == -2 && __multi3_nid_postfix(Int128Min, 2) == 0);
}

static void CheckDivision() {
    Require(__divsi3_nid_postfix(-7, 2) == -3 && __modsi3_nid_postfix(-7, 2) == -1);
    Require(__divdi3_nid_postfix(INT64_MIN, -1) == INT64_MIN && __moddi3_nid_postfix(INT64_MIN, -1) == 0);
    Require(__udivsi3_nid_postfix(0xffffffffu, 16) == 0x0fffffffu && __umodsi3_nid_postfix(0xffffffffu, 16) == 15);
    Require(__udivdi3_nid_postfix(~0ull, 3) == 0x5555555555555555ull && __umoddi3_nid_postfix(10, 4) == 2);
    std::int32_t remainder32 = 0;
    Require(__divmodsi4_nid_postfix(-9, 4, &remainder32) == -2 && remainder32 == -1);
    std::int64_t remainder64 = 0;
    Require(__divmoddi4_nid_postfix(9, -4, &remainder64) == -2 && remainder64 == 1);
    std::uint32_t uremainder32 = 0;
    Require(__udivmodsi4_nid_postfix(17, 5, &uremainder32) == 3 && uremainder32 == 2);
    std::uint64_t uremainder64 = 0;
    Require(__udivmoddi4_nid_postfix(17, 5, &uremainder64) == 3 && uremainder64 == 2);
    Require(__udivmoddi4_nid_postfix(17, 5, nullptr) == 3);
    Uint128 remainder128 = 0;
    Require(__udivmodti4_nid_postfix(Make(1, 5), 2, &remainder128) == Make(0, 0x8000000000000002ull) && remainder128 == 1);
    Require(__udivsi3_nid_postfix(7, 0) == 0 && __umodsi3_nid_postfix(7, 0) == 7);
    Require(__udivmodsi4_nid_postfix(7, 0, &uremainder32) == 0 && uremainder32 == 7);
    RequireThrows([] { __divsi3_nid_postfix(1, 0); });
    RequireThrows([] { __modsi3_nid_postfix(1, 0); });
    RequireThrows([] { __udivdi3_nid_postfix(1, 0); });
    RequireThrows([] { __umoddi3_nid_postfix(1, 0); });
    RequireThrows([] { __udivmodti4_nid_postfix(1, 0, nullptr); });
    Require(__divsi3_nid_postfix(INT32_MIN, -1) == INT32_MIN && __modsi3_nid_postfix(INT32_MIN, -1) == 0);
    Require(__divsi3_nid_postfix(0, 5) == 0 && __modsi3_nid_postfix(0, 5) == 0);
    Require(__divdi3_nid_postfix(0, 5) == 0 && __moddi3_nid_postfix(0, 5) == 0);
    Require(__divmodsi4_nid_postfix(INT32_MIN, -1, &remainder32) == INT32_MIN && remainder32 == 0);
    Require(__divmoddi4_nid_postfix(INT64_MIN, -1, &remainder64) == INT64_MIN && remainder64 == 0);
    Require(__udivsi3_nid_postfix(0, 5) == 0 && __umodsi3_nid_postfix(0, 5) == 0);
    Require(__udivdi3_nid_postfix(0, 5) == 0 && __umoddi3_nid_postfix(0, 5) == 0);
    Require(__udivmodsi4_nid_postfix(0, 5, &uremainder32) == 0 && uremainder32 == 0);
    Require(__udivmoddi4_nid_postfix(0, 5, &uremainder64) == 0 && uremainder64 == 0);
    Require(__udivmodti4_nid_postfix(0, 5, &remainder128) == 0 && remainder128 == 0);
    Require(__udivmodti4_nid_postfix(Make(~0ull, ~0ull), Make(~0ull, ~0ull), &remainder128) == 1 && remainder128 == 0);
    Require(__udivmodti4_nid_postfix(Make(0, 10), Make(1, 0), &remainder128) == 0 && remainder128 == 10);
}

static void CheckOverflow() {
    int overflow = -1;
    Require(__mulosi4_nid_postfix(0x10000, 0x8000, &overflow) == INT32_MIN && overflow == 1);
    Require(__mulosi4_nid_postfix(-0x10000, 0x8000, &overflow) == INT32_MIN && overflow == 0);
    Require(__mulodi4_nid_postfix(INT64_MIN, 1, &overflow) == INT64_MIN && overflow == 0);
    Require(__mulodi4_nid_postfix(INT64_MIN, -1, &overflow) == INT64_MIN && overflow == 1);
    Require(__mulodi4_nid_postfix(3037000500, 3037000500, &overflow) && overflow == 1);
    Require(__mulodi4_nid_postfix(3037000499, 3037000499, &overflow) == 9223372030926249001 && overflow == 0);
    Require(__muloti4_nid_postfix(Int128Max, -1, &overflow) == -Int128Max && overflow == 0);
    Require(__muloti4_nid_postfix(Int128Max, 2, &overflow) == -2 && overflow == 1);
    Require(__muloti4_nid_postfix(static_cast<Int128>(Make(0, 1ull << 63)), -static_cast<Int128>(Make(1, 0)), &overflow) == Int128Min
        && overflow == 0);
    Require(__mulvsi3_nid_postfix(-46341, 46340) == -2147441940);
    Require(__mulvdi3_nid_postfix(-4, 5) == -20 && __mulvti3_nid_postfix(-4, 5) == -20);
    RequireThrows([] { __mulvsi3_nid_postfix(46341, 46341); });
    RequireThrows([] { __mulvdi3_nid_postfix(INT64_MIN, -1); });
    RequireThrows([] { __mulvti3_nid_postfix(Int128Min, 2); });
    Require(__addvsi3_nid_postfix(INT32_MAX, INT32_MIN) == -1 && __addvdi3_nid_postfix(-1, 1) == 0);
    Require(__addvti3_nid_postfix(Int128Max, -1) == Int128Max - 1);
    Require(__subvsi3_nid_postfix(INT32_MIN, -1) == INT32_MIN + 1 && __subvdi3_nid_postfix(0, INT64_MAX) == -INT64_MAX);
    Require(__subvti3_nid_postfix(0, Int128Max) == Int128Min + 1);
    Require(__addvsi3_nid_postfix(INT32_MAX, 1) == INT32_MIN && __addvti3_nid_postfix(Int128Min, -1) == Int128Max);
    Require(__subvdi3_nid_postfix(INT64_MIN, 1) == INT64_MAX && __subvti3_nid_postfix(Int128Max, -1) == Int128Min);
    Require(__negvsi2_nid_postfix(5) == -5 && __negvdi2_nid_postfix(-5) == 5 && __negvti2_nid_postfix(Int128Max) == Int128Min + 1);
    Require(__absvsi2_nid_postfix(-5) == 5 && __absvdi2_nid_postfix(5) == 5 && __absvti2_nid_postfix(-Int128Max) == Int128Max);
    RequireThrows([] { __negvsi2_nid_postfix(INT32_MIN); });
    RequireThrows([] { __absvdi2_nid_postfix(INT64_MIN); });
    RequireThrows([] { __absvti2_nid_postfix(Int128Min); });
    Require(__mulosi4_nid_postfix(INT32_MIN, 1, &overflow) == INT32_MIN && overflow == 0);
    Require(__mulosi4_nid_postfix(INT32_MIN, -1, &overflow) == INT32_MIN && overflow == 1);
    Require(__mulosi4_nid_postfix(0, INT32_MIN, &overflow) == 0 && overflow == 0);
    Require(__mulodi4_nid_postfix(0, INT64_MIN, &overflow) == 0 && overflow == 0);
    Require(__muloti4_nid_postfix(Int128Min, 1, &overflow) == Int128Min && overflow == 0);
    Require(__muloti4_nid_postfix(Int128Min, -1, &overflow) == Int128Min && overflow == 1);
    Require(__muloti4_nid_postfix(0, Int128Min, &overflow) == 0 && overflow == 0);
    Require(__negvsi2_nid_postfix(0) == 0 && __negvdi2_nid_postfix(0) == 0 && __negvti2_nid_postfix(0) == 0);
    Require(__absvsi2_nid_postfix(0) == 0 && __absvdi2_nid_postfix(0) == 0 && __absvti2_nid_postfix(0) == 0);
}

int main() {
    CheckShifts();
    CheckBitCounts();
    CheckComparisons();
    CheckArithmetic();
    CheckDivision();
    CheckOverflow();
    return 0;
}
