#include "prx/libSceIcu/libSceIcu.hpp"
#include "prx/libSceIcu/tests/RecoveredSignatureChecks.hpp"
#include "prx/libc/include/General.hpp"
#include "prx/libkernel/include/Api.hpp"
#include "unicode/uidna.h"
#include "unicode/unistr.h"
#include <algorithm>
#include <atomic>
#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <exception>
#include <fstream>
#include <limits>
#include <sstream>
#include <string>
#include <thread>
#include <vector>
#include <source_location>
#include <stdexcept>
#include <string_view>
#include <type_traits>

namespace UidnaClose {

using IcuRecoveredTest::Require;

static_assert(std::is_same_v<decltype(&uidna_close_nid_postfix), void (APS5_VABI*)(UIDNA*)>);

namespace {

void CloseNull() {
    const auto before = NotImplementedCalls_nid_no_patch();
    uidna_close_nid_postfix(nullptr);
    uidna_close_nid_postfix(nullptr);
    Require(NotImplementedCalls_nid_no_patch() == before, "null close does not call NotImplemented");
}

void CloseUnregisteredThrows() {
    alignas(8) std::array<std::uint8_t, 64> storage;
    storage.fill(0xa5);
    const auto expected = storage;
    for (UIDNA* candidate : {
             reinterpret_cast<UIDNA*>(storage.data()),
             reinterpret_cast<UIDNA*>(std::uintptr_t{1}),
         }) {
        const auto before = NotImplementedCalls_nid_no_patch();
        bool threw = false;
        try {
            uidna_close_nid_postfix(candidate);
        } catch (const std::runtime_error&) {
            threw = true;
        }
        Require(threw, "unregistered close throws NotImplemented");
        Require(NotImplementedCalls_nid_no_patch() == before + 1, "unregistered close records NotImplemented");
        Require(storage == expected, "unregistered close does not mutate caller memory");
    }
}

}

void Run() {
    CloseNull();
    CloseUnregisteredThrows();
}

} // namespace UidnaClose

namespace UidnaNameToASCII {

using IcuRecoveredTest::Require;
using IcuRecoveredTest::RequireThrows;

static_assert(std::is_same_v<decltype(&uidna_nameToASCII_nid_postfix), std::int32_t (APS5_VABI*)(const UIDNA*, const char16_t*, std::int32_t, char16_t*, std::int32_t, UIDNAInfo*, std::int32_t*)>);

namespace {

constexpr std::int32_t IllegalArgument = 1;

constexpr std::uint32_t FillerWord = 0xa5a5a5a5;

std::array<std::uint32_t, 4> Filler() {
    std::array<std::uint32_t, 4> storage{};
    storage.fill(FillerWord);
    return storage;
}

bool Unchanged(const std::array<std::uint32_t, 4>& storage) {
    for (const auto word : storage) {
        if (word != FillerWord) return false;
    }
    return true;
}

} // namespace

void Run() {
    const std::array<std::int32_t, 7> failing{1, 2, 7, 15, 16, 27, 0x7fffffff};
    const std::array<std::int32_t, 4> passing{0, -128, -124, -1};
    auto idnaStorage = Filler();
    const auto* idna = reinterpret_cast<const UIDNA*>(idnaStorage.data());
    std::array<char16_t, 4> name{u'a', u'b', u'c', 0};
    std::array<char16_t, 4> dest{u'x', u'y', u'z', 0};
    std::array<std::int16_t, 8> info{};
    info[0] = 16;
    auto* pInfo = reinterpret_cast<UIDNAInfo*>(info.data());
    const auto probe = info;
    const auto probeName = name;
    const auto probeDest = dest;

    for (const auto incoming : failing) {
        std::int32_t status = incoming;
        Require(uidna_nameToASCII_nid_postfix(idna, nullptr, 5, dest.data(), -1, nullptr, &status) == 0);
        Require(status == incoming);
        Require(name == probeName && dest == probeDest);
        Require(info == probe);
    }

    for (const auto incoming : passing) {
        std::int32_t status = incoming;
        Require(uidna_nameToASCII_nid_postfix(idna, name.data(), 3, dest.data(), 4, nullptr, &status) == 0);
        Require(status == IllegalArgument);

        status = incoming;
        std::array<std::int16_t, 8> small{};
        small[0] = 15;
        Require(uidna_nameToASCII_nid_postfix(idna, name.data(), 3, dest.data(), 4, reinterpret_cast<UIDNAInfo*>(small.data()), &status) == 0);
        Require(status == IllegalArgument);
        Require(small[0] == 15 && small[1] == 0);

        status = incoming;
        Require(uidna_nameToASCII_nid_postfix(idna, nullptr, 3, dest.data(), 4, pInfo, &status) == 0);
        Require(status == IllegalArgument);

        status = incoming;
        Require(uidna_nameToASCII_nid_postfix(idna, name.data(), -2, dest.data(), 4, pInfo, &status) == 0);
        Require(status == IllegalArgument);

        status = incoming;
        Require(uidna_nameToASCII_nid_postfix(idna, name.data(), 3, nullptr, 4, pInfo, &status) == 0);
        Require(status == IllegalArgument);

        status = incoming;
        Require(uidna_nameToASCII_nid_postfix(idna, name.data(), 3, dest.data(), -1, pInfo, &status) == 0);
        Require(status == IllegalArgument);

        status = incoming;
        Require(uidna_nameToASCII_nid_postfix(idna, name.data(), 3, name.data(), 3, pInfo, &status) == 0);
        Require(status == IllegalArgument);
        Require(name == probeName && dest == probeDest);
        Require(info == probe);
    }

    for (const auto incoming : passing) {
        std::int32_t status = incoming;
        RequireThrows([&] { uidna_nameToASCII_nid_postfix(idna, name.data(), -1, dest.data(), 0, pInfo, &status); });
        Require(status == incoming);
        Require(name == probeName && dest == probeDest);

        status = incoming;
        RequireThrows([&] { uidna_nameToASCII_nid_postfix(idna, nullptr, 0, nullptr, 0, pInfo, &status); });
        Require(status == incoming);
        Require(Unchanged(idnaStorage));
    }

    RequireThrows([&] { uidna_nameToASCII_nid_postfix(idna, name.data(), 3, dest.data(), 4, pInfo, nullptr); });
    Require(name == probeName && dest == probeDest);
}

} // namespace UidnaNameToASCII

