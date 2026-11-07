// © 2016 and later: Unicode, Inc. and others.
// License & terms of use: http://www.unicode.org/copyright.html
#include "prx/libSceIcu/libSceIcu.hpp"
#include <array>
#include <bit>
#include <limits>
#include <stdexcept>

namespace {
using namespace Icu::Abi;
using GetBuffer = char* (APS5_VABI *)(ByteSink*, std::int32_t, std::int32_t, char*, std::int32_t, std::int32_t*);
template<typename TValue> TValue& Require(TValue* value) {
    if (!value) throw std::invalid_argument("ICU byte sink: null pointer");
    return *value;
}
struct SinkTable {
    std::ptrdiff_t offset;
    const TypeInfo* type;
    std::array<GuestFunction, 5> slots;
};
static_assert(sizeof(SinkTable) == 56 && alignof(SinkTable) == 8);
static_assert(offsetof(SinkTable, slots) == 16);
const GuestFunction* Table() {
    static const DerivedTypeInfo type{{SingleTypeTable_nid_no_patch(), "N6icu_6818CharStringByteSinkE"}, Rtti_nid_no_patch(Class::ByteSink)};
    static const SinkTable table{0, &type.type, {{
        reinterpret_cast<GuestFunction>(_ZN6icu_6818CharStringByteSinkD1Ev_nid_postfix),
        reinterpret_cast<GuestFunction>(_ZN6icu_6818CharStringByteSinkD0Ev_nid_postfix),
        reinterpret_cast<GuestFunction>(_ZN6icu_6818CharStringByteSink6AppendEPKci_nid_postfix),
        reinterpret_cast<GuestFunction>(_ZN6icu_6818CharStringByteSink15GetAppendBufferEiiPciPi_nid_postfix),
        reinterpret_cast<GuestFunction>(_ZN6icu_688ByteSink5FlushEv_nid_postfix),
    }}};
    return table.slots.data();
}
void Append(ByteSink* sink, const char* source, std::int32_t length) {
    Virtual_nid_no_patch<void (APS5_VABI *)(ByteSink*, const char*, std::int32_t)>(sink, 2)(sink, source, length);
}
std::int32_t Encode(char* output, std::uint32_t point) {
    if (point <= 0x7f) { output[0] = static_cast<char>(point); return 1; }
    if (point <= 0x7ff) {
        output[0] = static_cast<char>((point >> 6) | 0xc0);
        output[1] = static_cast<char>((point & 0x3f) | 0x80);
        return 2;
    }
    if (point <= 0xffff) {
        output[0] = static_cast<char>((point >> 12) | 0xe0);
        output[1] = static_cast<char>(((point >> 6) & 0x3f) | 0x80);
        output[2] = static_cast<char>((point & 0x3f) | 0x80);
        return 3;
    }
    output[0] = static_cast<char>((point >> 18) | 0xf0);
    output[1] = static_cast<char>(((point >> 12) & 0x3f) | 0x80);
    output[2] = static_cast<char>(((point >> 6) & 0x3f) | 0x80);
    output[3] = static_cast<char>((point & 0x3f) | 0x80);
    return 4;
}
std::int64_t Difference(const std::uint8_t* source, const std::uint8_t* limit) {
    Require(source);
    Require(limit);
    const auto first = reinterpret_cast<std::uintptr_t>(source);
    const auto last = reinterpret_cast<std::uintptr_t>(limit);
    const auto distance = first <= last ? last - first : first - last;
    if (distance > INT64_MAX) throw std::invalid_argument("ICU byte sink: pointer difference overflow");
    return first <= last ? static_cast<std::int64_t>(distance) : -static_cast<std::int64_t>(distance);
}
}

