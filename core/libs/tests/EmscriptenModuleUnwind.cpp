#include "prx/libkernel/include/Api.hpp"
#include "prx/libkernel/KernelErrors.hpp"
#include "prx/libSceSysmodule/include/Api.hpp"
#include <array>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <limits>

namespace {

struct UnsupportedPath {
    const char* function;
};

}

extern "C" [[noreturn]] void NotImplemented_nid_no_patch(const char* function) {
    throw UnsupportedPath{function};
}

int main() {
    for (const auto query : {&sceKernelGetModuleInfoForUnwind, &sceSysmoduleGetModuleInfoForUnwind}) {
        for (const auto address : {std::uint64_t{0}, std::uint64_t{0x10000}, std::numeric_limits<std::uint64_t>::max()}) {
            for (const int flags : {0, 1, -1}) {
                ModuleInfoForUnwind info;
                std::memset(&info, 0xa5, sizeof(info));
                std::array<unsigned char, sizeof(info)> before;
                std::memcpy(before.data(), &info, sizeof(info));
                try {
                    query(address, flags, &info);
                    std::abort();
                } catch (const UnsupportedPath& error) {
                    if (std::strcmp(error.function, "sceKernelGetModuleInfoForUnwind") != 0 ||
                        std::memcmp(before.data(), &info, sizeof(info)) != 0) std::abort();
                }
            }
        }
    }
    if (sceKernelGetModuleInfoForUnwind(0, 0, nullptr) != SCE_KERNEL_ERROR_EFAULT) std::abort();
}
