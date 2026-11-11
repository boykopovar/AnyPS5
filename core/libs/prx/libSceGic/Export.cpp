#include "prx/libSceGic/include/Api.hpp"
#include "prx/libkernel/File/include/File.hpp"
#include "prx/libkernel/include/Api.hpp"
#include <cstdint>
#include <cstddef>
#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"

namespace {

constexpr int ErrorInvalidArgument = static_cast<int>(0x81680016);
constexpr int ErrorDeviceUnavailable = static_cast<int>(0x816800ff);
constexpr std::uint32_t GetValueCommand = 0xc0104701;

struct Request {
    void* output;
    std::uint32_t reserved;
    std::uint32_t result;
};
static_assert(sizeof(Request) == ((GetValueCommand >> 16) & 0x1fff));

}

extern "C" {

int APS5_VABI sceGicGetValue(void* output) {
    if (output == nullptr) return ErrorInvalidArgument;
    const int descriptor = sceKernelOpen("/dev/gic", 2, 0);
    if (descriptor < 0) return ErrorDeviceUnavailable;
    Request request{output, 0, 0};
    const int status = ioctl_nid_postfix(descriptor, GetValueCommand, &request);
    sceKernelClose(descriptor);
    return status == 0 ? static_cast<int>(request.result) : ErrorDeviceUnavailable;
}

}
