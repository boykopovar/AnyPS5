#include "prx/libSceVcodec/include/InventoryExports.hpp"
#include "prx/libc/include/general/ExportMacros.hpp"
#include "prx/libc/include/GuestErrno.hpp"
#include "prx/libkernel/include/Api.hpp"
#include <array>
#include <cerrno>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <mutex>
#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"

namespace {

constexpr int ErrorPermission = static_cast<int>(0x80bf0000);
constexpr int ErrorInvalidArgument = static_cast<int>(0x80bf0001);
constexpr int ErrorNoSpace = static_cast<int>(0x80bf0002);
constexpr int ErrorExists = static_cast<int>(0x80bf0003);
constexpr int ErrorBusy = static_cast<int>(0x80bf0004);
constexpr int ErrorNoProcess = static_cast<int>(0x80bf0005);
constexpr int ErrorUnknown = static_cast<int>(0x80bf0800);
constexpr std::uint32_t OpenCommand = 0x9b508328;
constexpr std::uint32_t CloseCommand = 0x9b508329;
constexpr std::uint32_t AttachCommand = 0x9b50832a;
constexpr std::uint32_t ClassCommand = 0xdb50832b;
constexpr std::uint32_t SizeCommand = 0x9b50832c;
constexpr std::uint32_t TypeCommand = 0x9b50832d;
constexpr std::uint32_t StatusCommand = 0xdb50832e;
constexpr std::size_t RequestBytes = 0x1b50;
constexpr std::size_t AppInfoBytes = 0x98;

struct Request {
    std::uint32_t fields[RequestBytes / sizeof(std::uint32_t)];
};
static_assert(sizeof(Request) == RequestBytes);

std::once_flag g_opened;
int g_descriptor = -1;

int Descriptor() {
    std::call_once(g_opened, [] { g_descriptor = open_nid_postfix("/dev/uvd_bgt", 2, 0); });
    return g_descriptor;
}

int ErrorFromErrno(int error) {
    switch (error) {
    case 1:
    case 9:
        return ErrorPermission;
    case 3:
        return ErrorNoProcess;
    case 16:
        return ErrorBusy;
    case 17:
        return ErrorExists;
    case 22:
        return ErrorInvalidArgument;
    case 28:
        return ErrorNoSpace;
    default:
        return ErrorUnknown;
    }
}

template <std::uint32_t TCommand>
int Control(Request& request) {
    static_assert(((TCommand >> 16) & 0x1fff) == RequestBytes);
    const int result = ioctl_nid_postfix(Descriptor(), TCommand, &request);
    if (result < 0) return ErrorFromErrno(*__error_nid_postfix());
    return result;
}

std::uint32_t AppIdOf(std::int32_t pid) {
    std::array<std::uint8_t, AppInfoBytes> info{};
    if (sceKernelGetAppInfo(pid, info.data()) < 0) return 0xffffffff;
    std::uint32_t appId = 0;
    std::memcpy(&appId, info.data(), sizeof(appId));
    return appId;
}

std::uint32_t BudgetClass(const std::uint32_t* config) {
    const std::uint32_t mode = config[0];
    const std::uint32_t width = config[1];
    const std::uint32_t height = config[2];
    const std::uint32_t profile = config[4];
    if (mode == 0) return profile == 1 ? 1 : 0;
    if (mode != 1) return 0;
    if (profile == 200) {
        if (width > 0x500) return width < 0x781 && height < 0x441 ? 2 : 4;
        if (height < 0x301) return 1;
        return height < 0x441 ? 2 : 4;
    }
    if (profile != 0 || width > 0x3c0) return 0;
    return height < 0x241 ? 1 : 0;
}

}

extern "C" {

APS5_DUMMY_FUN

int APS5_VABI sceVcodecOpenHwBudget(std::int32_t pid, std::uint32_t value) {
    Request request{};
    request.fields[0] = AppIdOf(pid);
    request.fields[1] = static_cast<std::uint32_t>(pid);
    request.fields[2] = value;
    return Control<OpenCommand>(request);
}

int APS5_VABI sceVcodecOpenHwBudgetByAppid(std::uint32_t appId, std::uint32_t value) {
    Request request{};
    request.fields[0] = appId;
    request.fields[1] = 0xffffffff;
    request.fields[2] = value;
    return Control<OpenCommand>(request);
}

int APS5_VABI sceVcodecCloseHwBudget(std::uint32_t budget) {
    Request request{};
    request.fields[0] = budget;
    return Control<CloseCommand>(request);
}

int APS5_VABI libSceVcodecUnknown_X0qscgxx1MI_nid_no_patch(std::uint32_t budget, std::int32_t pid) {
    Request request{};
    request.fields[0] = budget;
    request.fields[1] = AppIdOf(pid);
    request.fields[2] = static_cast<std::uint32_t>(pid);
    return Control<AttachCommand>(request);
}
APS5_EXPORT("X0qscgxx1MI", libSceVcodecUnknown_X0qscgxx1MI_nid_no_patch);

int APS5_VABI libSceVcodecUnknown_Ipf4PM5PIHE_nid_no_patch(std::uint64_t* result, const void* config) {
    const int savedErrno = errno;
    if (result != nullptr) {
        std::uint64_t zero = 0;
        if (!GuestCopyOut_nid_no_patch(result, &zero, sizeof(zero))) {
            errno = savedErrno;
            return ErrorInvalidArgument;
        }
    }
    if (config == nullptr) return ErrorInvalidArgument;
    std::uint32_t fields[5]{};
    if (!GuestCopyIn_nid_no_patch(fields, config, sizeof(fields))) {
        errno = savedErrno;
        return ErrorInvalidArgument;
    }
    Request request{};
    std::memcpy(&request.fields[1], fields, 4 * sizeof(std::uint32_t));
    request.fields[5] = BudgetClass(fields);
    const int status = Control<ClassCommand>(request);
    if (status >= 0 && result != nullptr) {
        const std::uint64_t value = request.fields[0];
        if (!GuestCopyOut_nid_no_patch(result, &value, sizeof(value))) {
            errno = savedErrno;
            return ErrorInvalidArgument;
        }
    }
    return status;
}
APS5_EXPORT("Ipf4PM5PIHE", libSceVcodecUnknown_Ipf4PM5PIHE_nid_no_patch);

int APS5_VABI sceVcodecSetHwBudgetSize(std::uint32_t budget, std::uint32_t size) {
    Request request{};
    request.fields[0] = budget;
    request.fields[1] = size;
    return Control<SizeCommand>(request);
}

int APS5_VABI sceVcodecSetHwBudgetType(std::uint32_t budget, std::uint32_t first, std::uint32_t second) {
    Request request{};
    request.fields[0] = budget;
    request.fields[1] = first;
    request.fields[2] = second;
    return Control<TypeCommand>(request);
}

int APS5_VABI libSceVcodecUnknown_Ac6aYjr1pY8_nid_no_patch(void* output) {
    Request request{};
    const int status = Control<StatusCommand>(request);
    if (status < 0 || output == nullptr) return status;
    NotImplemented_nid_no_patch("Ac6aYjr1pY8: translating the device's budget status into the caller's structure");
    return status;
}
APS5_EXPORT("Ac6aYjr1pY8", libSceVcodecUnknown_Ac6aYjr1pY8_nid_no_patch);

}
