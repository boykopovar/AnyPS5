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
#include <string>
#include <vector>

extern "C" {
extern GuestLocale::Implementation* _ZSt21_sceLibcClassicLocale_nid_postfix;

GuestLocale::OutputIterator APS5_VABI _ZNKSt7num_putIcSt19ostreambuf_iteratorIcSt11char_traitsIcEEE6do_putES3_RSt8ios_basecPKv_nid_postfix(const GuestLocale::Facet*, GuestLocale::OutputIterator, GuestLocale::IosBase*, char, const void*);
GuestLocale::OutputIterator APS5_VABI _ZNKSt7num_putIcSt19ostreambuf_iteratorIcSt11char_traitsIcEEE6do_putES3_RSt8ios_basecb_nid_postfix(const GuestLocale::Facet*, GuestLocale::OutputIterator, GuestLocale::IosBase*, char, bool);
GuestLocale::OutputIterator APS5_VABI _ZNKSt7num_putIcSt19ostreambuf_iteratorIcSt11char_traitsIcEEE6do_putES3_RSt8ios_basecd_nid_postfix(const GuestLocale::Facet*, GuestLocale::OutputIterator, GuestLocale::IosBase*, char, double);
GuestLocale::OutputIterator APS5_VABI _ZNKSt7num_putIcSt19ostreambuf_iteratorIcSt11char_traitsIcEEE6do_putES3_RSt8ios_basece_nid_postfix(const GuestLocale::Facet*, GuestLocale::OutputIterator, GuestLocale::IosBase*, char, long double);
GuestLocale::OutputIterator APS5_VABI _ZNKSt7num_putIcSt19ostreambuf_iteratorIcSt11char_traitsIcEEE6do_putES3_RSt8ios_basecl_nid_postfix(const GuestLocale::Facet*, GuestLocale::OutputIterator, GuestLocale::IosBase*, char, std::int64_t);
GuestLocale::OutputIterator APS5_VABI _ZNKSt7num_putIcSt19ostreambuf_iteratorIcSt11char_traitsIcEEE6do_putES3_RSt8ios_basecm_nid_postfix(const GuestLocale::Facet*, GuestLocale::OutputIterator, GuestLocale::IosBase*, char, std::uint64_t);
GuestLocale::OutputIterator APS5_VABI _ZNKSt7num_putIcSt19ostreambuf_iteratorIcSt11char_traitsIcEEE6do_putES3_RSt8ios_basecx_nid_postfix(const GuestLocale::Facet*, GuestLocale::OutputIterator, GuestLocale::IosBase*, char, std::int64_t);
GuestLocale::OutputIterator APS5_VABI _ZNKSt7num_putIcSt19ostreambuf_iteratorIcSt11char_traitsIcEEE6do_putES3_RSt8ios_basecy_nid_postfix(const GuestLocale::Facet*, GuestLocale::OutputIterator, GuestLocale::IosBase*, char, std::uint64_t);

GuestLocale::OutputIterator APS5_VABI _ZNKSt7num_putIwSt19ostreambuf_iteratorIwSt11char_traitsIwEEE3putES3_RSt8ios_basewPKv_nid_postfix(const GuestLocale::Facet*, GuestLocale::OutputIterator, GuestLocale::IosBase*, std::uint16_t, const void*);
GuestLocale::OutputIterator APS5_VABI _ZNKSt7num_putIwSt19ostreambuf_iteratorIwSt11char_traitsIwEEE3putES3_RSt8ios_basewb_nid_postfix(const GuestLocale::Facet*, GuestLocale::OutputIterator, GuestLocale::IosBase*, std::uint16_t, bool);
GuestLocale::OutputIterator APS5_VABI _ZNKSt7num_putIwSt19ostreambuf_iteratorIwSt11char_traitsIwEEE3putES3_RSt8ios_basewd_nid_postfix(const GuestLocale::Facet*, GuestLocale::OutputIterator, GuestLocale::IosBase*, std::uint16_t, double);
GuestLocale::OutputIterator APS5_VABI _ZNKSt7num_putIwSt19ostreambuf_iteratorIwSt11char_traitsIwEEE3putES3_RSt8ios_basewe_nid_postfix(const GuestLocale::Facet*, GuestLocale::OutputIterator, GuestLocale::IosBase*, std::uint16_t, long double);
GuestLocale::OutputIterator APS5_VABI _ZNKSt7num_putIwSt19ostreambuf_iteratorIwSt11char_traitsIwEEE3putES3_RSt8ios_basewl_nid_postfix(const GuestLocale::Facet*, GuestLocale::OutputIterator, GuestLocale::IosBase*, std::uint16_t, std::int64_t);
GuestLocale::OutputIterator APS5_VABI _ZNKSt7num_putIwSt19ostreambuf_iteratorIwSt11char_traitsIwEEE3putES3_RSt8ios_basewm_nid_postfix(const GuestLocale::Facet*, GuestLocale::OutputIterator, GuestLocale::IosBase*, std::uint16_t, std::uint64_t);
GuestLocale::OutputIterator APS5_VABI _ZNKSt7num_putIwSt19ostreambuf_iteratorIwSt11char_traitsIwEEE3putES3_RSt8ios_basewx_nid_postfix(const GuestLocale::Facet*, GuestLocale::OutputIterator, GuestLocale::IosBase*, std::uint16_t, std::int64_t);
GuestLocale::OutputIterator APS5_VABI _ZNKSt7num_putIwSt19ostreambuf_iteratorIwSt11char_traitsIwEEE3putES3_RSt8ios_basewy_nid_postfix(const GuestLocale::Facet*, GuestLocale::OutputIterator, GuestLocale::IosBase*, std::uint16_t, std::uint64_t);

GuestLocale::OutputIterator APS5_VABI _ZNKSt7num_putIwSt19ostreambuf_iteratorIwSt11char_traitsIwEEE6do_putES3_RSt8ios_basewPKv_nid_postfix(const GuestLocale::Facet*, GuestLocale::OutputIterator, GuestLocale::IosBase*, std::uint16_t, const void*);
GuestLocale::OutputIterator APS5_VABI _ZNKSt7num_putIwSt19ostreambuf_iteratorIwSt11char_traitsIwEEE6do_putES3_RSt8ios_basewb_nid_postfix(const GuestLocale::Facet*, GuestLocale::OutputIterator, GuestLocale::IosBase*, std::uint16_t, bool);
GuestLocale::OutputIterator APS5_VABI _ZNKSt7num_putIwSt19ostreambuf_iteratorIwSt11char_traitsIwEEE6do_putES3_RSt8ios_basewd_nid_postfix(const GuestLocale::Facet*, GuestLocale::OutputIterator, GuestLocale::IosBase*, std::uint16_t, double);
GuestLocale::OutputIterator APS5_VABI _ZNKSt7num_putIwSt19ostreambuf_iteratorIwSt11char_traitsIwEEE6do_putES3_RSt8ios_basewe_nid_postfix(const GuestLocale::Facet*, GuestLocale::OutputIterator, GuestLocale::IosBase*, std::uint16_t, long double);
GuestLocale::OutputIterator APS5_VABI _ZNKSt7num_putIwSt19ostreambuf_iteratorIwSt11char_traitsIwEEE6do_putES3_RSt8ios_basewl_nid_postfix(const GuestLocale::Facet*, GuestLocale::OutputIterator, GuestLocale::IosBase*, std::uint16_t, std::int64_t);
GuestLocale::OutputIterator APS5_VABI _ZNKSt7num_putIwSt19ostreambuf_iteratorIwSt11char_traitsIwEEE6do_putES3_RSt8ios_basewm_nid_postfix(const GuestLocale::Facet*, GuestLocale::OutputIterator, GuestLocale::IosBase*, std::uint16_t, std::uint64_t);
GuestLocale::OutputIterator APS5_VABI _ZNKSt7num_putIwSt19ostreambuf_iteratorIwSt11char_traitsIwEEE6do_putES3_RSt8ios_basewx_nid_postfix(const GuestLocale::Facet*, GuestLocale::OutputIterator, GuestLocale::IosBase*, std::uint16_t, std::int64_t);
GuestLocale::OutputIterator APS5_VABI _ZNKSt7num_putIwSt19ostreambuf_iteratorIwSt11char_traitsIwEEE6do_putES3_RSt8ios_basewy_nid_postfix(const GuestLocale::Facet*, GuestLocale::OutputIterator, GuestLocale::IosBase*, std::uint16_t, std::uint64_t);

}

