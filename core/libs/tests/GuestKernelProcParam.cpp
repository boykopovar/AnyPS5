#include "prx/libkernel/include/Api.hpp"
#include "ProcessParametersFixture.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace {

alignas(std::uint64_t) std::array<std::byte, 64> process{};

void Require(bool condition, const char* what) {
    if (condition) return;
    std::fprintf(stderr, "sceKernelGetProcParam check failed: %s\n", what);
    std::exit(1);
}

}

int main() {
    const std::uint64_t processSize = process.size();
    const std::uint32_t processMagic = 0x4942524f;
    const std::uint32_t sdkVersion = 0x06500000;
    std::memcpy(process.data(), &processSize, sizeof(processSize));
    std::memcpy(process.data() + 8, &processMagic, sizeof(processMagic));
    std::memcpy(process.data() + 16, &sdkVersion, sizeof(sdkVersion));
    Require(ProcessParametersFixture::Install(process.data(), process.size()), "the process parameters were not installed");
    void* const parameters = sceKernelGetProcParam();
    Require(parameters == process.data(), "it did not return the process parameter block");
    Require(sceKernelGetProcParam() == parameters, "a second call returned another block");
    std::puts("sceKernelGetProcParam passed");
    return 0;
}
