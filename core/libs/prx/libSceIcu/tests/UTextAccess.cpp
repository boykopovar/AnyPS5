#include "prx/libSceIcu/libSceIcu.hpp"
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <stdexcept>
#include <string>

namespace {

void Require(bool value, const std::string& message) {
    if (!value) throw std::runtime_error("icu utext: " + message);
}

constexpr std::int32_t ZeroError = 0;
constexpr std::int32_t NoWritePermission = 30;
constexpr std::uint32_t Magic = 0x345ad82c;
constexpr std::size_t StructSize = 144;
constexpr char16_t SentinelUnit = 0xffff;

void RequireNothingExtracted(UText* text, std::int64_t start, std::int64_t limit, const std::string& message) {
    char16_t destination[8];
    for (auto& unit : destination) unit = SentinelUnit;
    std::int32_t status = ZeroError;
    const auto length = utext_extract_nid_postfix(text, start, limit, destination, 8, &status);
    Require(length == 0, message + " returned a length");
    Require(status == ZeroError, message + " reported a status");
    for (std::size_t index = 0; index < 8; ++index) {
        Require(destination[index] == SentinelUnit, message + " wrote to the destination buffer");
    }
}

UText* Prepare(std::uint8_t* storage) {
    std::memset(storage, 0, StructSize);
    std::memcpy(storage, &Magic, sizeof(Magic));
    const auto size = static_cast<std::int32_t>(StructSize);
    std::memcpy(storage + 12, &size, sizeof(size));
    return reinterpret_cast<UText*>(storage);
}

}

