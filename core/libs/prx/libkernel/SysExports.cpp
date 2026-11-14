#include "prx/libkernel/include/Api.hpp"
#include "prx/libkernel/AppMetadata/include/AppMetadata.hpp"
#include "prx/libkernel/KernelErrors.hpp"
#include "prx/libc/include/ApplicationHeap.hpp"
#include "prx/libc/include/General.hpp"
#include "prx/libc/include/GuestErrno.hpp"
#include "SceTypes.hpp"
#include <cerrno>
#include <cstddef>
#include <cstdint>
#include <cstring>

namespace {
constexpr std::uint32_t ParamSfoAttributeSupportNeoMode = 0x00800000;
}

extern "C" {

int APS5_VABI sceKernelGetAppCategoryType(int pid, std::uint32_t* categoryType) {
    if (categoryType == nullptr) return SCE_KERNEL_ERROR_EINVAL;
    if (pid < 0) {
        *__error_nid_postfix() = 3;
        return SCE_KERNEL_ERROR_ESRCH;
    }
    if (pid != 0 && pid != getpid_nid_postfix()) NotImplemented_nid_no_patch(__func__);
    std::uint32_t value = 0;
    if (!GetAppCategoryType_nid_no_patch(&value)) NotImplemented_nid_no_patch(__func__);
    if (!GuestCopyOut_nid_no_patch(categoryType, &value, sizeof(value))) return SCE_KERNEL_ERROR_EFAULT;
    return 0;
}

int APS5_VABI sceKernelIsNeoMode(void) {
    std::uint32_t attribute = 0;
    if (!GetAppParamSfoAttribute_nid_no_patch(&attribute)) NotImplemented_nid_no_patch(__func__);
    return (attribute & ParamSfoAttributeSupportNeoMode) != 0 ? 1 : 0;
}

int APS5_VABI sceKernelGetCompiledSdkVersion(std::int32_t* version) {
    if (version == nullptr) return SCE_KERNEL_ERROR_EINVAL;
    static const auto* parameters = static_cast<const std::byte*>(ApplicationProcessParameters_nid_no_patch());
    if (!GuestCopyOut_nid_no_patch(version, parameters + 0x10, sizeof(*version))) return SCE_KERNEL_ERROR_EFAULT;
    return 0;
}

int APS5_VABI sceKernelGetProsperoCompiledSdkVersion(std::int32_t* version) {
    if (version == nullptr) return SCE_KERNEL_ERROR_EINVAL;
    const int saved = errno;
    static const auto* parameters = static_cast<const std::byte*>(ApplicationProcessParameters_nid_no_patch());
    const bool copied = GuestCopyOut_nid_no_patch(version, parameters + 0x14, sizeof(*version));
    errno = saved;
    if (!copied) return SCE_KERNEL_ERROR_EFAULT;
    return 0;
}

int APS5_VABI sceKernelTitleWorkaroundIsEnabled(const KernelTitleWorkaround* workaround, std::int32_t bit, std::int32_t* enabled) {
    if (!workaround || !enabled) return static_cast<int>(0x8002000E);
    if (static_cast<std::uint32_t>(bit) > 0x7F) return static_cast<int>(0x80020016);
    KernelTitleWorkaround localWorkaround{};
    const int saved = errno;
    if (!GuestCopyIn_nid_no_patch(&localWorkaround, workaround, sizeof(localWorkaround))) {
        errno = saved;
        return static_cast<int>(0x8002000E);
    }
    const std::int32_t value = static_cast<std::int32_t>((localWorkaround.ids[bit >> 6] >> (bit & 0x3F)) & 1);
    const bool copied = GuestCopyOut_nid_no_patch(enabled, &value, sizeof(value));
    errno = saved;
    if (!copied) return static_cast<int>(0x8002000E);
    return 0;
}

int APS5_VABI sceKernelAllocateToolMemory(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceKernelAvailableToolMemorySize(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceKernelMapToolMemory(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceKernelReleaseToolMemory(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceKernelGetToolMemoryRange(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceKernelSetProcessProperty(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

}