namespace UidnaNameToUnicode {

using IcuRecoveredTest::Require;
using IcuRecoveredTest::RequireThrows;

static_assert(std::is_same_v<decltype(&uidna_nameToUnicode_nid_postfix), std::int32_t (APS5_VABI*)(const UIDNA*, const char16_t*, std::int32_t, char16_t*, std::int32_t, UIDNAInfo*, std::int32_t*)>);

namespace {

constexpr std::int32_t IllegalArgument = 1;

constexpr std::uint32_t FillerWord = 0xa5a5a5a5;

std::array<std::uint32_t, 4> Filler() {
    std::array<std::uint32_t, 4> storage{};
    storage.fill(FillerWord);
    return storage;
}

bool Unchanged(const std::array<std::uint32_t, 4>& storage) {
    for (const auto word : storage) {
        if (word != FillerWord) return false;
    }
    return true;
}

} // namespace

void Run() {
    const std::array<std::int32_t, 7> failing{1, 2, 7, 15, 16, 27, 0x7fffffff};
    const std::array<std::int32_t, 4> passing{0, -128, -124, -1};
    auto idnaStorage = Filler();
    const auto* idna = reinterpret_cast<const UIDNA*>(idnaStorage.data());
    std::array<char16_t, 4> name{u'a', u'b', u'c', 0};
    std::array<char16_t, 4> dest{u'x', u'y', u'z', 0};
    std::array<std::int16_t, 8> info{};
    info[0] = 16;
    auto* pInfo = reinterpret_cast<UIDNAInfo*>(info.data());
    const auto probe = info;
    const auto probeName = name;
    const auto probeDest = dest;

    for (const auto incoming : failing) {
        std::int32_t status = incoming;
        Require(uidna_nameToUnicode_nid_postfix(idna, nullptr, 5, dest.data(), -1, nullptr, &status) == 0);
        Require(status == incoming);
        Require(name == probeName && dest == probeDest);
        Require(info == probe);
    }

    for (const auto incoming : passing) {
        std::int32_t status = incoming;
        Require(uidna_nameToUnicode_nid_postfix(idna, name.data(), 3, dest.data(), 4, nullptr, &status) == 0);
        Require(status == IllegalArgument);

        status = incoming;
        std::array<std::int16_t, 8> small{};
        small[0] = 15;
        Require(uidna_nameToUnicode_nid_postfix(idna, name.data(), 3, dest.data(), 4, reinterpret_cast<UIDNAInfo*>(small.data()), &status) == 0);
        Require(status == IllegalArgument);
        Require(small[0] == 15 && small[1] == 0);

        status = incoming;
        Require(uidna_nameToUnicode_nid_postfix(idna, nullptr, 3, dest.data(), 4, pInfo, &status) == 0);
        Require(status == IllegalArgument);

        status = incoming;
        Require(uidna_nameToUnicode_nid_postfix(idna, name.data(), -2, dest.data(), 4, pInfo, &status) == 0);
        Require(status == IllegalArgument);

        status = incoming;
        Require(uidna_nameToUnicode_nid_postfix(idna, name.data(), 3, nullptr, 4, pInfo, &status) == 0);
        Require(status == IllegalArgument);

        status = incoming;
        Require(uidna_nameToUnicode_nid_postfix(idna, name.data(), 3, dest.data(), -1, pInfo, &status) == 0);
        Require(status == IllegalArgument);

        status = incoming;
        Require(uidna_nameToUnicode_nid_postfix(idna, name.data(), 3, name.data(), 3, pInfo, &status) == 0);
        Require(status == IllegalArgument);
        Require(name == probeName && dest == probeDest);
        Require(info == probe);
    }

    for (const auto incoming : passing) {
        std::int32_t status = incoming;
        RequireThrows([&] { uidna_nameToUnicode_nid_postfix(idna, name.data(), -1, dest.data(), 0, pInfo, &status); });
        Require(status == incoming);
        Require(name == probeName && dest == probeDest);

        status = incoming;
        RequireThrows([&] { uidna_nameToUnicode_nid_postfix(idna, nullptr, 0, nullptr, 0, pInfo, &status); });
        Require(status == incoming);
        Require(Unchanged(idnaStorage));
    }

    RequireThrows([&] { uidna_nameToUnicode_nid_postfix(idna, name.data(), 3, dest.data(), 4, pInfo, nullptr); });
    Require(name == probeName && dest == probeDest);
}

} // namespace UidnaNameToUnicode

