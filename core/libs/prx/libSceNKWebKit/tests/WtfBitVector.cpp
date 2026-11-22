#include "prx/libc/include/general/VabiMacros.hpp"
#include "prx/libSceNKWebKit/libSceNKWebKit.hpp"

#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>

static void Check(bool value, int line) {
    if (!value) {
        std::fprintf(stderr, "WTF::BitVector check failed at line %d\n", line);
        std::abort();
    }
}
#define Require(value) Check((value), __LINE__)

namespace {

using WTF::BitVector;
using OutOfLineBits = WTF::BitVector::OutOfLineBits;

constexpr std::uintptr_t InlineFlag = std::uintptr_t{1} << 63;

BitVector Inline(std::uintptr_t bits) {
    return BitVector{bits | InlineFlag};
}

bool IsInline(const BitVector& vector) {
    return vector.bitsOrPointer & InlineFlag;
}

OutOfLineBits* OutOfLine(const BitVector& vector) {
    return reinterpret_cast<OutOfLineBits*>(vector.bitsOrPointer << 1);
}

std::uintptr_t* Words(const BitVector& vector) {
    return reinterpret_cast<std::uintptr_t*>(OutOfLine(vector) + 1);
}

BitVector OutOfLineVector(std::size_t numBits) {
    BitVector vector = Inline(0);
    _ZN3WTF9BitVector6resizeEm(&vector, numBits);
    return vector;
}

void Release(BitVector& vector) {
    if (!IsInline(vector)) _ZN3WTF8fastFreeEPv(OutOfLine(vector));
    vector = Inline(0);
}

}  // namespace

static void TestCreate() {
    OutOfLineBits* one = _ZN3WTF9BitVector13OutOfLineBits6createEm(1);
    OutOfLineBits* word = _ZN3WTF9BitVector13OutOfLineBits6createEm(64);
    OutOfLineBits* more = _ZN3WTF9BitVector13OutOfLineBits6createEm(65);
    Require(one->numBits == 64);
    Require(word->numBits == 64);
    Require(more->numBits == 128);
    _ZN3WTF8fastFreeEPv(one);
    _ZN3WTF8fastFreeEPv(word);
    _ZN3WTF8fastFreeEPv(more);
}

static void TestResize() {
    BitVector vector = Inline(0x5);
    _ZN3WTF9BitVector6resizeEm(&vector, 10);
    Require(vector.bitsOrPointer == (0x5 | InlineFlag));

    _ZN3WTF9BitVector6resizeEm(&vector, 200);
    Require(!IsInline(vector));
    Require((vector.bitsOrPointer >> 63) == 0);
    Require(OutOfLine(vector)->numBits == 256);
    Require(Words(vector)[0] == 0x5);
    Require(Words(vector)[1] == 0 && Words(vector)[2] == 0 && Words(vector)[3] == 0);

    Words(vector)[1] = 0x10;
    Words(vector)[3] = 0x20;
    _ZN3WTF9BitVector6resizeEm(&vector, 300);
    Require(OutOfLine(vector)->numBits == 320);
    Require(Words(vector)[0] == 0x5 && Words(vector)[1] == 0x10 && Words(vector)[3] == 0x20 && Words(vector)[4] == 0);

    _ZN3WTF9BitVector6resizeEm(&vector, 100);
    Require(OutOfLine(vector)->numBits == 128);
    Require(Words(vector)[0] == 0x5 && Words(vector)[1] == 0x10);

    _ZN3WTF9BitVector6resizeEm(&vector, 63);
    Require(vector.bitsOrPointer == (0x5 | InlineFlag));
}

static void TestSetSlow() {
    BitVector source = OutOfLineVector(130);
    Words(source)[0] = 0x1;
    Words(source)[2] = 0x3;

    BitVector copy = Inline(0x7);
    _ZN3WTF9BitVector7setSlowERKS0_(&copy, &source);
    Require(!IsInline(copy));
    Require(OutOfLine(copy) != OutOfLine(source));
    Require(OutOfLine(copy)->numBits == 192);
    Require(Words(copy)[0] == 0x1 && Words(copy)[1] == 0 && Words(copy)[2] == 0x3);

    BitVector small = Inline(0x9);
    _ZN3WTF9BitVector7setSlowERKS0_(&copy, &small);
    Require(copy.bitsOrPointer == small.bitsOrPointer);

    BitVector empty{0};
    BitVector deleted{1};
    BitVector target = OutOfLineVector(100);
    _ZN3WTF9BitVector7setSlowERKS0_(&target, &deleted);
    Require(target.bitsOrPointer == 1);
    _ZN3WTF9BitVector7setSlowERKS0_(&target, &empty);
    Require(target.bitsOrPointer == 0);

    Release(source);
}

