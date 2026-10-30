#include "prx/libkernel_sys/include/Api.hpp"
#include "prx/libkernel_sys/include/AdditionalExports.hpp"
#include "ProcessParametersFixture.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace {

alignas(16) std::array<std::byte, 0x40> process{};

void Require(bool condition, const char* message) {
    if (!condition) {
        std::fprintf(stderr, "KERNEL SYS SDK VERSION: %s\n", message);
        std::abort();
    }
}

}

int main() {
    const std::uint64_t size = process.size();
    const std::uint32_t magic = 0x4942524f;
    const std::uint32_t ps4SdkVersion = 0x07008001;
    const std::uint32_t ps5SdkVersion = 0x0a008001;
    std::memcpy(process.data(), &size, sizeof(size));
    std::memcpy(process.data() + 8, &magic, sizeof(magic));
    std::memcpy(process.data() + 0x10, &ps4SdkVersion, sizeof(ps4SdkVersion));
    std::memcpy(process.data() + 0x14, &ps5SdkVersion, sizeof(ps5SdkVersion));
    Require(ProcessParametersFixture::Install(process.data(), process.size()), "process parameters were not installed");

    Require(sceKernelGetCompiledSdkVersion(nullptr) == static_cast<int>(0x80020016), "null output is not EINVAL");
    std::array<std::int32_t, 2> output{-1, 0x5a5a5a5a};
    Require(sceKernelGetCompiledSdkVersion(output.data()) == 0, "query failed");
    Require(output[0] == 0x07008001, "version is not the 32-bit field at offset 0x10 of the process parameters");
    Require(output[1] == 0x5a5a5a5a, "the query wrote past the version");

    Require(sceKernelGetProsperoCompiledSdkVersion(nullptr) == static_cast<int>(0x80020016), "null output is not EINVAL");
    output = {-1, 0x5a5a5a5a};
    Require(sceKernelGetProsperoCompiledSdkVersion(output.data()) == 0, "prospero query failed");
    Require(output[0] == 0x0a008001, "prospero version is not the 32-bit field at offset 0x14 of the process parameters");
    Require(output[1] == 0x5a5a5a5a, "the prospero query wrote past the version");

    Require(sceKernelGetPs4CompiledSdkVersion(nullptr) == static_cast<int>(0x80020016), "null output is not EINVAL");
    output = {-1, 0x5a5a5a5a};
    Require(sceKernelGetPs4CompiledSdkVersion(output.data()) == 0, "ps4 query failed");
    Require(output[0] == 0x07008001, "ps4 version is not the 32-bit field at offset 0x10 of the process parameters");
    Require(output[1] == 0x5a5a5a5a, "the ps4 query wrote past the version");

    std::puts("KERNEL SYS SDK VERSION tests passed");
    return 0;
}