namespace UidnaOpenUTS46 {

using IcuRecoveredTest::Require;
using IcuRecoveredTest::RequireThrows;

static_assert(std::is_same_v<decltype(&uidna_openUTS46_nid_postfix), UIDNA* (APS5_VABI*)(std::uint32_t, std::int32_t*)>);

void Run() {
    const std::array<std::int32_t, 7> failing{1, 2, 7, 15, 16, 27, 0x7fffffff};
    const std::array<std::int32_t, 4> passing{0, -128, -124, -1};
    for (const auto incoming : failing) {
        std::int32_t status = incoming;
        Require(uidna_openUTS46_nid_postfix(0, &status) == nullptr);
        Require(status == incoming);

        status = incoming;
        Require(uidna_openUTS46_nid_postfix(0xffffffffu, &status) == nullptr);
        Require(status == incoming);
    }

    for (const auto incoming : passing) {
        std::int32_t status = incoming;
        auto* idna = uidna_openUTS46_nid_postfix(0, &status);
        Require(idna != nullptr && status == incoming);
        uidna_close_nid_postfix(idna);

        status = incoming;
        idna = uidna_openUTS46_nid_postfix(0xffffffffu, &status);
        Require(idna != nullptr && status == incoming);
        uidna_close_nid_postfix(idna);
    }

    RequireThrows([&] { uidna_openUTS46_nid_postfix(0, nullptr); });
}

} // namespace UidnaOpenUTS46

void CheckIcuIdna() {
    UidnaClose::Run();
    UidnaNameToASCII::Run();
    UidnaNameToUnicode::Run();
    UidnaOpenUTS46::Run();
}

