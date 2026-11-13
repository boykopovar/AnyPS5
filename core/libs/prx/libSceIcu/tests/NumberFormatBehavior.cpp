#include "prx/libSceIcu/libSceIcu.hpp"
#include "prx/libSceIcu/tests/NumberFormatOracle.hpp"
#include "prx/libSceIcu/tests/RecoveredSignatureChecks.hpp"
#include "prx/libkernel/libkernel.hpp"
#include <algorithm>
#include <array>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <exception>
#include <limits>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include <vector>

namespace {

using IcuRecoveredTest::Require;
namespace Oracle = NumberFormatOracle;

static_assert(std::is_same_v<decltype(&unum_open_nid_postfix), UNumberFormat* (APS5_VABI*)(std::int32_t, const char16_t*, std::int32_t, const char*, UParseError*, std::int32_t*)>);
static_assert(std::is_same_v<decltype(&unum_clone_nid_postfix), UNumberFormat* (APS5_VABI*)(const UNumberFormat*, std::int32_t*)>);
static_assert(std::is_same_v<decltype(&unum_close_nid_postfix), void (APS5_VABI*)(UNumberFormat*)>);
static_assert(std::is_same_v<decltype(&unum_formatDouble_nid_postfix), std::int32_t (APS5_VABI*)(const UNumberFormat*, double, char16_t*, std::int32_t, UFieldPosition*, std::int32_t*)>);
static_assert(std::is_same_v<decltype(&unum_formatDecimal_nid_postfix), std::int32_t (APS5_VABI*)(const UNumberFormat*, const char*, std::int32_t, char16_t*, std::int32_t, UFieldPosition*, std::int32_t*)>);
static_assert(std::is_same_v<decltype(&unum_getSymbol_nid_postfix), std::int32_t (APS5_VABI*)(const UNumberFormat*, std::int32_t, char16_t*, std::int32_t, std::int32_t*)>);
static_assert(std::is_same_v<decltype(&unum_getTextAttribute_nid_postfix), std::int32_t (APS5_VABI*)(const UNumberFormat*, std::int32_t, char16_t*, std::int32_t, std::int32_t*)>);
static_assert(std::is_same_v<decltype(&unum_setAttribute_nid_postfix), void (APS5_VABI*)(UNumberFormat*, std::int32_t, std::int32_t)>);
static_assert(std::is_same_v<decltype(&unum_setTextAttribute_nid_postfix), void (APS5_VABI*)(UNumberFormat*, std::int32_t, const char16_t*, std::int32_t, std::int32_t*)>);

template<class T>
struct Guarded {
    std::uint32_t before = 0x1234a55a;
    T value;
    std::uint32_t after = 0x5678a55a;

    Guarded() { std::memset(&value, 0xa5, sizeof(value)); }
    void Check() const { Require(before == 0x1234a55a && after == 0x5678a55a, "wire record outside guards"); }
};

template<class Function>
void Refuses(Function&& function) {
    bool refused = false;
    try { function(); } catch (const std::exception&) { refused = true; }
    Require(refused, "invalid guest memory or formatter token is refused");
}

struct Pair {
    UNumberFormat* guest = nullptr;
    void* native = nullptr;

