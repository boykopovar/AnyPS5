#include "prx/libc/include/ApplicationHeap.hpp"
#include "prx/libc/include/Api.hpp"
#include "prx/libc/include/GuestLocale.hpp"
#include "prx/libc/include/GuestTime.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>

extern "C" {
extern std::uint64_t _ZNSt8__sce_v25ctypeItE2idE_nid_postfix;
extern std::uint64_t _ZNSt8__sce_v27codecvtItc9_MbstatetE2idE_nid_postfix;
}

void* APS5_VABI TestHeapAllocate(std::size_t bytes) { return std::malloc(bytes); }
void APS5_VABI TestHeapFree(void* pointer) { std::free(pointer); }
void* APS5_VABI TestHeapCalloc(std::size_t count, std::size_t bytes) { return std::calloc(count, bytes); }
void* APS5_VABI TestHeapReallocate(void* pointer, std::size_t bytes) { return std::realloc(pointer, bytes); }
void* APS5_VABI TestHeapAlign(std::size_t, std::size_t bytes) { return std::malloc(bytes); }
void* APS5_VABI TestHeapRealign(void* pointer, std::size_t bytes, std::size_t) { return std::realloc(pointer, bytes); }
int APS5_VABI TestHeapPosixAlign(void** pointer, std::size_t, std::size_t bytes) {
    *pointer = std::malloc(bytes);
    return *pointer != nullptr ? 0 : -1;
}

enum UshortFacet { Ctype, NumGet, NumPut, Numpunct, Collate, Messages, MoneyGet, MoneyPut, Moneypunct, MoneypunctIntl, TimeGet, TimePut, Codecvt };

static unsigned failures = 0;

static void Require(bool value, const std::string& what) {
    if (value) return;
    std::fprintf(stderr, "FAIL: %s\n", what.c_str());
    ++failures;
}

template<typename T> static T Slot(const void* facet, int index) {
    return reinterpret_cast<T>((*static_cast<void* const* const*>(facet))[index]);
}

struct UshortString {
    union {
        std::uint16_t buffer[8];
        std::uint16_t* pointer;
    };
    std::uint64_t size;
    std::uint64_t capacity;
};

static std::u16string Text(const UshortString& value) {
    const std::uint16_t* data = value.capacity > 7 ? value.pointer : value.buffer;
    return std::u16string(data, data + value.size);
}

struct InputIterator {
    GuestLocale::Streambuf* buffer;
    bool got;
    std::uint16_t value;
};

struct InputVtable {
    const void* reserved[7];
    int (APS5_VABI *underflow)(GuestLocale::Streambuf* self);
    int (APS5_VABI *uflow)(GuestLocale::Streambuf* self);
};

struct Source {
    GuestLocale::Streambuf buffer{};
    InputVtable vtable{};
    std::array<std::uint16_t, 64> storage{};
    std::uint16_t* next = nullptr;
    std::int32_t count = 0;
};

static int APS5_VABI EmptyRead(GuestLocale::Streambuf*) { return -1; }

static InputIterator Load(Source& source, const std::u16string& text) {
    source.vtable.underflow = EmptyRead;
    source.vtable.uflow = EmptyRead;
    source.buffer.vtable = reinterpret_cast<const GuestLocale::StreambufVtable*>(&source.vtable);
    std::memcpy(source.storage.data(), text.data(), text.size() * sizeof(char16_t));
    source.next = source.storage.data();
    source.count = static_cast<std::int32_t>(text.size());
    source.buffer.reserved[6] = &source.next;
    source.buffer.reservedCounts[1] = &source.count;
    return {&source.buffer, false, 0};
}

struct Sink {
    GuestLocale::Streambuf buffer{};
    GuestLocale::StreambufVtable vtable{};
    std::array<std::uint16_t, 64> storage{};
    std::uint16_t* next = nullptr;
    std::uint16_t* end = nullptr;
    std::int32_t count = 0;
};

