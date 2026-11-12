#include "prx/libSceDiscMap/include/Api.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"

#include <cstdint>
#include <cstdio>
#include <cstdlib>

namespace {

constexpr std::int32_t noBitmapInfo = static_cast<std::int32_t>(0x81100004);

void Require(bool condition, const char* message) {
    if (!condition) {
        std::fprintf(stderr, "DiscMap: %s\n", message);
        std::abort();
    }
}

}

int main() {
    std::int32_t result = 0x5a5a5a5a;
    Require(sceDiscMapIsRequestOnHDD("/app0/data.bin", 0, 0x10000, &result) == noBitmapInfo, "sceDiscMapIsRequestOnHDD did not report the missing bitmap");
    Require(result == 0x5a5a5a5a, "sceDiscMapIsRequestOnHDD wrote its result");
    std::uint64_t flags = ~0ull;
    std::uint64_t out1 = ~0ull;
    std::uint64_t out2 = ~0ull;
    Require(sceDiscMapUnknown_ioKMruft1ek("/app0/data.bin", 0x8000, 0x10000, &flags, &out1, &out2) == noBitmapInfo, "ioKMruft1ek did not report the missing bitmap");
    Require(flags == ~0ull && out1 == ~0ull && out2 == ~0ull, "ioKMruft1ek wrote an output");
    std::uint64_t ret1 = 0x5a5a5a5a5a5a5a5aull;
    std::uint64_t ret2 = 0x5a5a5a5a5a5a5a5aull;
    Require(sceDiscMapGetPackageSize(0, &ret1, &ret2) == noBitmapInfo, "sceDiscMapGetPackageSize did not report missing bitmap");
    Require(ret1 == 0x5a5a5a5a5a5a5a5aull && ret2 == 0x5a5a5a5a5a5a5a5aull, "sceDiscMapGetPackageSize wrote its outputs");
    Require(sceDiscMapGetPackageSize(0, nullptr, nullptr) == noBitmapInfo, "sceDiscMapGetPackageSize with null outputs did not report missing bitmap");
    std::puts("DiscMap tests passed");
    return 0;
}