    Pair(std::int32_t style, const char* locale, const char16_t* pattern = nullptr, std::int32_t length = -1) {
        std::int32_t status = 0;
        std::int32_t expected = 0;
        native = Oracle::Open(style, pattern, length, locale, nullptr, &expected);
        Require(native && expected <= 0, "independent source-backed native formatter opens");
        guest = unum_open_nid_postfix(style, pattern, length, locale, nullptr, &status);
        Require(guest && status == expected, "mapped valid formatter matches native open/status");
    }
    ~Pair() { unum_close_nid_postfix(guest); Oracle::Close(native); }
};

std::int32_t GuestOutput(const UNumberFormat* handle, int operation, double number, const char* decimal,
                         std::int32_t length, int selector, char16_t* output, std::int32_t capacity,
                         Oracle::Field* field, std::int32_t* status) {
    if (operation == 0) return unum_formatDouble_nid_postfix(handle, number, output, capacity, reinterpret_cast<UFieldPosition*>(field), status);
    if (operation == 1) return unum_formatDecimal_nid_postfix(handle, decimal, length, output, capacity, reinterpret_cast<UFieldPosition*>(field), status);
    if (operation == 2) return unum_getSymbol_nid_postfix(handle, selector, output, capacity, status);
    return unum_getTextAttribute_nid_postfix(handle, selector, output, capacity, status);
}

std::int32_t NativeOutput(const void* handle, int operation, double number, const char* decimal,
                          std::int32_t length, int selector, char16_t* output, std::int32_t capacity,
                          Oracle::Field* field, std::int32_t* status) {
    if (operation == 0) return Oracle::Double(handle, number, output, capacity, field, status);
    if (operation == 1) return Oracle::Decimal(handle, decimal, length, output, capacity, field, status);
    if (operation == 2) return Oracle::Symbol(handle, selector, output, capacity, status);
    return Oracle::Text(handle, selector, output, capacity, status);
}

void Compare(const UNumberFormat* guest, const void* native, int operation, double number = 1234.5,
             const char* decimal = "1234.5", std::int32_t length = -1, int selector = 0, bool fields = true) {
    std::int32_t preflightStatus = 0;
    const auto required = NativeOutput(native, operation, number, decimal, length, selector, nullptr, 0, nullptr, &preflightStatus);
    Require(required < 4096, "bounded native fixture output");
    const auto fitted = std::max(required, 0);
    for (const auto capacity : {0, 1, 3, fitted, fitted + 1, fitted + 7}) {
        for (const auto incoming : {0, -128, -124, -1}) {
            for (bool nullOutput : {false, true}) {
                if (nullOutput && capacity != 0) continue;
                std::vector<char16_t> actual(static_cast<std::size_t>(capacity) + 4, 0xa55a);
                auto expected = actual;
                Guarded<Oracle::Field> actualField;
                actualField.value = {selector, -91, -92};
                auto expectedField = actualField;
                auto status = incoming;
                auto nativeStatus = incoming;
                const auto wanted = NativeOutput(native, operation, number, decimal, length, selector,
                    nullOutput ? nullptr : expected.data() + 2, capacity, fields ? &expectedField.value : nullptr, &nativeStatus);
                const auto count = GuestOutput(guest, operation, number, decimal, length, selector,
                    nullOutput ? nullptr : actual.data() + 2, capacity, fields ? &actualField.value : nullptr, &status);
                if (count != wanted || status != nativeStatus || actual != expected)
                    std::fprintf(stderr, "number differential op=%d cap=%d incoming=%d result=%d/%d status=%d/%d\n",
                        operation, capacity, incoming, count, wanted, status, nativeStatus);
                Require(count == wanted && status == nativeStatus && actual == expected,
                    "native count/status and complete short/exact/full buffer writes match");
                Require(actual[0] == 0xa55a && actual[1] == 0xa55a && actual[capacity + 2] == 0xa55a && actual[capacity + 3] == 0xa55a,
                    "number output never crosses caller capacity");
                Require(std::memcmp(&actualField.value, &expectedField.value, sizeof(Oracle::Field)) == 0, "field indexes match native including warnings/overflow");
                actualField.Check();
                expectedField.Check();
            }
        }
    }
}

void Literal(UNumberFormat* guest, double number, std::u16string_view expected) {
    std::array<char16_t, 128> output;
    output.fill(0xa55a);
    std::int32_t status = 0;
    const auto count = unum_formatDouble_nid_postfix(guest, number, output.data() + 1, 126, nullptr, &status);
    Require(count == static_cast<std::int32_t>(expected.size()) && status <= 0 &&
        std::u16string_view(output.data() + 1, count) == expected && output[count + 1] == 0 &&
        output[0] == 0xa55a && output[count + 2] == 0xa55a, "independent literal, terminator and tail");
}

void Open() {
    std::fputs("classic number fixture: valid decimal unum_open\n", stderr);
    Pair decimal(1, "en_US");
    Compare(decimal.guest, decimal.native, 0);
    for (const auto* locale : {"root", "de_DE", "fr_FR", "ar_EG", "hi_IN", "ja_JP", "en_US@numbers=hanidec", "zz_ZZ"}) {
        Pair handle(1, locale);
        Compare(handle.guest, handle.native, 0);
    }
    for (const auto style : {2, 3, 4, 5, 6, 7, 8, 10, 11, 12, 14, 15, 16}) {
        Pair handle(style, "en_US");
        Compare(handle.guest, handle.native, 0, 42.5);
    }
    for (const auto* pattern : {u"#,##0.00", u"0.0.0", u"0;0;0", u"'prefix' 0.000;('neg'0.000)"}) {
        for (const auto incoming : {0, -128, -124}) {
            Guarded<Oracle::Parse> actual;
            auto expected = actual;
            auto status = incoming;
            auto nativeStatus = incoming;
            auto* native = Oracle::Open(0, pattern, -1, "en_US", &expected.value, &nativeStatus);
            auto* guest = unum_open_nid_postfix(0, pattern, -1, "en_US", reinterpret_cast<UParseError*>(&actual.value), &status);
            Require(bool(guest) == bool(native) && status == nativeStatus, "pattern parser success and diagnostics status match native");
            Require(std::memcmp(&actual.value, &expected.value, sizeof(Oracle::Parse)) == 0, "complete parse record preserves native modifications only");
            actual.Check();
            expected.Check();
            if (guest) Compare(guest, native, 0);
            unum_close_nid_postfix(guest);
            Oracle::Close(native);
        }
    }
    for (const auto style : {0, 9}) {
        const auto* pattern = style == 9 ? u"%main: 0: =0=;" : u"0.00";
        for (const auto length : {-1, -2, INT32_MIN}) {
            Guarded<Oracle::Parse> actual;
            auto expected = actual;
            std::int32_t status = 0;
            std::int32_t nativeStatus = 0;
            auto* native = Oracle::Open(style, pattern, length, "en_US", &expected.value, &nativeStatus);
            auto* guest = unum_open_nid_postfix(style, pattern, length, "en_US", reinterpret_cast<UParseError*>(&actual.value), &status);
            Require(guest && native && status == nativeStatus, "all negative pattern lengths preserve native NUL semantics");
            Require(std::memcmp(&actual.value, &expected.value, sizeof(actual.value)) == 0, "decimal untouched parse record and rule-based zeroing match native");
            Compare(guest, native, 0, 42.5);
            unum_close_nid_postfix(guest);
            Oracle::Close(native);
            actual.Check();
        }
    }
    const std::u16string pattern{u'0', u'.', u'0', 0, u'X'};
    Guarded<Oracle::Parse> actual;
    auto expected = actual;
    std::int32_t status = 0;
    std::int32_t nativeStatus = 0;
    auto* native = Oracle::Open(0, pattern.data(), pattern.size(), "en_US", &expected.value, &nativeStatus);
    auto* guest = unum_open_nid_postfix(0, pattern.data(), pattern.size(), "en_US", reinterpret_cast<UParseError*>(&actual.value), &status);
    Require(bool(guest) == bool(native) && status == nativeStatus && std::memcmp(&actual.value, &expected.value, sizeof(Oracle::Parse)) == 0,
        "explicit pattern length retains embedded NUL and suffix");
    actual.Check();
    unum_close_nid_postfix(guest);
    Oracle::Close(native);
}

void Format() {
    std::fputs("classic number fixture: valid double/decimal formatting\n", stderr);
    for (const auto* locale : {"en_US", "de_DE", "ar_EG", "hi_IN"}) {
        for (const auto style : {1, 2, 3, 4}) {
            Pair handle(style, locale);
            for (double value : {0.0, -0.0, 1234.5678, -98.25, std::numeric_limits<double>::infinity(),
                     -std::numeric_limits<double>::infinity(), std::numeric_limits<double>::quiet_NaN()}) {
                for (int field : {0, 1, 2, 4, 7}) Compare(handle.guest, handle.native, 0, value, nullptr, 0, field);
            }
            for (const auto* value : {"123456789012345678901234567890.123456789012345", "-0", "1.234567890123456789E+29", "not-a-decimal"}) {
                Compare(handle.guest, handle.native, 1, 0, value);
                Compare(handle.guest, handle.native, 1, 0, value, static_cast<std::int32_t>(std::strlen(value)), 1, false);
            }
            Compare(handle.guest, handle.native, 1, 0, "123.25", -7);
            const std::array<char, 7> embedded{'1', '2', 0, '9', '.', '5', 0};
            Compare(handle.guest, handle.native, 1, 0, embedded.data(), 6);
        }
    }
    Pair fixed(0, "en_US", u"#,##0.00");
    Literal(fixed.guest, -1234.5, u"-1,234.50");
    Pair precision(0, "en_US", u"0.##############################");
    constexpr const char* exact = "123456789012345678901234567890.123456789012345";
    constexpr std::u16string_view wanted = u"123456789012345678901234567890.123456789012345";
    std::array<char16_t, 96> output{};
    std::int32_t status = 0;
    const auto count = unum_formatDecimal_nid_postfix(precision.guest, exact, -1, output.data(), output.size(), nullptr, &status);
    Require(status <= 0 && count == static_cast<std::int32_t>(wanted.size()) && std::u16string_view(output.data(), count) == wanted,
        "decimal formatting preserves precision beyond binary double");
}

void Attributes() {
    std::fputs("classic number fixture: valid attributes\n", stderr);
    Pair handle(1, "de_DE");
    for (const auto symbol : {0, 1, 3, 6, 8, 27}) Compare(handle.guest, handle.native, 2, 0, nullptr, 0, symbol);
    for (const auto attribute : {0, 1, 2, 3, 4, 5}) Compare(handle.guest, handle.native, 3, 0, nullptr, 0, attribute);
    for (const auto [attribute, value] : {std::pair{1, 0}, std::pair{6, 4}, std::pair{7, 4}, std::pair{9, 10}, std::pair{11, 1}}) {
        unum_setAttribute_nid_postfix(handle.guest, attribute, value);
        Oracle::SetAttribute(handle.native, attribute, value);
        Compare(handle.guest, handle.native, 0, -1234.56789);
    }
    for (const auto attribute : {0, 1, 2, 3, 5}) {
        const auto* text = attribute == 5 ? u"JPY" : u"prefix/后缀";
        std::int32_t status = -128;
        std::int32_t expected = -128;
        unum_setTextAttribute_nid_postfix(handle.guest, attribute, text, -1, &status);
        Oracle::SetText(handle.native, attribute, text, -1, &expected);
        Require(status == expected, "text setter preserves native warning/error behavior");
        Compare(handle.guest, handle.native, 3, 0, nullptr, 0, attribute);
        Compare(handle.guest, handle.native, 0, -12.75);
    }
    std::u16string prefix(1025, u'P');
    prefix[512] = 0;
    prefix.back() = u'Z';
    std::int32_t status = 0;
    std::int32_t expected = 0;
    unum_setTextAttribute_nid_postfix(handle.guest, 0, prefix.data(), prefix.size(), &status);
    Oracle::SetText(handle.native, 0, prefix.data(), prefix.size(), &expected);
    Require(status == expected, "multi-chunk explicit text setter retains embedded NUL suffix");
    Compare(handle.guest, handle.native, 3, 0, nullptr, 0, 0);
    Compare(handle.guest, handle.native, 0, 1.5);
    std::array<char16_t, 8> buffer{};
    status = 0;
    expected = 0;
    Require(Oracle::Symbol(handle.native, 28, buffer.data(), buffer.size(), &expected) > 0 && expected <= 0,
        "ICU78 native approximately-sign symbol exists");
    Require(unum_getSymbol_nid_postfix(handle.guest, 28, buffer.data(), buffer.size(), &status) == 0 && status == 1,
        "inherited guest symbol upper bound remains explicit compatibility difference");
    buffer.fill(0xa55a);
    const auto unchanged = buffer;
    Require(unum_getSymbol_nid_postfix(handle.guest, 0, buffer.data(), buffer.size(), nullptr) == 0 && buffer == unchanged,
        "inherited null symbol status returns without writing output");
}

void Lifetime() {
    std::fputs("classic number fixture: valid clone and independent state\n", stderr);
    Pair handle(0, "en_US", u"0.00");
    std::int32_t status = 0;
    std::int32_t expected = 0;
    auto* clone = unum_clone_nid_postfix(handle.guest, &status);
    auto* native = Oracle::Clone(handle.native, &expected);
    Require(clone && native && clone != handle.guest && status == expected, "clone owns a distinct formatter token");
    unum_setAttribute_nid_postfix(clone, 8, 3);
    Oracle::SetAttribute(native, 8, 3);
    unum_setTextAttribute_nid_postfix(clone, 0, u"clone:", -1, &status);
    Oracle::SetText(native, 0, u"clone:", -1, &expected);
    Require(status == expected, "clone text state follows native");
    Literal(handle.guest, 12.5, u"12.50");
    Literal(clone, 12.5, u"clone:12.500");
    Compare(clone, native, 0, 12.5);
    auto* closed = handle.guest;
    unum_close_nid_postfix(handle.guest);
    handle.guest = nullptr;
    Oracle::Close(handle.native);
    handle.native = nullptr;
    Compare(clone, native, 0, -12.5);
    for (int index = 0; index < 32; ++index) {
        Pair fresh(1, "en_US");
        Require(fresh.guest != closed, "stale formatter identity is not recycled");
        std::int32_t local = 0;
        Refuses([&] { unum_clone_nid_postfix(closed, &local); });
    }
    Refuses([&] { unum_close_nid_postfix(closed); });
    Refuses([&] { unum_setAttribute_nid_postfix(closed, 1, 1); });
    std::array<char16_t, 32> output{};
    Refuses([&] { unum_formatDouble_nid_postfix(closed, 1.0, output.data(), output.size(), nullptr, &status); });
    status = 0;
    auto* foreign = unumsys_openByName_nid_postfix("latn", &status);
    Require(foreign && status <= 0, "independent numbering token for cross-kind refusal");
    Refuses([&] { unum_clone_nid_postfix(reinterpret_cast<UNumberFormat*>(foreign), &status); });
    Refuses([&] { unum_close_nid_postfix(reinterpret_cast<UNumberFormat*>(foreign)); });
    unumsys_close_nid_postfix(foreign);
    unum_close_nid_postfix(clone);
    Oracle::Close(native);
    unum_close_nid_postfix(nullptr);
}

void Memory() {
    constexpr std::size_t page = 0x4000;
    constexpr std::size_t bytes = 3 * page;
    std::int64_t physical = 0;
    Require(sceKernelAllocateDirectMemory(0, 0x7fffffffffll, bytes, 0, 0, &physical) == 0, "allocate formatter guest fixture");
    void* address = nullptr;
    Require(sceKernelMapDirectMemory(&address, bytes, 3, 0, physical, 0) == 0, "map formatter guest fixture");
    auto* data = static_cast<std::uint8_t*>(address);
    std::fill_n(data, bytes, 0xa5);
    Require(sceKernelMprotect(data, page, 0) == 0 && sceKernelMprotect(data + 2 * page, page, 0) == 0, "formatter inaccessible guard pages");
    auto* status = reinterpret_cast<std::int32_t*>(data + page);
    auto* badStatus = reinterpret_cast<std::int32_t*>(data);
    auto* badText = reinterpret_cast<char16_t*>(data);
    auto* badFormat = reinterpret_cast<UNumberFormat*>(std::uintptr_t{1});
    *status = 7;
    Require(sceKernelMprotect(data + page, page, 1) == 0, "read-only prefailed status page");
    Require(unum_open_nid_postfix(0, badText, INT32_MAX, reinterpret_cast<char*>(data), reinterpret_cast<UParseError*>(data), status) == nullptr, "prefailed open touches no pattern/locale/parse");
    Require(unum_clone_nid_postfix(badFormat, status) == nullptr, "prefailed clone does not read handle");
    Require(unum_formatDouble_nid_postfix(badFormat, 1, badText, INT32_MAX, reinterpret_cast<UFieldPosition*>(data), status) == -1, "prefailed double preserves all inaccessible optional arguments");
    Require(unum_formatDecimal_nid_postfix(badFormat, reinterpret_cast<char*>(data), INT32_MAX, badText, INT32_MAX, reinterpret_cast<UFieldPosition*>(data), status) == -1, "prefailed decimal before inaccessible input/output");
    Require(unum_getTextAttribute_nid_postfix(badFormat, 0, badText, INT32_MAX, status) == -1, "prefailed text getter before handle/output");
    Require(unum_getSymbol_nid_postfix(badFormat, 0, badText, INT32_MAX, status) == 0, "prefailed symbol before handle/output");
    unum_setTextAttribute_nid_postfix(badFormat, 0, badText, INT32_MAX, status);
    Require(*status == 7, "all prefailed operations leave read-only status intact");
    Require(sceKernelMprotect(data + page, page, 3) == 0, "restore formatter status page");
    *status = 0;
    auto* locale = reinterpret_cast<char*>(data + 2 * page - 6);
    std::memcpy(locale, "en_US", 6);
    auto* handle = unum_open_nid_postfix(1, nullptr, -1, locale, nullptr, status);
    Require(handle && *status <= 0, "mapped locale with NUL exactly at page end opens");
    *status = 0;
    auto* ignored = unum_open_nid_postfix(1, badText, INT32_MAX, "en_US", reinterpret_cast<UParseError*>(data), status);
    Require(ignored && *status <= 0, "nonpattern style ignores inaccessible pattern and parse record");
    unum_close_nid_postfix(ignored);
    auto* output = reinterpret_cast<char16_t*>(data + page + 512);
    auto* field = reinterpret_cast<Oracle::Field*>(data + page + 256);
    *field = {0, -1, -1};
    *status = 0;
    Require(unum_formatDouble_nid_postfix(handle, 1234.5, output + 1, 16, reinterpret_cast<UFieldPosition*>(field), status) == 7 && *status <= 0,
        "mapped output and field record are usable");
    Require(std::u16string_view(output + 1, 7) == u"1,234.5" && output[0] == 0xa5a5 && output[8] == 0 && output[9] == 0xa5a5,
        "mapped number output literal, terminator and tail guards");
    Require(field->field == 0 && field->begin == 0 && field->end == 5, "mapped integer field includes grouping separator");
    *status = 0;
    Require(unum_formatDouble_nid_postfix(handle, 1234.5, output + 1, INT32_MAX, nullptr, status) == 7 && *status <= 0,
        "large declared output capacity is bounded by actual formatted output");
    const auto refuses = [&](auto&& function) {
        *status = 0;
        Refuses(function);
    };
    refuses([&] { unum_open_nid_postfix(1, nullptr, -1, reinterpret_cast<char*>(data), nullptr, status); });
    refuses([&] { unum_open_nid_postfix(0, badText, 4, "en_US", nullptr, status); });
    std::int32_t parseStatus = 0;
    Oracle::Parse parseRecord;
    std::memset(&parseRecord, 0xa5, sizeof(parseRecord));
    auto* parseNative = Oracle::Open(0, u"0.0.0", -1, "en_US", &parseRecord, &parseStatus);
    *status = 0;
    auto* parseGuest = unum_open_nid_postfix(0, u"0.0.0", -1, "en_US", reinterpret_cast<UParseError*>(data), status);
    Require(bool(parseGuest) == bool(parseNative) && *status == parseStatus, "decimal parse record remains unused even for invalid pattern");
    if (parseGuest) unum_close_nid_postfix(parseGuest);
    if (parseNative) Oracle::Close(parseNative);
    refuses([&] { unum_clone_nid_postfix(handle, badStatus); });
    refuses([&] { unum_formatDouble_nid_postfix(handle, 1, output, 16, nullptr, badStatus); });
    refuses([&] { unum_formatDouble_nid_postfix(handle, 1, badText, 16, nullptr, status); });
    refuses([&] { unum_formatDouble_nid_postfix(handle, 1, output, 16, reinterpret_cast<UFieldPosition*>(data), status); });
    refuses([&] { unum_formatDecimal_nid_postfix(handle, reinterpret_cast<char*>(data), 4, output, 16, nullptr, status); });
    refuses([&] { unum_formatDecimal_nid_postfix(handle, "1.25", -1, output, 16, nullptr, badStatus); });
    refuses([&] { unum_getSymbol_nid_postfix(handle, 0, badText, 4, status); });
    refuses([&] { unum_getTextAttribute_nid_postfix(handle, 0, output, 16, badStatus); });
    refuses([&] { unum_setTextAttribute_nid_postfix(handle, 0, badText, 4, status); });
    refuses([&] { unum_setTextAttribute_nid_postfix(handle, 0, u"prefix", -1, badStatus); });
    auto* unterminated = reinterpret_cast<char*>(data + 2 * page - 1);
    *unterminated = '1';
    refuses([&] { unum_formatDecimal_nid_postfix(handle, unterminated, -1, output, 16, nullptr, status); });
    *status = 0;
    Require(sceKernelMprotect(data + page, page, 1) == 0, "read-only formatter destination page");
    Refuses([&] { unum_formatDouble_nid_postfix(handle, 1, output, 16, nullptr, status); });
    Require(sceKernelMprotect(data + page, page, 3) == 0, "restore formatter mapping before cleanup");
    unum_close_nid_postfix(handle);
    Require(sceKernelMunmap(reinterpret_cast<std::uint64_t>(data), bytes) == 0, "unmap formatter fixture");
    Require(sceKernelReleaseDirectMemory(physical, bytes) == 0, "release formatter fixture memory");
}

}

int main(int argc, char** argv) {
    try {
        const std::string_view mode = argc > 1 ? argv[1] : "open";
        if (mode == "open") Open();
        else if (mode == "format") Format();
        else if (mode == "attributes") Attributes();
        else if (mode == "lifetime") Lifetime();
        else if (mode == "memory") Memory();
        else Require(false, "known classic-number test mode");
        return 0;
    } catch (const std::exception& error) {
        std::fprintf(stderr, "classic number fixture failure: %s\n", error.what());
        return 1;
    }
}