int main() {
    const char16_t text[] = {0x0041, 0xd83d, 0xde00, 0x0042, 0};
    alignas(16) std::uint8_t storage[StructSize];
    alignas(16) std::uint8_t cloneStorage[StructSize];
    UText* source = Prepare(storage);

    std::int32_t status = ZeroError;
    UText* handle = utext_openUChars_nid_postfix(source, text, 4, &status);
    Require(status == ZeroError && handle == source, "utext_openUChars did not fill the supplied struct");
    Require(utext_nativeLength_nid_postfix(handle) == 4, "utext_nativeLength returned the wrong length");
    Require(utext_getNativeIndex_nid_postfix(handle) == 0, "the new text is not at index 0");
    Require(utext_isWritable_nid_postfix(handle) == 0, "a UChar string text reports itself writable");
    Require(utext_hasMetaData_nid_postfix(handle) == 0, "a UChar string text reports metadata");
    Require(utext_isLengthExpensive_nid_postfix(handle) == 0, "a known length reports itself expensive");

    Require(utext_next32_nid_postfix(handle) == 0x0041, "utext_next32 did not return A");
    Require(utext_next32_nid_postfix(handle) == 0x1f600, "utext_next32 did not join the surrogate pair");
    Require(utext_getNativeIndex_nid_postfix(handle) == 3, "the index is not past the surrogate pair");
    Require(utext_current32_nid_postfix(handle) == 0x0042, "utext_current32 did not return B");
    Require(utext_previous32_nid_postfix(handle) == 0x1f600, "utext_previous32 did not return the pair");
    Require(utext_getNativeIndex_nid_postfix(handle) == 1, "utext_previous32 left the wrong index");

    utext_setNativeIndex_nid_postfix(handle, 3);
    Require(utext_getNativeIndex_nid_postfix(handle) == 3, "utext_setNativeIndex did not take");
    Require(utext_char32At_nid_postfix(handle, 1) == 0x1f600, "utext_char32At did not decode the pair");
    Require(utext_char32At_nid_postfix(handle, 3) == 0x0042, "utext_char32At did not return B");
    Require(utext_getPreviousNativeIndex_nid_postfix(handle) == 1, "utext_getPreviousNativeIndex is wrong");
    Require(utext_moveIndex32_nid_postfix(handle, -1) == 1, "utext_moveIndex32 could not step back");
    Require(utext_getNativeIndex_nid_postfix(handle) == 1, "utext_moveIndex32 left the wrong index");
    Require(utext_next32From_nid_postfix(handle, 3) == 0x0042, "utext_next32From did not return B");
    Require(utext_previous32From_nid_postfix(handle, 3) == 0x1f600, "utext_previous32From did not return the pair");

    char16_t extracted[8] = {};
    status = ZeroError;
    const auto extractedLength = utext_extract_nid_postfix(handle, 0, 4, extracted, 8, &status);
    Require(status == ZeroError && extractedLength == 4, "utext_extract returned the wrong length");
    Require(extracted[0] == 0x0041 && extracted[1] == 0xd83d && extracted[2] == 0xde00 && extracted[3] == 0x0042 &&
        extracted[4] == 0, "utext_extract returned the wrong characters");

    char16_t tight[2] = {};
    status = ZeroError;
    Require(utext_extract_nid_postfix(handle, 0, 4, tight, 2, &status) == 4, "utext_extract preflight is wrong");
    Require(status != ZeroError, "utext_extract did not report the overflow");

    status = ZeroError;
    UText* copy = utext_clone_nid_postfix(Prepare(cloneStorage), handle, 0, 0, &status);
    Require(status == ZeroError && copy == reinterpret_cast<UText*>(cloneStorage),
        "utext_clone did not fill the supplied struct");
    Require(utext_nativeLength_nid_postfix(copy) == 4, "the clone has the wrong length");
    utext_setNativeIndex_nid_postfix(copy, 3);
    Require(utext_equals_nid_postfix(handle, copy) == 0, "the texts compare equal from different positions");
    utext_setNativeIndex_nid_postfix(handle, 3);
    Require(utext_equals_nid_postfix(handle, copy) == 1, "the texts do not compare equal from the same position");

    status = ZeroError;
    Require(utext_replace_nid_postfix(handle, 0, 1, text, 1, &status) == 0, "utext_replace accepted a write");
    Require(status == NoWritePermission, "utext_replace did not report the read-only text");
    status = ZeroError;
    utext_copy_nid_postfix(handle, 0, 1, 2, 0, &status);
    Require(status == NoWritePermission, "utext_copy did not report the read-only text");

    utext_freeze_nid_postfix(copy);
    Require(utext_isWritable_nid_postfix(copy) == 0, "utext_freeze left the clone writable");
    utext_close(copy);
    utext_close(handle);

    alignas(16) std::uint8_t emptyStorage[StructSize];
    status = ZeroError;
    UText* terminated = utext_openUChars_nid_postfix(Prepare(emptyStorage), nullptr, 0, &status);
    Require(status == ZeroError && terminated != nullptr, "utext_openUChars did not accept the empty string");
    Require(utext_nativeLength_nid_postfix(terminated) == 0, "the empty string has a length");
    Require(utext_next32_nid_postfix(terminated) == -1, "the empty string yielded a character");
    utext_close(terminated);

    // A start at or past the end of a NUL-terminated string has nothing to copy.
    // The provider must report the access unavailable and the extraction must leave
    // the caller's buffer exactly as it found it, not filled with a stale unit.
    const char16_t emptyNulText[] = {0};
    alignas(16) std::uint8_t emptyNulStorage[StructSize];
    status = ZeroError;
    UText* emptyNul = utext_openUChars_nid_postfix(Prepare(emptyNulStorage), emptyNulText, -1, &status);
    Require(status == ZeroError && emptyNul != nullptr, "utext_openUChars did not accept the NUL-terminated empty string");
    RequireNothingExtracted(emptyNul, 1, 4, "a start past the NUL-terminated empty string");
    utext_close(emptyNul);

    const char16_t shortText[] = {0x0061, 0x0062, 0x0063, 0};
    alignas(16) std::uint8_t shortStorage[StructSize];
    status = ZeroError;
    UText* shortHandle = utext_openUChars_nid_postfix(Prepare(shortStorage), shortText, -1, &status);
    Require(status == ZeroError && shortHandle != nullptr, "utext_openUChars did not accept the three-unit string");
    RequireNothingExtracted(shortHandle, 5, 8, "a start past the three-unit string");
    RequireNothingExtracted(shortHandle, 50, 60, "a far start past the three-unit string");
    Require(utext_nativeLength_nid_postfix(shortHandle) == 3, "the three-unit string measured the wrong length");
    utext_close(shortHandle);

    char16_t boundaryText[33] = {};
    for (std::int32_t unit = 0; unit < 32; ++unit) {
        boundaryText[unit] = static_cast<char16_t>(0x0041 + unit);
    }
    alignas(16) std::uint8_t boundaryStorage[StructSize];
    status = ZeroError;
    UText* boundaryHandle = utext_openUChars_nid_postfix(Prepare(boundaryStorage), boundaryText, -1, &status);
    Require(status == ZeroError && boundaryHandle != nullptr, "utext_openUChars did not accept the 32-unit string");
    RequireNothingExtracted(boundaryHandle, 32, 40, "a start at the 32-unit chunk boundary");
    RequireNothingExtracted(boundaryHandle, 33, 40, "a start past the 32-unit chunk boundary");
    Require(utext_nativeLength_nid_postfix(boundaryHandle) == 32, "the 32-unit string measured the wrong length");
    utext_close(boundaryHandle);
    return 0;
}