void CheckIcuIdnaUts46() {
    using IcuRecoveredTest::Require;
    const auto before = NotImplementedCalls_nid_no_patch();
    for (int iteration = 0; iteration < 2; ++iteration) {
        std::int32_t status = 0;
        auto* idna = uidna_openUTS46_nid_postfix(0, &status);
        Require(idna != nullptr && status == 0, "UTS46 opens an owned handle");

        const auto convert = [&](auto function, const char16_t* name, std::int32_t length,
                                 std::u16string_view expected) {
            alignas(4) std::array<std::uint8_t, 24> info;
            info.fill(0xa5);
            const std::int16_t infoSize = 16;
            std::memcpy(info.data() + 4, &infoSize, sizeof(infoSize));
            std::array<char16_t, 24> output;
            output.fill(0xa55a);
            status = 0;
            const auto count = function(idna, name, length, output.data() + 2, 20,
                                        reinterpret_cast<UIDNAInfo*>(info.data() + 4), &status);
            Require(count == static_cast<std::int32_t>(expected.size()), "UTS46 returns the mapped length");
            Require(status == 0, "UTS46 succeeds for a valid mapped domain");
            for (std::size_t i = 0; i < output.size(); ++i) {
                const auto wanted = i >= 2 && i < expected.size() + 2 ? expected[i - 2] :
                                    i == expected.size() + 2 ? char16_t{} : char16_t{0xa55a};
                Require(output[i] == wanted, "UTS46 writes the mapped domain and terminator within output guards");
            }
            for (std::size_t i = 0; i < info.size(); ++i) {
                const auto wanted = i == 4 ? std::uint8_t{16} :
                                    i >= 5 && i < 20 ? std::uint8_t{} : std::uint8_t{0xa5};
                Require(info[i] == wanted, "UTS46 clears valid-domain info fields without crossing its guards");
            }
        };

        convert(uidna_nameToASCII_nid_postfix, u"Bücher.de", 9, u"xn--bcher-kva.de");
        convert(uidna_nameToUnicode_nid_postfix, u"xn--bcher-kva.de", -1, u"bücher.de");
        uidna_close_nid_postfix(idna);
    }
    uidna_close_nid_postfix(nullptr);
    Require(NotImplementedCalls_nid_no_patch() == before, "UTS46 lifecycle and conversions are implemented");
}

namespace UidnaBehavior {

using IcuRecoveredTest::Require;
using IcuRecoveredTest::RequireThrows;
using GuestConvert = decltype(&uidna_nameToASCII_nid_postfix);
using NativeConvert = decltype(&uidna_nameToASCII);

struct Handle {
    UIDNA* value;

    explicit Handle(std::uint32_t options) {
        std::int32_t status = 0;
        value = uidna_openUTS46_nid_postfix(options, &status);
        Require(value && status == 0, "open UTS46 test handle");
    }

    ~Handle() { uidna_close_nid_postfix(value); }
};

struct Info {
    std::uint32_t before = 0xa5a5a5a5;
    UIDNAInfo value = UIDNA_INFO_INITIALIZER;
    std::array<std::uint8_t, 8> extension{};
    std::uint32_t after = 0xa5a5a5a5;

    explicit Info(bool extended = false) {
        std::memset(&value, 0xa5, sizeof(value));
        value.size = extended ? 24 : 16;
        extension.fill(0xa5);
    }

