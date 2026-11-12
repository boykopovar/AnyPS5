#include "prx/libSceLibreSsl3/libSceLibreSsl3.hpp"

#include <array>
#include <bit>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <exception>
#include <filesystem>
#include <source_location>

#ifdef _WIN32
#include <windows.h>
#else
#include <dlfcn.h>
#endif

namespace {

using Getter = decltype(&ECDSA_SIG_get0_nid_postfix);

void Require(bool condition, std::source_location location = std::source_location::current()) {
    if (condition) return;
    std::fprintf(stderr, "LibreSSL3 ECDSA signature getter check failed at line %u\n", location.line());
    std::abort();
}

void Run(Getter getter) {
    std::array<int, 3> values{};
    for (unsigned fields = 0; fields < 4; ++fields) {
        LibreSsl3EcdsaSig sig{fields & 1 ? &values[0] : nullptr, fields & 2 ? &values[1] : nullptr};
        std::array<unsigned char, sizeof(sig)> before;
        std::memcpy(before.data(), &sig, sizeof(sig));
        const void* const sentinel = &values[2];
        for (unsigned outputs = 0; outputs < 4; ++outputs) {
            const void* r = sentinel;
            const void* s = sentinel;
            getter(&sig, outputs & 1 ? &r : nullptr, outputs & 2 ? &s : nullptr);
            Require(r == (outputs & 1 ? sig.r : sentinel));
            Require(s == (outputs & 2 ? sig.s : sentinel));
            Require(std::memcmp(before.data(), &sig, sizeof(sig)) == 0);
            const void* shared = sentinel;
            getter(&sig, outputs & 1 ? &shared : nullptr, outputs & 2 ? &shared : nullptr);
            Require(shared == (outputs & 2 ? sig.s : outputs & 1 ? sig.r : sentinel));
            Require(std::memcmp(before.data(), &sig, sizeof(sig)) == 0);
        }
    }
    getter(nullptr, nullptr, nullptr);
    for (unsigned outputs = 1; outputs < 4; ++outputs) {
        const void* r = &values[2];
        const void* s = &values[2];
        bool threw = false;
        try { getter(nullptr, outputs & 1 ? &r : nullptr, outputs & 2 ? &s : nullptr); }
        catch (const std::exception&) { threw = true; }
        Require(threw && r == &values[2] && s == &values[2]);
    }
}

}

int main(int argc, char** argv) {
    Require(argc == 2);
#ifdef _WIN32
    const auto library = LoadLibraryExW(std::filesystem::path(argv[1]).c_str(), nullptr, LOAD_WITH_ALTERED_SEARCH_PATH);
    Require(library != nullptr);
    const auto getter = std::bit_cast<Getter>(GetProcAddress(library, "z8fsXF8JuL0"));
#else
    const auto library = dlopen(argv[1], RTLD_NOW | RTLD_LOCAL);
    if (!library) std::fprintf(stderr, "%s\n", dlerror());
    Require(library != nullptr);
    const auto getter = reinterpret_cast<Getter>(dlsym(library, "z8fsXF8JuL0"));
#endif
    Require(getter != nullptr);
    try { Run(getter); }
    catch (const std::exception& error) {
        std::fprintf(stderr, "ECDSA_SIG_get0 threw: %s\n", error.what());
        return 1;
    }
    std::puts("LibreSSL3 ECDSA signature getter tests passed");
}
