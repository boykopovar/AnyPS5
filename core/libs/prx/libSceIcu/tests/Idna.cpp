#include "prx/libSceIcu/libSceIcu.hpp"
#include "prx/libSceIcu/tests/RecoveredSignatureChecks.hpp"
#include "prx/libc/include/General.hpp"
#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <exception>
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

void CloseNonNullThrows() {
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
        Require(threw, "non-null close throws NotImplemented");
        Require(NotImplementedCalls_nid_no_patch() == before + 1, "non-null close records NotImplemented");
        Require(storage == expected, "non-null close does not mutate caller memory");
    }
}

}

void Run() {
    CloseNull();
    CloseNonNullThrows();
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
        RequireThrows([&] { uidna_openUTS46_nid_postfix(0, &status); });
        Require(status == incoming);

        status = incoming;
        RequireThrows([&] { uidna_openUTS46_nid_postfix(0xffffffffu, &status); });
        Require(status == incoming);
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
