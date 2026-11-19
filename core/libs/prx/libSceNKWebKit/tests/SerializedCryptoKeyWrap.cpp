#include "prx/libSceNKWebKit/include/SerializedCryptoKeyWrapExports.hpp"

#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <initializer_list>

namespace {

using ByteVector = NKWebKit::WtfAbi::Vector<unsigned char>;

static_assert(sizeof(ByteVector) == 16);
static_assert(offsetof(ByteVector, buffer) == 0);
static_assert(offsetof(ByteVector, capacity) == 8);
static_assert(offsetof(ByteVector, size) == 12);

using Transform = decltype(&_ZN7WebCore23wrapSerializedCryptoKeyERKN3WTF6VectorIhLm0ENS0_15CrashOnOverflowELm16ENS0_10FastMallocEEES6_RS4_);

void Check(bool value, int line) {
    if (!value) {
        std::fprintf(stderr, "serialized crypto key wrap check failed at line %d\n", line);
        std::abort();
    }
}

#define Require(value) Check((value), __LINE__)

ByteVector Bytes(std::initializer_list<unsigned char> values) {
    ByteVector vector;
    for (unsigned char value : values) {
        vector.append(value);
    }
    return vector;
}

bool Equals(const ByteVector& vector, std::initializer_list<unsigned char> values) {
    if (vector.size != values.size()) return false;
    return values.size() == 0 || std::memcmp(vector.buffer, values.begin(), values.size()) == 0;
}

void CopiesIntoEmptyResult(Transform transform) {
    ByteVector master = Bytes({9, 8, 7, 6});
    ByteVector key = Bytes({1, 2, 3});
    ByteVector result;
    Require(transform(master, key, result));
    Require(Equals(result, {1, 2, 3}));
    Require(result.capacity == 3);
    Require(result.buffer != nullptr);
    Require(result.buffer != key.buffer);
    Require(Equals(key, {1, 2, 3}));
    Require(Equals(master, {9, 8, 7, 6}));
}

void ReplacesExistingResult(Transform transform) {
    ByteVector master;
    ByteVector key = Bytes({0x10, 0x20});
    ByteVector result = Bytes({0xAA, 0xBB, 0xCC, 0xDD, 0xEE});
    unsigned char* oldBuffer = result.buffer;
    Require(transform(master, key, result));
    Require(Equals(result, {0x10, 0x20}));
    Require(result.capacity == 2);
    Require(result.buffer != oldBuffer);
    Require(result.buffer != key.buffer);
}

void EmptyKeyGivesEmptyResult(Transform transform) {
    ByteVector master = Bytes({1});
    ByteVector key;
    ByteVector result = Bytes({4, 5, 6});
    Require(transform(master, key, result));
    Require(result.size == 0);
    Require(result.capacity == 0);
    Require(result.buffer == nullptr);

    ByteVector reserved(16);
    ByteVector second = Bytes({7});
    Require(transform(master, reserved, second));
    Require(second.size == 0);
    Require(second.capacity == 0);
    Require(second.buffer == nullptr);
}

void IgnoresMasterKey(Transform transform) {
    ByteVector firstMaster = Bytes({1, 1, 1, 1});
    ByteVector secondMaster = Bytes({2, 2, 2, 2, 2, 2, 2, 2});
    ByteVector emptyMaster;
    ByteVector key = Bytes({0x5A, 0x00, 0xFF, 0x7E});
    ByteVector first;
    ByteVector second;
    ByteVector third;
    Require(transform(firstMaster, key, first));
    Require(transform(secondMaster, key, second));
    Require(transform(emptyMaster, key, third));
    Require(Equals(first, {0x5A, 0x00, 0xFF, 0x7E}));
    Require(Equals(second, {0x5A, 0x00, 0xFF, 0x7E}));
    Require(Equals(third, {0x5A, 0x00, 0xFF, 0x7E}));
}

void RoundTrips() {
    ByteVector master = Bytes({3, 1, 4, 1, 5});
    ByteVector key = Bytes({2, 7, 1, 8, 2, 8});
    ByteVector wrapped;
    ByteVector unwrapped;
    Require(_ZN7WebCore23wrapSerializedCryptoKeyERKN3WTF6VectorIhLm0ENS0_15CrashOnOverflowELm16ENS0_10FastMallocEEES6_RS4_(master, key, wrapped));
    Require(_ZN7WebCore25unwrapSerializedCryptoKeyERKN3WTF6VectorIhLm0ENS0_15CrashOnOverflowELm16ENS0_10FastMallocEEES6_RS4_(master, wrapped, unwrapped));
    Require(Equals(unwrapped, {2, 7, 1, 8, 2, 8}));
    Require(unwrapped.buffer != wrapped.buffer);
}

void RunAll(Transform transform) {
    CopiesIntoEmptyResult(transform);
    ReplacesExistingResult(transform);
    EmptyKeyGivesEmptyResult(transform);
    IgnoresMasterKey(transform);
}

}  // namespace

int main() {
    RunAll(&_ZN7WebCore23wrapSerializedCryptoKeyERKN3WTF6VectorIhLm0ENS0_15CrashOnOverflowELm16ENS0_10FastMallocEEES6_RS4_);
    RunAll(&_ZN7WebCore25unwrapSerializedCryptoKeyERKN3WTF6VectorIhLm0ENS0_15CrashOnOverflowELm16ENS0_10FastMallocEEES6_RS4_);
    RoundTrips();
    std::puts("serialized crypto key wrap tests passed");
    return 0;
}