static void TestClearAll() {
    BitVector small = Inline(0xFF);
    _ZN3WTF9BitVector8clearAllEv(&small);
    Require(small.bitsOrPointer == InlineFlag);

    BitVector large = OutOfLineVector(150);
    Words(large)[0] = ~std::uintptr_t{0};
    Words(large)[2] = 0x4;
    _ZN3WTF9BitVector8clearAllEv(&large);
    Require(OutOfLine(large)->numBits == 192);
    Require(Words(large)[0] == 0 && Words(large)[1] == 0 && Words(large)[2] == 0);
    Release(large);
}

static void TestMergeSlow() {
    BitVector large = OutOfLineVector(200);
    Words(large)[0] = 0x100;
    Words(large)[3] = 0x8;

    BitVector small = Inline(0x3);
    _ZN3WTF9BitVector9mergeSlowERKS0_(&small, &large);
    Require(!IsInline(small));
    Require(OutOfLine(small)->numBits == 256);
    Require(Words(small)[0] == 0x103 && Words(small)[1] == 0 && Words(small)[3] == 0x8);

    BitVector bits = Inline(0x30);
    _ZN3WTF9BitVector9mergeSlowERKS0_(&large, &bits);
    Require(Words(large)[0] == 0x130 && Words(large)[3] == 0x8);

    BitVector shorter = OutOfLineVector(70);
    Words(shorter)[1] = 0x2;
    _ZN3WTF9BitVector9mergeSlowERKS0_(&large, &shorter);
    Require(OutOfLine(large)->numBits == 256);
    Require(Words(large)[1] == 0x2 && Words(large)[3] == 0x8);

    Release(large);
    Release(small);
    Release(shorter);
}

static void TestExcludeSlow() {
    BitVector large = OutOfLineVector(130);
    Words(large)[0] = 0xF0;
    Words(large)[1] = 0xF;

    BitVector small = Inline(0xFF);
    _ZN3WTF9BitVector11excludeSlowERKS0_(&small, &large);
    Require(small.bitsOrPointer == (0x0F | InlineFlag));

    BitVector mask = Inline(0x30);
    _ZN3WTF9BitVector11excludeSlowERKS0_(&large, &mask);
    Require(Words(large)[0] == 0xC0 && Words(large)[1] == 0xF);

    BitVector other = OutOfLineVector(70);
    Words(other)[0] = 0x80;
    Words(other)[1] = 0x1;
    _ZN3WTF9BitVector11excludeSlowERKS0_(&large, &other);
    Require(Words(large)[0] == 0x40 && Words(large)[1] == 0xE && Words(large)[2] == 0);

    Release(large);
    Release(other);
}

static void TestQueries() {
    BitVector vector = OutOfLineVector(200);
    Require(_ZNK3WTF9BitVector11isEmptySlowEv(&vector));
    Require(_ZNK3WTF9BitVector12bitCountSlowEv(&vector) == 0);
    Require(_ZNK3WTF9BitVector12hashSlowCaseEv(&vector) == 0);

    Words(vector)[0] = 0xF;
    Words(vector)[3] = 0x3;
    Require(!_ZNK3WTF9BitVector11isEmptySlowEv(&vector));
    Require(_ZNK3WTF9BitVector12bitCountSlowEv(&vector) == 6);
    Require(_ZNK3WTF9BitVector12hashSlowCaseEv(&vector) == (0xF ^ 0x3));

    Words(vector)[3] = ~std::uintptr_t{0};
    Require(_ZNK3WTF9BitVector12bitCountSlowEv(&vector) == 68);
    Release(vector);
}

static void TestEqualsSlowCase() {
    BitVector small = Inline(0x21);
    BitVector large = OutOfLineVector(200);
    Words(large)[0] = 0x21;
    Require(_ZNK3WTF9BitVector14equalsSlowCaseERKS0_(&small, &large));
    Require(_ZNK3WTF9BitVector14equalsSlowCaseERKS0_(&large, &small));

    Words(large)[2] = 0x1;
    Require(!_ZNK3WTF9BitVector14equalsSlowCaseERKS0_(&small, &large));

    BitVector shorter = OutOfLineVector(100);
    Words(shorter)[0] = 0x21;
    Require(!_ZNK3WTF9BitVector14equalsSlowCaseERKS0_(&large, &shorter));
    Words(large)[2] = 0;
    Require(_ZNK3WTF9BitVector14equalsSlowCaseERKS0_(&large, &shorter));
    Require(_ZNK3WTF9BitVector14equalsSlowCaseERKS0_(&shorter, &large));

    Words(shorter)[1] = 0x4;
    Require(!_ZNK3WTF9BitVector14equalsSlowCaseERKS0_(&shorter, &large));

    BitVector same = Inline(0x21);
    BitVector different = Inline(0x22);
    Require(_ZNK3WTF9BitVector14equalsSlowCaseERKS0_(&small, &same));
    Require(!_ZNK3WTF9BitVector14equalsSlowCaseERKS0_(&small, &different));

    Release(large);
    Release(shorter);
}

int main() {
    TestCreate();
    TestResize();
    TestSetSlow();
    TestClearAll();
    TestMergeSlow();
    TestExcludeSlow();
    TestQueries();
    TestEqualsSlowCase();
    return 0;
}
