#include "prx/libSceIcu/libSceIcu.hpp"
#include "prx/libSceIcu/tests/RecoveredSignatureChecks.hpp"
#include "unicode/uidna.h"
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
using GuestConvert = decltype(&uidna_nameToASCII_nid_postfix);

void Nid(const char* path) {
#ifdef _WIN32
    auto module = LoadLibraryA(path);
#else
    auto module = dlopen(path, RTLD_NOW | RTLD_LOCAL);
#endif
    Require(module != nullptr, "load actual NID-patched ICU PRX");
    const auto import = [&](const char* symbol) {
#ifdef _WIN32
        auto* address = GetProcAddress(module, symbol);
#else
        auto* address = dlsym(module, symbol);
#endif
        Require(address != nullptr, "resolve IDNA NID");
        return address;
    };
    const auto open = reinterpret_cast<decltype(&uidna_openUTS46_nid_postfix)>(import("8qHjnAH4A94"));
    const auto close = reinterpret_cast<decltype(&uidna_close_nid_postfix)>(import("RJ---xceJAY"));
    const auto ascii = reinterpret_cast<GuestConvert>(import("lAgN2aoiNHM"));
    const auto unicode = reinterpret_cast<GuestConvert>(import("+4NYlDLIb6M"));
    std::int32_t status = 0;
    auto* handle = open(0x30, &status);
    Require(handle && status == 0, "NID open creates a usable UTS46 handle");
    std::array<char16_t, 32> output{};
    UIDNAInfo info = UIDNA_INFO_INITIALIZER;
    Require(ascii(handle, u"faß.de", -1, output.data(), 32, &info, &status) == 13 && status == 0 &&
            std::u16string_view(output.data()) == u"xn--fa-hia.de", "NID ASCII conversion with nontransitional option");
    Require(unicode(handle, output.data(), -1, output.data() + 16, 16, &info, &status) == 6 && status == 0 &&
            std::u16string_view(output.data() + 16) == u"faß.de", "NID Unicode conversion");
    close(handle);
#ifdef _WIN32
    Require(FreeLibrary(module) != 0, "unload NID ICU PRX");
#else
    Require(dlclose(module) == 0, "unload NID ICU PRX");
#endif
}

} // namespace

int main(int argc, char* argv[]) {
    Require(argc == 2, "patched PRX path argument");
    Nid(argv[1]);
}
