#include "prx/libc/include/GuestLocale.hpp"

#include <Testing/Test.hpp>

#include <array>
#include <climits>
#include <cstring>
#include <cwchar>
#include <exception>
#include <string>
#include <string_view>
#include <thread>
#include <vector>

extern "C" {
extern GuestLocale::Implementation* _ZSt21_sceLibcClassicLocale_nid_postfix;
GuestLocale::Implementation* APS5_VABI _ZNSt6locale16_GetgloballocaleEv_nid_postfix();
GuestLocale::Implementation* APS5_VABI _ZNSt6locale5_InitEv_nid_postfix();
void APS5_VABI _ZNSt6locale5facet9_RegisterEv_nid_postfix(GuestLocale::Facet* self);
void APS5_VABI _ZNSt8_LocinfoC1EPKc_nid_postfix(GuestLocale::LocinfoStorage* self, const char* localeName);
void APS5_VABI _ZNSt8_LocinfoD1Ev_nid_postfix(GuestLocale::LocinfoStorage* self);
void APS5_VABI _ZNSt8ios_baseD2Ev_nid_postfix(GuestLocale::IosBase* self);
const short* APS5_VABI _Getpctype_nid_postfix();
const short* APS5_VABI _Getptolower_nid_postfix();
const short* APS5_VABI _Getptoupper_nid_postfix();
int APS5_VABI _Mbtowcx_nid_postfix(std::uint16_t* dst, const char* src, std::size_t count, std::mbstate_t* st);
int APS5_VABI _Wctombx_nid_postfix(char* dst, std::uint16_t src, std::mbstate_t* st);
void APS5_VABI _Locksyslock_nid_postfix();
void APS5_VABI _Unlocksyslock_nid_postfix();
}

namespace {

using Testing::Case;
using Testing::Require;
using Testing::RequireEqual;

using Retain = void (APS5_VABI *)(GuestLocale::Facet*);
using Release = GuestLocale::Facet* (APS5_VABI *)(GuestLocale::Facet*);

template<typename TAction>
void RequireRejected(TAction action, const char* message) {
    try {
        action();
    } catch (const Testing::Failure&) {
        throw;
    } catch (const std::exception&) {
        return;
    }
    Testing::Fail(std::string(message) + ": did not throw");
}

class ClassicLocaleFixture {
public:
    ClassicLocaleFixture() : locale(_ZSt21_sceLibcClassicLocale_nid_postfix) {
        Require(locale != nullptr, "classic locale exported");
        initial = locale->base.references;
        const std::byte* table = nullptr;
        std::memcpy(&table, locale, sizeof(table));
        std::memcpy(&retain, table + 0x10, sizeof(retain));
        std::memcpy(&release, table + 0x18, sizeof(release));
    }

    ~ClassicLocaleFixture() {
        locale->base.references = initial;
    }

    ClassicLocaleFixture(const ClassicLocaleFixture&) = delete;
    ClassicLocaleFixture& operator=(const ClassicLocaleFixture&) = delete;