static GuestLocale::OutputIterator Arm(Sink& sink) {
    sink.buffer.vtable = &sink.vtable;
    sink.next = sink.storage.data();
    sink.end = sink.storage.data() + sink.storage.size();
    sink.count = static_cast<std::int32_t>(sink.storage.size());
    sink.buffer.putNext = reinterpret_cast<char**>(&sink.next);
    sink.buffer.putEnd = reinterpret_cast<char**>(&sink.end);
    sink.buffer.reservedCounts[2] = &sink.count;
    return {false, &sink.buffer};
}

static std::u16string Written(const Sink& sink) {
    return std::u16string(reinterpret_cast<const char16_t*>(sink.storage.data()), reinterpret_cast<const char16_t*>(sink.next));
}

int main() {
    std::array<void*, 10> heapApi{};
    heapApi[0] = reinterpret_cast<void*>(&TestHeapAllocate);
    heapApi[1] = reinterpret_cast<void*>(&TestHeapFree);
    heapApi[2] = reinterpret_cast<void*>(&TestHeapCalloc);
    heapApi[3] = reinterpret_cast<void*>(&TestHeapReallocate);
    heapApi[4] = reinterpret_cast<void*>(&TestHeapAlign);
    heapApi[5] = reinterpret_cast<void*>(&TestHeapRealign);
    heapApi[6] = reinterpret_cast<void*>(&TestHeapPosixAlign);
    ApplicationHeapRegister_nid_no_patch(heapApi.data());

    static const GuestLocale::SceV2Locinfo info{0, "C", "C"};
    static auto* locale = _ZNSt8__sce_v26locale7_Locimp11_New_LocimpEb_nid_postfix(false);
    _ZNSt8__sce_v26locale7_Locimp8_MakelocERKNS_8_LocinfoEiPS1_PKS0__nid_postfix(&info, 0x3f, locale, nullptr);
    const std::uint64_t base = _ZNSt8__sce_v25ctypeItE2idE_nid_postfix;
    Require(_ZNSt8__sce_v27codecvtItc9_MbstatetE2idE_nid_postfix == base + Codecvt, "the unsigned short facets follow _Makeushloc's order");
    if (failures != 0) std::_Exit(1);
    const auto facet = [](UshortFacet which) { return locale->facets[_ZNSt8__sce_v25ctypeItE2idE_nid_postfix + which]; };
    for (int which = Ctype; which <= Codecvt; ++which) Require(facet(static_cast<UshortFacet>(which)) != nullptr, "_Makeushloc installs facet " + std::to_string(which));
    if (failures != 0) std::_Exit(1);

    GuestLocale::SceV2IosBase stream{};
    stream.locale = &locale;

    Require(Slot<std::uint16_t (APS5_VABI *)(const void*, char)>(facet(Ctype), 12)(facet(Ctype), 'A') == u'A', "ctype<unsigned short>::widen");
    Require(Slot<bool (APS5_VABI *)(const void*, short, std::uint16_t)>(facet(Ctype), 4)(facet(Ctype), 0x20, u'7'), "ctype<unsigned short>::is digit");

    UshortString name{};
    Slot<UshortString* (APS5_VABI *)(UshortString*, const void*)>(facet(Numpunct), 8)(&name, facet(Numpunct));
    Require(Text(name) == u"true", "numpunct<unsigned short>::truename");

    Sink sink;
    auto out = Slot<GuestLocale::OutputIterator (APS5_VABI *)(const void*, GuestLocale::OutputIterator, GuestLocale::SceV2IosBase*, std::uint16_t, std::int64_t)>(facet(NumPut), 5)(facet(NumPut), Arm(sink), &stream, u' ', -42);
    Require(!out.failed && Written(sink) == u"-42", "num_put<unsigned short> long");

    Source source;
    std::int64_t number = 0;
    std::int32_t state = 0;
    Slot<InputIterator (APS5_VABI *)(const void*, InputIterator, InputIterator, GuestLocale::SceV2IosBase*, std::int32_t*, std::int64_t*)>(facet(NumGet), 7)(facet(NumGet), Load(source, u"123"), {nullptr, false, 0}, &stream, &state, &number);
    Require(number == 123 && state == 1, "num_get<unsigned short> long");

    const std::u16string left = u"abc";
    const std::u16string right = u"abd";
    Require(Slot<int (APS5_VABI *)(const void*, const char16_t*, const char16_t*, const char16_t*, const char16_t*)>(facet(Collate), 4)(facet(Collate), left.data(), left.data() + 3, right.data(), right.data() + 3) == -1, "collate<unsigned short>::compare");
    const std::u16string shorter = u"ab";
    Require(Slot<int (APS5_VABI *)(const void*, const char16_t*, const char16_t*, const char16_t*, const char16_t*)>(facet(Collate), 4)(facet(Collate), shorter.data(), shorter.data() + 2, left.data(), left.data() + 3) == -1, "collate<unsigned short>::compare puts a prefix first");

    Require(Slot<int (APS5_VABI *)(const void*, const void*, const void*)>(facet(Messages), 4)(facet(Messages), nullptr, nullptr) == -1, "messages<unsigned short>::open");

    UshortString digits{};
    digits.buffer[0] = u'7';
    digits.buffer[1] = u'5';
    digits.size = 2;
    digits.capacity = 7;
    out = Slot<GuestLocale::OutputIterator (APS5_VABI *)(const void*, GuestLocale::OutputIterator, bool, GuestLocale::SceV2IosBase*, std::uint16_t, const UshortString*)>(facet(MoneyPut), 5)(facet(MoneyPut), Arm(sink), false, &stream, u' ', &digits);
    Require(!out.failed && !Written(sink).empty(), "money_put<unsigned short> digits");

    UshortString got{};
    state = 0;
    Slot<InputIterator (APS5_VABI *)(const void*, InputIterator, InputIterator, std::int32_t, GuestLocale::SceV2IosBase*, std::int32_t*, UshortString*)>(facet(MoneyGet), 5)(facet(MoneyGet), Load(source, u"75"), {nullptr, false, 0}, 0, &stream, &state, &got);
    Require((state & 2) == 0 && !Text(got).empty(), "money_get<unsigned short> digits");

    for (const UshortFacet which : {Moneypunct, MoneypunctIntl}) {
        UshortString symbol{};
        Require(Slot<UshortString* (APS5_VABI *)(UshortString*, const void*)>(facet(which), 7)(&symbol, facet(which)) == &symbol && symbol.capacity >= 7, "moneypunct<unsigned short>::curr_symbol");
    }

    GuestTm time{};
    state = 0;
    Slot<InputIterator (APS5_VABI *)(const void*, InputIterator, InputIterator, GuestLocale::SceV2IosBase*, std::int32_t*, GuestTm*)>(facet(TimeGet), 5)(facet(TimeGet), Load(source, u"10:20:30"), {nullptr, false, 0}, &stream, &state, &time);
    Require(time.tm_hour == 10 && time.tm_min == 20 && time.tm_sec == 30 && state == 1, "time_get<unsigned short>::get_time");
    state = 0;
    Slot<InputIterator (APS5_VABI *)(const void*, InputIterator, InputIterator, GuestLocale::SceV2IosBase*, std::int32_t*, GuestTm*)>(facet(TimeGet), 8)(facet(TimeGet), Load(source, u"march"), {nullptr, false, 0}, &stream, &state, &time);
    Require(time.tm_mon == 2 && state == 0, "time_get<unsigned short>::get_monthname ignores case");

    time.tm_year = 124;
    out = Slot<GuestLocale::OutputIterator (APS5_VABI *)(const void*, GuestLocale::OutputIterator, GuestLocale::SceV2IosBase*, std::uint16_t, const GuestTm*, char, char)>(facet(TimePut), 4)(facet(TimePut), Arm(sink), &stream, u' ', &time, 'Y', 0);
    Require(!out.failed && Written(sink) == u"2024", "time_put<unsigned short> %Y");

    Require(Slot<bool (APS5_VABI *)(const void*)>(facet(Codecvt), 4)(facet(Codecvt)) == false, "codecvt<unsigned short>::always_noconv");

    if (failures != 0) std::fprintf(stderr, "%u failures\n", failures);
    std::fflush(stderr);
    std::_Exit(failures == 0 ? 0 : 1);
}
