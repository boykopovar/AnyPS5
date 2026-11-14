#include "prx/libSceLibcInternal/include/Api.hpp"
#include "prx/libc/include/Api.hpp"
#include "prx/libc/include/ApplicationHeap.hpp"
#include "prx/libc/include/GuestLocale.hpp"
#include "prx/libc/GuestStringStorage.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <exception>
#include <functional>
#include <limits>
#include <new>
#include <stdexcept>
#include <string>

struct TypeRecord { const void* vtable; const char* name; const TypeRecord* base; };

using Thrower = void (APS5_VABI *)(const void*);
using GuestWhat = const char* (APS5_VABI *)(const void*);

extern "C" {
extern const TypeRecord _ZTISt12out_of_range_nid_postfix;
extern const TypeRecord _ZTISt12length_error_nid_postfix;
extern const TypeRecord _ZTISt12system_error_nid_postfix;
extern const TypeRecord _ZTINSt8ios_base7failureE_nid_postfix;

void APS5_VABI SceV2Catch(Thrower thrower, const void* argument);

const void* SceV2CatchType = nullptr;
const void* SceV2OtherType = nullptr;
int SceV2Selector = 0;
}

namespace {

char caught[128];
std::size_t inspectBytes = 0;
LibcException::SystemErrorObject inspectedSystem{};
long live = 0;
bool failNextAllocation = false;
std::size_t lastAllocationSize = 0;

void* APS5_VABI TestAllocate(std::size_t size) {
    lastAllocationSize = size;
    if (failNextAllocation) {
        failNextAllocation = false;
        throw std::bad_alloc();
    }
    ++live;
    return std::malloc(size);
}
void APS5_VABI TestFree(void* pointer) {
    if (pointer != nullptr) --live;
    std::free(pointer);
}
void* APS5_VABI TestCalloc(std::size_t count, std::size_t size) {
    ++live;
    return std::calloc(count, size);
}
void* APS5_VABI TestReallocate(void* pointer, std::size_t size) {
    if (pointer == nullptr) ++live;
    return std::realloc(pointer, size);
}
void* APS5_VABI TestAlign(std::size_t, std::size_t size) {
    ++live;
    return std::malloc(size);
}
void* APS5_VABI TestRealign(void* pointer, std::size_t size, std::size_t) {
    if (pointer == nullptr) ++live;
    return std::realloc(pointer, size);
}
int APS5_VABI TestPosixAlign(void** pointer, std::size_t, std::size_t size) {
    ++live;
    *pointer = std::malloc(size);
    return *pointer ? 0 : 12;
}

}

extern "C" void APS5_VABI SceV2Inspect(void* object) {
    const auto* vtable = *static_cast<const void* const* const*>(object);
    const auto what = reinterpret_cast<GuestWhat>(const_cast<void*>(vtable[2]));
    std::snprintf(caught, sizeof(caught), "%s|%s", static_cast<const TypeRecord*>(vtable[-1])->name, what(object));
    std::memcpy(&inspectedSystem, object, inspectBytes);
}