    GuestLocale::Implementation* const locale;
    std::uint32_t initial = 0;
    Retain retain = nullptr;
    Release release = nullptr;
};

const Case globalLocale{"Locale_GlobalLocale_IsClassicLocale", [] {
    auto* locale = _ZSt21_sceLibcClassicLocale_nid_postfix;
    Require(locale != nullptr, "classic locale exported");
    RequireEqual(_ZNSt6locale16_GetgloballocaleEv_nid_postfix() == locale, true, "global locale");
}};

const Case init{"Locale_InitCalledRepeatedly_ReturnsClassicLocale", [] {
    auto* locale = _ZSt21_sceLibcClassicLocale_nid_postfix;
    RequireEqual(_ZNSt6locale5_InitEv_nid_postfix() == locale, true, "first init");
    RequireEqual(_ZNSt6locale5_InitEv_nid_postfix() == locale, true, "second init");
}};

const Case retainRelease{"Locale_RetainThenRelease_RestoresReferences", [] {
    const ClassicLocaleFixture fixture;
    auto* base = &fixture.locale->base;
    fixture.retain(base);
    RequireEqual(base->references, fixture.initial + 1, "after retain");
    RequireEqual(fixture.release(base) == nullptr, true, "release result");
    RequireEqual(base->references, fixture.initial, "after release");
}};

const Case releaseAtInitial{"Locale_ReleaseWithoutRetain_KeepsReferences", [] {
    const ClassicLocaleFixture fixture;
    auto* base = &fixture.locale->base;
    RequireEqual(fixture.release(base) == nullptr, true, "release result");
    RequireEqual(base->references, fixture.initial, "references");
}};

const Case retainNull{"Locale_RetainNull_Throws", [] {
    const ClassicLocaleFixture fixture;
    RequireRejected([&] { fixture.retain(nullptr); }, "retain(nullptr)");
}};

const Case deleteClassic{"Locale_DeleteClassicLocale_ThrowsAndKeepsReferences", [] {
    const ClassicLocaleFixture fixture;
    auto* base = &fixture.locale->base;
    RequireRejected([&] { base->vtable->deleteObject(base); }, "deleteObject");
    RequireEqual(base->references, fixture.initial, "references");
}};

const Case concurrentReferences{"Locale_ConcurrentRetainRelease_KeepsReferences", [] {
    const ClassicLocaleFixture fixture;
    auto* base = &fixture.locale->base;
    std::vector<std::thread> threads;
    for (int thread = 0; thread < 4; ++thread) {
        threads.emplace_back([&] {
            for (int iteration = 0; iteration < 10000; ++iteration) {
                fixture.retain(base);
                fixture.release(base);
            }
        });
    }
    for (auto& thread : threads) thread.join();
    RequireEqual(base->references, fixture.initial, "references");
}};

const Case classicShape{"Locale_ClassicLocale_HasOneEmptyFacetSlotAndNameC", [] {
    const auto* locale = _ZSt21_sceLibcClassicLocale_nid_postfix;
    Require(locale != nullptr, "classic locale exported");
    RequireEqual(locale->facetCount, std::uint64_t{1}, "facet count");
    RequireEqual(locale->facets[0] == nullptr, true, "facet slot empty");
    RequireEqual(std::string_view(locale->name), std::string_view("C"), "name");
    RequireEqual(locale->transparent, false, "transparent");
}};

const Case locinfoAlignment{"Locinfo_ConstructC_ZeroesOnlyItsStorage", [] {
    alignas(16) std::array<std::byte, 80> bytes;
    bytes.fill(std::byte{0x5a});
    auto* storage = reinterpret_cast<GuestLocale::LocinfoStorage*>(bytes.data() + 8);
    _ZNSt8_LocinfoC1EPKc_nid_postfix(storage, "C");
    for (std::size_t index = 0; index < bytes.size(); ++index) {
        const auto expected = index >= 8 && index < 72 ? std::byte{} : std::byte{0x5a};
        RequireEqual(bytes[index], expected, "byte " + std::to_string(index));
    }
    _ZNSt8_LocinfoD1Ev_nid_postfix(storage);
}};

const Case locinfoUnknown{"Locinfo_UnknownLocale_Throws", [] {
    alignas(16) std::array<std::byte, 80> bytes{};
    auto* storage = reinterpret_cast<GuestLocale::LocinfoStorage*>(bytes.data() + 8);
    RequireRejected([&] { _ZNSt8_LocinfoC1EPKc_nid_postfix(storage, "unknown-locale"); }, "unknown-locale");
}};

const Case locinfoNull{"Locinfo_NullStorage_Throws", [] {
    RequireRejected([] { _ZNSt8_LocinfoC1EPKc_nid_postfix(nullptr, "C"); }, "null storage");
}};

const Case classification{"CharacterTables_Classification_MatchesCLocale", [] {
    const auto* table = _Getpctype_nid_postfix();
    struct Example {
        int character;
        short expected;
    };
    const Example examples[] = {
        {-1, 0}, {'A', 0x03}, {'a', 0x11}, {'G', 0x02}, {'0', 0x21}, {' ', 0x04}, {'\t', 0xc0}, {'\n', 0xc0}, {'!', 0x08},
    };
    for (const auto& example : examples) {
        RequireEqual(table[example.character], example.expected, "classification of " + std::to_string(example.character));
    }
    for (int value = 128; value < 256; ++value) {
        RequireEqual(table[value], short{0}, "classification of " + std::to_string(value));
    }
}};

const Case caseMaps{"CharacterTables_CaseMaps_MapOnlyAsciiLetters", [] {
    const auto* lower = _Getptolower_nid_postfix();
    const auto* upper = _Getptoupper_nid_postfix();
    RequireEqual(lower[-1], short{-1}, "tolower of EOF");
    RequireEqual(upper[-1], short{-1}, "toupper of EOF");
    for (int value = 0; value < 256; ++value) {
        RequireEqual(static_cast<int>(lower[value]), value >= 'A' && value <= 'Z' ? value + 32 : value, "tolower of " + std::to_string(value));
        RequireEqual(static_cast<int>(upper[value]), value >= 'a' && value <= 'z' ? value - 32 : value, "toupper of " + std::to_string(value));
    }
}};

const Case asciiRoundTrip{"CharacterConversions_Ascii_RoundTripsThroughSingleBytes", [] {
    const auto* classification = _Getpctype_nid_postfix();
    for (std::uint16_t value = 0; value < 128; ++value) {
        const std::string label = "character " + std::to_string(value);
        std::array<char, MB_LEN_MAX + 1> bytes{};
        bytes.fill('!');
        std::mbstate_t encodeState{};
        RequireEqual(_Wctombx_nid_postfix(bytes.data(), value, &encodeState), 1, label + " encoded length");
        RequireEqual(bytes[0], static_cast<char>(value), label + " encoded byte");
        RequireEqual(bytes[1], '!', label + " encoding stays bounded");
        std::array<std::uint16_t, 2> wide{0xffff, 0x1234};
        std::mbstate_t decodeState{};
        RequireEqual(_Mbtowcx_nid_postfix(wide.data(), bytes.data(), 2, &decodeState), value == 0 ? 0 : 1, label + " decoded length");
        RequireEqual(wide[0], value, label + " decoded value");
        RequireEqual(wide[1], std::uint16_t{0x1234}, label + " decoding stays bounded");
        const bool alnum = (value >= '0' && value <= '9') || (value >= 'A' && value <= 'Z') || (value >= 'a' && value <= 'z');
        RequireEqual((classification[static_cast<unsigned char>(bytes[0])] & 0x232) != 0, alnum, label + " alphanumeric class");
    }
}};

const Case conversionArguments{"CharacterConversions_InvalidArguments_Throw", [] {
    std::mbstate_t state{};
    char byte{};
    std::uint16_t wide{};
    RequireRejected([&] { _Mbtowcx_nid_postfix(&wide, "]", 0, &state); }, "mbtowc zero count");
    RequireRejected([&] { _Mbtowcx_nid_postfix(nullptr, "]", 1, &state); }, "mbtowc null destination");
    RequireRejected([&] { _Mbtowcx_nid_postfix(&wide, nullptr, 1, &state); }, "mbtowc null source");
    RequireRejected([&] { _Mbtowcx_nid_postfix(&wide, "]", 1, nullptr); }, "mbtowc null state");
    RequireRejected([&] { _Wctombx_nid_postfix(nullptr, ']', &state); }, "wctomb null destination");
    RequireRejected([&] { _Wctombx_nid_postfix(&byte, ']', nullptr); }, "wctomb null state");
}};

struct GuardedStream {
    std::uint64_t before = 0x12345678;
    GuestLocale::IosBase stream{};
    std::uint64_t after = 0x87654321;
};

const Case streamDestruction{"IosBaseDestructor_WithoutCallbacks_ClearsOnlyTheLocale", [] {
    GuardedStream object;
    object.stream.locale = &_ZSt21_sceLibcClassicLocale_nid_postfix;
    _ZNSt8ios_baseD2Ev_nid_postfix(&object.stream);
    RequireEqual(object.stream.locale == nullptr, true, "locale cleared");
    RequireEqual(object.before, std::uint64_t{0x12345678}, "guard before");
    RequireEqual(object.after, std::uint64_t{0x87654321}, "guard after");
}};

const Case streamCallbacks{"IosBaseDestructor_WithCallbacks_ThrowsAndKeepsLocale", [] {
    GuardedStream object;
    object.stream.locale = &_ZSt21_sceLibcClassicLocale_nid_postfix;
    object.stream.callbacks = &object;
    RequireRejected([&] { _ZNSt8ios_baseD2Ev_nid_postfix(&object.stream); }, "destructor with callbacks");
    RequireEqual(object.stream.locale == &_ZSt21_sceLibcClassicLocale_nid_postfix, true, "locale kept");
}};

const Case systemLock{"SystemLock_NestedLockAndUnlock_DoesNotDeadlock", [] {
    _Locksyslock_nid_postfix();
    _Locksyslock_nid_postfix();
    _Unlocksyslock_nid_postfix();
    _Unlocksyslock_nid_postfix();
}};

} // namespace
