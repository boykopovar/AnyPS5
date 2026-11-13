#include "prx/libSceIcu/libSceIcu.hpp"
#include "prx/libSceIcu/tests/NumberingSystemOracle.hpp"
#include "prx/libSceIcu/tests/RecoveredSignatureChecks.hpp"
#include "prx/libkernel/libkernel.hpp"
#include <algorithm>
#include <array>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <exception>
#include <set>
#include <string>
#include <string_view>

namespace {

using IcuRecoveredTest::Require;

template<class Function>
void Refuses(Function&& function) {
    bool refused = false;
    try { function(); } catch (const std::exception&) { refused = true; }
    Require(refused, "invalid memory or foreign/stale handle is refused");
}

void Compare(const char* name, bool byName, std::string_view literal = {}, int algorithmic = -1) {
    std::int32_t status = 0;
    std::int32_t expected = 0;
    auto* native = NumberingOracleOpen(name, byName, &expected);
    auto* guest = byName ? unumsys_openByName_nid_postfix(name, &status) : unumsys_open_nid_postfix(name, &status);
    Require(bool(guest) == bool(native) && status == expected, "native success/failure and status match");
    if (!guest) return;
    const auto actualName = std::string_view(unumsys_getName_nid_postfix(guest));
    Require(actualName == NumberingOracleName(native), "numbering name follows actual locale/keyword data");
    Require(unumsys_isAlgorithmic_nid_postfix(guest) == NumberingOracleAlgorithmic(native), "algorithmic flag matches native");
    if (!literal.empty()) Require(actualName == literal, "fixed nondefault numbering selection");
    if (algorithmic >= 0) Require(unumsys_isAlgorithmic_nid_postfix(guest) == algorithmic, "fixed algorithmic property");
    unumsys_close_nid_postfix(guest);
    NumberingOracleClose(native);
}

void Selection() {
    std::fputs("numbering fixture: valid unumsys_openByName\n", stderr);
    Compare("latn", true, "latn", 0);
    Compare("arab", true, "arab", 0);
    Compare("hanidec", true, "hanidec", 0);
    Compare("roman", true, "roman", 1);
    Compare("en_US", false, "latn", 0);
    Compare("ar_EG", false, "arab", 0);
    Compare("en@numbers=hanidec", false, "hanidec", 0);
    Compare("en@numbers=roman", false, "roman", 1);
    for (const auto* locale : {"", "root", "fa_IR", "bn", "th_TH@numbers=native", "zh@numbers=traditional",
             "zh@numbers=finance", "en@numbers=default", "en@numbers=not-a-system", "zz_ZZ", "en-u-nu-arab"}) Compare(locale, false);
    for (const auto* name : {"", "not-a-system", "LATN", "native", "default", "latn-extra"}) Compare(name, true);
}

void Enumeration() {
    std::int32_t status = 0;
    std::int32_t expected = 0;
    auto* native = NumberingOracleEnumeration(&expected);
    std::fputs("numbering fixture: valid unumsys_openAvailableNames\n", stderr);
    auto* guest = unumsys_openAvailableNames_nid_postfix(&status);
    Require(guest && native && status == expected, "available names produce a usable guest enumeration");
    const auto count = uenum_count_nid_postfix(guest, &status);
    Require(count == NumberingOracleCount(native, &expected) && count > 0 && status == expected, "count through guest callback ABI");
    std::set<std::string> names;
    for (std::int32_t index = 0; index <= count; ++index) {
        std::int32_t length = 0x55;
        std::int32_t nativeLength = 0x55;
        const auto* wanted = NumberingOracleNext(native, &nativeLength, &expected);
        const auto* actual = uenum_next_nid_postfix(guest, &length, &status);
        Require(bool(actual) == bool(wanted) && length == nativeLength && status == expected, "next cursor, EOF and length match native");
        if (actual) {
            Require(std::string_view(actual, length) == std::string_view(wanted, nativeLength), "entire available-name sequence matches native");
            Require(names.emplace(actual, length).second, "each available name occurs once");
            Compare(actual, true, std::string_view(actual, length));
        }
    }
    Require(names.contains("latn") && names.contains("arab") && names.contains("roman") && names.contains("hanidec"), "real packaged numbering systems are available");
    guest->reset(guest, &status);
    NumberingOracleReset(native, &expected);
    for (std::int32_t index = 0; index <= count; ++index) {
        std::int32_t length = 0x66;
        std::int32_t nativeLength = 0x66;
        if (index % 2 == 0) {
            const auto* wanted = NumberingOracleUNext(native, &nativeLength, &expected);
            const auto* actual = uenum_unext_nid_postfix(guest, &length, &status);
            Require(bool(actual) == bool(wanted) && length == nativeLength && status == expected, "UTF16 cursor and EOF match native");
            if (actual) Require(std::u16string_view(actual, length) == std::u16string_view(wanted, nativeLength), "UTF16 names match native");
        } else {
            const auto* wanted = NumberingOracleNext(native, &nativeLength, &expected);
            const auto* actual = uenum_next_nid_postfix(guest, &length, &status);
            Require(bool(actual) == bool(wanted) && length == nativeLength && status == expected, "mixed calls share one cursor");
            if (actual) Require(std::string_view(actual, length) == std::string_view(wanted, nativeLength), "mixed next matches native");
        }
    }
    guest->reset(guest, &status);
    NumberingOracleReset(native, &expected);
    Require(std::string_view(uenum_next_nid_postfix(guest, nullptr, &status)) == NumberingOracleNext(native, nullptr, &expected), "null resultLength retains normal behavior");
    uenum_close_nid_postfix(guest);
    NumberingOracleEnumClose(native);
    Refuses([&] { uenum_count_nid_postfix(guest, &status); });
}

void Status() {
    for (const auto incoming : {1, 7, 15, 0x7fffffff}) {
        auto status = incoming;
        const auto* invalid = reinterpret_cast<const char*>(std::uintptr_t{1});
        Require(unumsys_open_nid_postfix(invalid, &status) == nullptr && status == incoming, "failed status before locale access");
        Require(unumsys_openByName_nid_postfix(invalid, &status) == nullptr && status == incoming, "failed status before name access");
        Require(unumsys_openAvailableNames_nid_postfix(&status) == nullptr && status == incoming, "enumeration preserves failed status");
    }
    for (const auto incoming : {0, -128, -124, -1}) {
        auto status = incoming;
        auto nativeStatus = incoming;
        auto* native = NumberingOracleOpen("arab", true, &nativeStatus);
        auto* guest = unumsys_openByName_nid_postfix("arab", &status);
        Require(guest && native && status == nativeStatus && std::string_view(unumsys_getName_nid_postfix(guest)) == "arab", "warnings still execute native selection");
        unumsys_close_nid_postfix(guest);
        NumberingOracleClose(native);
    }
    Refuses([] { unumsys_open_nid_postfix("en", nullptr); });
    Refuses([] { unumsys_openAvailableNames_nid_postfix(nullptr); });
    std::int32_t status = 0;
    Refuses([&] { unumsys_open_nid_postfix(nullptr, &status); });
    Refuses([&] { unumsys_openByName_nid_postfix(nullptr, &status); });
    auto* closed = unumsys_openByName_nid_postfix("arab", &status);
    unumsys_close_nid_postfix(closed);
    for (int index = 0; index < 128; ++index) {
        auto* fresh = unumsys_openByName_nid_postfix("latn", &status);
        Require(fresh != closed, "old token is not recycled");
        Refuses([&] { unumsys_getName_nid_postfix(closed); });
        unumsys_close_nid_postfix(fresh);
    }
    Refuses([&] { unumsys_close_nid_postfix(closed); });
    Refuses([] { unumsys_getName_nid_postfix(nullptr); });
    Refuses([] { unumsys_isAlgorithmic_nid_postfix(reinterpret_cast<const UNumberingSystem*>(std::uintptr_t{1})); });
    auto* enumeration = unumsys_openAvailableNames_nid_postfix(&status);
    Refuses([&] { unumsys_getName_nid_postfix(reinterpret_cast<UNumberingSystem*>(enumeration)); });
    uenum_close_nid_postfix(enumeration);
    unumsys_close_nid_postfix(nullptr);
}

void InaccessibleStatus() {
    std::int32_t status = 0;
    auto* enumeration = unumsys_openAvailableNames_nid_postfix(&status);
    Require(enumeration && status <= 0, "valid enumeration for checked dispatch status");
    auto* invalid = reinterpret_cast<std::int32_t*>(std::uintptr_t{1});
    Refuses([&] { uenum_count_nid_postfix(enumeration, invalid); });
    Refuses([&] { uenum_next_nid_postfix(enumeration, nullptr, invalid); });
    Refuses([&] { uenum_unext_nid_postfix(enumeration, nullptr, invalid); });
    uenum_close_nid_postfix(enumeration);
}

void CrossKind() {
    std::int32_t status = 0;
    auto* system = unumsys_openByName_nid_postfix("latn", &status);
    Require(system && status <= 0, "valid system for reverse cross-kind checks");
    auto* enumeration = reinterpret_cast<UEnumeration*>(system);
    for (int phase = 0; phase < 2; ++phase) {
        status = 0;
        Refuses([&] { uenum_count_nid_postfix(enumeration, &status); });
        Refuses([&] { uenum_next_nid_postfix(enumeration, nullptr, &status); });
        Refuses([&] { uenum_unext_nid_postfix(enumeration, nullptr, &status); });
        Refuses([&] { uenum_close_nid_postfix(enumeration); });
        if (phase == 0) unumsys_close_nid_postfix(system);
    }
}

void Memory() {
    constexpr std::size_t page = 0x4000;
    constexpr std::size_t bytes = page * 3;
    std::int64_t physical = 0;
    Require(sceKernelAllocateDirectMemory(0, 0x7fffffffffll, bytes, 0, 0, &physical) == 0);
    void* address = nullptr;
    Require(sceKernelMapDirectMemory(&address, bytes, 3, 0, physical, 0) == 0);
    auto* data = static_cast<char*>(address);
    Require(sceKernelMprotect(data, page, 0) == 0);
    Require(sceKernelMprotect(data + page * 2, page, 0) == 0);
    auto* name = data + page * 2 - 5;
    std::memcpy(name, "latn", 5);
    auto* status = reinterpret_cast<std::int32_t*>(data + page);
    *status = 0;
    auto* handle = unumsys_openByName_nid_postfix(name, status);
    Require(handle && *status <= 0 && std::string_view(unumsys_getName_nid_postfix(handle)) == "latn", "NUL at readable page end");
    unumsys_close_nid_postfix(handle);
    std::fill_n(name, 5, 'x');
    Refuses([&] { unumsys_openByName_nid_postfix(name, status); });
    *status = 7;
    Require(sceKernelMprotect(data + page, page, 1) == 0);
    Require(unumsys_open_nid_postfix(reinterpret_cast<const char*>(data), status) == nullptr && *status == 7, "read-only failed status returns before guard access");
    Require(sceKernelMprotect(data + page, page, 3) == 0);
    *status = 0;
    auto* enumeration = unumsys_openAvailableNames_nid_postfix(status);
    auto* length = reinterpret_cast<std::int32_t*>(data + page * 2 - 4);
    *length = 0x55;
    Require(uenum_next_nid_postfix(enumeration, length, status) && *length > 0, "mapped resultLength written through callback");
    Require(sceKernelMprotect(data + page, page, 1) == 0);
    Refuses([&] { uenum_next_nid_postfix(enumeration, length, status); });
    Require(sceKernelMprotect(data + page, page, 3) == 0);
    uenum_close_nid_postfix(enumeration);
    Require(sceKernelMunmap(reinterpret_cast<std::uint64_t>(data), bytes) == 0);
    Require(sceKernelReleaseDirectMemory(physical, bytes) == 0);
}

} // namespace

int main(int argc, char** argv) {
    try {
        const std::string_view mode = argc > 1 ? argv[1] : "selection";
        if (mode == "selection") Selection();
        else if (mode == "enumeration") Enumeration();
        else if (mode == "status") Status();
        else if (mode == "memory") Memory();
        else if (mode == "inaccessible_status") InaccessibleStatus();
        else if (mode == "cross_kind") CrossKind();
        else Require(false, "unknown numbering test mode");
    } catch (const std::exception& error) {
        std::fprintf(stderr, "numbering fixture failed: %s\n", error.what());
        return 1;
    }
}
