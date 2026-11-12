#include "prx/libSceDiscId/include/Api.hpp"
#include "prx/libc/include/GuestErrno.hpp"
#include "prx/libkernel/File/include/File.hpp"
#include "prx/libkernel/include/Api.hpp"
#include <cstdint>
#include <cstddef>
#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"

namespace {

constexpr int ErrorInvalidArgument = static_cast<int>(0x81680016);
constexpr int ErrorDeviceUnavailable = static_cast<int>(0x816800ff);
constexpr std::uint32_t GetCommand = 0xc0104401;
constexpr std::uint32_t Get2Command = 0xc0104402;

struct Request {
    void* output;
    std::uint32_t value;
    std::uint32_t result;
};
static_assert(sizeof(Request) == ((GetCommand >> 16) & 0x1fff));
static_assert(sizeof(Request) == ((Get2Command >> 16) & 0x1fff));

int Send(std::uint32_t command, Request& request) {
    const int descriptor = sceKernelOpen("/dev/duid", 2, 0);
    if (descriptor < 0) return ErrorDeviceUnavailable;
    const int status = ioctl_nid_postfix(descriptor, command, &request);
    sceKernelClose(descriptor);
    return status == 0 ? 0 : *__error_nid_postfix();
}

}

extern "C" {

int APS5_VABI sceDiscIdGet(DiscId* discId) {
    if (discId == nullptr) return ErrorInvalidArgument;
    Request request{discId, 0, 0};
    const int status = Send(GetCommand, request);
    return status == 0 ? static_cast<int>(request.value) : status;
}

int APS5_VABI sceDiscIdGet2(DiscId2* discId, std::uint32_t flags) {
    if (discId == nullptr || flags != 0) return ErrorInvalidArgument;
    Request request{discId, 0, 0};
    const int status = Send(Get2Command, request);
    return status == 0 ? static_cast<int>(request.result) : status;
}

}
