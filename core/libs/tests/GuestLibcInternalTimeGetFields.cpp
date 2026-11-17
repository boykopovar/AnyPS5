#include "prx/libSceLibcInternal/include/TimeGetFields.hpp"
#include "prx/libc/include/Api.hpp"
#include "prx/libc/include/ApplicationHeap.hpp"
#include "prx/libc/include/General.hpp"
#include "prx/libc/include/GuestLocale.hpp"
#include "prx/libc/include/GuestTime.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include <array>
#include <cerrno>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#else
#include <sys/mman.h>
#include <unistd.h>
#endif

extern "C" {
extern GuestLocale::Implementation* _ZSt21_sceLibcClassicLocale_nid_postfix;
extern std::uint64_t _ZNSt5ctypeIwE2idE_nid_postfix;
}

namespace {

struct WideTimeGetStorage {
    GuestLocale::Facet base;
    const char16_t* days;
    const char16_t* months;
    const char16_t* amPm;
    std::int32_t dateOrder;
    std::uint32_t reserved;
    std::array<std::byte, 48> untouchedSuffix;
};

struct TimePutCharStorage {
    GuestLocale::Facet base;
    GuestLocale::TimeInfo names;
};

static_assert(sizeof(WideTimeGetStorage) == 0x60);
static_assert(sizeof(TimePutCharStorage) == 0xb8);

struct OutputStreambuf {
    GuestLocale::Streambuf buffer;
    char* next = nullptr;
    char* end = nullptr;
    std::string narrowOut;
    std::u16string wideOut;
};

int APS5_VABI OverflowNarrow(GuestLocale::Streambuf* self, int character) {
    auto* stream = reinterpret_cast<OutputStreambuf*>(self);
    stream->narrowOut.push_back(static_cast<char>(character));
    return character;
}

int APS5_VABI OverflowWide(GuestLocale::Streambuf* self, int character) {
    auto* stream = reinterpret_cast<OutputStreambuf*>(self);
    stream->wideOut.push_back(static_cast<char16_t>(character));
    return character;
}

const GuestLocale::StreambufVtable g_narrowOutVtable{{}, OverflowNarrow};
const GuestLocale::StreambufVtable g_wideOutVtable{{}, OverflowWide};

void* APS5_VABI TestHeapAllocate(std::size_t size) { return std::malloc(size); }
void APS5_VABI TestHeapFree(void* pointer) { std::free(pointer); }
void* APS5_VABI TestHeapCalloc(std::size_t count, std::size_t size) { return std::calloc(count, size); }
void* APS5_VABI TestHeapReallocate(void* pointer, std::size_t size) { return std::realloc(pointer, size); }
void* APS5_VABI TestHeapAlign(std::size_t, std::size_t size) { return std::malloc(size); }
void* APS5_VABI TestHeapRealign(void* pointer, std::size_t size, std::size_t) { return std::realloc(pointer, size); }
int APS5_VABI TestHeapPosixAlign(void** pointer, std::size_t, std::size_t size) {
    *pointer = std::malloc(size);
    return *pointer != nullptr ? 0 : 12;
}

void Require(bool condition, const char* message) {
    if (condition) return;
    std::fprintf(stderr, "libc internal time_get_fields: %s\n", message);
    std::exit(1);
}

template<typename TCall>
bool Throws(TCall&& call) {
    try {
        call();
        return false;
    } catch (...) {
        return true;
    }
}

void CheckGroup1ConstructionAndLifetime() {
    const std::uint64_t before = NotImplementedCalls_nid_no_patch();

    for (const bool complete : {true, false}) {
        WideTimeGetStorage storage{};
        storage.untouchedSuffix.fill(std::byte{0x5a});
        const auto expectedSuffix = storage.untouchedSuffix;
        if (complete) {
            _ZNSt8time_getIwSt19istreambuf_iteratorIwSt11char_traitsIwEEEC1EPKcm_nid_postfix(&storage.base, "C", 2);
        } else {
            _ZNSt8time_getIwSt19istreambuf_iteratorIwSt11char_traitsIwEEEC2EPKcm_nid_postfix(&storage.base, "C", 2);
        }
        Require(storage.base.vtable != nullptr && storage.base.references == 2 && storage.base.reserved == 0, "wide time_get ctor sets facet header");
        Require(storage.days != nullptr && std::u16string_view(storage.days) == u":Sun:Sunday:Mon:Monday:Tue:Tuesday:Wed:Wednesday:Thu:Thursday:Fri:Friday:Sat:Saturday", "wide time_get days table");
        Require(storage.months != nullptr && std::u16string_view(storage.months) == u":Jan:January:Feb:February:Mar:March:Apr:April:May:May:Jun:June:Jul:July:Aug:August:Sep:September:Oct:October:Nov:November:Dec:December", "wide time_get months table");
        Require(storage.amPm != nullptr && std::u16string_view(storage.amPm) == u":AM:am:PM:pm", "wide time_get am/pm table");
        Require(storage.dateOrder == 2 && storage.reserved == 0, "wide time_get dateorder is mdy");
        Require(storage.untouchedSuffix == expectedSuffix, "wide time_get ctor does not touch bytes past 0x30");
        const auto* vtable = reinterpret_cast<const GuestLocale::TimeGetVtable*>(storage.base.vtable);
        Require(vtable->dateOrder(&storage.base) == 2, "wide time_get vtable dateOrder works");
        _ZNSt8time_getIwSt19istreambuf_iteratorIwSt11char_traitsIwEEED2Ev_nid_postfix(&storage.base);
        Require(storage.base.references == 2 && storage.untouchedSuffix == expectedSuffix, "wide time_get D2 preserves fields");
    }

    auto* heapWideGet = static_cast<WideTimeGetStorage*>(_Znwm_nid_postfix(sizeof(WideTimeGetStorage)));
    std::memset(heapWideGet, 0, sizeof(*heapWideGet));
    _ZNSt8time_getIwSt19istreambuf_iteratorIwSt11char_traitsIwEEEC1EPKcm_nid_postfix(&heapWideGet->base, "C", 1);
    heapWideGet->base.vtable->retain(&heapWideGet->base);
    Require(heapWideGet->base.references == 2, "wide time_get retain increments refs");
    Require(heapWideGet->base.vtable->release(&heapWideGet->base) == nullptr && heapWideGet->base.references == 1, "first release returns null");
    auto* toDelete = heapWideGet->base.vtable->release(&heapWideGet->base);
    Require(toDelete == &heapWideGet->base, "last release returns facet pointer");
    toDelete->vtable->deleteObject(toDelete);

    GuestLocale::Facet charFacet{};
    _ZNSt8time_getIcSt19istreambuf_iteratorIcSt11char_traitsIcEEED2Ev_nid_postfix(&charFacet);

    GuestLocale::LocinfoStorage info{};
    _ZNSt8_LocinfoC1EPKc_nid_postfix(&info, "C");
    const GuestLocale::TimeInfo* expectedTimes = _Getptimes_nid_postfix();

    TimePutCharStorage putFromLocinfo{};
    _ZNSt8time_putIcSt19ostreambuf_iteratorIcSt11char_traitsIcEEEC1ERKSt8_Locinfom_nid_postfix(&putFromLocinfo.base, &info, 3);
    Require(putFromLocinfo.base.vtable != nullptr && putFromLocinfo.base.references == 3, "time_put<char> C1 locinfo ctor");
    Require(std::memcmp(&putFromLocinfo.names, expectedTimes, sizeof(*expectedTimes)) == 0, "time_put<char> C1 populates TimeInfo");

    TimePutCharStorage putDefault{};
    _ZNSt8time_putIcSt19ostreambuf_iteratorIcSt11char_traitsIcEEEC2Em_nid_postfix(&putDefault.base, 1);
    Require(putDefault.base.vtable != nullptr && putDefault.base.references == 1, "time_put<char> C2 default ctor");
    Require(std::memcmp(&putDefault.names, expectedTimes, sizeof(*expectedTimes)) == 0, "time_put<char> C2 populates TimeInfo");

    GuestTm tm{};
    tm.tm_year = 126;
    tm.tm_mon = 10;
    tm.tm_mday = 16;
    tm.tm_hour = 9;
    tm.tm_min = 4;
    tm.tm_sec = 5;
    OutputStreambuf narrowStream{};
    narrowStream.buffer = {&g_narrowOutVtable, {}, &narrowStream.next, {}, &narrowStream.end};
    const auto* putCharVtable = reinterpret_cast<const GuestLocale::TimePutVtable*>(putDefault.base.vtable);
    auto narrowOut = putCharVtable->put(&putDefault.base, {false, &narrowStream.buffer}, nullptr, ' ', &tm, 'Y', 0);
    Require(!narrowOut.failed && narrowStream.narrowOut == "2026", "time_put<char> vtable put formats %Y");

    struct WideTimePutGuarded {
        GuestLocale::Facet base;
        std::array<std::byte, 16> guard;
    } putWideGuarded{};
    putWideGuarded.guard.fill(std::byte{0xa5});
    const auto expectedGuard = putWideGuarded.guard;
    _ZNSt8time_putIwSt19ostreambuf_iteratorIwSt11char_traitsIwEEEC1Em_nid_postfix(&putWideGuarded.base, 4);
    Require(putWideGuarded.base.vtable != nullptr && putWideGuarded.base.references == 4 && putWideGuarded.guard == expectedGuard, "time_put<wchar_t> C1 writes only 0x10-byte Facet header");

    OutputStreambuf wideStream{};
    wideStream.buffer = {&g_wideOutVtable, {}, &wideStream.next, {}, &wideStream.end};
    const auto* putWideVtable = reinterpret_cast<const GuestLocale::WideTimePutVtable*>(putWideGuarded.base.vtable);
    auto wideOut = putWideVtable->put(&putWideGuarded.base, {false, &wideStream.buffer}, nullptr, u' ', &tm, 'Y', 0);
    Require(!wideOut.failed && wideStream.wideOut == u"2026", "time_put<wchar_t> vtable put formats %Y");
    _ZNSt8_LocinfoD1Ev_nid_postfix(&info);

    WideTimeGetStorage invalidStorage{};
    Require(Throws([&] { _ZNSt8time_getIwSt19istreambuf_iteratorIwSt11char_traitsIwEEEC1EPKcm_nid_postfix(nullptr, "C", 0); }), "wide time_get C1 null self throws");
    Require(Throws([&] { _ZNSt8time_getIwSt19istreambuf_iteratorIwSt11char_traitsIwEEEC2EPKcm_nid_postfix(nullptr, "C", 0); }), "wide time_get C2 null self throws");
    Require(Throws([&] { _ZNSt8time_getIwSt19istreambuf_iteratorIwSt11char_traitsIwEEEC1EPKcm_nid_postfix(&invalidStorage.base, nullptr, 0); }), "wide time_get C1 null name throws");
    Require(Throws([&] { _ZNSt8time_getIwSt19istreambuf_iteratorIwSt11char_traitsIwEEEC2EPKcm_nid_postfix(&invalidStorage.base, "en_US", 0); }), "wide time_get C2 non-C name throws");
    Require(Throws([] { _ZNSt8time_getIcSt19istreambuf_iteratorIcSt11char_traitsIcEEED2Ev_nid_postfix(nullptr); }), "char time_get D2 null throws");
    Require(Throws([] { _ZNSt8time_getIwSt19istreambuf_iteratorIwSt11char_traitsIwEEED2Ev_nid_postfix(nullptr); }), "wide time_get D2 null throws");
    Require(Throws([&] { _ZNSt8time_putIcSt19ostreambuf_iteratorIcSt11char_traitsIcEEEC1ERKSt8_Locinfom_nid_postfix(nullptr, &info, 0); }), "char time_put C1 null self throws");
    Require(Throws([&] { _ZNSt8time_putIcSt19ostreambuf_iteratorIcSt11char_traitsIcEEEC1ERKSt8_Locinfom_nid_postfix(&putFromLocinfo.base, nullptr, 0); }), "char time_put C1 null locinfo throws");
    Require(Throws([] { _ZNSt8time_putIcSt19ostreambuf_iteratorIcSt11char_traitsIcEEEC2Em_nid_postfix(nullptr, 0); }), "char time_put C2 null self throws");
    Require(Throws([] { _ZNSt8time_putIwSt19ostreambuf_iteratorIwSt11char_traitsIwEEEC1Em_nid_postfix(nullptr, 0); }), "wide time_put C1 null self throws");
    Require(Throws([&] { _ZNSt8time_putIwSt19ostreambuf_iteratorIwSt11char_traitsIwEEE5_InitERKSt8_Locinfo_nid_postfix(&putWideGuarded.base, &info); }), "wide time_put _Init still throws");
    Require(NotImplementedCalls_nid_no_patch() > before, "error paths reported via NotImplemented");
}

template<typename TChar>
struct InputStreambuf {
    GuestLocale::Streambuf buffer{};
    char* next = nullptr;
    char* end = nullptr;
    std::basic_string<TChar> storage;
    std::size_t index = 0;
    int underflows = 0;
    int uflows = 0;