    void Check() const {
        Require(before == 0xa5a5a5a5 && after == 0xa5a5a5a5, "info preserves outside guards");
        Require(value.reservedB3 == 0 && value.reservedI2 == 0 && value.reservedI3 == 0, "info reserved fields cleared");
        for (auto byte : extension) Require(byte == (value.size == 24 ? 0 : 0xa5), "info honors declared extension extent");
    }
};

void Buffers() {
    for (const auto options : {0u, 2u, 4u, 8u, 0x10u, 0x20u, 0x40u, 0x7eu, 0xffffffffu}) {
        Handle guest(options);
        UErrorCode nativeStatus = U_ZERO_ERROR;
        auto* native = uidna_openUTS46(options, &nativeStatus);
        Require(native && nativeStatus == 0, "pinned native handle opens");
        const std::array<std::u16string, 10> inputs{
            u"Bücher.de", u"xn--bcher-kva.de", u"faß.de", u"bu\u0308cher.de", u"a..b", u"a.שלום",
            std::u16string{u'a', 0, u'b'}, std::u16string{0xd800, u'.', u'x'}, u"a\u200cb", u""
        };
        for (const auto& input : inputs) {
            for (bool toAscii : {false, true}) {
                const GuestConvert guestConvert = toAscii ? uidna_nameToASCII_nid_postfix : uidna_nameToUnicode_nid_postfix;
                const NativeConvert nativeConvert = toAscii ? uidna_nameToASCII : uidna_nameToUnicode;
                for (const auto capacity : {0, 1, 4, 8, 9, 16, 17, 32}) {
                    for (const auto warning : {0, -127, -124, INT32_MIN}) {
                        std::array<char16_t, 36> actual;
                        actual.fill(0xa55a);
                        auto expected = actual;
                        Info actualInfo(true);
                        Info expectedInfo(true);
                        std::int32_t status = warning;
                        nativeStatus = static_cast<UErrorCode>(warning);
                        const auto required = nativeConvert(native, input.data(), static_cast<std::int32_t>(input.size()),
                            expected.data() + 2, capacity, &expectedInfo.value, &nativeStatus);
                        const auto result = guestConvert(guest.value, input.data(), static_cast<std::int32_t>(input.size()),
                            actual.data() + 2, capacity, &actualInfo.value, &status);
                        Require(result == required && status == nativeStatus, "capacity/warning matches pinned native API");
                        Require(std::memcmp(&actualInfo.value, &expectedInfo.value, 24) == 0, "all extended info matches native API");
                        actualInfo.Check();
                        const auto written = result < capacity ? result + 1 : capacity;
                        for (std::int32_t i = 0; i < written; ++i) Require(actual[i + 2] == expected[i + 2], "native buffer prefix/termination");
                        for (std::size_t i = static_cast<std::size_t>(written) + 2; i < actual.size(); ++i)
                            Require(actual[i] == 0xa55a, "untouched output tail and after guard");
                        Require(actual[0] == 0xa55a && actual[1] == 0xa55a, "before output guard");
                    }
                }
            }
        }
        uidna_close(native);
    }
    {
        Handle guest(0);
        UErrorCode nativeStatus = U_ZERO_ERROR;
        auto* native = uidna_openUTS46(0, &nativeStatus);
        Require(native && nativeStatus == 0, "long explicit input native handle opens");
        std::u16string input(1025, u'A');
        for (std::size_t i = 1; i < input.size(); i += 2) input[i] = u'.';
        input[512] = 0;
        input.replace(input.size() - 7, 7, u"EXAMPLE");
        for (bool toAscii : {false, true}) {
            const GuestConvert guestConvert = toAscii ? uidna_nameToASCII_nid_postfix : uidna_nameToUnicode_nid_postfix;
            const NativeConvert nativeConvert = toAscii ? uidna_nameToASCII : uidna_nameToUnicode;
            UIDNAInfo preflight = UIDNA_INFO_INITIALIZER;
            nativeStatus = U_ZERO_ERROR;
            const auto required = nativeConvert(native, input.data(), static_cast<std::int32_t>(input.size()),
                nullptr, 0, &preflight, &nativeStatus);
            Require(required > 512, "explicit native input includes the suffix after embedded NUL");
            for (const auto capacity : {required, required + 1}) {
                std::vector<char16_t> actual(static_cast<std::size_t>(capacity) + 4, 0xa55a);
                auto expected = actual;
                Info actualInfo(true);
                Info expectedInfo(true);
                std::int32_t status = 0;
                nativeStatus = U_ZERO_ERROR;
                const auto count = nativeConvert(native, input.data(), static_cast<std::int32_t>(input.size()),
                    expected.data() + 2, capacity, &expectedInfo.value, &nativeStatus);
                const auto result = guestConvert(guest.value, input.data(), static_cast<std::int32_t>(input.size()),
                    actual.data() + 2, capacity, &actualInfo.value, &status);
                Require(result == count && status == nativeStatus, "long explicit input exact-capacity status matches native");
                Require(actual == expected, "long explicit input preserves native output, terminator and entire guarded tail");
                Require(actual[0] == 0xa55a && actual[1] == 0xa55a && actual[capacity + 2] == 0xa55a &&
                        actual[capacity + 3] == 0xa55a, "long explicit input leaves both outside guards intact");
                Require(std::memcmp(&actualInfo.value, &expectedInfo.value, 24) == 0, "long explicit input info matches native");
                actualInfo.Check();
            }
        }
        uidna_close(native);
    }
    Handle handle(0);
    for (auto convert : {uidna_nameToASCII_nid_postfix, uidna_nameToUnicode_nid_postfix}) {
        Info info(true);
        std::int32_t status = 0;
        Require(convert(handle.value, u"ABC", -1, nullptr, 0, &info.value, &status) == 3 && status == 15, "null preflight");
        info.Check();
        std::array<char16_t, 5> output{0xa55a, 0xa55a, 0xa55a, 0xa55a, 0xa55a};
        status = -124;
        Require(convert(handle.value, u"ABC", 3, output.data(), INT32_MAX, &info.value, &status) == 3 && status == 0, "large capacity bounded by actual result");
        Require(output == std::array<char16_t, 5>{u'a', u'b', u'c', 0, 0xa55a}, "large capacity does not overwrite tail");
        status = 7;
        Require(convert(reinterpret_cast<UIDNA*>(1), reinterpret_cast<const char16_t*>(1), INT32_MAX,
            reinterpret_cast<char16_t*>(1), INT32_MAX, reinterpret_cast<UIDNAInfo*>(1), &status) == 0 && status == 7,
            "prefailed status touches no other argument");
        status = 0;
        const auto before = output;
        Require(convert(handle.value, nullptr, 0, output.data(), 5, &info.value, &status) == 0 && status == 0,
                "null zero-length input is an empty domain");
        Require(info.value.errors == UIDNA_ERROR_EMPTY_LABEL && output[0] == 0, "empty domain has an info error and terminator");
        Require(std::equal(output.begin() + 1, output.end(), before.begin() + 1), "empty domain leaves output tail");
    }
}

void Lifetime() {
    auto* dead = [&] { Handle handle(0); return handle.value; }();
    RequireThrows([&] { uidna_close_nid_postfix(dead); });
    std::int32_t status = 0;
    Info info;
    std::array<char16_t, 32> output{};
    RequireThrows([&] { uidna_nameToASCII_nid_postfix(dead, u"ABC", -1, output.data(), 32, &info.value, &status); });
    Handle handle(0x30);
    std::atomic_bool ready{false};
    std::atomic_bool proceed{false};
    auto* racing = uidna_openUTS46_nid_postfix(0, &status);
    Require(racing && status == 0, "race handle opens");
    std::thread worker([&] {
        Info localInfo;
        std::array<char16_t, 32> localOutput{};
        std::int32_t localStatus = 0;
        ready.store(true);
        while (!proceed.load()) std::this_thread::yield();
        try {
            const auto count = uidna_nameToASCII_nid_postfix(racing, u"Bücher.de", -1, localOutput.data(), 32, &localInfo.value, &localStatus);
            Require(count == 16 && localStatus == 0 && localInfo.value.errors == 0, "concurrent conversion completes coherently");
        } catch (const std::runtime_error& failure) {
            Require(std::string_view(failure.what()) == "Convert not implemented", "only a concurrently closed handle may be refused");
        }
    });
    while (!ready.load()) std::this_thread::yield();
    proceed.store(true);
    uidna_close_nid_postfix(racing);
    worker.join();
    for (auto convert : {uidna_nameToASCII_nid_postfix, uidna_nameToUnicode_nid_postfix}) {
        status = 0;
        Require(convert(handle.value, u"faß.de", -1, output.data(), 32, &info.value, &status) ==
            (convert == uidna_nameToASCII_nid_postfix ? 13 : 6), "independent nontransitional handle survives other closes");
        Require(status == 0 && info.value.isTransitionalDifferent, "deviation flag retained");
    }
}

std::string Trim(std::string text) {
    const auto begin = text.find_first_not_of(" \t\r");
    if (begin == std::string::npos) return {};
    const auto end = text.find_last_not_of(" \t\r");
    return text.substr(begin, end - begin + 1);
}

std::u16string Decode(const std::string& text, const std::u16string& fallback) {
    if (text.empty()) return fallback;
    if (text == "\"\"") return {};
    const auto decoded = icu::UnicodeString::fromUTF8(text).unescape();
    return {decoded.getBuffer(), static_cast<std::size_t>(decoded.length())};
}

void Conformance(int operation) {
    std::ifstream file(ANYPS5_ICU_IDNA_TEST_DATA, std::ios::binary);
    Require(file.good(), "pinned IdnaTestV2.txt available");
    Handle handle(2 | 4 | 8 | 0x40 | (operation == 2 ? 0 : 0x30));
    const auto convert = operation == 0 ? uidna_nameToUnicode_nid_postfix : uidna_nameToASCII_nid_postfix;
    const auto nativeConvert = operation == 0 ? uidna_nameToUnicode : uidna_nameToASCII;
    UErrorCode nativeStatus = U_ZERO_ERROR;
    auto* native = uidna_openUTS46(2 | 4 | 8 | 0x40 | (operation == 2 ? 0 : 0x30), &nativeStatus);
    Require(native && nativeStatus == 0, "native conformance control opens");
    std::string line;
    unsigned lineNumber = 0;
    unsigned cases = 0;
    while (std::getline(file, line)) {
        ++lineNumber;
        line.erase(line.find('#') == std::string::npos ? line.size() : line.find('#'));
        if (Trim(line).empty()) continue;
        std::array<std::string, 7> fields;
        std::istringstream record(line);
        for (auto& field : fields) { std::getline(record, field, ';'); field = Trim(field); }
        const auto input = Decode(fields[0], {});
        const auto unicode = Decode(fields[1], input);
        const auto asciiN = Decode(fields[3], unicode);
        const auto expected = operation == 0 ? unicode : operation == 1 ? asciiN : Decode(fields[5], asciiN);
        const auto unicodeErrors = fields[2].empty() ? "[]" : fields[2];
        const auto asciiNErrors = fields[4].empty() ? unicodeErrors : fields[4];
        const auto errors = operation == 0 ? unicodeErrors : operation == 1 ? asciiNErrors : fields[6].empty() ? asciiNErrors : fields[6];
        Info info;
        std::int32_t status = 0;
        const auto required = convert(handle.value, input.data(), static_cast<std::int32_t>(input.size()), nullptr, 0, &info.value, &status);
        Require(status == 15 || (required == 0 && status == -124), "vector preflight status");
        std::vector<char16_t> output(static_cast<std::size_t>(required) + 3, 0xa55a);
        status = 0;
        const auto actual = convert(handle.value, input.data(), static_cast<std::int32_t>(input.size()), output.data() + 1,
            required + 1, &info.value, &status);
        UIDNAInfo nativeInfo = UIDNA_INFO_INITIALIZER;
        nativeStatus = U_ZERO_ERROR;
        const auto nativeRequired = nativeConvert(native, input.data(), static_cast<std::int32_t>(input.size()),
            nullptr, 0, &nativeInfo, &nativeStatus);
        std::vector<char16_t> nativeOutput(static_cast<std::size_t>(nativeRequired) + 1);
        nativeStatus = U_ZERO_ERROR;
        const auto nativeActual = nativeConvert(native, input.data(), static_cast<std::int32_t>(input.size()),
            nativeOutput.data(), nativeRequired + 1, &nativeInfo, &nativeStatus);
        Require(actual == nativeActual && status == nativeStatus && std::memcmp(&info.value, &nativeInfo, 16) == 0,
                "every vector status/complete info matches pinned native control");
        Require(std::equal(output.begin() + 1, output.begin() + 1 + actual, nativeOutput.begin()),
                "every vector output including invalid domains matches pinned native control");
        bool expectedErrors = errors != "[]";
        const std::u16string_view result(output.data() + 1, static_cast<std::size_t>(actual));
        if (operation != 0 && errors == "[A4_2]" && info.value.errors == 0 && result.ends_with(u".") && result.find(u"..") == result.npos)
            expectedErrors = false;
        const bool valid = actual == required && status <= 0 && (info.value.errors != 0) == expectedErrors &&
            (expectedErrors || result == expected) && output.front() == 0xa55a && output.back() == 0xa55a && output[required + 1] == 0;
        if (!valid) {
            std::fprintf(stderr, "IDNA vector line %u operation %d: status %d errors %x expected %s lengths %d/%zu\n",
                lineNumber, operation, status, info.value.errors, errors.c_str(), actual, expected.size());
            Require(false, "pinned UTS46 conformance vector");
        }
        info.Check();
        ++cases;
    }
    uidna_close(native);
    Require(cases == 6391, "complete pinned conformance corpus consumed");
    std::printf("UTS46 operation %d: %u conformance vectors\n", operation, cases);
}

void Memory() {
    constexpr std::size_t page = 0x4000;
    constexpr std::size_t bytes = page * 3;
    std::int64_t physical = 0;
    Require(sceKernelAllocateDirectMemory(0, 0x7fffffffffll, bytes, 0, 0, &physical) == 0, "allocate mapped guest test memory");
    void* address = nullptr;
    Require(sceKernelMapDirectMemory(&address, bytes, 3, 0, physical, 0) == 0, "map guest test memory");
    auto* data = static_cast<std::uint8_t*>(address);
    std::fill_n(data, bytes, 0xa5);
    Handle handle(0);
    const auto inaccessible = reinterpret_cast<char16_t*>(data + page * 2);
    Require(sceKernelMprotect(data + page * 2, page, 0) == 0, "protect inaccessible page");
    RequireThrows([&] { uidna_openUTS46_nid_postfix(0, reinterpret_cast<std::int32_t*>(inaccessible)); });
    for (auto convert : {uidna_nameToASCII_nid_postfix, uidna_nameToUnicode_nid_postfix}) {
        Info info;
        std::int32_t status = 7;
        Require(convert(reinterpret_cast<UIDNA*>(1), inaccessible, -1, inaccessible, 32,
            reinterpret_cast<UIDNAInfo*>(inaccessible), &status) == 0 && status == 7, "prefailure bypasses protected arguments");
        status = 0;
        const auto beforeInfo = info;
        Require(convert(handle.value, inaccessible, -2, inaccessible, -1, &info.value, &status) == 0 && status == 1,
                "argument error precedes inaccessible input/output access");
        Require(std::memcmp(&info, &beforeInfo, sizeof(info)) == 0, "argument error preserves complete info");
        status = 0;
        RequireThrows([&] { convert(handle.value, inaccessible, -1, nullptr, 0, &info.value, &status); });
        RequireThrows([&] { convert(handle.value, u"abc", -1, nullptr, 0, reinterpret_cast<UIDNAInfo*>(inaccessible), &status); });
        RequireThrows([&] { convert(handle.value, u"abc", -1, nullptr, 0, &info.value, reinterpret_cast<std::int32_t*>(inaccessible)); });
        RequireThrows([&] { convert(handle.value, u"abc", -1, inaccessible, 4, &info.value, &status); });
        auto* extended = reinterpret_cast<UIDNAInfo*>(data + page - 4);
        const std::int16_t size = 24;
        std::memcpy(extended, &size, sizeof(size));
        Require(sceKernelMprotect(data + page, page, 2) == 0, "make info extension write-only");
        std::array<char16_t, 4> output{};
        status = 0;
        Require(convert(handle.value, u"abc", -1, output.data(), 4, extended, &status) == 3 && status == 0, "output-only info extension needs no input read");
        Require(sceKernelMprotect(data + page, page, 3) == 0, "inspect info extension");
        for (std::size_t i = 2; i < 24; ++i) Require(data[page - 4 + i] == 0, "declared info extent cleared");
        Require(data[page - 5] == 0xa5 && data[page + 20] == 0xa5, "extended info boundary guards");
        std::fill_n(data, page * 2, 0xa5);
        const std::int32_t failed = 7;
        std::memcpy(data, &failed, sizeof(failed));
        Require(sceKernelMprotect(data, page, 1) == 0, "readonly status/output page");
        Require(convert(handle.value, inaccessible, -1, inaccessible, 4, reinterpret_cast<UIDNAInfo*>(inaccessible),
            reinterpret_cast<std::int32_t*>(data)) == 0, "readonly prefailed status is never rewritten");
        status = 0;
        RequireThrows([&] { convert(handle.value, u"abc", -1, reinterpret_cast<char16_t*>(data + 16), 4, &info.value, &status); });
        Require(data[16] == 0xa5 && data[17] == 0xa5, "readonly output unchanged after controlled refusal");
        Require(sceKernelMprotect(data, page, 3) == 0, "restore writable memory");
        std::fill_n(data, page * 2, 0xa5);
    }
    Require(sceKernelMunmap(reinterpret_cast<std::uint64_t>(data), bytes) == 0, "unmap guest test memory");
    Require(sceKernelReleaseDirectMemory(physical, bytes) == 0, "release guest test memory");
}


} // namespace UidnaBehavior

void CheckIcuIdnaNative() { UidnaBehavior::Buffers(); UidnaBehavior::Lifetime(); }
void CheckIcuIdnaConformance(int operation) { UidnaBehavior::Conformance(operation); }
void CheckIcuIdnaMemory() { UidnaBehavior::Memory(); }
