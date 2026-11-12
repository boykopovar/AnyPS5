#include "prx/libkernel_sys/include/Api.hpp"
#include "prx/libkernel_sys/include/AdditionalExports.hpp"
#include "ProcessParametersFixture.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"

#include <array>
#include <cerrno>
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
    Require(sceKernelGetCompiledSdkVersion(reinterpret_cast<std::int32_t*>(0x10)) == static_cast<int>(0x8002000e), "unwritable output is not EFAULT");
    std::array<std::int32_t, 2> output{-1, 0x5a5a5a5a};
    Require(sceKernelGetCompiledSdkVersion(output.data()) == 0, "query failed");
    Require(output[0] == 0x07008001, "version is not the 32-bit field at offset 0x10 of the process parameters");
    Require(output[1] == 0x5a5a5a5a, "the query wrote past the version");

    Require(sceKernelGetCompiledSdkVersionByPid(0, nullptr) == static_cast<int>(0x80020016), "pid 0 null output is not EINVAL");
    Require(sceKernelGetCompiledSdkVersionByPid(0, reinterpret_cast<std::int32_t*>(0x10)) == static_cast<int>(0x8002000e), "pid 0 unwritable output is not EFAULT");
    output = {-1, 0x5a5a5a5a};
    Require(sceKernelGetCompiledSdkVersionByPid(0, output.data()) == 0, "pid 0 query failed");
    Require(output[0] == 0x07008001, "pid 0 version is not ps4 sdk version");
    Require(output[1] == 0x5a5a5a5a, "pid 0 query wrote past version");

    output = {-1, 0x5a5a5a5a};
    Require(sceKernelGetCompiledSdkVersionByPid(-1, output.data()) == 0, "pid -1 query failed");
    Require(output[0] == 0x07008001, "pid -1 version is not ps4 sdk version");

    output = {-1, 0x5a5a5a5a};
    Require(sceKernelGetCompiledSdkVersionByPid(getpid_nid_postfix(), output.data()) == 0, "own pid query failed");
    Require(output[0] == 0x07008001, "own pid version is not ps4 sdk version");

    Require(sceKernelGetCompiledSdkVersionByPid(getpid_nid_postfix() + 1000, output.data()) == static_cast<int>(0x80020003), "other pid is not ESRCH");

    errno = 0x42;
    Require(sceKernelGetProsperoCompiledSdkVersion(nullptr) == static_cast<int>(0x80020016), "null output is not EINVAL");
    Require(errno == 0x42, "errno was not preserved on null prospero output");
    errno = 0x43;
    Require(sceKernelGetProsperoCompiledSdkVersion(reinterpret_cast<std::int32_t*>(0x10)) == static_cast<int>(0x8002000e), "unwritable prospero output is not EFAULT");
    Require(errno == 0x43, "errno was not preserved on unwritable prospero output");
    output = {-1, 0x5a5a5a5a};
    errno = 0x44;
    Require(sceKernelGetProsperoCompiledSdkVersion(output.data()) == 0, "prospero query failed");
    Require(errno == 0x44, "errno was not preserved on prospero query");
    Require(output[0] == 0x0a008001, "prospero version is not the 32-bit field at offset 0x14 of the process parameters");
    Require(output[1] == 0x5a5a5a5a, "the prospero query wrote past the version");

    Require(sceKernelGetProsperoCompiledSdkVersionByPid(0, nullptr) == static_cast<int>(0x80020016), "prospero pid 0 null output is not EINVAL");
    output = {-1, 0x5a5a5a5a};
    Require(sceKernelGetProsperoCompiledSdkVersionByPid(0, output.data()) == 0, "prospero pid 0 query failed");
    Require(output[0] == 0x0a008001, "prospero pid 0 version is not ps5 sdk version");
    Require(output[1] == 0x5a5a5a5a, "prospero pid 0 query wrote past version");

    output = {-1, 0x5a5a5a5a};
    Require(sceKernelGetProsperoCompiledSdkVersionByPid(-1, output.data()) == 0, "prospero pid -1 query failed");
    Require(output[0] == 0x0a008001, "prospero pid -1 version is not ps5 sdk version");

    output = {-1, 0x5a5a5a5a};
    Require(sceKernelGetProsperoCompiledSdkVersionByPid(getpid_nid_postfix(), output.data()) == 0, "prospero own pid query failed");
    Require(output[0] == 0x0a008001, "prospero own pid version is not ps5 sdk version");

    Require(sceKernelGetProsperoCompiledSdkVersionByPid(getpid_nid_postfix() + 1000, output.data()) == static_cast<int>(0x80020003), "prospero other pid is not ESRCH");

    errno = 0x42;
    Require(sceKernelGetPs4CompiledSdkVersion(nullptr) == static_cast<int>(0x80020016), "null output is not EINVAL");
    Require(errno == 0x42, "errno was not preserved on null ps4 output");
    errno = 0x43;
    Require(sceKernelGetPs4CompiledSdkVersion(reinterpret_cast<std::int32_t*>(0x10)) == static_cast<int>(0x8002000e), "unwritable ps4 output is not EFAULT");
    Require(errno == 0x43, "errno was not preserved on unwritable ps4 output");
    output = {-1, 0x5a5a5a5a};
    errno = 0x44;
    Require(sceKernelGetPs4CompiledSdkVersion(output.data()) == 0, "ps4 query failed");
    Require(errno == 0x44, "errno was not preserved on ps4 query");
    Require(output[0] == 0x07008001, "ps4 version is not the 32-bit field at offset 0x10 of the process parameters");
    Require(output[1] == 0x5a5a5a5a, "the ps4 query wrote past the version");

    Require(sceKernelGetPs4CompiledSdkVersionByPid(0, nullptr) == static_cast<int>(0x80020016), "ps4 pid 0 null output is not EINVAL");
    output = {-1, 0x5a5a5a5a};
    Require(sceKernelGetPs4CompiledSdkVersionByPid(0, output.data()) == 0, "ps4 pid 0 query failed");
    Require(output[0] == 0x07008001, "ps4 pid 0 version is not ps4 sdk version");
    Require(output[1] == 0x5a5a5a5a, "ps4 pid 0 query wrote past version");

    output = {-1, 0x5a5a5a5a};
    Require(sceKernelGetPs4CompiledSdkVersionByPid(-1, output.data()) == 0, "ps4 pid -1 query failed");
    Require(output[0] == 0x07008001, "ps4 pid -1 version is not ps4 sdk version");

    output = {-1, 0x5a5a5a5a};
    Require(sceKernelGetPs4CompiledSdkVersionByPid(getpid_nid_postfix(), output.data()) == 0, "ps4 own pid query failed");
    Require(output[0] == 0x07008001, "ps4 own pid version is not ps4 sdk version");

    Require(sceKernelGetPs4CompiledSdkVersionByPid(getpid_nid_postfix() + 1000, output.data()) == static_cast<int>(0x80020003), "ps4 other pid is not ESRCH");

    std::puts("KERNEL SYS SDK VERSION tests passed");
    return 0;
}
