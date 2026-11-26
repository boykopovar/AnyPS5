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
        Require(address != nullptr, "resolve date-pattern NID");
        return address;
    };
    const auto open = reinterpret_cast<decltype(&udatpg_open_nid_postfix)>(import("rB50kBGWBd0"));
    const auto close = reinterpret_cast<decltype(&udatpg_close_nid_postfix)>(import("xw2VCWP1Dm8"));
    const auto best = reinterpret_cast<decltype(&udatpg_getBestPattern_nid_postfix)>(import("8XT6ORy2e3Q"));
    const auto options = reinterpret_cast<decltype(&udatpg_getBestPatternWithOptions_nid_postfix)>(import("VahFo5sMp0Y"));
    const auto skeleton = reinterpret_cast<decltype(&udatpg_getSkeleton_nid_postfix)>(import("DCv6GOS9838"));
    const auto field = reinterpret_cast<decltype(&udatpg_getFieldDisplayName_nid_postfix)>(import("WJYkB+wa9ho"));
    std::int32_t status = 0;
    auto* handle = open("en_US", &status);
    Require(handle && status <= 0, "NID open produces usable generator");
    std::array<char16_t, 64> output{};
    status = 0;
    Require(best(handle, u"yMd", 3, output.data(), 64, &status) == 5 && status <= 0 &&
        std::u16string_view(output.data()) == u"M/d/y", "NID best-pattern conversion");
    status = 0;
    Require(options(handle, u"HHHmmmsss", 9, UDATPG_MATCH_ALL_FIELDS_LENGTH, output.data(), 64, &status) == 11 &&
        status <= 0 && std::u16string_view(output.data()) == u"HHH:mmm:sss", "NID match-option widths");
    status = 0;
    Require(skeleton(nullptr, u"yyyy-MM-dd", -1, output.data(), 64, &status) == 8 && status <= 0 &&
        std::u16string_view(output.data()) == u"yyyyMMdd", "NID static skeleton with null handle");
    status = 0;
    Require(field(handle, 1, 0, output.data(), 64, &status) == 4 && status <= 0 &&
        std::u16string_view(output.data()) == u"year", "NID localized field name");
    close(handle);
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
