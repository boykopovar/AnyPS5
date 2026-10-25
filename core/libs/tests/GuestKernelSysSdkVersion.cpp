#include "prx/libkernel_sys/include/Api.hpp"
#include "ProcessParametersFixture.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>

extern "C" int* APS5_VABI __error_nid_postfix();
extern "C" int APS5_VABI getpid_nid_postfix();

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

    Require(sceKernelGetCompiledSdkVersionByPid(getpid_nid_postfix(), nullptr) == static_cast<int>(0x80020016), "by-pid null output is not EINVAL");
    Require(sceKernelGetCompiledSdkVersionByPid(123456, output.data()) == static_cast<int>(0x80020003), "by-pid unknown pid is not ESRCH");
    output[0] = -1;
    Require(sceKernelGetCompiledSdkVersionByPid(0, output.data()) == 0, "by-pid with 0 failed");
    Require(output[0] == 0x07008001, "by-pid with 0 wrote unexpected version");
    output[0] = -1;
    Require(sceKernelGetCompiledSdkVersionByPid(-1, output.data()) == 0, "by-pid with -1 failed");
    Require(output[0] == 0x07008001, "by-pid with -1 wrote unexpected version");
    output[0] = -1;
    Require(sceKernelGetCompiledSdkVersionByPid(getpid_nid_postfix(), output.data()) == 0, "by-pid with current pid failed");
    Require(output[0] == 0x07008001, "by-pid with current pid wrote unexpected version");

    *__error_nid_postfix() = 0;
    Require(get_sdk_compiled_version_nid_postfix(nullptr) == -1, "null output does not fail with -1");
    Require(*__error_nid_postfix() == 14, "null output does not set errno to EFAULT");
    std::array<std::int32_t, 2> sdkOutput{-1, 0x5a5a5a5a};
    *__error_nid_postfix() = 0;
    Require(get_sdk_compiled_version_nid_postfix(sdkOutput.data()) == 0, "syscall query failed");
    Require(*__error_nid_postfix() == 0, "successful query mutated errno");
    Require(sdkOutput[0] == 0x07008001, "syscall version is not the 32-bit field at offset 0x10 of the process parameters");
    Require(sdkOutput[1] == 0x5a5a5a5a, "the syscall query wrote past the version");

    std::puts("KERNEL SYS SDK VERSION tests passed");
    return 0;
}
