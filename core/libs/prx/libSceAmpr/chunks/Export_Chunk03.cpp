extern "C" {
#include "prx/libc/include/Api.hpp"
#include "prx/libSceAmpr/include/Api.hpp"
#include <cerrno>
#include <cstdint>
#include <cstddef>
#include <ctime>
#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"
#include "prx/libc/include/SonyCrt.hpp"
#include "prx/libkernel/Apr/include/AprCommandBuffer.hpp"
#include "prx/libkernel/Time/include/Time.hpp"
#include "prx/libc/include/GuestHeap.hpp"
#include <algorithm>
#include <array>
#include <atomic>
#include <cerrno>
#include <cstring>
#include <limits>
#include <memory>
#include <mutex>
#include <stdexcept>
#include <string>
#include <thread>
#include <unordered_map>

static constexpr std::size_t MaxNopPayloadBytes = 60u;
static constexpr int SCE_KERNEL_ERROR_EFAULT = 0x8002000E;
static constexpr int SCE_KERNEL_ERROR_EINVAL = 0x80020016;
static constexpr int SCE_KERNEL_ERROR_EPERM = 0x80020001;
static constexpr int SCE_KERNEL_ERROR_ESRCH = 0x80020003;
static constexpr int SCE_KERNEL_ERROR_EBUSY = 0x80020010;
static constexpr int SCE_KERNEL_ERROR_ENOMEM = 0x8002000C;

namespace {

struct ErrnoGuard {
    int saved{errno};
    ~ErrnoGuard() { errno = saved; }
};

template <typename T>
bool CopyGuestIn(T& dst, const void* src) {
    const ErrnoGuard guard;
    return GuestCopyIn_nid_no_patch(&dst, src, sizeof(T));
}

template <typename T>
bool CopyGuestOut(void* dst, const T& src) {
    const ErrnoGuard guard;
    return GuestCopyOut_nid_no_patch(dst, &src, sizeof(T));
}

bool CopyGuestInBytes(void* dst, const void* src, std::size_t bytes) {
    const ErrnoGuard guard;
    return GuestCopyIn_nid_no_patch(dst, src, bytes);
}

bool CopyGuestOutBytes(void* dst, const void* src, std::size_t bytes) {
    const ErrnoGuard guard;
    return GuestCopyOut_nid_no_patch(dst, src, bytes);
}

bool CopyGuestCString(std::string& dst, const char* src, std::size_t maximum) {
    const ErrnoGuard guard;
    return CopyGuestString_nid_no_patch(src, maximum, dst) == 0;
}

}

std::int64_t APS5_VABI sceAmprAmmMeasureAmmCommandSizeModifyMtypeProtect(std::uint64_t address, std::uint64_t size, std::int32_t type, std::int32_t protection, std::int32_t mask) {
    (void)type;
    return ValidProtect(address, size, protection, mask) ? std::int64_t{sizeof(Apr::AmmProtectCommand)} : AmmMeasureInvalid;
}

std::int64_t APS5_VABI sceAmprAmmMeasureAmmCommandSizeModifyMtypeProtectWithGpuMaskId(std::uint64_t address, std::uint64_t size, std::int32_t type, std::int32_t protection, std::int32_t mask, std::uint8_t gpuMaskId) {
    (void)gpuMaskId;
    return sceAmprAmmMeasureAmmCommandSizeModifyMtypeProtect(address, size, type, protection, mask);
}

int APS5_VABI sceAmprAmmCommandBufferMapAsPrt(Apr::CommandBufferObject* buffer, std::uint64_t address, std::uint64_t size) {
    if (!buffer) return SCE_KERNEL_ERROR_EINVAL;
    if (!ValidMapRange(address, size)) return SCE_KERNEL_ERROR_EINVAL;
    const Apr::AmmUnmapCommand command{{Apr::Opcode::AmmMapAsPrt, sizeof(command)}, address, size};
    return RecordPrt(buffer, &command, sizeof(command));
}

int APS5_VABI sceAmprAmmCommandBufferAllocatePaForPrt(Apr::CommandBufferObject* buffer, std::uint64_t address, std::uint64_t size, std::int32_t type, std::int32_t protection) {
    if (!ValidAmmProtection(protection)) return SCE_KERNEL_ERROR_EINVAL;
    if (!buffer) return SCE_KERNEL_ERROR_EINVAL;
    if (!ValidMapRange(address, size)) return SCE_KERNEL_ERROR_EINVAL;
    const Apr::AmmProtectCommand command{{Apr::Opcode::AmmAllocatePaForPrt, sizeof(command)}, address, size, type, protection, PrtAllocationMask, 0};
    return RecordPrt(buffer, &command, sizeof(command));
}

int APS5_VABI sceAmprAmmCommandBufferRemapIntoPrt(Apr::CommandBufferObject* buffer, std::uint64_t address, std::uint64_t source, std::uint64_t size, std::int32_t protection, std::uint32_t opcode) {
    if (!buffer) return SCE_KERNEL_ERROR_EINVAL;
    if (!buffer->base) return SCE_KERNEL_ERROR_EPERM;
    if (!ValidRemap(address, source, size, protection)) return SCE_KERNEL_ERROR_EINVAL;
    const Apr::AmmRemapCommand command{{Apr::Opcode::AmmRemapIntoPrt, sizeof(command)}, address, source, size, protection, static_cast<std::int32_t>(opcode ? opcode : 1011u)};
    return AppendAmm(buffer, &command, sizeof(command));
}

int APS5_VABI sceAmprAmmCommandBufferUnmapToPrt(Apr::CommandBufferObject* buffer, std::uint64_t address, std::uint64_t size) {
    if (!buffer) return SCE_KERNEL_ERROR_EINVAL;
    if (!buffer->base) return SCE_KERNEL_ERROR_EPERM;
    if (!ValidMapRange(address, size)) return SCE_KERNEL_ERROR_EINVAL;
    const Apr::AmmUnmapCommand command{{Apr::Opcode::AmmUnmapToPrt, sizeof(command)}, address, size};
    return AppendAmm(buffer, &command, sizeof(command));
}

std::int64_t APS5_VABI sceAmprAmmMeasureAmmCommandSizeMapAsPrt(std::uint64_t address, std::uint64_t size) {
    return ValidMapRange(address, size) ? std::int64_t{sizeof(Apr::AmmUnmapCommand)} : AmmMeasureInvalid;
}

std::int64_t APS5_VABI sceAmprAmmMeasureAmmCommandSizeAllocatePaForPrt(std::uint64_t address, std::uint64_t size, std::int32_t type, std::int32_t protection) {
    (void)type;
    return ValidMapRange(address, size) && ValidAmmProtection(protection) ? std::int64_t{sizeof(Apr::AmmProtectCommand)} : AmmMeasureInvalid;
}

int APS5_VABI sceAmprAmmGiveDirectMemory(std::int64_t searchStart, std::int64_t searchEnd, std::size_t size, std::size_t alignment, int usage, std::int64_t* offset) {
    return AmmGiveDirectMemory_nid_no_patch(searchStart, searchEnd, size, alignment, usage, offset);
}

int APS5_VABI sceAmprAmmGetVirtualAddressRanges(std::uint64_t* start, std::uint64_t* end, std::uint64_t* multimapStart, std::uint64_t* multimapEnd) {
    if (!start || !end || !multimapStart || !multimapEnd) throw std::invalid_argument("sceAmprAmmGetVirtualAddressRanges: null output");
    std::uint64_t localStart = 0;
    std::uint64_t localEnd = 0;
    std::uint64_t localMultimapStart = 0;
    std::uint64_t localMultimapEnd = 0;
    AmmVirtualAddressRanges_nid_no_patch(&localStart, &localEnd, &localMultimapStart, &localMultimapEnd);
    if (!CopyGuestOut(start, localStart) || !CopyGuestOut(end, localEnd) ||
        !CopyGuestOut(multimapStart, localMultimapStart) || !CopyGuestOut(multimapEnd, localMultimapEnd))
        return SCE_KERNEL_ERROR_EFAULT;
    return 0;
}

int APS5_VABI sceAmprAmmSetPageTablePoolOccupancyNotificationThreshold(int threshold) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceAmprAmmSubmitCommandBuffer(void* base, std::uint32_t offset, std::uint32_t priority) {
    if (priority > 2u) return SCE_KERNEL_ERROR_EINVAL;
    if (!base) return SCE_KERNEL_ERROR_EPERM;
    AmmSubmit_nid_no_patch(ResolveBufferBase(base), offset);
    return 0;
}

int APS5_VABI sceAmprAmmSubmitCommandBuffer2(void* base, std::uint32_t offset, std::uint32_t priority, AmmSubmitResult* result, std::uint32_t* id) {
    if (priority > 2u) return SCE_KERNEL_ERROR_EINVAL;
    if (!base) return SCE_KERNEL_ERROR_EPERM;
    const auto submitted = AmmSubmit_nid_no_patch(ResolveBufferBase(base), offset);
    if (result) {
        const AmmSubmitResult value{0, 0};
        if (!CopyGuestOut(result, value)) return SCE_KERNEL_ERROR_EFAULT;
    }
    if (id && !CopyGuestOut(id, submitted)) return SCE_KERNEL_ERROR_EFAULT;
    return 0;
}

int APS5_VABI sceAmprAmmSubmitCommandBuffer3(void* base, std::uint32_t offset, std::uint32_t priority, std::uint32_t* id) {
    if (priority > 2u) return SCE_KERNEL_ERROR_EINVAL;
    if (!base) return SCE_KERNEL_ERROR_EPERM;
    const auto submitted = AmmSubmit_nid_no_patch(ResolveBufferBase(base), offset);
    if (id && !CopyGuestOut(id, submitted)) return SCE_KERNEL_ERROR_EFAULT;
    return 0;
}

int APS5_VABI sceAmprAmmWaitCommandBufferCompletion(std::uint32_t id) {
    return AmmSubmitted_nid_no_patch(id) ? 0 : SCE_KERNEL_ERROR_ESRCH;
}

int APS5_VABI _ZNKSt8time_getIcSt19istreambuf_iteratorIcSt11char_traitsIcEEE3getES3_S3_RSt8ios_baseRNSt5_IosbIiE8_IostateEP2tmPKcSE__nid_postfix(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI _ZNSt8time_getIcSt19istreambuf_iteratorIcSt11char_traitsIcEEE7_GetcatEPPKNSt6locale5facetEPKS5__nid_postfix(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

APS5_EXPORT("0hlfW1O4Aa4", sceAmprLocaleconv);
SonyLconv* APS5_VABI sceAmprLocaleconv(void) {
    return SonyLocaleconv_nid_no_patch();
}

APS5_EXPORT("802pFCwC9w0", sceAmprUdivti3);
unsigned __int128 APS5_VABI sceAmprUdivti3(unsigned __int128 dividend, unsigned __int128 divisor) {
    return SonyUdivti3(dividend, divisor);
}

APS5_EXPORT("MU25eqxSDTw", sceAmprSinh);
double APS5_VABI sceAmprSinh(double x, double y) {
    return SonySinh(x, y);
}

APS5_EXPORT("-vXEQdRADLI", sceAmprStrtokS);
char* APS5_VABI sceAmprStrtokS(char* str, std::size_t* remaining, const char* delimiters, char** context) {
    return SonyStrtokS(str, remaining, delimiters, context);
}

APS5_EXPORT("2UFh+YKfuzk", sceAmprCtimeS);
int APS5_VABI sceAmprCtimeS(char* buffer, std::size_t size, const std::int64_t* time) {
    return SonyCtimeS(buffer, size, time);
}

APS5_EXPORT("6f5f-qx4ucA", sceAmprWcscpyS);
int APS5_VABI sceAmprWcscpyS(std::uint16_t* dest, std::size_t size, const std::uint16_t* src) {
    return SonyWcscpyS(dest, size, src);
}

APS5_EXPORT("ENLfKJEZTjE", sceAmprSetConstraintHandlerS);
void* APS5_VABI sceAmprSetConstraintHandlerS(void* handler) {
    return SonySetConstraintHandlerS_nid_no_patch(handler);
}

APS5_EXPORT("Jo9ON-AX9eU", sceAmprFltrounds);
int APS5_VABI sceAmprFltrounds(void) {
    return SonyFltrounds();
}

APS5_EXPORT("ODGONXcSmz4", sceAmprIgnoreHandlerS);
void APS5_VABI sceAmprIgnoreHandlerS(const char* message, void* pointer, int error) {
    SonyIgnoreHandlerS_nid_no_patch(message, pointer, error);
}

APS5_EXPORT("sZLrjx-yEx4", sceAmprWcstombsS);
int APS5_VABI sceAmprWcstombsS(std::size_t* result, char* destination, std::size_t capacity, const std::uint16_t* source, std::size_t limit) {
    return WideToMultibyteChecked_nid_no_patch(result, destination, capacity, source, limit);
}

APS5_EXPORT("VUzjXknPPBs", sceAmprMbstowcs);
std::size_t APS5_VABI sceAmprMbstowcs(std::uint16_t* dest, const char* src, std::size_t size) {
    return SonyMbstowcs(dest, src, size);
}

APS5_EXPORT("qPe7-h5Jnuc", sceAmprAsctimeS);
int APS5_VABI sceAmprAsctimeS(char* buffer, std::size_t size, const GuestTm* time) {
    return SonyAsctimeS(buffer, size, time);
}

APS5_EXPORT("qlWiRfOJx1A", sceAmprFpclassifyd);
int APS5_VABI sceAmprFpclassifyd(double value) {
    return SonyFpclassifyd(value);
}

int APS5_VABI sceAmprAmmGetUsageStatsData(AmmUsageStatsData* stats) {
    if (!stats) return SCE_KERNEL_ERROR_EINVAL;
    std::uint64_t sizeInBytes = 0;
    if (!CopyGuestIn(sizeInBytes, stats)) return SCE_KERNEL_ERROR_EFAULT;
    if (sizeInBytes > sizeof(AmmUsageStatsData)) return SCE_KERNEL_ERROR_EINVAL;
    AmmUsageStatsData out{};
    out.sizeInBytes = sizeInBytes;
    out.numPageTablePoolEntries = AmmPageTablePoolEntries;
    out.ringIdleFlags = AmmRingIdleFlags;
    if (!CopyGuestOutBytes(stats, &out, static_cast<std::size_t>(sizeInBytes))) return SCE_KERNEL_ERROR_EFAULT;
    return 0;
}
}
