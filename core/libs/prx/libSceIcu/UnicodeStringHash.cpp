#include "prx/libSceIcu/include/LegacyExports.hpp"
#include "prx/libc/include/General.hpp"
#include "unicode/uchar.h"
#include "unicode/unistr.h"
#include <cerrno>
#include <cstddef>
#include <cstdint>
#include <cstring>

namespace {

constexpr std::size_t GuestObjectSize = 64;
constexpr std::size_t FlagsOffset = 8;
constexpr std::size_t LongLengthOffset = 12;
constexpr std::size_t StackBufferOffset = 10;
constexpr std::size_t ArrayOffset = 24;
constexpr std::int32_t StackBufferCapacity = 27;
constexpr std::int16_t IsBogus = 1;
constexpr std::int16_t UsingStackBuffer = 2;
constexpr int LengthShift = 5;
constexpr std::int32_t ChunkSize = 256;

bool CopyIn(void* destination, const void* source, std::size_t bytes) {
    const int savedErrno = errno;
    const bool copied = GuestCopyIn_nid_no_patch(destination, source, bytes);
    errno = savedErrno;
    return copied;
}

icu::UnicodeString ReadGuest(const void* object, const char* caller) {
    unsigned char raw[GuestObjectSize];
    if (!CopyIn(raw, object, sizeof(raw))) NotImplemented_nid_no_patch(caller);
    std::int16_t flags = 0;
    std::memcpy(&flags, raw + FlagsOffset, sizeof(flags));
    icu::UnicodeString result;
    if (flags & IsBogus) {
        result.setToBogus();
        return result;
    }
    std::int32_t length = flags >> LengthShift;
    if (flags < 0) std::memcpy(&length, raw + LongLengthOffset, sizeof(length));
    if (length < 0) NotImplemented_nid_no_patch(caller);
    if (length == 0) return result;
    if (flags & UsingStackBuffer) {
        if (length > StackBufferCapacity) NotImplemented_nid_no_patch(caller);
        char16_t chars[StackBufferCapacity];
        std::memcpy(chars, raw + StackBufferOffset, static_cast<std::size_t>(length) * sizeof(char16_t));
        result.setTo(chars, length);
        return result;
    }
    const char16_t* array = nullptr;
    std::memcpy(&array, raw + ArrayOffset, sizeof(array));
    char16_t chunk[ChunkSize];
    for (std::int32_t offset = 0; offset < length; offset += ChunkSize) {
        const std::int32_t count = length - offset < ChunkSize ? length - offset : ChunkSize;
        if (!CopyIn(chunk, array + offset, static_cast<std::size_t>(count) * sizeof(char16_t))) NotImplemented_nid_no_patch(caller);
        result.append(chunk, count);
    }
    return result;
}

}

extern "C" {

std::int32_t APS5_VABI uhash_hashUnicodeString_nid_postfix(Icu::Element key) {
    if (!key.pointer) return 0;
    return ReadGuest(key.pointer, __func__).hashCode();
}

std::int32_t APS5_VABI uhash_hashCaselessUnicodeString_nid_postfix(Icu::Element key) {
    if (!key.pointer) return 0;
    return ReadGuest(key.pointer, __func__).foldCase().hashCode();
}

std::int8_t APS5_VABI uhash_compareUnicodeString_nid_postfix(Icu::Element left, Icu::Element right) {
    if (left.pointer == right.pointer) return 1;
    if (!left.pointer || !right.pointer) return 0;
    return ReadGuest(left.pointer, __func__) == ReadGuest(right.pointer, __func__);
}

std::int8_t APS5_VABI uhash_compareCaselessUnicodeString_nid_postfix(Icu::Element left, Icu::Element right) {
    if (left.pointer == right.pointer) return 1;
    if (!left.pointer || !right.pointer) return 0;
    return ReadGuest(left.pointer, __func__).caseCompare(ReadGuest(right.pointer, __func__), U_FOLD_CASE_DEFAULT) == 0;
}

}