    void Setup(std::basic_string<TChar> text, bool buffered);
    GuestLocale::InputIterator Begin() {
        return GuestLocale::InputIterator{{reinterpret_cast<std::uint64_t>(&buffer), 0}};
    }
    char RemainingFirst() const {
        if (next != nullptr && next < end) return *next;
        return index < storage.size() ? static_cast<char>(storage[index]) : '\0';
    }
};

template<typename TChar>
int APS5_VABI InputUnderflow(GuestLocale::Streambuf* self) {
    auto* stream = reinterpret_cast<InputStreambuf<TChar>*>(self);
    ++stream->underflows;
    if (stream->index >= stream->storage.size()) return -1;
    if constexpr (sizeof(TChar) == 1) {
        return static_cast<unsigned char>(stream->storage[stream->index]);
    } else {
        return static_cast<std::uint16_t>(stream->storage[stream->index]);
    }
}

template<typename TChar>
int APS5_VABI InputUflow(GuestLocale::Streambuf* self) {
    auto* stream = reinterpret_cast<InputStreambuf<TChar>*>(self);
    ++stream->uflows;
    if (stream->index >= stream->storage.size()) return -1;
    const auto element = stream->storage[stream->index++];
    if constexpr (sizeof(TChar) == 1) {
        return static_cast<unsigned char>(element);
    } else {
        return static_cast<std::uint16_t>(element);
    }
}

struct InputStreambufVtable {
    std::array<std::uintptr_t, 9> slots{};
};

template<typename TChar>
const InputStreambufVtable& GetInputVtable() {
    static const InputStreambufVtable vt = [] {
        InputStreambufVtable table{};
        table.slots[7] = reinterpret_cast<std::uintptr_t>(&InputUnderflow<TChar>);
        table.slots[8] = reinterpret_cast<std::uintptr_t>(&InputUflow<TChar>);
        return table;
    }();
    return vt;
}

template<typename TChar>
void InputStreambuf<TChar>::Setup(std::basic_string<TChar> text, bool buffered) {
    storage = std::move(text);
    index = 0;
    underflows = 0;
    uflows = 0;
    next = nullptr;
    end = nullptr;
    buffer = {};
    buffer.vtable = reinterpret_cast<const GuestLocale::StreambufVtable*>(GetInputVtable<TChar>().slots.data());
    if (buffered && !storage.empty()) {
        next = reinterpret_cast<char*>(storage.data());
        end = next + storage.size() * sizeof(TChar);
        index = storage.size();
        buffer.reserved[6] = &next;
        buffer.reservedCounts[2] = &end;
    }
}

GuestLocale::InputIterator EndIter() {
    return GuestLocale::InputIterator{{0, 1}};
}

bool APS5_VABI CustomWideIs(const GuestLocale::Facet*, short mask, std::uint16_t ch) {
    if (mask == 0x44) return ch == u' ' || ch == u'\t';
    if (mask == 0x20) return ch >= u'0' && ch <= u'9';
    return false;
}

char APS5_VABI CustomWideNarrow(const GuestLocale::Facet*, std::uint16_t ch, char fallback) {
    return ch <= 0x7f ? static_cast<char>(ch) : fallback;
}

std::uint16_t APS5_VABI CustomWideToLower(const GuestLocale::Facet*, std::uint16_t ch) {
    return (ch >= u'A' && ch <= u'Z') ? static_cast<std::uint16_t>(ch - u'A' + u'a') : ch;
}

struct WideCtypeFixture {
    GuestLocale::Facet base;
    std::array<std::uintptr_t, 16> vt{};
};

WideCtypeFixture MakeWideCtype() {
    WideCtypeFixture f{};
    f.vt[4] = reinterpret_cast<std::uintptr_t>(&CustomWideIs);
    f.vt[8] = reinterpret_cast<std::uintptr_t>(&CustomWideToLower);
    f.vt[14] = reinterpret_cast<std::uintptr_t>(&CustomWideNarrow);
    f.base.vtable = reinterpret_cast<const GuestLocale::FacetVtable*>(f.vt.data());
    return f;
}

void CheckGroup2GetInt() {
    const std::uint64_t before = NotImplementedCalls_nid_no_patch();
    WideTimeGetStorage facet{};
    _ZNSt8time_getIwSt19istreambuf_iteratorIwSt11char_traitsIwEEEC1EPKcm_nid_postfix(&facet.base, "C", 0);
    WideCtypeFixture ctype = MakeWideCtype();

    for (const bool buffered : {true, false}) {
        InputStreambuf<char16_t> sb{};
        {
            sb.Setup(u"42abc", buffered);
            auto first = sb.Begin();
            auto last = EndIter();
            int val = -1;
            const auto state = _ZNKSt8time_getIwSt19istreambuf_iteratorIwSt11char_traitsIwEEE7_GetintERS3_S5_iiRiRKSt5ctypeIwE_nid_postfix(&facet.base, &first, &last, 0, 99, &val, &ctype.base);
            Require(state == 0 && val == 42, "valid integer without eof");
        }
        {
            sb.Setup(u"0007", buffered);
            auto first = sb.Begin();
            auto last = EndIter();
            int val = -1;
            const auto state = _ZNKSt8time_getIwSt19istreambuf_iteratorIwSt11char_traitsIwEEE7_GetintERS3_S5_iiRiRKSt5ctypeIwE_nid_postfix(&facet.base, &first, &last, 0, 9999, &val, &ctype.base);
            Require(state == 1 && val == 7, "leading zeros collapsed and eofbit set");
        }
        {
            sb.Setup(u" 5", buffered);
            auto first = sb.Begin();
            auto last = EndIter();
            int val = -1;
            const auto state = _ZNKSt8time_getIwSt19istreambuf_iteratorIwSt11char_traitsIwEEE7_GetintERS3_S5_iiRiRKSt5ctypeIwE_nid_postfix(&facet.base, &first, &last, 1, 31, &val, &ctype.base);
            Require(state == 1 && val == 5, "leading space allowed in _Getint_v2");
        }
        {
            sb.Setup(u"1234", buffered);
            auto first = sb.Begin();
            auto last = EndIter();
            int val = -1;
            const auto state = _ZNKSt8time_getIwSt19istreambuf_iteratorIwSt11char_traitsIwEEE7_GetintERS3_S5_iiRiRKSt5ctypeIwE_nid_postfix(&facet.base, &first, &last, 0, 59, &val, &ctype.base);
            Require(state == 0 && val == 12, "max width respects _Hi digits");
        }
        {
            sb.Setup(u"65", buffered);
            auto first = sb.Begin();
            auto last = EndIter();
            int val = 777;
            const auto state = _ZNKSt8time_getIwSt19istreambuf_iteratorIwSt11char_traitsIwEEE7_GetintERS3_S5_iiRiRKSt5ctypeIwE_nid_postfix(&facet.base, &first, &last, 0, 59, &val, &ctype.base);
            Require(state == 3 && val == 777, "out of range sets failbit+eofbit and leaves val unchanged");
        }
        {
            sb.Setup(u"xyz", buffered);
            auto first = sb.Begin();
            auto last = EndIter();
            int val = 888;
            const auto state = _ZNKSt8time_getIwSt19istreambuf_iteratorIwSt11char_traitsIwEEE7_GetintERS3_S5_iiRiRKSt5ctypeIwE_nid_postfix(&facet.base, &first, &last, 0, 59, &val, &ctype.base);
            Require(state == 2 && val == 888, "non-digit sets failbit without eofbit");
        }
        {
            sb.Setup(u"+7", buffered);
            auto first = sb.Begin();
            auto last = EndIter();
            int val = -1;
            const auto state = _ZNKSt8time_getIwSt19istreambuf_iteratorIwSt11char_traitsIwEEE7_GetintERS3_S5_iiRiRKSt5ctypeIwE_nid_postfix(&facet.base, &first, &last, 0, 59, &val, &ctype.base);
            Require(state == 1 && val == 7, "plus sign accepted");
        }
        {
            sb.Setup(u"-5", buffered);
            auto first = sb.Begin();
            auto last = EndIter();
            int val = 0;
            const auto state = _ZNKSt8time_getIwSt19istreambuf_iteratorIwSt11char_traitsIwEEE7_GetintERS3_S5_iiRiRKSt5ctypeIwE_nid_postfix(&facet.base, &first, &last, -9, 9, &val, &ctype.base);
            Require(state == 1 && val == -5, "minus sign accepted inside range");
        }
        {
            sb.Setup(u"-5", buffered);
            auto first = sb.Begin();
            auto last = EndIter();
            int val = 666;
            const auto state = _ZNKSt8time_getIwSt19istreambuf_iteratorIwSt11char_traitsIwEEE7_GetintERS3_S5_iiRiRKSt5ctypeIwE_nid_postfix(&facet.base, &first, &last, 0, 59, &val, &ctype.base);
            Require(state == 3 && val == 666, "negative value below range fails");
        }
    }

    InputStreambuf<char16_t> emptySb{};
    emptySb.Setup(u"", false);
    auto emptyFirst = emptySb.Begin();
    auto last = EndIter();
    int val = 999;
    Require(_ZNKSt8time_getIwSt19istreambuf_iteratorIwSt11char_traitsIwEEE7_GetintERS3_S5_iiRiRKSt5ctypeIwE_nid_postfix(&facet.base, &emptyFirst, &last, 0, 59, &val, &ctype.base) == 3 && val == 999, "empty input returns eofbit|failbit");

    Require(Throws([&] { _ZNKSt8time_getIwSt19istreambuf_iteratorIwSt11char_traitsIwEEE7_GetintERS3_S5_iiRiRKSt5ctypeIwE_nid_postfix(nullptr, &emptyFirst, &last, 0, 59, &val, &ctype.base); }), "_Getint null self throws");
    Require(Throws([&] { _ZNKSt8time_getIwSt19istreambuf_iteratorIwSt11char_traitsIwEEE7_GetintERS3_S5_iiRiRKSt5ctypeIwE_nid_postfix(&facet.base, nullptr, &last, 0, 59, &val, &ctype.base); }), "_Getint null first throws");
    Require(Throws([&] { _ZNKSt8time_getIwSt19istreambuf_iteratorIwSt11char_traitsIwEEE7_GetintERS3_S5_iiRiRKSt5ctypeIwE_nid_postfix(&facet.base, &emptyFirst, &last, 0, 59, &val, nullptr); }), "_Getint null ctype throws");
    Require(NotImplementedCalls_nid_no_patch() > before, "_Getint error paths reported");
}

template<typename TChar>
std::basic_string<TChar> Ascii(const char* text) {
    std::basic_string<TChar> out;
    for (const char* p = text; *p != '\0'; ++p) out.push_back(static_cast<TChar>(static_cast<unsigned char>(*p)));
    return out;
}

template<typename TChar, typename TGetTimeFn, typename TGetYearFn, typename TGetMonthFn>
void CheckGroup3Typed(const GuestLocale::Facet* facet, TGetTimeFn getTime, TGetYearFn getYear, TGetMonthFn getMonth) {
    GuestLocale::IosBase ios{};
    GuestLocale::Implementation* classic = _ZSt21_sceLibcClassicLocale_nid_postfix;
    ios.locale = &classic;
    InputStreambuf<TChar> sb{};

    for (const bool buffered : {true, false}) {
        {
            sb.Setup(Ascii<TChar>("23:59:60"), buffered);
            GuestTm tm{};
            std::int32_t state = 0;
            getTime(facet, sb.Begin(), EndIter(), &ios, &state, &tm);
            Require(state == 1 && tm.tm_hour == 23 && tm.tm_min == 59 && tm.tm_sec == 60, "do_get_time parses leap second to eof");
        }
        {
            sb.Setup(Ascii<TChar>("07:08:09Z"), buffered);
            GuestTm tm{};
            std::int32_t state = 0;
            getTime(facet, sb.Begin(), EndIter(), &ios, &state, &tm);
            Require(state == 0 && tm.tm_hour == 7 && tm.tm_min == 8 && tm.tm_sec == 9 && sb.RemainingFirst() == 'Z', "do_get_time leaves trailing character unconsumed");
        }
        {
            sb.Setup(Ascii<TChar>("12-34:56"), buffered);
            GuestTm tm{};
            tm.tm_min = 44;
            std::int32_t state = 0;
            getTime(facet, sb.Begin(), EndIter(), &ios, &state, &tm);
            Require(state == 2 && tm.tm_hour == 12 && tm.tm_min == 44, "do_get_time rejects non-colon separator");
        }
        {
            sb.Setup(Ascii<TChar>("12:34"), buffered);
            GuestTm tm{};
            tm.tm_sec = 55;
            std::int32_t state = 0;
            getTime(facet, sb.Begin(), EndIter(), &ios, &state, &tm);
            Require(state == 3 && tm.tm_sec == 55, "do_get_time truncated at second separator sets eofbit|failbit");
        }

        const struct {
            const char* text;
            int expectedYear;
            std::int32_t expectedState;
        } yearCases[] = {
            {"2026", 126, 1},
            {"68", 168, 1},
            {"69", 69, 1},
            {"0069", -1831, 1},
            {"99", 99, 1},
            {"00", 100, 1},
            {"2025x", 125, 0},
        };
        for (const auto& c : yearCases) {
            sb.Setup(Ascii<TChar>(c.text), buffered);
            GuestTm tm{};
            std::int32_t state = 0;
            getYear(facet, sb.Begin(), EndIter(), &ios, &state, &tm);
            Require(state == c.expectedState && tm.tm_year == c.expectedYear, "do_get_year conversion");
        }

        {
            sb.Setup(Ascii<TChar>("nOvEmBeR"), buffered);
            GuestTm tm{};
            std::int32_t state = 0;
            getMonth(facet, sb.Begin(), EndIter(), &ios, &state, &tm);
            Require(state == 0 && tm.tm_mon == 10, "do_get_monthname case-insensitive full name");
        }
        {
            sb.Setup(Ascii<TChar>("jAnX"), buffered);
            GuestTm tm{};
            std::int32_t state = 0;
            getMonth(facet, sb.Begin(), EndIter(), &ios, &state, &tm);
            Require(state == 0 && tm.tm_mon == 0 && sb.RemainingFirst() == 'X', "do_get_monthname stops cleanly at abbreviated match");
        }
        {
            sb.Setup(Ascii<TChar>("May"), buffered);
            GuestTm tm{};
            std::int32_t state = 0;
            getMonth(facet, sb.Begin(), EndIter(), &ios, &state, &tm);
            Require(state == 0 && tm.tm_mon == 4, "do_get_monthname resolves May");
        }
        {
            sb.Setup(Ascii<TChar>("JanuaX"), buffered);
            GuestTm tm{};
            tm.tm_mon = 9;
            std::int32_t state = 0;
            getMonth(facet, sb.Begin(), EndIter(), &ios, &state, &tm);
            Require(state == 2 && tm.tm_mon == 9, "do_get_monthname fails partial long name mismatch");
        }
        {
            sb.Setup(Ascii<TChar>(""), buffered);
            GuestTm tm{};
            tm.tm_mon = 9;
            std::int32_t state = 0;
            getMonth(facet, sb.Begin(), EndIter(), &ios, &state, &tm);
            Require(state == 2 && tm.tm_mon == 9, "do_get_monthname fails empty input");
        }
    }
}

void CheckGroup3Fields() {
    const std::uint64_t before = NotImplementedCalls_nid_no_patch();
    GuestLocale::Facet charFacet{};
    CheckGroup3Typed<char>(
        &charFacet,
        &_ZNKSt8time_getIcSt19istreambuf_iteratorIcSt11char_traitsIcEEE11do_get_timeES3_S3_RSt8ios_baseRNSt5_IosbIiE8_IostateEP2tm_nid_postfix,
        &_ZNKSt8time_getIcSt19istreambuf_iteratorIcSt11char_traitsIcEEE11do_get_yearES3_S3_RSt8ios_baseRNSt5_IosbIiE8_IostateEP2tm_nid_postfix,
        &_ZNKSt8time_getIcSt19istreambuf_iteratorIcSt11char_traitsIcEEE16do_get_monthnameES3_S3_RSt8ios_baseRNSt5_IosbIiE8_IostateEP2tm_nid_postfix);

    WideTimeGetStorage wideFacet{};
    _ZNSt8time_getIwSt19istreambuf_iteratorIwSt11char_traitsIwEEEC1EPKcm_nid_postfix(&wideFacet.base, "C", 0);
    const auto* vt = reinterpret_cast<const GuestLocale::TimeGetVtable*>(wideFacet.base.vtable);
    CheckGroup3Typed<char16_t>(&wideFacet.base, vt->getTime, vt->getYear, vt->getMonthname);

    GuestTm tm{};
    std::int32_t state = 0;
    GuestLocale::IosBase ios{};
    Require(Throws([&] { _ZNKSt8time_getIcSt19istreambuf_iteratorIcSt11char_traitsIcEEE11do_get_timeES3_S3_RSt8ios_baseRNSt5_IosbIiE8_IostateEP2tm_nid_postfix(nullptr, EndIter(), EndIter(), &ios, &state, &tm); }), "char do_get_time null self throws");
    Require(Throws([&] { _ZNKSt8time_getIcSt19istreambuf_iteratorIcSt11char_traitsIcEEE11do_get_yearES3_S3_RSt8ios_baseRNSt5_IosbIiE8_IostateEP2tm_nid_postfix(&charFacet, EndIter(), EndIter(), &ios, nullptr, &tm); }), "char do_get_year null state throws");
    Require(Throws([&] { _ZNKSt8time_getIcSt19istreambuf_iteratorIcSt11char_traitsIcEEE16do_get_monthnameES3_S3_RSt8ios_baseRNSt5_IosbIiE8_IostateEP2tm_nid_postfix(&charFacet, EndIter(), EndIter(), &ios, &state, nullptr); }), "char do_get_monthname null tm throws");
    Require(Throws([&] { _ZNKSt8time_getIwSt19istreambuf_iteratorIwSt11char_traitsIwEEE11do_get_timeES3_S3_RSt8ios_baseRNSt5_IosbIiE8_IostateEP2tm_nid_postfix(&wideFacet.base, EndIter(), EndIter(), &ios, &state, nullptr); }), "wide do_get_time null tm throws");
    Require(Throws([&] { _ZNKSt8time_getIwSt19istreambuf_iteratorIwSt11char_traitsIwEEE11do_get_yearES3_S3_RSt8ios_baseRNSt5_IosbIiE8_IostateEP2tm_nid_postfix(nullptr, EndIter(), EndIter(), &ios, &state, &tm); }), "wide do_get_year null self throws");
    Require(Throws([&] { _ZNKSt8time_getIwSt19istreambuf_iteratorIwSt11char_traitsIwEEE16do_get_monthnameES3_S3_RSt8ios_baseRNSt5_IosbIiE8_IostateEP2tm_nid_postfix(&wideFacet.base, EndIter(), EndIter(), &ios, nullptr, &tm); }), "wide do_get_monthname null state throws");
    Require(NotImplementedCalls_nid_no_patch() > before, "Group 3 error paths reported");
}

struct DispatchEntry {
    char specifier;
    char modifier;
};

struct DispatchFacet {
    GuestLocale::Facet base;
    std::vector<DispatchEntry> calls;
    bool setFailOnNext = false;
};

GuestLocale::InputIterator APS5_VABI MockDoGet(const GuestLocale::Facet* self, GuestLocale::InputIterator first, GuestLocale::InputIterator last, GuestLocale::IosBase* stream, std::int32_t* state, GuestTm* time, char specifier, char modifier) {
    auto* facet = const_cast<DispatchFacet*>(reinterpret_cast<const DispatchFacet*>(self));
    facet->calls.push_back({specifier, modifier});
    if (facet->setFailOnNext) {
        *state |= 2;
        return first;
    }
    if (specifier == 'H') {
        return _ZNKSt8time_getIwSt19istreambuf_iteratorIwSt11char_traitsIwEEE11do_get_timeES3_S3_RSt8ios_baseRNSt5_IosbIiE8_IostateEP2tm_nid_postfix(self, first, last, stream, state, time);
    }
    if (specifier == 'Y') {
        return _ZNKSt8time_getIwSt19istreambuf_iteratorIwSt11char_traitsIwEEE11do_get_yearES3_S3_RSt8ios_baseRNSt5_IosbIiE8_IostateEP2tm_nid_postfix(self, first, last, stream, state, time);
    }
    *state |= 2;
    return first;
}

const GuestLocale::TimeGetVtable g_mockWideTimeGetVtable{
    {nullptr, nullptr, nullptr, nullptr},
    nullptr,
    nullptr,
    nullptr,
    nullptr,
    nullptr,
    nullptr,
    MockDoGet,
};

void CheckGroup4GetAndGetFmt() {
    const std::uint64_t before = NotImplementedCalls_nid_no_patch();
    WideTimeGetStorage realFacet{};
    _ZNSt8time_getIwSt19istreambuf_iteratorIwSt11char_traitsIwEEEC1EPKcm_nid_postfix(&realFacet.base, "C", 0);

    GuestLocale::IosBase ios{};
    GuestLocale::Implementation* classic = _ZSt21_sceLibcClassicLocale_nid_postfix;
    ios.locale = &classic;

    InputStreambuf<char16_t> sb{};
    {
        sb.Setup(u"AbC   2026", true);
        GuestTm tm{};
        std::int32_t state = 2;
        const std::u16string fmt = u"aBc 2026";
        _ZNKSt8time_getIwSt19istreambuf_iteratorIwSt11char_traitsIwEEE3getES3_S3_RSt8ios_baseRNSt5_IosbIiE8_IostateEP2tmPKwSE__nid_postfix(
            &realFacet.base, sb.Begin(), EndIter(), &ios, &state, &tm,
            reinterpret_cast<const std::uint16_t*>(fmt.data()),
            reinterpret_cast<const std::uint16_t*>(fmt.data() + fmt.size()));
        Require(state == 0, "get resets state, matches case-insensitive literals, and skips multiple spaces");
    }
    {
        sb.Setup(u"12:34:56", false);
        GuestTm tm{};
        std::int32_t state = 0;
        const std::u16string fmt = u"%H";
        Require(Throws([&] {
            _ZNKSt8time_getIwSt19istreambuf_iteratorIwSt11char_traitsIwEEE3getES3_S3_RSt8ios_baseRNSt5_IosbIiE8_IostateEP2tmPKwSE__nid_postfix(
                &realFacet.base, sb.Begin(), EndIter(), &ios, &state, &tm,
                reinterpret_cast<const std::uint16_t*>(fmt.data()),
                reinterpret_cast<const std::uint16_t*>(fmt.data() + fmt.size()));
        }), "C1-constructed wide time_get throws NotImplemented when hitting unimplemented do_get slot");
    }

    DispatchFacet dispatch{{&g_mockWideTimeGetVtable.facet, 1, 0}, {}, false};
    Require(_ZNSt5ctypeIwE2idE_nid_postfix != 0, "ctype<wchar_t>::id initialized after FindFacet");
    std::vector<GuestLocale::Facet*> facets(_ZNSt5ctypeIwE2idE_nid_postfix + 1, nullptr);
    WideCtypeFixture customCtype = MakeWideCtype();
    facets[_ZNSt5ctypeIwE2idE_nid_postfix] = &customCtype.base;
    GuestLocale::Implementation customLoc{{nullptr, 1, 0}, facets.data(), facets.size(), 5, false, "C"};
    GuestLocale::Implementation* customPtr = &customLoc;
    GuestLocale::IosBase customIos{};
    customIos.locale = &customPtr;

    for (const bool buffered : {true, false}) {
        {
            dispatch.calls.clear();
            dispatch.setFailOnNext = false;
            sb.Setup(u"2026 /  09:10:11!", buffered);
            GuestTm tm{};
            std::int32_t state = 0;
            const std::u16string fmt = u"%EY / %OH";
            _ZNKSt8time_getIwSt19istreambuf_iteratorIwSt11char_traitsIwEEE3getES3_S3_RSt8ios_baseRNSt5_IosbIiE8_IostateEP2tmPKwSE__nid_postfix(
                &dispatch.base, sb.Begin(), EndIter(), &customIos, &state, &tm,
                reinterpret_cast<const std::uint16_t*>(fmt.data()),
                reinterpret_cast<const std::uint16_t*>(fmt.data() + fmt.size()));
            Require(state == 0 && tm.tm_year == 126 && tm.tm_hour == 9 && tm.tm_min == 10 && tm.tm_sec == 11, "get dispatches modifiers E/O and parses through do_get");
            Require(dispatch.calls.size() == 2 && dispatch.calls[0].specifier == 'Y' && dispatch.calls[0].modifier == 'E' && dispatch.calls[1].specifier == 'H' && dispatch.calls[1].modifier == 'O', "get forwards exact specifier/modifier pairs");
            Require(sb.RemainingFirst() == '!', "get leaves unconsumed trailing char");
        }
        {
            dispatch.calls.clear();
            dispatch.setFailOnNext = true;
            sb.Setup(u"12:34:56", buffered);
            GuestTm tm{};
            std::int32_t state = 0;
            const std::u16string fmt = u"%H %Y";
            _ZNKSt8time_getIwSt19istreambuf_iteratorIwSt11char_traitsIwEEE3getES3_S3_RSt8ios_baseRNSt5_IosbIiE8_IostateEP2tmPKwSE__nid_postfix(
                &dispatch.base, sb.Begin(), EndIter(), &customIos, &state, &tm,
                reinterpret_cast<const std::uint16_t*>(fmt.data()),
                reinterpret_cast<const std::uint16_t*>(fmt.data() + fmt.size()));
            Require(state == 2 && dispatch.calls.size() == 1, "get stops immediately once state becomes non-goodbit");
        }
        {
            dispatch.calls.clear();
            dispatch.setFailOnNext = false;
            sb.Setup(u"2026-09:10:11", buffered);
            GuestTm tm{};
            std::int32_t state = 0;
            _ZNKSt8time_getIwSt19istreambuf_iteratorIwSt11char_traitsIwEEE7_GetfmtES3_S3_RSt8ios_baseRNSt5_IosbIiE8_IostateEP2tmPKc_nid_postfix(
                &dispatch.base, sb.Begin(), EndIter(), &customIos, &state, &tm, "%Y-%H");
            Require(state == 1 && tm.tm_year == 126 && tm.tm_hour == 9 && tm.tm_sec == 11, "_Getfmt dispatches format specifiers and sets eofbit");
            Require(dispatch.calls.size() == 2 && dispatch.calls[0].modifier == '\0' && dispatch.calls[1].modifier == '\0', "_Getfmt passes zero modifier");
        }
    }

    {
        sb.Setup(u"a", true);
        GuestTm tm{};
        std::int32_t state = 0;
        const std::u16string trailingPct = u"a%";
        _ZNKSt8time_getIwSt19istreambuf_iteratorIwSt11char_traitsIwEEE3getES3_S3_RSt8ios_baseRNSt5_IosbIiE8_IostateEP2tmPKwSE__nid_postfix(
            &dispatch.base, sb.Begin(), EndIter(), &ios, &state, &tm,
            reinterpret_cast<const std::uint16_t*>(trailingPct.data()),
            reinterpret_cast<const std::uint16_t*>(trailingPct.data() + trailingPct.size()));
        Require(state == 3, "get trailing % after input end sets eofbit|failbit");

        sb.Setup(u"ab", true);
        state = 0;
        const std::u16string trailingMod = u"a%E";
        _ZNKSt8time_getIwSt19istreambuf_iteratorIwSt11char_traitsIwEEE3getES3_S3_RSt8ios_baseRNSt5_IosbIiE8_IostateEP2tmPKwSE__nid_postfix(
            &dispatch.base, sb.Begin(), EndIter(), &ios, &state, &tm,
            reinterpret_cast<const std::uint16_t*>(trailingMod.data()),
            reinterpret_cast<const std::uint16_t*>(trailingMod.data() + trailingMod.size()));
        Require(state == 2, "get trailing modifier sets failbit");

        sb.Setup(u"a", true);
        state = 0;
        _ZNKSt8time_getIwSt19istreambuf_iteratorIwSt11char_traitsIwEEE7_GetfmtES3_S3_RSt8ios_baseRNSt5_IosbIiE8_IostateEP2tmPKc_nid_postfix(
            &dispatch.base, sb.Begin(), EndIter(), &ios, &state, &tm, "a%");
        Require(state == 3, "_Getfmt trailing % sets failbit|eofbit");
    }

    GuestTm tm{};
    std::int32_t state = 0;
    const std::u16string emptyFmt;
    const auto* fmtPtr = reinterpret_cast<const std::uint16_t*>(emptyFmt.data());
    Require(Throws([&] { _ZNKSt8time_getIwSt19istreambuf_iteratorIwSt11char_traitsIwEEE3getES3_S3_RSt8ios_baseRNSt5_IosbIiE8_IostateEP2tmPKwSE__nid_postfix(nullptr, EndIter(), EndIter(), &ios, &state, &tm, fmtPtr, fmtPtr); }), "get null self throws");
    Require(Throws([&] { _ZNKSt8time_getIwSt19istreambuf_iteratorIwSt11char_traitsIwEEE3getES3_S3_RSt8ios_baseRNSt5_IosbIiE8_IostateEP2tmPKwSE__nid_postfix(&dispatch.base, EndIter(), EndIter(), &ios, nullptr, &tm, fmtPtr, fmtPtr); }), "get null state throws");
    Require(Throws([&] { _ZNKSt8time_getIwSt19istreambuf_iteratorIwSt11char_traitsIwEEE3getES3_S3_RSt8ios_baseRNSt5_IosbIiE8_IostateEP2tmPKwSE__nid_postfix(&dispatch.base, EndIter(), EndIter(), &ios, &state, &tm, fmtPtr + 1, fmtPtr); }), "get inverted format range throws");
    Require(Throws([&] { _ZNKSt8time_getIwSt19istreambuf_iteratorIwSt11char_traitsIwEEE7_GetfmtES3_S3_RSt8ios_baseRNSt5_IosbIiE8_IostateEP2tmPKc_nid_postfix(nullptr, EndIter(), EndIter(), &ios, &state, &tm, ""); }), "_Getfmt null self throws");
    Require(Throws([&] { _ZNKSt8time_getIwSt19istreambuf_iteratorIwSt11char_traitsIwEEE7_GetfmtES3_S3_RSt8ios_baseRNSt5_IosbIiE8_IostateEP2tmPKc_nid_postfix(&dispatch.base, EndIter(), EndIter(), &ios, &state, &tm, nullptr); }), "_Getfmt null fmt throws");
    Require(NotImplementedCalls_nid_no_patch() > before, "Group 4 error paths reported");
}

template<typename TCall>
void RequireInvalid(TCall call, const char* text) {
    bool threw = false;
    errno = 71;
    try {
        call();
    } catch (const std::invalid_argument& error) {
        threw = std::strstr(error.what(), text) != nullptr;
    }
    Require(threw && errno == 71, text);
}

class GuardedPages {
public:
    GuardedPages() {
#ifdef _WIN32
        SYSTEM_INFO info{};
        GetSystemInfo(&info);
        pageSize = info.dwPageSize;
        memory = static_cast<std::byte*>(VirtualAlloc(nullptr, pageSize * 2, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE));
        Require(memory != nullptr, "guarded allocation succeeds");
        DWORD previous = 0;
        Require(VirtualProtect(memory + pageSize, pageSize, PAGE_NOACCESS, &previous) != 0, "guard page protection succeeds");
#else
        const long size = sysconf(_SC_PAGESIZE);
        Require(size > 0, "host page size is available");
        pageSize = static_cast<std::size_t>(size);
        void* allocation = mmap(nullptr, pageSize * 2, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
        Require(allocation != MAP_FAILED, "guarded allocation succeeds");
        memory = static_cast<std::byte*>(allocation);
        Require(mprotect(memory + pageSize, pageSize, PROT_NONE) == 0, "guard page protection succeeds");
#endif
    }

    ~GuardedPages() {
#ifdef _WIN32
        VirtualFree(memory, 0, MEM_RELEASE);
#else
        munmap(memory, pageSize * 2);
#endif
    }

    GuardedPages(const GuardedPages&) = delete;
    GuardedPages& operator=(const GuardedPages&) = delete;

    template<typename TElement>
    TElement* Last(std::size_t count) { return reinterpret_cast<TElement*>(memory + pageSize) - count; }

    void ReadOnly() {
#ifdef _WIN32
        DWORD previous = 0;
        Require(VirtualProtect(memory, pageSize, PAGE_READONLY, &previous) != 0, "read-only protection succeeds");
#else
        Require(mprotect(memory, pageSize, PROT_READ) == 0, "read-only protection succeeds");
#endif
    }

private:
    std::byte* memory = nullptr;
    std::size_t pageSize = 0;
};

template<typename TPointer>
TPointer Inaccessible() {
    return reinterpret_cast<TPointer>(std::uintptr_t{0x10});
}

void CheckGroup5GuestMemory() {
    constexpr auto getInt = _ZNKSt8time_getIwSt19istreambuf_iteratorIwSt11char_traitsIwEEE7_GetintERS3_S5_iiRiRKSt5ctypeIwE_nid_postfix;
    constexpr auto getTime = _ZNKSt8time_getIwSt19istreambuf_iteratorIwSt11char_traitsIwEEE11do_get_timeES3_S3_RSt8ios_baseRNSt5_IosbIiE8_IostateEP2tm_nid_postfix;
    constexpr auto getYear = _ZNKSt8time_getIwSt19istreambuf_iteratorIwSt11char_traitsIwEEE11do_get_yearES3_S3_RSt8ios_baseRNSt5_IosbIiE8_IostateEP2tm_nid_postfix;
    constexpr auto getMonth = _ZNKSt8time_getIwSt19istreambuf_iteratorIwSt11char_traitsIwEEE16do_get_monthnameES3_S3_RSt8ios_baseRNSt5_IosbIiE8_IostateEP2tm_nid_postfix;
    constexpr auto get = _ZNKSt8time_getIwSt19istreambuf_iteratorIwSt11char_traitsIwEEE3getES3_S3_RSt8ios_baseRNSt5_IosbIiE8_IostateEP2tmPKwSE__nid_postfix;
    constexpr auto getFmt = _ZNKSt8time_getIwSt19istreambuf_iteratorIwSt11char_traitsIwEEE7_GetfmtES3_S3_RSt8ios_baseRNSt5_IosbIiE8_IostateEP2tmPKc_nid_postfix;
    WideTimeGetStorage facet{};
    _ZNSt8time_getIwSt19istreambuf_iteratorIwSt11char_traitsIwEEEC1EPKcm_nid_postfix(&facet.base, "C", 0);
    WideCtypeFixture ctype = MakeWideCtype();
    InputStreambuf<char16_t> sb{};
    GuestLocale::IosBase ios{};
    GuestTm tm{};
    std::int32_t state = 0;
    int val = -1;

    sb.Setup(u"42", true);
    auto first = sb.Begin();
    auto last = EndIter();
    RequireInvalid([&] { getInt(&facet.base, Inaccessible<GuestLocale::InputIterator*>(), &last, 0, 99, &val, &ctype.base); }, "unreadable iterator");
    RequireInvalid([&] { getInt(&facet.base, &first, Inaccessible<GuestLocale::InputIterator*>(), 0, 99, &val, &ctype.base); }, "unreadable iterator");
    RequireInvalid([&] { getInt(&facet.base, &first, &last, 0, 99, Inaccessible<int*>(), &ctype.base); }, "unwritable value");
    sb.Setup(u"xyz", true);
    first = sb.Begin();
    Require(getInt(&facet.base, &first, &last, 0, 99, Inaccessible<int*>(), &ctype.base) == 2, "_Getint leaves an unwritable value alone when it fails");
    GuardedPages iteratorPages;
    auto* guardedLast = iteratorPages.Last<GuestLocale::InputIterator>(1);
    *guardedLast = EndIter();
    iteratorPages.ReadOnly();
    sb.Setup(u"42", true);
    first = sb.Begin();
    Require(getInt(&facet.base, &first, guardedLast, 0, 99, &val, &ctype.base) == 1 && val == 42, "_Getint accepts an end iterator it does not change in read-only memory");
    sb.Setup(u"42", true);
    first = sb.Begin();
    val = 5;
    Require(getInt(&facet.base, &first, &first, 0, 99, &val, &ctype.base) == 3 && val == 5, "_Getint treats an aliased first and last as one iterator");

    sb.Setup(u"12:34:56", true);
    RequireInvalid([&] { getTime(&facet.base, sb.Begin(), EndIter(), &ios, Inaccessible<std::int32_t*>(), &tm); }, "unreadable state");
    sb.Setup(u"12:34:56", true);
    RequireInvalid([&] { getTime(&facet.base, sb.Begin(), EndIter(), &ios, &state, Inaccessible<GuestTm*>()); }, "unwritable time");
    sb.Setup(u"xx", true);
    state = 0;
    getTime(&facet.base, sb.Begin(), EndIter(), &ios, &state, Inaccessible<GuestTm*>());
    Require(state == 2, "do_get_time leaves an unwritable tm alone when the hour fails");
    GuardedPages statePages;
    auto* readOnlyState = statePages.Last<std::int32_t>(1);
    statePages.ReadOnly();
    sb.Setup(u"2026", true);
    RequireInvalid([&] { getYear(&facet.base, sb.Begin(), EndIter(), &ios, readOnlyState, &tm); }, "unwritable state");
    sb.Setup(u"Feb", true);
    RequireInvalid([&] { getMonth(Inaccessible<GuestLocale::Facet*>(), sb.Begin(), EndIter(), &ios, &state, &tm); }, "unreadable facet vtable");
    sb.Setup(u"Feb", true);
    RequireInvalid([&] { getMonth(&facet.base, sb.Begin(), EndIter(), &ios, &state, Inaccessible<GuestTm*>()); }, "unwritable time");
    sb.Setup(u"Feb", true);
    tm = {};
    Require(getMonth(&facet.base, sb.Begin(), EndIter(), &ios, &state, &tm).words[0] == 0 && tm.tm_mon == 1, "do_get_monthname reads the months table of a constructed facet");

    DispatchFacet dispatch{{&g_mockWideTimeGetVtable.facet, 1, 0}, {}, false};
    GuardedPages formatPages;
    auto* format = formatPages.Last<std::uint16_t>(2);
    format[0] = u'a';
    format[1] = u'b';
    sb.Setup(u"ab", true);
    state = 0;
    get(&dispatch.base, sb.Begin(), EndIter(), &ios, &state, &tm, format, format + 2);
    Require(state == 0, "get reads a format that ends at a guard page");
    sb.Setup(u"abc", true);
    RequireInvalid([&] { get(&dispatch.base, sb.Begin(), EndIter(), &ios, &state, &tm, format, format + 3); }, "unreadable format");
    sb.Setup(u"a", true);
    state = 0;
    get(&dispatch.base, sb.Begin(), EndIter(), &ios, &state, &tm, format, format + (std::size_t{1} << 40));
    Require(state == 3, "get with an oversized format reads only what it matches");
    sb.Setup(u"ab", true);
    RequireInvalid([&] { get(Inaccessible<GuestLocale::Facet*>(), sb.Begin(), EndIter(), &ios, &state, &tm, format, format + 2); }, "unreadable facet vtable");
    RequireInvalid([&] { get(&dispatch.base, sb.Begin(), EndIter(), &ios, Inaccessible<std::int32_t*>(), &tm, format, format + 2); }, "unwritable state");
    const GuestLocale::Facet broken{Inaccessible<const GuestLocale::FacetVtable*>(), 1, 0};
    const std::u16string directive = u"%H";
    sb.Setup(u"12", true);
    RequireInvalid([&] { get(&broken, sb.Begin(), EndIter(), &ios, &state, &tm, reinterpret_cast<const std::uint16_t*>(directive.data()), reinterpret_cast<const std::uint16_t*>(directive.data() + directive.size())); }, "unreadable facet vtable");

    auto* narrow = formatPages.Last<char>(2);
    narrow[0] = 'a';
    narrow[1] = '\0';
    sb.Setup(u"a", true);
    state = 0;
    getFmt(&dispatch.base, sb.Begin(), EndIter(), &ios, &state, &tm, narrow);
    Require(state == 1, "_Getfmt reads a format whose terminator ends at a guard page");
    narrow[1] = 'b';
    sb.Setup(u"ab", true);
    RequireInvalid([&] { getFmt(&dispatch.base, sb.Begin(), EndIter(), &ios, &state, &tm, narrow); }, "unreadable format");
    sb.Setup(u"x", true);
    RequireInvalid([&] { getFmt(&dispatch.base, sb.Begin(), EndIter(), &ios, Inaccessible<std::int32_t*>(), &tm, "a"); }, "unreadable state");
    sb.Setup(u"a", true);
    state = 0;
    getFmt(&broken, sb.Begin(), EndIter(), &ios, &state, &tm, "a");
    Require(state == 1, "_Getfmt reads no vtable slot for a literal format");
    sb.Setup(u"12", true);
    RequireInvalid([&] { getFmt(&broken, sb.Begin(), EndIter(), &ios, &state, &tm, "%H"); }, "unreadable facet vtable");

    RequireInvalid([&] { _ZNSt8time_putIwSt19ostreambuf_iteratorIwSt11char_traitsIwEEEC1Em_nid_postfix(Inaccessible<GuestLocale::Facet*>(), 1); }, "unwritable facet");
}

void WarmUpHostExceptionRuntime() {
    try {
        throw std::runtime_error("warm-up");
    } catch (const std::runtime_error&) {
    }
}

} // namespace

int main() {
    std::array<void*, 10> api{
        reinterpret_cast<void*>(&TestHeapAllocate),
        reinterpret_cast<void*>(&TestHeapFree),
        reinterpret_cast<void*>(&TestHeapCalloc),
        reinterpret_cast<void*>(&TestHeapReallocate),
        reinterpret_cast<void*>(&TestHeapAlign),
        reinterpret_cast<void*>(&TestHeapRealign),
        reinterpret_cast<void*>(&TestHeapPosixAlign),
    };
    ApplicationHeapRegister_nid_no_patch(api.data());
    CheckGroup1ConstructionAndLifetime();
    CheckGroup2GetInt();
    CheckGroup3Fields();
    CheckGroup4GetAndGetFmt();
    WarmUpHostExceptionRuntime();
    CheckGroup5GuestMemory();
    return 0;
}