namespace {

using GuestLocale::String;

static_assert(offsetof(String, reserved) == 0);
static_assert(offsetof(String, buffer) == 8);
static_assert(offsetof(String, size) == 24);
static_assert(offsetof(String, capacity) == 32);
static_assert(sizeof(String) == 40);

constexpr std::size_t NoPosition = std::numeric_limits<std::size_t>::max();
constexpr std::uint64_t Allocator = 0x5a5a5a5a5a5a5a5a;

void Require(bool condition, const char* message) {
    if (condition) return;
    std::fprintf(stderr, "libc internal basic_string: %s\n", message);
    std::abort();
}

String Empty() {
    String string{};
    string.reserved = Allocator;
    string.capacity = GuestLocale::StringInlineCapacity;
    return string;
}

const char* Data(const String& string) {
    return string.capacity > GuestLocale::StringInlineCapacity ? string.pointer : string.buffer;
}

bool Holds(const String& string, const char* text) {
    const std::size_t length = std::strlen(text);
    return string.reserved == Allocator && string.size == length && std::memcmp(Data(string), text, length) == 0 && Data(string)[length] == 0;
}

String Make(const char* text) {
    String string = Empty();
    _ZNSs6assignEPKcm_nid_postfix(&string, text, std::strlen(text));
    return string;
}

void APS5_VABI Invoke(const void* call) {
    (*static_cast<const std::function<void()>*>(call))();
}

void RequireThrowAs(const std::function<void()>& call, const void* catchType, const void* otherType, int selector, const char* expected, const char* message) {
    SceV2CatchType = catchType;
    SceV2OtherType = otherType;
    SceV2Selector = 0;
    caught[0] = 0;
    SceV2Catch(Invoke, &call);
    Require(SceV2Selector == selector && std::strcmp(caught, expected) == 0, message);
}

void RequireThrow(const std::function<void()>& call, int selector, const char* expected, const char* message) {
    RequireThrowAs(call, &_ZTISt12out_of_range_nid_postfix, &_ZTISt12length_error_nid_postfix, selector, expected, message);
}

void RequireOutOfRange(const std::function<void()>& call, const char* message) {
    RequireThrow(call, 2, "St12out_of_range|invalid string position", message);
}

void RequireLengthError(const std::function<void()>& call, const char* message) {
    RequireThrow(call, 1, "St12length_error|string too long", message);
}

void CheckReporters() {
    String string = Empty();
    RequireOutOfRange([&] { _ZNKSs5_XranEv_nid_postfix(&string); }, "_Xran throws out_of_range");
    RequireLengthError([&] { _ZNKSs5_XlenEv_nid_postfix(&string); }, "_Xlen throws length_error");
    RequireOutOfRange([&] { _ZNKSbIwSt11char_traitsIwESaIwEE5_XranEv_nid_postfix(&string); }, "wide _Xran throws out_of_range");
    RequireLengthError([&] { _ZNKSbIwSt11char_traitsIwESaIwEE5_XlenEv_nid_postfix(&string); }, "wide _Xlen throws length_error");
}

void CheckIosFailureDoraise() {
    LibcException::SystemErrorObject failure{};
    const LibcException::ErrorCode code{3, 5, _ZSt16generic_categoryv_nid_postfix()};
    _ZNSt12system_errorC2ESt10error_codePKc_nid_postfix(&failure, code, "stream");
    const std::string expected = std::string("NSt8ios_base7failureE|") + _ZNKSt12system_error4whatEv_nid_postfix(&failure.base);
    const auto raise = [&] { _ZNKSt8ios_base7failure8_DoraiseEv_nid_postfix(&failure); };
    inspectBytes = sizeof(LibcException::SystemErrorObject);
    RequireThrowAs(raise, &_ZTINSt8ios_base7failureE_nid_postfix, &_ZTISt12out_of_range_nid_postfix, 2, expected.c_str(),
        "ios_base::failure::_Doraise throws ios_base::failure with the same message");
    Require(inspectedSystem.base.message == failure.base.message, "ios_base::failure::_Doraise shares the message");
    Require(inspectedSystem.code.reserved == 3 && inspectedSystem.code.value == 5 && inspectedSystem.code.category == code.category,
        "ios_base::failure::_Doraise copies the error code");
    RequireThrowAs(raise, &_ZTISt12system_error_nid_postfix, &_ZTISt12out_of_range_nid_postfix, 2, expected.c_str(),
        "ios_base::failure::_Doraise is caught as system_error");
    inspectBytes = 0;
    bool rejected = false;
    try {
        _ZNKSt8ios_base7failure8_DoraiseEv_nid_postfix(nullptr);
    } catch (const std::invalid_argument&) {
        rejected = true;
    }
    Require(rejected, "ios_base::failure::_Doraise rejects a null object");
    _ZNSt12system_errorD2Ev_nid_postfix(&failure.base);
}

void CheckAssignCharacters() {
    String string = Empty();
    Require(_ZNSs6assignEmc_nid_postfix(&string, 3, 'x') == &string, "assign(n, c) returns *this");
    Require(Holds(string, "xxx") && string.capacity == 15, "assign(n, c) stays in the inline buffer");
    _ZNSs6assignEmc_nid_postfix(&string, 20, 'y');
    Require(Holds(string, std::string(20, 'y').c_str()) && string.capacity == 31, "assign(n, c) grows to size | 15");
    _ZNSs6assignEmc_nid_postfix(&string, 0, 'z');
    Require(Holds(string, "") && string.capacity == 31, "assign(0, c) keeps the storage");
    RequireLengthError([&] { _ZNSs6assignEmc_nid_postfix(&string, NoPosition, 'a'); }, "assign(npos, c) throws length_error");
    Require(Holds(string, ""), "a failed assign leaves the string unchanged");
    GuestLocale::ReleaseStringStorage(string);
}

void CheckAssignPointer() {
    String string = Empty();
    const char text[] = "hello\0world";
    Require(_ZNSs6assignEPKcm_nid_postfix(&string, text, 11) == &string, "assign(s, n) returns *this");
    Require(string.size == 11 && std::memcmp(string.buffer, text, 12) == 0, "assign(s, n) copies embedded nulls");
    _ZNSs6assignEPKcm_nid_postfix(&string, "abcdefghijklmnopqrstuvwxyz", 26);
    Require(Holds(string, "abcdefghijklmnopqrstuvwxyz") && string.capacity == 31, "assign(s, n) moves to the heap");
    char* storage = string.pointer;
    _ZNSs6assignEPKcm_nid_postfix(&string, string.pointer + 3, 4);
    Require(Holds(string, "defg") && string.pointer == storage && string.capacity == 31, "assign(s, n) from inside *this reuses the storage");
    _ZNSs6assignEPKcm_nid_postfix(&string, string.pointer, 2);
    Require(Holds(string, "de"), "assign(s, n) from the start of *this truncates");
    GuestLocale::ReleaseStringStorage(string);

    String terminator = Make("abc");
    _ZNSs6assignEPKcm_nid_postfix(&terminator, Data(terminator) + 1, 3);
    Require(terminator.size == 3 && std::memcmp(Data(terminator), "bc\0", 3) == 0 && Data(terminator)[3] == 0,
        "assign(s, n) preserves the initialized terminator in an overlapping range");
    GuestLocale::ReleaseStringStorage(terminator);

    String inlineGrowth = Make("abcdefghijklmno");
    _ZNSs6assignEPKcm_nid_postfix(&inlineGrowth, Data(inlineGrowth), 16);
    Require(inlineGrowth.size == 16 && inlineGrowth.capacity == 31
        && std::memcmp(Data(inlineGrowth), "abcdefghijklmno\0", 16) == 0 && Data(inlineGrowth)[16] == 0,
        "assign(s, n) copies an overlapping inline range before growing");
    GuestLocale::ReleaseStringStorage(inlineGrowth);

    String heapOverlap = Make("abcdefghijklmnopqrs");
    heapOverlap.pointer[20] = 'x';
    _ZNSs6assignEPKcm_nid_postfix(&heapOverlap, heapOverlap.pointer + 1, 20);
    Require(heapOverlap.size == 20 && std::memcmp(Data(heapOverlap), "bcdefghijklmnopqrs\0x", 20) == 0
        && Data(heapOverlap)[20] == 0,
        "assign(s, n) moves an overlapping heap range beyond the logical terminator");
    GuestLocale::ReleaseStringStorage(heapOverlap);
}

void CheckAssignSubstring() {
    String source = Make("0123456789abcdefghij");
    String string = Make("old");
    Require(_ZNSs6assignERKSsmm_nid_postfix(&string, &source, 4, 3) == &string, "assign(str, pos, n) returns *this");
    Require(Holds(string, "456"), "assign(str, pos, n) copies the substring");
    _ZNSs6assignERKSsmm_nid_postfix(&string, &source, 2, NoPosition);
    Require(Holds(string, "23456789abcdefghij") && string.capacity == 31, "assign(str, pos, npos) copies to the end");
    _ZNSs6assignERKSsmm_nid_postfix(&string, &source, 20, 5);
    Require(Holds(string, ""), "assign(str, size(), n) is empty");
    RequireOutOfRange([&] { _ZNSs6assignERKSsmm_nid_postfix(&string, &source, 21, 1); }, "assign(str, pos > size(), n) throws out_of_range");
    _ZNSs6assignERKSsmm_nid_postfix(&source, &source, 5, 6);
    Require(Holds(source, "56789a") && source.capacity == 31, "assign(*this, pos, n) keeps the storage");
    _ZNSs6assignERKSsmm_nid_postfix(&source, &source, 0, NoPosition);
    Require(Holds(source, "56789a"), "assign(*this, 0, npos) is unchanged");
    GuestLocale::ReleaseStringStorage(source);
    GuestLocale::ReleaseStringStorage(string);
}

void CheckAppendSubstring() {
    String source = Make("ABCDEFGH");
    String string = Make("ab");
    Require(_ZNSs6appendERKSsmm_nid_postfix(&string, &source, 1, 3) == &string, "append(str, pos, n) returns *this");
    Require(Holds(string, "abBCD") && string.capacity == 15, "append(str, pos, n) appends the substring");
    _ZNSs6appendERKSsmm_nid_postfix(&string, &source, 6, NoPosition);
    Require(Holds(string, "abBCDGH"), "append(str, pos, npos) appends to the end");
    RequireOutOfRange([&] { _ZNSs6appendERKSsmm_nid_postfix(&string, &source, 9, 0); }, "append(str, pos > size(), n) throws out_of_range");
    _ZNSs6appendERKSsmm_nid_postfix(&string, &string, 0, NoPosition);
    Require(Holds(string, "abBCDGHabBCDGH") && string.capacity == 15, "append(*this) within the inline buffer");
    _ZNSs6appendERKSsmm_nid_postfix(&string, &string, 2, 5);
    Require(Holds(string, "abBCDGHabBCDGHBCDGH") && string.capacity == 31, "append(*this) across a reallocation");
    GuestLocale::ReleaseStringStorage(source);
    GuestLocale::ReleaseStringStorage(string);
}

void CheckAppendCharacters() {
    String string = Make("go");
    Require(_ZNSs6appendEmc_nid_postfix(&string, 3, 'o') == &string, "append(n, c) returns *this");
    Require(Holds(string, "goooo"), "append(n, c) appends the characters");
    _ZNSs6appendEmc_nid_postfix(&string, 0, 'x');
    Require(Holds(string, "goooo"), "append(0, c) is unchanged");
    RequireLengthError([&] { _ZNSs6appendEmc_nid_postfix(&string, NoPosition - 5, 'x'); }, "append past max_size() throws length_error");
    _ZNSs6appendEmc_nid_postfix(&string, 5000, '!');
    Require(string.size == 5005 && string.capacity == (5005 | 15) && string.reserved == Allocator, "append(n, c) grows a large allocation");
    Require((reinterpret_cast<std::uintptr_t>(string.pointer) & 0x1f) == 0, "large storage is 32-byte aligned");
    Require(std::memcmp(string.pointer, "goooo!", 6) == 0 && string.pointer[5004] == '!' && string.pointer[5005] == 0, "append(n, c) keeps the prefix and terminates");
    GuestLocale::ReleaseStringStorage(string);
}

void CheckErase() {
    String string = Make("0123456789abcdefghij");
    Require(_ZNSs5eraseEmm_nid_postfix(&string, 2, 3) == &string, "erase returns *this");
    Require(Holds(string, "0156789abcdefghij") && string.capacity == 31, "erase(pos, n) removes the range");
    _ZNSs5eraseEmm_nid_postfix(&string, 10, NoPosition);
    Require(Holds(string, "0156789abc"), "erase(pos, npos) truncates");
    _ZNSs5eraseEmm_nid_postfix(&string, 10, 4);
    Require(Holds(string, "0156789abc"), "erase(size(), n) is unchanged");
    RequireOutOfRange([&] { _ZNSs5eraseEmm_nid_postfix(&string, 11, 0); }, "erase(pos > size(), n) throws out_of_range");
    _ZNSs5eraseEmm_nid_postfix(&string, 0, NoPosition);
    Require(Holds(string, "") && string.capacity == 31, "erase(0, npos) keeps the storage");
    GuestLocale::ReleaseStringStorage(string);
    String small = Make("abc");
    _ZNSs5eraseEmm_nid_postfix(&small, 0, 1);
    Require(Holds(small, "bc") && small.capacity == 15, "erase works on the inline buffer");
}

char* WideBytes(String* string) {
    return string->capacity > 7 ? string->pointer : string->buffer;
}

const char* WideBytes(const String* string) {
    return string->capacity > 7 ? string->pointer : string->buffer;
}

String MakeWide(const std::uint16_t* text, std::size_t size) {
    String string{};
    string.reserved = Allocator;
    string.size = size;
    string.capacity = 7;
    std::memcpy(string.buffer, text, size * sizeof(std::uint16_t));
    const std::uint16_t terminator = 0;
    std::memcpy(string.buffer + size * sizeof(terminator), &terminator, sizeof(terminator));
    return string;
}

String MakeWideHeap(const std::uint16_t* text, std::size_t size, std::size_t capacity) {
    Require(capacity > 7 && size <= capacity, "wide heap setup has a valid capacity");
    String string{};
    string.reserved = Allocator;
    string.size = size;
    string.capacity = capacity;
    string.pointer = static_cast<char*>(GuestStringStorage::Allocate((capacity + 1) * sizeof(std::uint16_t)));
    std::memcpy(string.pointer, text, size * sizeof(std::uint16_t));
    const std::uint16_t terminator = 0;
    std::memcpy(string.pointer + size * sizeof(terminator), &terminator, sizeof(terminator));
    return string;
}

void ReleaseWide(String& string) {
    if (string.capacity > 7) {
        GuestStringStorage::Deallocate(string.pointer, (string.capacity + 1) * sizeof(std::uint16_t));
        string.pointer = nullptr;
        string.capacity = 7;
        string.size = 0;
    }
}

bool WideHolds(const String& string, const std::uint16_t* expected, std::size_t size) {
    if (string.reserved != Allocator || string.size != size || string.capacity < size || (string.capacity > 7 && string.pointer == nullptr)) return false;
    const std::uint16_t terminator = 0;
    std::uint16_t actualTerminator = 1;
    std::memcpy(&actualTerminator, WideBytes(&string) + size * sizeof(terminator), sizeof(actualTerminator));
    return std::memcmp(WideBytes(&string), expected, size * sizeof(std::uint16_t)) == 0 && actualTerminator == 0;
}

void CheckWideModifiersAndNarrowInsert() {
    struct CaseResult {
        const char* label;
        bool threwExpectedStub;
        bool stateMatches;
        bool returnMatches;
        char diagnostic[128];
    };
    std::array<CaseResult, 8> results{};
    std::size_t resultIndex = 0;
    auto run = [&](const char* label, const char* stubName, String& string, const std::function<bool()>& stateMatches, const std::function<String*()>& call) {
        CaseResult& result = results[resultIndex++];
        result.label = label;
        String* returned = nullptr;
        try {
            returned = call();
        } catch (const std::exception& error) {
            std::snprintf(result.diagnostic, sizeof(result.diagnostic), "%s", error.what());
            const std::string expectedDiagnostic = std::string(stubName) + " not implemented";
            result.threwExpectedStub = expectedDiagnostic == error.what();
        } catch (...) {
            std::snprintf(result.diagnostic, sizeof(result.diagnostic), "%s", "non-standard exception");
        }
        result.stateMatches = stateMatches();
        result.returnMatches = returned == &string;
    };

    const std::uint16_t abcdef[]{'a', 'b', 'c', 'd', 'e', 'f'};
    const std::uint16_t abef[]{'a', 'b', 'e', 'f'};
    String erase = MakeWide(abcdef, 6);
    run("wide erase", "_ZNSbIwSt11char_traitsIwESaIwEE5eraseEmm_nid_postfix", erase, [&] { return WideHolds(erase, abef, 4); },
        [&] { return _ZNSbIwSt11char_traitsIwESaIwEE5eraseEmm_nid_postfix(&erase, 2, 2); });

    const std::uint16_t ab[]{'a', 'b'};
    const std::uint16_t wxyz[]{'W', 'X', 'Y', 'Z'};
    const std::uint16_t abxy[]{'a', 'b', 'X', 'Y'};
    String appendSubstring = MakeWide(ab, 2);
    String appendSource = MakeWide(wxyz, 4);
    run("wide append substring", "_ZNSbIwSt11char_traitsIwESaIwEE6appendERKS2_mm_nid_postfix", appendSubstring, [&] { return WideHolds(appendSubstring, abxy, 4); },
        [&] { return _ZNSbIwSt11char_traitsIwESaIwEE6appendERKS2_mm_nid_postfix(&appendSubstring, &appendSource, 1, 2); });

    const std::uint16_t abqqq[]{'a', 'b', 'Q', 'Q', 'Q'};
    String appendFill = MakeWide(ab, 2);
    run("wide append fill", "_ZNSbIwSt11char_traitsIwESaIwEE6appendEmw_nid_postfix", appendFill, [&] { return WideHolds(appendFill, abqqq, 5); },
        [&] { return _ZNSbIwSt11char_traitsIwESaIwEE6appendEmw_nid_postfix(&appendFill, 3, 'Q'); });

    const std::uint16_t pointerExpected[]{'p', 'q', 0, 'r'};
    const std::uint16_t pointerInput[]{'p', 'q', 0, 'r'};
    String assignPointer = MakeWide(ab, 2);
    run("wide assign pointer", "_ZNSbIwSt11char_traitsIwESaIwEE6assignEPKwm_nid_postfix", assignPointer, [&] { return WideHolds(assignPointer, pointerExpected, 4); },
        [&] { return _ZNSbIwSt11char_traitsIwESaIwEE6assignEPKwm_nid_postfix(&assignPointer, pointerInput, 4); });

    const std::uint16_t sourceText[]{'0', '1', '2', '3', '4', '5'};
    const std::uint16_t substringExpected[]{'1', '2', '3'};
    String assignSubstring = MakeWide(ab, 2);
    String assignSource = MakeWide(sourceText, 6);
    run("wide assign substring", "_ZNSbIwSt11char_traitsIwESaIwEE6assignERKS2_mm_nid_postfix", assignSubstring, [&] { return WideHolds(assignSubstring, substringExpected, 3); },
        [&] { return _ZNSbIwSt11char_traitsIwESaIwEE6assignERKS2_mm_nid_postfix(&assignSubstring, &assignSource, 1, 3); });

    const std::uint16_t fillExpected[]{'F', 'F', 'F', 'F'};
    String assignFill = MakeWide(ab, 2);
    run("wide assign fill", "sceLibcInternalPending4_101_nid_no_patch", assignFill, [&] { return WideHolds(assignFill, fillExpected, 4); },
        [&] { return _ZNSbIwSt11char_traitsIwESaIwEE6assignEmw_nid_postfix(&assignFill, 4, 'F'); });

    const std::uint16_t insertExpected[]{'a', 'w', 'w', 'b'};
    String insertFill = MakeWide(ab, 2);
    run("wide insert fill", "sceLibcInternalPending4_102_nid_no_patch", insertFill, [&] { return WideHolds(insertFill, insertExpected, 4); },
        [&] { return _ZNSbIwSt11char_traitsIwESaIwEE6insertEmmw_nid_postfix(&insertFill, 1, 2, 'w'); });

    String narrowInsert = Make("ab");
    run("narrow insert fill", "sceLibcInternalPending4_116_nid_no_patch", narrowInsert, [&] { return Holds(narrowInsert, "axxb"); },
        [&] { return _ZNSs6insertEmmc_nid_postfix(&narrowInsert, 1, 2, 'x'); });

    bool failed = false;
    for (const CaseResult& result : results) {
        if (result.threwExpectedStub) {
            std::fprintf(stderr, "libc internal basic_string RED: %s: %s\n", result.label, result.diagnostic);
            failed = true;
        } else if (result.diagnostic[0] != 0) {
            std::fprintf(stderr, "libc internal basic_string RED: %s: unexpected exception %s\n", result.label, result.diagnostic);
            failed = true;
        }
        if (!result.stateMatches) {
            std::fprintf(stderr, "libc internal basic_string: %s mutation mismatch\n", result.label);
            failed = true;
        }
        if (!result.returnMatches) {
            std::fprintf(stderr, "libc internal basic_string: %s return mismatch\n", result.label);
            failed = true;
        }
    }
    GuestLocale::ReleaseStringStorage(narrowInsert);
    Require(WideHolds(appendSource, wxyz, 4) && WideHolds(assignSource, sourceText, 6), "source strings remain unchanged");
    if (failed) std::exit(1);
}

void CheckNarrowInsertGrowth() {
    const std::string original(63, 'a');
    std::string expected = original;
    expected.insert(31, 1, 'x');
    String string = Make(original.c_str());
    Require(string.capacity == 63, "narrow insert growth starts with capacity 63");
    Require(_ZNSs6insertEmmc_nid_postfix(&string, 31, 1, 'x') == &string, "narrow insert growth returns *this");
    Require(Holds(string, expected.c_str()) && string.capacity == 94,
        "narrow insert uses the unrounded one-and-a-half-capacity branch");
    GuestLocale::ReleaseStringStorage(string);
}

void CheckWideCapacityAndAliases() {
    const std::uint16_t inlineText[]{'a', 'b', 'c', 'd', 'e', 'f', 'g'};
    const std::uint16_t inlineExpected[]{'a', 'b', 'c', 'd', 'e', 'f', 'g', 'h'};
    String inlineGrowth = MakeWide(inlineText, 7);
    Require(_ZNSbIwSt11char_traitsIwESaIwEE6appendEmw_nid_postfix(&inlineGrowth, 1, 'h') == &inlineGrowth,
        "inline wide append returns *this");
    Require(WideHolds(inlineGrowth, inlineExpected, 8) && inlineGrowth.capacity == 15,
        "wide inline capacity grows from seven units");
    ReleaseWide(inlineGrowth);

    std::array<std::uint16_t, 31> growthInput{};
    for (std::size_t index = 0; index < growthInput.size(); ++index) growthInput[index] = static_cast<std::uint16_t>('a' + index % 26);
    std::array<std::uint16_t, 32> growthExpected{};
    std::copy(growthInput.begin(), growthInput.end(), growthExpected.begin());
    growthExpected.back() = 'z';
    String repeatedGrowth = MakeWideHeap(growthInput.data(), growthInput.size(), 31);
    Require(_ZNSbIwSt11char_traitsIwESaIwEE6appendEmw_nid_postfix(&repeatedGrowth, 1, 'z') == &repeatedGrowth,
        "wide repeated append returns *this");
    Require(WideHolds(repeatedGrowth, growthExpected.data(), growthExpected.size()) && repeatedGrowth.capacity == 46,
        "wide repeated growth follows the native one-and-a-half-capacity branch");
    ReleaseWide(repeatedGrowth);

    const std::uint16_t six[]{'a', 'b', 'c', 'd', 'e', 'f'};
    const std::uint16_t twelve[]{'a', 'b', 'c', 'd', 'e', 'f', 'a', 'b', 'c', 'd', 'e', 'f'};
    String inlineSelfAppend = MakeWide(six, 6);
    Require(_ZNSbIwSt11char_traitsIwESaIwEE6appendERKS2_mm_nid_postfix(&inlineSelfAppend, &inlineSelfAppend, 0, 6) == &inlineSelfAppend,
        "wide inline self append returns *this");
    Require(WideHolds(inlineSelfAppend, twelve, 12) && inlineSelfAppend.capacity == 15,
        "wide self append survives inline-to-heap growth");
    ReleaseWide(inlineSelfAppend);

    std::array<std::uint16_t, 62> doubled{};
    std::copy(growthInput.begin(), growthInput.end(), doubled.begin());
    std::copy(growthInput.begin(), growthInput.end(), doubled.begin() + 31);
    String heapSelfAppend = MakeWideHeap(growthInput.data(), growthInput.size(), 31);
    Require(_ZNSbIwSt11char_traitsIwESaIwEE6appendERKS2_mm_nid_postfix(&heapSelfAppend, &heapSelfAppend, 0, 31) == &heapSelfAppend,
        "wide heap self append returns *this");
    Require(WideHolds(heapSelfAppend, doubled.data(), doubled.size()) && heapSelfAppend.capacity >= 62,
        "wide self append survives heap-to-heap growth");
    ReleaseWide(heapSelfAppend);

    const std::uint16_t ten[]{'a', 'b', 'c', 'd', 'e', 'f', 'g', 'h', 'i', 'j'};
    const std::uint16_t suffix[]{'c', 'd', 'e', 'f', 'g', 'h', 'i', 'j'};
    String alias = MakeWideHeap(ten, 10, 15);
    const auto* aliasSource = reinterpret_cast<const std::uint16_t*>(WideBytes(&alias) + 2 * sizeof(std::uint16_t));
    Require(_ZNSbIwSt11char_traitsIwESaIwEE6assignEPKwm_nid_postfix(&alias, aliasSource, 9) == &alias,
        "wide self-range pointer assignment returns *this");
    Require(WideHolds(alias, suffix, 8), "wide self-range pointer assignment clamps to the logical end");
    ReleaseWide(alias);

    const std::uint16_t selfText[]{'a', 'b', 'c', 'd', 'e', 'f'};
    const std::uint16_t selfSubstring[]{'c', 'd', 'e'};
    String selfAssign = MakeWideHeap(selfText, 6, 15);
    Require(_ZNSbIwSt11char_traitsIwESaIwEE6assignERKS2_mm_nid_postfix(&selfAssign, &selfAssign, 2, 3) == &selfAssign,
        "wide self substring assignment returns *this");
    Require(WideHolds(selfAssign, selfSubstring, 3), "wide self substring assignment retains the selected range");
    ReleaseWide(selfAssign);

    const std::uint16_t abc[]{'a', 'b', 'c'};
    const std::uint16_t zero[]{0};
    String terminator = MakeWideHeap(abc, 3, 15);
    const auto* terminatorSource = reinterpret_cast<const std::uint16_t*>(WideBytes(&terminator) + 3 * sizeof(std::uint16_t));
    Require(_ZNSbIwSt11char_traitsIwESaIwEE6assignEPKwm_nid_postfix(&terminator, terminatorSource, 1) == &terminator,
        "wide terminator pointer assignment returns *this");
    Require(WideHolds(terminator, zero, 1), "wide source at the logical end follows the external-copy path");
    ReleaseWide(terminator);
}

void CheckWideBoundaryAndFailures() {
    std::array<std::uint16_t, 2032> belowBoundary{};
    std::array<std::uint16_t, 2040> atBoundary{};
    for (std::size_t index = 0; index < belowBoundary.size(); ++index) belowBoundary[index] = static_cast<std::uint16_t>('a' + index % 26);
    for (std::size_t index = 0; index < atBoundary.size(); ++index) atBoundary[index] = static_cast<std::uint16_t>('A' + index % 26);

    const std::uint16_t empty[]{0};
    String below = MakeWide(empty, 0);
    lastAllocationSize = 0;
    _ZNSbIwSt11char_traitsIwESaIwEE6assignEPKwm_nid_postfix(&below, belowBoundary.data(), belowBoundary.size());
    Require(below.capacity == 2039 && lastAllocationSize == (below.capacity + 1) * 2,
        "wide storage below the large-allocation threshold uses the direct allocation size");
    Require(WideHolds(below, belowBoundary.data(), belowBoundary.size()), "wide below-boundary assignment copies all units");
    ReleaseWide(below);

    String at = MakeWide(empty, 0);
    lastAllocationSize = 0;
    _ZNSbIwSt11char_traitsIwESaIwEE6assignEPKwm_nid_postfix(&at, atBoundary.data(), atBoundary.size());
    Require(at.capacity == 2047 && lastAllocationSize == (at.capacity + 1) * 2 + 0x27,
        "wide 0x800-unit storage uses the padded large allocation");
    Require((reinterpret_cast<std::uintptr_t>(at.pointer) & 0x1f) == 0,
        "wide large allocation is 32-byte aligned");
    Require(WideHolds(at, atBoundary.data(), atBoundary.size()), "wide boundary assignment copies all units");
    ReleaseWide(at);

    const std::uint16_t shortText[]{'a', 'b', 'c'};
    String range = MakeWide(shortText, 3);
    RequireOutOfRange([&] { _ZNSbIwSt11char_traitsIwESaIwEE5eraseEmm_nid_postfix(&range, 4, 0); },
        "wide erase past size throws out_of_range");
    Require(WideHolds(range, shortText, 3), "wide range failure preserves the object");

    String tooLong = MakeWide(shortText, 3);
    RequireLengthError([&] {
        _ZNSbIwSt11char_traitsIwESaIwEE6appendEmw_nid_postfix(&tooLong, std::numeric_limits<std::size_t>::max() / 2, 'x');
    }, "wide append beyond max_size throws length_error");
    Require(WideHolds(tooLong, shortText, 3), "wide length failure preserves the object");

    String failedGrowth = MakeWideHeap(belowBoundary.data(), 10, 15);
    char* originalPointer = failedGrowth.pointer;
    const auto originalSize = failedGrowth.size;
    const auto originalCapacity = failedGrowth.capacity;
    const long originalLive = live;
    lastAllocationSize = 0;
    failNextAllocation = true;
    bool caughtBadAlloc = false;
    try {
        _ZNSbIwSt11char_traitsIwESaIwEE6appendEmw_nid_postfix(&failedGrowth, 10, 'z');
    } catch (const std::bad_alloc&) {
        caughtBadAlloc = true;
    }
    Require(caughtBadAlloc && !failNextAllocation && lastAllocationSize != 0,
        "wide growth reports allocation failure from the application heap");
    Require(failedGrowth.pointer == originalPointer && failedGrowth.size == originalSize && failedGrowth.capacity == originalCapacity
        && live == originalLive && WideHolds(failedGrowth, belowBoundary.data(), 10),
        "wide allocation failure preserves bytes, size, capacity and ownership");
    ReleaseWide(failedGrowth);
}

}

int main() {
    const std::array<void*, 10> api{
        reinterpret_cast<void*>(&TestAllocate), reinterpret_cast<void*>(&TestFree),
        reinterpret_cast<void*>(&TestCalloc), reinterpret_cast<void*>(&TestReallocate),
        reinterpret_cast<void*>(&TestAlign), reinterpret_cast<void*>(&TestRealign),
        reinterpret_cast<void*>(&TestPosixAlign), nullptr, nullptr, nullptr};
    ApplicationHeapRegister_nid_no_patch(api.data());
    const long before = live;
    CheckReporters();
    CheckIosFailureDoraise();
    CheckAssignCharacters();
    CheckAssignPointer();
    CheckAssignSubstring();
    CheckAppendSubstring();
    CheckAppendCharacters();
    CheckErase();
    CheckWideModifiersAndNarrowInsert();
    CheckNarrowInsertGrowth();
    CheckWideCapacityAndAliases();
    CheckWideBoundaryAndFailures();
    Require(live == before, "every string allocation is released");
    std::puts("libc internal basic_string: ok");
    return 0;
}
