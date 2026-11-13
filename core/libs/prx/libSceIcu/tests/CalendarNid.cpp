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
        Require(address != nullptr, "resolve calendar-state NID");
        return address;
    };
    const auto open = reinterpret_cast<decltype(&ucal_open_nid_postfix)>(import("smwWdxL1Kug"));
    const auto close = reinterpret_cast<decltype(&ucal_close_nid_postfix)>(import("HxMnub1lWFI"));
    const auto clone = reinterpret_cast<decltype(&ucal_clone_nid_postfix)>(import("ryv9lh7cH5I"));
    const auto get = reinterpret_cast<decltype(&ucal_get_nid_postfix)>(import("Lp1KUGve2kA"));
    const auto time = reinterpret_cast<decltype(&ucal_setMillis_nid_postfix)>(import("fhxZ1PMqRW4"));
    const auto cutover = reinterpret_cast<decltype(&ucal_setGregorianChange_nid_postfix)>(import("w0+41mBFHoY"));
    std::int32_t status = 0;
    auto* original = open(u"UTC", -1, "en_US", 1, &status);
    Require(original && status <= 0, "actual NID opens an explicit Gregorian UTC calendar");
    status = 0;
    time(original, -12219724800000.0, &status);
    Require(status <= 0 && get(original, 1, &status) == 1582 && get(original, 2, &status) == 8 && get(original, 5, &status) == 30,
        "actual NID default cutover gives Julian date for Gregorian epoch1582-10-10");
    auto* copy = clone(original, &status);
    Require(copy && status <= 0, "actual NID clones state into independent object");
    cutover(copy, -8640000000000000.0, &status);
    Require(status <= 0 && get(copy, 2, &status) == 9 && get(copy, 5, &status) == 10,
        "actual NID proleptic Gregorian cutover changes clone fields");
    Require(get(original, 2, &status) == 8 && get(original, 5, &status) == 30, "actual NID original cutover remains unchanged");
    time(copy, 1577934245000.0, &status);
    Require(status <= 0 && get(copy, 1, &status) == 2020 && get(copy, 11, &status) == 3 && get(copy, 12, &status) == 4,
        "actual NID millisecond assignment and field extraction");
    close(copy);
    close(original);
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