extern "C" {
void APS5_VABI _ZN6icu_6818CharStringByteSinkC1EPNS_10CharStringE_nid_postfix(Icu::Abi::CharStringByteSink* self, Icu::Abi::CharString* destination) {
    Require(destination);
    Require(self) = {Table(), destination};
}

void APS5_VABI _ZN6icu_6818CharStringByteSinkC2EPNS_10CharStringE_nid_postfix(Icu::Abi::CharStringByteSink* self, Icu::Abi::CharString* destination) {
    Require(destination);
    Require(self) = {Table(), destination};
}

void APS5_VABI _ZN6icu_6818CharStringByteSinkD1Ev_nid_postfix(Icu::Abi::CharStringByteSink* self) {
    Require(self);
    _ZN6icu_688ByteSinkD2Ev_nid_postfix(self);
}

void APS5_VABI _ZN6icu_6818CharStringByteSinkD2Ev_nid_postfix(Icu::Abi::CharStringByteSink* self) {
    Require(self);
    _ZN6icu_688ByteSinkD2Ev_nid_postfix(self);
}

void APS5_VABI _ZN6icu_6818CharStringByteSinkD0Ev_nid_postfix(Icu::Abi::CharStringByteSink* self) {
    Require(self);
    _ZN6icu_688ByteSinkD2Ev_nid_postfix(self);
    Free_nid_no_patch(self);
}

void APS5_VABI _ZN6icu_6818CharStringByteSink6AppendEPKci_nid_postfix(Icu::Abi::CharStringByteSink* self, const char* bytes, std::int32_t length) {
    std::int32_t status = 0;
    _ZN6icu_6810CharString6appendEPKciR10UErrorCode_nid_postfix(Require(self).destination, bytes, length, &status);
}

char* APS5_VABI _ZN6icu_6818CharStringByteSink15GetAppendBufferEiiPciPi_nid_postfix(Icu::Abi::CharStringByteSink* self, std::int32_t minimum, std::int32_t desired, char* scratch, std::int32_t scratchCapacity, std::int32_t* capacity) {
    Require(capacity);
    if (minimum < 1 || scratchCapacity < minimum) { *capacity = 0; return nullptr; }
    std::int32_t status = 0;
    auto* result = _ZN6icu_6810CharString15getAppendBufferEiiRiR10UErrorCode_nid_postfix(Require(self).destination, minimum, desired, capacity, &status);
    if (status <= 0) return result;
    *capacity = scratchCapacity;
    return scratch;
}

void APS5_VABI _ZN6icu_6812ByteSinkUtil14appendTwoBytesEiRNS_8ByteSinkE_nid_postfix(std::int32_t point, Icu::Abi::ByteSink* sink) {
    const char bytes[]{static_cast<char>((point >> 6) | 0xc0), static_cast<char>((point & 0x3f) | 0x80)};
    Append(sink, bytes, 2);
}

void APS5_VABI _ZN6icu_6812ByteSinkUtil15appendCodePointEiiRNS_8ByteSinkEPNS_5EditsE_nid_postfix(std::int32_t length, std::int32_t point, Icu::Abi::ByteSink* sink, Icu::Abi::Edits* edits) {
    char bytes[4]{};
    const auto count = Encode(bytes, static_cast<std::uint32_t>(point));
    if (edits) _ZN6icu_685Edits10addReplaceEii_nid_postfix(edits, length, count);
    Append(sink, bytes, count);
}

void APS5_VABI _ZN6icu_6812ByteSinkUtil23appendNonEmptyUnchangedEPKhiRNS_8ByteSinkEjPNS_5EditsE_nid_postfix(const std::uint8_t* source, std::int32_t length, Icu::Abi::ByteSink* sink, std::uint32_t options, Icu::Abi::Edits* edits) {
    if (edits) _ZN6icu_685Edits12addUnchangedEi_nid_postfix(edits, length);
    if (!(options & 0x4000)) Append(sink, reinterpret_cast<const char*>(source), length);
}

std::int8_t APS5_VABI _ZN6icu_6812ByteSinkUtil15appendUnchangedEPKhS2_RNS_8ByteSinkEjPNS_5EditsER10UErrorCode_nid_postfix(const std::uint8_t* source, const std::uint8_t* limit, Icu::Abi::ByteSink* sink, std::uint32_t options, Icu::Abi::Edits* edits, std::int32_t* status) {
    if (Require(status) > 0) return 0;
    const auto difference = Difference(source, limit);
    if (difference > INT32_MAX) { *status = 8; return 0; }
    const auto length = std::bit_cast<std::int32_t>(static_cast<std::uint32_t>(difference));
    if (length > 0) _ZN6icu_6812ByteSinkUtil23appendNonEmptyUnchangedEPKhiRNS_8ByteSinkEjPNS_5EditsE_nid_postfix(source, length, sink, options, edits);
    return 1;
}

std::int8_t APS5_VABI _ZN6icu_6812ByteSinkUtil12appendChangeEiPKDsiRNS_8ByteSinkEPNS_5EditsER10UErrorCode_nid_postfix(std::int32_t length, const char16_t* source, std::int32_t sourceLength, Icu::Abi::ByteSink* sink, Icu::Abi::Edits* edits, std::int32_t* status) {
    if (Require(status) > 0) return 0;
    char scratch[200]{};
    std::int32_t total = 0;
    for (std::int32_t i = 0; i < sourceLength;) {
        auto desired = sourceLength - i;
        desired = desired < INT32_MAX / 3 ? desired * 3 : desired < INT32_MAX / 2 ? desired * 2 : INT32_MAX;
        std::int32_t capacity = 0;
        auto getBuffer = Virtual_nid_no_patch<GetBuffer>(sink, 3);
        auto* buffer = getBuffer(sink, 4, desired, scratch, sizeof(scratch), &capacity);
        if (capacity < 4) throw std::invalid_argument("ICU byte sink: insufficient append capacity");
        Require(buffer);
        capacity -= 3;
        std::int32_t count = 0;
        while (i < sourceLength && count < capacity) {
            Require(source);
            std::uint32_t point = source[i];
            i = CheckedInt_nid_no_patch(static_cast<std::int64_t>(i) + 1);
            if (point >= 0xd800 && point <= 0xdbff) {
                point = ((point - 0xd800) << 10) + source[i] - 0xdc00 + 0x10000;
                i = CheckedInt_nid_no_patch(static_cast<std::int64_t>(i) + 1);
            }
            count += Encode(buffer + count, point);
        }
        if (count > INT32_MAX - total) { *status = 8; return 0; }
        Append(sink, buffer, count);
        total += count;
    }
    if (edits) _ZN6icu_685Edits10addReplaceEii_nid_postfix(edits, length, total);
    return 1;
}

std::int8_t APS5_VABI _ZN6icu_6812ByteSinkUtil12appendChangeEPKhS2_PKDsiRNS_8ByteSinkEPNS_5EditsER10UErrorCode_nid_postfix(const std::uint8_t* source, const std::uint8_t* limit, const char16_t* replacement, std::int32_t replacementLength, Icu::Abi::ByteSink* sink, Icu::Abi::Edits* edits, std::int32_t* status) {
    if (Require(status) > 0) return 0;
    const auto difference = Difference(source, limit);
    if (difference > INT32_MAX) { *status = 8; return 0; }
    return _ZN6icu_6812ByteSinkUtil12appendChangeEiPKDsiRNS_8ByteSinkEPNS_5EditsER10UErrorCode_nid_postfix(std::bit_cast<std::int32_t>(static_cast<std::uint32_t>(difference)), replacement, replacementLength, sink, edits, status);
}

}
