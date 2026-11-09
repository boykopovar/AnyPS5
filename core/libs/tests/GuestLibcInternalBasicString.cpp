#include "prx/libSceLibcInternal/include/Api.hpp"
#include "prx/libc/include/Api.hpp"
#include "prx/libc/include/ApplicationHeap.hpp"
#include "prx/libc/include/GuestLocale.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <functional>
#include <limits>
#include <string>

struct TypeRecord { const void* vtable; const char* name; const TypeRecord* base; };

using Thrower = void (APS5_VABI *)(const void*);
using GuestWhat = const char* (APS5_VABI *)(const void*);

extern "C" {
extern const TypeRecord _ZTISt12out_of_range_nid_postfix;
extern const TypeRecord _ZTISt12length_error_nid_postfix;

void APS5_VABI SceV2Catch(Thrower thrower, const void* argument);

const void* SceV2CatchType = nullptr;
const void* SceV2OtherType = nullptr;
int SceV2Selector = 0;
}

namespace {

char caught[128];
long live = 0;

void* APS5_VABI TestAllocate(std::size_t size) {
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
}

namespace {

using GuestLocale::String;

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

void RequireThrow(const std::function<void()>& call, int selector, const char* expected, const char* message) {
    SceV2CatchType = &_ZTISt12out_of_range_nid_postfix;
    SceV2OtherType = &_ZTISt12length_error_nid_postfix;
    SceV2Selector = 0;
    caught[0] = 0;
    SceV2Catch(Invoke, &call);
    Require(SceV2Selector == selector && std::strcmp(caught, expected) == 0, message);
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
    CheckAssignCharacters();
    CheckAssignPointer();
    CheckAssignSubstring();
    CheckAppendSubstring();
    CheckAppendCharacters();
    CheckErase();
    Require(live == before, "every string allocation is released");
    std::puts("libc internal basic_string: ok");
    return 0;
}
