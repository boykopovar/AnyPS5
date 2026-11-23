#include "prx/libSceIcu/libSceIcu.hpp"
#include "prx/libSceIcu/tests/RecoveredSignatureChecks.hpp"
#include <array>
#include <cstdint>
#include <string_view>
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#else
#include <dlfcn.h>
#endif

namespace {

using IcuRecoveredTest::Require;

void Check(const char* path) {
#ifdef _WIN32
    auto module = LoadLibraryA(path);
#else
    auto module = dlopen(path, RTLD_NOW | RTLD_LOCAL);
#endif
    Require(module != nullptr, "load actual canonical NID-patched ICU artifact");
    const auto import = [&](const char* name) {
#ifdef _WIN32
        auto* address = GetProcAddress(module, name);
#else
        auto* address = dlsym(module, name);
#endif
        Require(address != nullptr, "resolve numbering-system NID");
        return address;
    };
    const auto open = reinterpret_cast<decltype(&unumsys_open_nid_postfix)>(import("AEpz1OQ1KsU"));
    const auto byName = reinterpret_cast<decltype(&unumsys_openByName_nid_postfix)>(import("M3eV0iVeCyw"));
    const auto close = reinterpret_cast<decltype(&unumsys_close_nid_postfix)>(import("4nYP7N-XMmo"));
    const auto name = reinterpret_cast<decltype(&unumsys_getName_nid_postfix)>(import("wzqQz8S7nss"));
    const auto algorithmic = reinterpret_cast<decltype(&unumsys_isAlgorithmic_nid_postfix)>(import("6e3wrg5zRDk"));
    const auto available = reinterpret_cast<decltype(&unumsys_openAvailableNames_nid_postfix)>(import("Fqh7XMoqVU0"));
    const auto count = reinterpret_cast<decltype(&uenum_count_nid_postfix)>(import("p0PRJbphhqk"));
    const auto next = reinterpret_cast<decltype(&uenum_next_nid_postfix)>(import("oXVgB59UiKM"));
    const auto unext = reinterpret_cast<decltype(&uenum_unext_nid_postfix)>(import("zdiM3wxIsUY"));
    const auto enumClose = reinterpret_cast<decltype(&uenum_close_nid_postfix)>(import("GYJuIfJTnoo"));
    std::int32_t status = 0;
    auto* handle = open("en@numbers=hanidec", &status);
    Require(handle && status <= 0 && std::string_view(name(handle)) == "hanidec" && algorithmic(handle) == 0,
        "actual NID locale/keyword selection and queries");
    close(handle);
    status = 0;
    handle = byName("roman", &status);
    Require(handle && status <= 0 && std::string_view(name(handle)) == "roman" && algorithmic(handle) != 0,
        "actual NID named algorithmic system");
    close(handle);
    status = 0;
    auto* enumeration = available(&status);
    Require(enumeration && status <= 0, "actual NID enumeration opens");
    const auto total = count(enumeration, &status);
    Require(total > 0 && status <= 0, "actual NID enumeration count uses guest callback ABI");
    bool latin = false;
    bool roman = false;
    for (std::int32_t index = 0; index < total; ++index) {
        std::int32_t length = 0;
        const auto* value = next(enumeration, &length, &status);
        Require(value && status <= 0 && length > 0, "actual NID next returns a usable name");
        const auto text = std::string_view(value, length);
        latin |= text == "latn";
        roman |= text == "roman";
    }
    Require(latin && roman && next(enumeration, nullptr, &status) == nullptr && status <= 0,
        "actual NID enumeration includes decimal and algorithmic names then reaches EOF");
    enumeration->reset(enumeration, &status);
    std::int32_t length = 0;
    Require(unext(enumeration, &length, &status) && length > 0 && status <= 0, "actual NID UTF16 iteration after guest ABI reset");
    enumClose(enumeration);
    close(nullptr);
#ifdef _WIN32
    Require(FreeLibrary(module) != 0, "unload actual ICU artifact");
#else
    Require(dlclose(module) == 0, "unload actual ICU artifact");
#endif
}

} // namespace

int main(int argc, char** argv) {
    Require(argc == 2, "canonical patched ICU path argument");
    Check(argv[1]);
}
