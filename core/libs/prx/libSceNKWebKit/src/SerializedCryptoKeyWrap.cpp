#include "prx/libSceNKWebKit/include/SerializedCryptoKeyWrapExports.hpp"

#include <cstddef>

namespace {

using ByteVector = NKWebKit::WtfAbi::Vector<unsigned char>;

static_assert(sizeof(ByteVector) == 16);
static_assert(offsetof(ByteVector, buffer) == 0);
static_assert(offsetof(ByteVector, capacity) == 8);
static_assert(offsetof(ByteVector, size) == 12);

}  // namespace

extern "C" {

bool APS5_VABI _ZN7WebCore23wrapSerializedCryptoKeyERKN3WTF6VectorIhLm0ENS0_15CrashOnOverflowELm16ENS0_10FastMallocEEES6_RS4_(const ByteVector&, const ByteVector& key, ByteVector& result) {
    result = ByteVector(key);
    return true;
}

bool APS5_VABI _ZN7WebCore25unwrapSerializedCryptoKeyERKN3WTF6VectorIhLm0ENS0_15CrashOnOverflowELm16ENS0_10FastMallocEEES6_RS4_(const ByteVector&, const ByteVector& wrappedKey, ByteVector& key) {
    key = ByteVector(wrappedKey);
    return true;
}

}
