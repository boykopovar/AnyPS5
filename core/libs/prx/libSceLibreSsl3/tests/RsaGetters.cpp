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

using Factors = void (APS5_VABI*)(const LibreSsl3Rsa*, const void**, const void**);
using CrtParams = void (APS5_VABI*)(const LibreSsl3Rsa*, const void**, const void**, const void**);

void Require(bool condition, std::source_location location = std::source_location::current()) {
    if (condition) return;
    std::fprintf(stderr, "LibreSSL3 RSA getter check failed at line %u\n", location.line());
    std::abort();
}

void Run(Factors factors, CrtParams crtParams) {
    std::array<int, 6> values{};
    for (unsigned fields = 0; fields < 32; ++fields) {
        LibreSsl3Rsa rsa{};
        rsa.p = fields & 1 ? &values[0] : nullptr;
        rsa.q = fields & 2 ? &values[1] : nullptr;
        rsa.dmp1 = fields & 4 ? &values[2] : nullptr;
        rsa.dmq1 = fields & 8 ? &values[3] : nullptr;
        rsa.iqmp = fields & 16 ? &values[4] : nullptr;
        rsa.references = 37;
        rsa.flags = 123;
        std::array<unsigned char, sizeof(rsa)> before;
        std::memcpy(before.data(), &rsa, sizeof(rsa));
        const void* const sentinel = &values[5];
        for (unsigned outputs = 0; outputs < 4; ++outputs) {
            const void* p = sentinel;
            const void* q = sentinel;
            factors(&rsa, outputs & 1 ? &p : nullptr, outputs & 2 ? &q : nullptr);
            Require(p == (outputs & 1 ? rsa.p : sentinel));
            Require(q == (outputs & 2 ? rsa.q : sentinel));
            const void* shared = sentinel;
            factors(&rsa, outputs & 1 ? &shared : nullptr, outputs & 2 ? &shared : nullptr);
            Require(shared == (outputs & 2 ? rsa.q : outputs & 1 ? rsa.p : sentinel));
            Require(std::memcmp(before.data(), &rsa, sizeof(rsa)) == 0);
        }
        for (unsigned outputs = 0; outputs < 8; ++outputs) {
            const void* dmp1 = sentinel;
            const void* dmq1 = sentinel;
            const void* iqmp = sentinel;
            crtParams(&rsa, outputs & 1 ? &dmp1 : nullptr, outputs & 2 ? &dmq1 : nullptr,
                outputs & 4 ? &iqmp : nullptr);
            Require(dmp1 == (outputs & 1 ? rsa.dmp1 : sentinel));
            Require(dmq1 == (outputs & 2 ? rsa.dmq1 : sentinel));
            Require(iqmp == (outputs & 4 ? rsa.iqmp : sentinel));
            const void* shared = sentinel;
            crtParams(&rsa, outputs & 1 ? &shared : nullptr, outputs & 2 ? &shared : nullptr,
                outputs & 4 ? &shared : nullptr);
            Require(shared == (outputs & 4 ? rsa.iqmp : outputs & 2 ? rsa.dmq1 : outputs & 1 ? rsa.dmp1 : sentinel));
            Require(std::memcmp(before.data(), &rsa, sizeof(rsa)) == 0);
        }
    }
    factors(nullptr, nullptr, nullptr);
    crtParams(nullptr, nullptr, nullptr, nullptr);
    for (unsigned outputs = 1; outputs < 4; ++outputs) {
        const void* value = &values[5];
        bool threw = false;
        try { factors(nullptr, outputs & 1 ? &value : nullptr, outputs & 2 ? &value : nullptr); }
        catch (const std::exception&) { threw = true; }
        Require(threw && value == &values[5]);
    }
    for (unsigned outputs = 1; outputs < 8; ++outputs) {
        const void* value = &values[5];
        bool threw = false;
        try {
            crtParams(nullptr, outputs & 1 ? &value : nullptr, outputs & 2 ? &value : nullptr,
                outputs & 4 ? &value : nullptr);
        }
        catch (const std::exception&) { threw = true; }
        Require(threw && value == &values[5]);
    }
}

}

int main(int argc, char** argv) {
    Require(argc == 2);
#ifdef _WIN32
    const auto library = LoadLibraryExW(std::filesystem::path(argv[1]).c_str(), nullptr, LOAD_WITH_ALTERED_SEARCH_PATH);
    Require(library != nullptr);
    const auto factors = std::bit_cast<Factors>(GetProcAddress(library, "PcUZS7XHwHk"));
    const auto crtParams = std::bit_cast<CrtParams>(GetProcAddress(library, "eyrIagsax0w"));
#else
    const auto library = dlopen(argv[1], RTLD_NOW | RTLD_LOCAL);
    if (!library) std::fprintf(stderr, "%s\n", dlerror());
    Require(library != nullptr);
    const auto factors = reinterpret_cast<Factors>(dlsym(library, "PcUZS7XHwHk"));
    const auto crtParams = reinterpret_cast<CrtParams>(dlsym(library, "eyrIagsax0w"));
#endif
    Require(factors != nullptr && crtParams != nullptr);
    bool failed = false;
    try { factors(nullptr, nullptr, nullptr); }
    catch (const std::exception& error) {
        std::fprintf(stderr, "RSA_get0_factors threw: %s\n", error.what());
        failed = true;
    }
    try { crtParams(nullptr, nullptr, nullptr, nullptr); }
    catch (const std::exception& error) {
        std::fprintf(stderr, "RSA_get0_crt_params threw: %s\n", error.what());
        failed = true;
    }
    if (failed) return 1;
    try { Run(factors, crtParams); }
    catch (const std::exception& error) {
        std::fprintf(stderr, "RSA getter threw: %s\n", error.what());
        failed = true;
    }
    if (failed) return 1;
    std::puts("LibreSSL3 RSA getter tests passed");
}