namespace {

constexpr std::int32_t IosLeft = 0x2;
constexpr std::int32_t IosInternal = 0x8;
constexpr std::int32_t IosHex = 0x40;
constexpr std::int32_t IosShowBase = 0x80;
constexpr std::int32_t IosUppercase = 0x200;
constexpr std::int32_t IosBoolAlpha = 0x8000;

void Require(bool condition, const char* message) {
    if (condition) return;
    std::fprintf(stderr, "libc internal num put: %s\n", message);
    std::abort();
}

void* APS5_VABI TestAllocate(std::size_t size) { return std::malloc(size); }
void APS5_VABI TestFree(void* pointer) { std::free(pointer); }
void* APS5_VABI TestCalloc(std::size_t count, std::size_t size) { return std::calloc(count, size); }
void* APS5_VABI TestReallocate(void* pointer, std::size_t size) { return std::realloc(pointer, size); }
void* APS5_VABI TestAlign(std::size_t, std::size_t size) { return std::malloc(size); }
void* APS5_VABI TestRealign(void* pointer, std::size_t size, std::size_t) { return std::realloc(pointer, size); }
int APS5_VABI TestPosixAlign(void** pointer, std::size_t, std::size_t size) {
    *pointer = std::malloc(size);
    return *pointer ? 0 : 12;
}

struct NarrowStream {
    GuestLocale::Streambuf buffer{};
    char area[64]{};
    char* next = area;
    char* end = area + sizeof(area);
    std::string overflowed;
    bool overflowFails = false;
};

int APS5_VABI NarrowOverflow(GuestLocale::Streambuf* self, int character) {
    auto* stream = reinterpret_cast<NarrowStream*>(self);
    if (stream->overflowFails) return -1;
    stream->overflowed += static_cast<char>(character);
    return character;
}

const GuestLocale::StreambufVtable g_narrowVtable{{}, NarrowOverflow};

struct WideStream {
    GuestLocale::Streambuf buffer{};
    char area[128]{};
    char* next = area;
    char* end = area + sizeof(area);
    std::vector<std::uint16_t> overflowed;
    bool overflowFails = false;
};

int APS5_VABI WideOverflow(GuestLocale::Streambuf* self, int character) {
    auto* stream = reinterpret_cast<WideStream*>(self);
    if (stream->overflowFails) return -1;
    stream->overflowed.push_back(static_cast<std::uint16_t>(character));
    return character;
}

const GuestLocale::StreambufVtable g_wideVtable{{}, WideOverflow};

void MakeNarrow(GuestLocale::IosBase& stream, NarrowStream& result) {
    stream.locale = &_ZSt21_sceLibcClassicLocale_nid_postfix;
    result = NarrowStream{};
    result.buffer.vtable = &g_narrowVtable;
    result.next = result.area;
    result.end = result.area + sizeof(result.area);
    result.buffer.putNext = &result.next;
    result.buffer.putEnd = &result.end;
}

void MakeWide(GuestLocale::IosBase& stream, WideStream& result) {
    stream.locale = &_ZSt21_sceLibcClassicLocale_nid_postfix;
    result = WideStream{};
    result.buffer.vtable = &g_wideVtable;
    result.next = result.area;
    result.end = result.area + sizeof(result.area);
    result.buffer.putNext = &result.next;
    result.buffer.putEnd = &result.end;
}

std::string Taken(NarrowStream& stream) {
    return std::string(stream.area, static_cast<std::size_t>(stream.next - stream.area)) + stream.overflowed;
}

std::u16string Taken(WideStream& stream) {
    std::u16string result;
    for (char* cursor = stream.area; cursor != stream.next; cursor += 2) {
        std::uint16_t value = 0;
        std::memcpy(&value, cursor, 2);
        result.push_back(static_cast<char16_t>(value));
    }
    for (std::uint16_t value : stream.overflowed) result.push_back(static_cast<char16_t>(value));
    return result;
}

std::string Narrow(GuestLocale::IosBase& ios, std::int64_t value) {
    NarrowStream stream;
    MakeNarrow(ios, stream);
    const auto out = _ZNKSt7num_putIcSt19ostreambuf_iteratorIcSt11char_traitsIcEEE6do_putES3_RSt8ios_basecl_nid_postfix(nullptr, GuestLocale::OutputIterator{false, &stream.buffer}, &ios, '*', value);
    Require(!out.failed && out.buffer == &stream.buffer, "the narrow long put succeeds");
    Require(ios.width == 0, "the narrow long put resets the width");
    return Taken(stream);
}

std::u16string Wide(GuestLocale::IosBase& ios, std::int64_t value) {
    WideStream stream;
    MakeWide(ios, stream);
    const auto out = _ZNKSt7num_putIwSt19ostreambuf_iteratorIwSt11char_traitsIwEEE3putES3_RSt8ios_basewl_nid_postfix(nullptr, GuestLocale::OutputIterator{false, &stream.buffer}, &ios, '*', value);
    Require(!out.failed && out.buffer == &stream.buffer, "the wide long put succeeds");
    Require(ios.width == 0, "the wide long put resets the width");
    return Taken(stream);
}

void CheckIntegers() {
    GuestLocale::IosBase ios{};
    Require(Narrow(ios, -42) == "-42", "a long prints in decimal");
    Require(Narrow(ios, 0) == "0", "zero prints as one digit");
    Require(Narrow(ios, -9223372036854775807LL) == "-9223372036854775807", "the smallest long prints every digit");

    ios.width = 6;
    Require(Narrow(ios, -42) == "***-42", "the width pads the narrow field");
    ios.width = 6;
    ios.flags = IosLeft;
    Require(Narrow(ios, -42) == "-42***", "the left adjust pads after the value");
    ios.flags = 0;

    ios.width = 8;
    ios.flags = IosHex | IosShowBase | IosUppercase | IosInternal;
    Require(Narrow(ios, 255) == "0X****FF", "the flags pick the base, case and pad position");
    ios.flags = 0;

    WideStream wide;
    MakeWide(ios, wide);
    const auto wideOut = _ZNKSt7num_putIwSt19ostreambuf_iteratorIwSt11char_traitsIwEEE3putES3_RSt8ios_basewm_nid_postfix(nullptr, GuestLocale::OutputIterator{false, &wide.buffer}, &ios, '*', std::uint64_t(18446744073709551615ull));
    Require(!wideOut.failed, "the wide unsigned long put succeeds");
    Require(Taken(wide) == u"18446744073709551615", "the largest unsigned long prints every digit");

    NarrowStream narrow;
    MakeNarrow(ios, narrow);
    const auto narrowOut = _ZNKSt7num_putIcSt19ostreambuf_iteratorIcSt11char_traitsIcEEE6do_putES3_RSt8ios_basecy_nid_postfix(nullptr, GuestLocale::OutputIterator{false, &narrow.buffer}, &ios, '*', std::uint64_t(18446744073709551615ull));
    Require(!narrowOut.failed, "the narrow unsigned long long put succeeds");
    Require(Taken(narrow) == "18446744073709551615", "the narrow and wide digits agree");
}

void CheckFloating() {
    GuestLocale::IosBase ios{};
    ios.precision = 6;
    NarrowStream narrow;
    MakeNarrow(ios, narrow);
    Require(!_ZNKSt7num_putIcSt19ostreambuf_iteratorIcSt11char_traitsIcEEE6do_putES3_RSt8ios_basecd_nid_postfix(nullptr, GuestLocale::OutputIterator{false, &narrow.buffer}, &ios, '*', 0.125).failed, "the narrow double put succeeds");
    Require(Taken(narrow) == "0.125", "a double prints its exact value");

    MakeNarrow(ios, narrow);
    Require(!_ZNKSt7num_putIcSt19ostreambuf_iteratorIcSt11char_traitsIcEEE6do_putES3_RSt8ios_basece_nid_postfix(nullptr, GuestLocale::OutputIterator{false, &narrow.buffer}, &ios, '*', 0.5L).failed, "the narrow long double put succeeds");
    Require(Taken(narrow) == "0.5", "a long double prints its exact value");

    WideStream wide;
    MakeWide(ios, wide);
    Require(!_ZNKSt7num_putIwSt19ostreambuf_iteratorIwSt11char_traitsIwEEE6do_putES3_RSt8ios_basewd_nid_postfix(nullptr, GuestLocale::OutputIterator{false, &wide.buffer}, &ios, '*', 1.5).failed, "the wide double put succeeds");
    Require(Taken(wide) == u"1.5", "a wide double prints its exact value");

    MakeWide(ios, wide);
    Require(!_ZNKSt7num_putIwSt19ostreambuf_iteratorIwSt11char_traitsIwEEE3putES3_RSt8ios_basewe_nid_postfix(nullptr, GuestLocale::OutputIterator{false, &wide.buffer}, &ios, '*', 2.5L).failed, "the wide long double put succeeds");
    Require(Taken(wide) == u"2.5", "a wide long double prints its exact value");
}

void CheckBool() {
    GuestLocale::IosBase ios{};
    NarrowStream narrow;
    MakeNarrow(ios, narrow);
    Require(!_ZNKSt7num_putIcSt19ostreambuf_iteratorIcSt11char_traitsIcEEE6do_putES3_RSt8ios_basecb_nid_postfix(nullptr, GuestLocale::OutputIterator{false, &narrow.buffer}, &ios, '*', false).failed, "the narrow bool put succeeds");
    Require(Taken(narrow) == "0", "a bool prints as a number without boolalpha");

    ios.flags = IosBoolAlpha;
    MakeNarrow(ios, narrow);
    Require(!_ZNKSt7num_putIcSt19ostreambuf_iteratorIcSt11char_traitsIcEEE6do_putES3_RSt8ios_basecb_nid_postfix(nullptr, GuestLocale::OutputIterator{false, &narrow.buffer}, &ios, '*', false).failed, "the narrow boolalpha put succeeds");
    Require(Taken(narrow) == "false", "boolalpha prints the false name");

    WideStream wide;
    MakeWide(ios, wide);
    Require(!_ZNKSt7num_putIwSt19ostreambuf_iteratorIwSt11char_traitsIwEEE3putES3_RSt8ios_basewb_nid_postfix(nullptr, GuestLocale::OutputIterator{false, &wide.buffer}, &ios, '*', true).failed, "the wide boolalpha put succeeds");
    Require(Taken(wide) == u"true", "the wide boolalpha prints the true name");

    ios.flags = 0;
    MakeWide(ios, wide);
    Require(!_ZNKSt7num_putIwSt19ostreambuf_iteratorIwSt11char_traitsIwEEE6do_putES3_RSt8ios_basewb_nid_postfix(nullptr, GuestLocale::OutputIterator{false, &wide.buffer}, &ios, '*', true).failed, "the wide bool put succeeds");
    Require(Taken(wide) == u"1", "a wide bool prints as a number without boolalpha");
}

void CheckPointer() {
    GuestLocale::IosBase ios{};
    NarrowStream narrow;
    MakeNarrow(ios, narrow);
    int object = 0;
    Require(!_ZNKSt7num_putIcSt19ostreambuf_iteratorIcSt11char_traitsIcEEE6do_putES3_RSt8ios_basecPKv_nid_postfix(nullptr, GuestLocale::OutputIterator{false, &narrow.buffer}, &ios, '*', &object).failed, "the narrow pointer put succeeds");
    const std::string narrowText = Taken(narrow);
    Require(!narrowText.empty() && narrowText.find_first_not_of("0123456789abcdefABCDEF") == std::string::npos, "a pointer prints as hexadecimal digits");

    WideStream wide;
    MakeWide(ios, wide);
    Require(!_ZNKSt7num_putIwSt19ostreambuf_iteratorIwSt11char_traitsIwEEE3putES3_RSt8ios_basewPKv_nid_postfix(nullptr, GuestLocale::OutputIterator{false, &wide.buffer}, &ios, '*', &object).failed, "the wide pointer put succeeds");
    const std::u16string wideText = Taken(wide);
    Require(wideText.size() == narrowText.size(), "the wide pointer has the same length");
    Require(std::u16string(narrowText.begin(), narrowText.end()) == wideText, "the wide pointer agrees with the narrow one");
}

void CheckFailure() {
    GuestLocale::IosBase ios{};
    NarrowStream narrow;
    MakeNarrow(ios, narrow);
    narrow.overflowFails = true;
    narrow.end = narrow.next;
    const auto out = _ZNKSt7num_putIcSt19ostreambuf_iteratorIcSt11char_traitsIcEEE6do_putES3_RSt8ios_basecl_nid_postfix(nullptr, GuestLocale::OutputIterator{false, &narrow.buffer}, &ios, '*', -42);
    Require(out.failed && out.buffer == &narrow.buffer, "a full buffer that refuses the overflow fails the narrow iterator");

    WideStream wide;
    MakeWide(ios, wide);
    wide.overflowFails = true;
    wide.end = wide.next;
    const auto wideOut = _ZNKSt7num_putIwSt19ostreambuf_iteratorIwSt11char_traitsIwEEE3putES3_RSt8ios_basewl_nid_postfix(nullptr, GuestLocale::OutputIterator{false, &wide.buffer}, &ios, '*', -42);
    Require(wideOut.failed && wideOut.buffer == &wide.buffer, "a full buffer that refuses the overflow fails the wide iterator");
}

}

int main() {
    const std::array<void*, 10> api{
        reinterpret_cast<void*>(&TestAllocate), reinterpret_cast<void*>(&TestFree),
        reinterpret_cast<void*>(&TestCalloc), reinterpret_cast<void*>(&TestReallocate),
        reinterpret_cast<void*>(&TestAlign), reinterpret_cast<void*>(&TestRealign),
        reinterpret_cast<void*>(&TestPosixAlign), nullptr, nullptr, nullptr};
    ApplicationHeapRegister_nid_no_patch(api.data());

    CheckIntegers();
    CheckFloating();
    CheckBool();
    CheckPointer();
    CheckFailure();
    return 0;
}
