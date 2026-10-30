#include "prx/libSceAddressSanitizer/include/Api.hpp"
#include "prx/libSceAddressSanitizer/src/ShadowAddressSpace.hpp"
#include "prx/libSceAddressSanitizer/src/ShadowCheck.hpp"
#include "prx/libSceAddressSanitizer/src/ErrorReport.hpp"
#include "prx/libSceAddressSanitizer/src/Report.hpp"
#include "prx/libSceAddressSanitizer/src/Container.hpp"
#include "prx/libSceAddressSanitizer/src/Ubsan.hpp"
#include <algorithm>
#include <atomic>
#include <cstdio>
#include <cstdint>
#include <cstring>
#include <limits>
#include <mutex>
#include <string>
#include <cstddef>
#include "SceTypes.hpp"
#include "prx/libc/include/Api.hpp"
#include "prx/libc/include/General.hpp"

static void RequireShadow_nid_no_patch(std::uintptr_t address, std::uintptr_t size, const char* function) {
    const auto begin = AsanRuntime::ShadowBase + (address >> 3);
    const auto end = size == 0 ? begin : AsanRuntime::ShadowBase + ((address + size - 1) >> 3) + 1;
    if (!AsanRuntime::ShadowRangeAvailable_nid_no_patch(begin, std::min(end, AsanRuntime::ShadowBase + AsanRuntime::ShadowSize))) {
        ShadowUnavailable_nid_no_patch(function);
    }
}

static void UpdateIntraObjectShadow_nid_no_patch(std::uintptr_t address, std::uintptr_t size, bool poison, const char* function) {
    const auto end = address + size;
    if (size == 0 || size > 4096 || end < address || (end & 7) != 0 || end > (1ull << 47) ||
        (address < AsanRuntime::ShadowBase + AsanRuntime::ShadowSize && end > AsanRuntime::ShadowBase)) {
        InvalidArgument_nid_no_patch(function);
    }
    RequireShadow_nid_no_patch(address, size, function);
    auto* shadow = reinterpret_cast<std::uint8_t*>(AsanRuntime::ShadowBase + (address >> 3));
    auto* shadowEnd = reinterpret_cast<std::uint8_t*>(AsanRuntime::ShadowBase + (end >> 3));
    if ((address & 7) != 0) {
        *shadow++ = poison ? static_cast<std::uint8_t>(address & 7) : 0;
    }
    while (shadow != shadowEnd) {
        *shadow++ = poison ? 0xbb : 0;
    }
}

constexpr bool kPoisonArrayCookie = true;
constexpr std::uint8_t kAsanArrayCookieMagic = 0xac;
constexpr std::uint8_t kAsanHeapFreeMagic = 0xfd;

constexpr std::uintptr_t kAllocaRedzoneSize = 32;
constexpr std::uintptr_t kAllocaRedzoneMask = 31;
constexpr bool kPoisonPartial = true;
constexpr std::uint8_t kAsanAllocaLeftMagic = 0xca;
constexpr std::uint8_t kAsanAllocaRightMagic = 0xcb;

static void FastPoisonShadow_nid_no_patch(std::uintptr_t address, std::uintptr_t size, std::uint8_t value) {
    std::memset(reinterpret_cast<void*>(AsanRuntime::ShadowBase + (address >> 3)), value, size >> 3);
}

static void FastPoisonShadowPartialRightRedzone_nid_no_patch(std::uintptr_t address, std::uintptr_t sz, std::uintptr_t redzoneSize, std::uint8_t value) {
    auto* shadow = reinterpret_cast<std::int8_t*>(AsanRuntime::ShadowBase + (address >> 3));
    for (std::uintptr_t i = 0; i < redzoneSize; i += 8, ++shadow) {
        if (i + 8 <= sz) {
            *shadow = 0;
        } else if (i >= sz) {
            *shadow = static_cast<std::int8_t>(value);
        } else {
            *shadow = static_cast<std::int8_t>(kPoisonPartial ? (sz - i) : 0);
        }
    }
}

constexpr bool kAllowUserPoisoning = true;
constexpr std::int8_t kAsanUserPoisonedMemoryMagic = static_cast<std::int8_t>(0xf7);

namespace {

struct ShadowSegmentEndpoint {
    std::int8_t* chunk;
    std::int8_t offset;
    std::int8_t value;

    explicit ShadowSegmentEndpoint(std::uintptr_t a)
        : chunk(reinterpret_cast<std::int8_t*>(AsanRuntime::ShadowBase + (a >> 3))),
          offset(static_cast<std::int8_t>(a & 7)),
          value(chunk < reinterpret_cast<std::int8_t*>(AsanRuntime::ShadowBase + AsanRuntime::ShadowSize) ? *chunk : 0) {}
};

}  // namespace

namespace AsanRuntime {

extern "C" ErrorReport* CurrentErrorReport_nid_no_patch() noexcept {
    static ErrorReport report{};
    return &report;
}

static std::atomic<ErrorReportCallback> errorReportCallback{nullptr};

extern "C" ErrorReportCallback LoadErrorReportCallback_nid_no_patch() noexcept {
    return errorReportCallback.load(std::memory_order_acquire);
}

}  // namespace AsanRuntime

constexpr int kExitCode = 1;

static std::mutex reportMutex;
static thread_local bool reportInProgress = false;
static std::atomic<void (APS5_VABI*)()> deathCallback{nullptr};

namespace AsanRuntime {

extern "C" [[noreturn]] void Die_nid_no_patch() {
    if (const auto callback = deathCallback.load(std::memory_order_acquire)) {
        callback();
    }
    _Exit_nid_postfix(kExitCode);
}

void EmitDiagnostic_nid_no_patch(const std::string& text) {
    std::lock_guard lock(reportMutex);
    std::fputs(text.c_str(), stderr);
    std::fflush(stderr);
}

[[noreturn]] void ReportFatal_nid_no_patch(const ErrorReport& state, const std::string& text) {
    if (reportInProgress) {
        Die_nid_no_patch();
    }
    reportInProgress = true;
    std::unique_lock lock(reportMutex);
    *CurrentErrorReport_nid_no_patch() = state;
    std::fputs(text.c_str(), stderr);
    std::fflush(stderr);
    if (const auto callback = LoadErrorReportCallback_nid_no_patch()) {
        callback(text.c_str());
    }
    std::fputs("ABORTING\n", stderr);
    std::fflush(stderr);
    Die_nid_no_patch();
}

}  // namespace AsanRuntime

using AsanRuntime::Die_nid_no_patch;

[[noreturn]] static void ReportGenericError_nid_no_patch(std::uintptr_t pc, std::uintptr_t bp, std::uintptr_t sp, std::uintptr_t address, bool isWrite, std::uintptr_t accessSize) {
    const bool inMemory = address < (1ULL << 47) &&
        !(address >= AsanRuntime::ShadowBase && address < AsanRuntime::ShadowBase + AsanRuntime::ShadowSize) &&
        AsanRuntime::ShadowRangeAvailable_nid_no_patch(AsanRuntime::ShadowBase + (address >> 3), std::min(AsanRuntime::ShadowBase + (address >> 3) + 3, AsanRuntime::ShadowBase + AsanRuntime::ShadowSize));
    const char* bug = "unknown-crash";
    std::uint8_t shadowValue = 0;
    if (inMemory) {
        bug = AsanRuntime::DescribeBug_nid_no_patch(reinterpret_cast<const std::uint8_t*>(AsanRuntime::ShadowBase + (address >> 3)), accessSize, shadowValue);
    }
    std::string text;
    AsanRuntime::AppendReportHeader_nid_no_patch(text, bug, address, pc, bp, sp, isWrite, accessSize);
    const auto dumpCenter = (AsanRuntime::ShadowBase + (address >> 3)) & ~static_cast<std::uintptr_t>(15);
    if (inMemory && AsanRuntime::ShadowRangeAvailable_nid_no_patch(std::max(dumpCenter - 80, AsanRuntime::ShadowBase), std::min(dumpCenter + 96, AsanRuntime::ShadowBase + AsanRuntime::ShadowSize))) {
        AsanRuntime::AppendShadowMemory_nid_no_patch(text, AsanRuntime::ShadowBase, AsanRuntime::ShadowSize, address);
    }
    AsanRuntime::ReportFatal_nid_no_patch({true, pc, bp, sp, address, isWrite, accessSize, bug, false}, text);
}

static void RequireGuestRange_nid_no_patch(std::uintptr_t address, std::uintptr_t size, const char* function) {
    const auto end = address + size;
    if (end < address || end > (1ULL << 47) ||
        (address < AsanRuntime::ShadowBase + AsanRuntime::ShadowSize && end > AsanRuntime::ShadowBase)) {
        InvalidArgument_nid_no_patch(function);
    }
    RequireShadow_nid_no_patch(address, size, function);
}

static void CheckRegion_nid_no_patch(std::uintptr_t address, std::uintptr_t size, bool isWrite, std::uintptr_t pc, std::uintptr_t bp) {
    if (const auto bad = __asan_region_is_poisoned(address, size); bad != 0) {
        std::uintptr_t local = 0;
        ReportGenericError_nid_no_patch(pc, bp, reinterpret_cast<std::uintptr_t>(&local), bad, isWrite, size);
    }
}

[[noreturn]] static void ReportSizeOverflow_nid_no_patch(std::uintptr_t offset, std::uintptr_t size) {
    std::string text = AsanRuntime::ReportSeparator_nid_no_patch();
    text += "ERROR: AddressSanitizer: negative-size-param: (size=";
    text += std::to_string(static_cast<std::intptr_t>(size));
    text += ")\n";
    AsanRuntime::ReportFatal_nid_no_patch({false, 0, 0, 0, offset, false, size, "negative-size-param", true}, text);
}

[[noreturn]] static void ReportRangesOverlap_nid_no_patch(const char* function, std::uintptr_t address1, std::uintptr_t length1, std::uintptr_t address2, std::uintptr_t length2) {
    std::string bug = function;
    bug += "-param-overlap";
    std::string text = AsanRuntime::ReportSeparator_nid_no_patch();
    text += "ERROR: AddressSanitizer: " + bug + ": memory ranges [";
    AsanRuntime::AppendPointer_nid_no_patch(text, address1);
    text += ",";
    AsanRuntime::AppendPointer_nid_no_patch(text, address1 + length1);
    text += ") and [";
    AsanRuntime::AppendPointer_nid_no_patch(text, address2);
    text += ", ";
    AsanRuntime::AppendPointer_nid_no_patch(text, address2 + length2);
    text += ") overlap\n";
    static thread_local std::string description;
    description = bug;
    AsanRuntime::ReportFatal_nid_no_patch({false, 0, 0, 0, address1, false, 0, description.c_str(), true}, text);
}

static void AccessRange_nid_no_patch(std::uintptr_t address, std::uintptr_t size, bool isWrite, std::uintptr_t pc, std::uintptr_t bp) {
    if (address > address + size) {
        ReportSizeOverflow_nid_no_patch(address, size);
    }
    if (size == 0) {
        return;
    }
    RequireGuestRange_nid_no_patch(address, size, "AccessRange");
    CheckRegion_nid_no_patch(address, size, isWrite, pc, bp);
}

static bool RangesOverlap_nid_no_patch(std::uintptr_t address1, std::uintptr_t length1, std::uintptr_t address2, std::uintptr_t length2) {
    return !(address1 + length1 <= address2 || address2 + length2 <= address1);
}

[[noreturn]] static void ReportBadContainerParams_nid_no_patch(std::uintptr_t beg, std::uintptr_t end, std::uintptr_t oldMid, std::uintptr_t newMid) {
    std::string text = AsanRuntime::ReportSeparator_nid_no_patch();
    text += "ERROR: AddressSanitizer: bad parameters to __sanitizer_annotate_contiguous_container:\n      beg     : ";
    AsanRuntime::AppendPointer_nid_no_patch(text, beg);
    text += "\n      end     : ";
    AsanRuntime::AppendPointer_nid_no_patch(text, end);
    text += "\n      old_mid : ";
    AsanRuntime::AppendPointer_nid_no_patch(text, oldMid);
    text += "\n      new_mid : ";
    AsanRuntime::AppendPointer_nid_no_patch(text, newMid);
    text += "\n";
    if (!AsanRuntime::Aligned_nid_no_patch(beg)) {
        text += "ERROR: beg is not aligned by 8\n";
    }
    AsanRuntime::ReportFatal_nid_no_patch({false, 0, 0, 0, 0, false, 0, "bad-__sanitizer_annotate_contiguous_container", true}, text);
}

[[noreturn]] static void ReportBadDoubleEndedParams_nid_no_patch(std::uintptr_t storageBeg, std::uintptr_t storageEnd, std::uintptr_t oldBeg, std::uintptr_t oldEnd, std::uintptr_t newBeg, std::uintptr_t newEnd) {
    std::string text = AsanRuntime::ReportSeparator_nid_no_patch();
    text += "ERROR: AddressSanitizer: bad parameters to __sanitizer_annotate_double_ended_contiguous_container:\n      storage_beg        : ";
    AsanRuntime::AppendPointer_nid_no_patch(text, storageBeg);
    text += "\n      storage_end        : ";
    AsanRuntime::AppendPointer_nid_no_patch(text, storageEnd);
    text += "\n      old_container_beg  : ";
    AsanRuntime::AppendPointer_nid_no_patch(text, oldBeg);
    text += "\n      old_container_end  : ";
    AsanRuntime::AppendPointer_nid_no_patch(text, oldEnd);
    text += "\n      new_container_beg  : ";
    AsanRuntime::AppendPointer_nid_no_patch(text, newBeg);
    text += "\n      new_container_end  : ";
    AsanRuntime::AppendPointer_nid_no_patch(text, newEnd);
    text += "\n";
    if (!AsanRuntime::Aligned_nid_no_patch(storageBeg)) {
        text += "ERROR: storage_beg is not aligned by 8\n";
    }
    AsanRuntime::ReportFatal_nid_no_patch({false, 0, 0, 0, 0, false, 0, "bad-__sanitizer_annotate_double_ended_contiguous_container", true}, text);
}

static bool AddressIsPoisoned_nid_no_patch(std::uintptr_t address) {
    const auto value = *reinterpret_cast<const std::int8_t*>(AsanRuntime::ShadowBase + (address >> 3));
    return value != 0 && static_cast<std::int8_t>(address & 7) >= value;
}

static void CheckAccess_nid_no_patch(std::uintptr_t address, std::uintptr_t size, bool isWrite, std::uintptr_t pc, std::uintptr_t bp, const char* function) {
    if (address + size < address || address + size > (1ULL << 47) ||
        (address < AsanRuntime::ShadowBase + AsanRuntime::ShadowSize && address + size > AsanRuntime::ShadowBase)) {
        InvalidArgument_nid_no_patch(function);
    }
    RequireShadow_nid_no_patch(address, size, function);
    const auto* shadow = reinterpret_cast<const std::uint8_t*>(AsanRuntime::ShadowBase + (address >> 3));
    std::uintptr_t value = shadow[0];
    if (size > 8) {
        std::uint16_t pair;
        std::memcpy(&pair, shadow, sizeof(pair));
        value = pair;
    }
    if (value == 0) {
        if (size > 8 && (address & 7) != 0 && AddressIsPoisoned_nid_no_patch(address + size - 1)) {
            CheckRegion_nid_no_patch(address, size, isWrite, pc, bp);
        }
        return;
    }
    if (size >= 8 || static_cast<std::int8_t>((address & 7) + size - 1) >= static_cast<std::int8_t>(value)) {
        std::uintptr_t local = 0;
        ReportGenericError_nid_no_patch(pc, bp, reinterpret_cast<std::uintptr_t>(&local), address, isWrite, size);
    }
}

static void CheckSmallRegion_nid_no_patch(std::uintptr_t address, std::uintptr_t size, bool isWrite, std::uintptr_t pc, std::uintptr_t bp) {
    if (__asan_region_is_poisoned(address, size) == 0) {
        return;
    }
    if (AddressIsPoisoned_nid_no_patch(address) || AddressIsPoisoned_nid_no_patch(address + size - 1)) {
        std::uintptr_t local = 0;
        ReportGenericError_nid_no_patch(pc, bp, reinterpret_cast<std::uintptr_t>(&local), __asan_region_is_poisoned(address, size), isWrite, size);
    }
}

extern "C" {

int APS5_VABI __asan_addr_is_in_fake_stack() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI __asan_address_is_poisoned(const volatile void* address) {
    const auto value = reinterpret_cast<std::uintptr_t>(address);
    if (value >= (1ull << 47) ||
        (value >= AsanRuntime::ShadowBase && value < AsanRuntime::ShadowBase + AsanRuntime::ShadowSize)) {
        InvalidArgument_nid_no_patch(__func__);
    }
    RequireShadow_nid_no_patch(value, 1, __func__);
    return FindFirstPoisoned_nid_no_patch(reinterpret_cast<const std::int8_t*>(AsanRuntime::ShadowBase), AsanRuntime::ShadowSize, value, 1).has_value() ? 1 : 0;
}

int APS5_VABI __asan_after_dynamic_init() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI __asan_aligned_alloc() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

void APS5_VABI __asan_alloca_poison(std::uintptr_t addr, std::uintptr_t size) {
    if ((addr & kAllocaRedzoneMask) != 0 || addr < kAllocaRedzoneSize) {
        InvalidArgument_nid_no_patch(__func__);
    }
    std::uintptr_t leftRedzoneAddr = addr - kAllocaRedzoneSize;
    std::uintptr_t partialRzAddr = addr + size;
    if (partialRzAddr < addr || partialRzAddr + kAllocaRedzoneMask < partialRzAddr) {
        InvalidArgument_nid_no_patch(__func__);
    }
    std::uintptr_t rightRzAddr = (partialRzAddr + kAllocaRedzoneMask) & ~kAllocaRedzoneMask;
    if (rightRzAddr + kAllocaRedzoneSize < rightRzAddr) {
        InvalidArgument_nid_no_patch(__func__);
    }
    std::uintptr_t rightRedzoneEnd = rightRzAddr + kAllocaRedzoneSize;
    if (rightRedzoneEnd < leftRedzoneAddr || rightRedzoneEnd > (1ULL << 47) ||
        (leftRedzoneAddr < AsanRuntime::ShadowBase + AsanRuntime::ShadowSize && rightRedzoneEnd > AsanRuntime::ShadowBase)) {
        InvalidArgument_nid_no_patch(__func__);
    }
    RequireShadow_nid_no_patch(leftRedzoneAddr, rightRedzoneEnd - leftRedzoneAddr, __func__);
    std::uintptr_t partialRzAligned = partialRzAddr & ~static_cast<std::uintptr_t>(7);
    FastPoisonShadow_nid_no_patch(leftRedzoneAddr, kAllocaRedzoneSize, kAsanAllocaLeftMagic);
    FastPoisonShadowPartialRightRedzone_nid_no_patch(partialRzAligned, partialRzAddr % 8, rightRzAddr - partialRzAligned, kAsanAllocaRightMagic);
    FastPoisonShadow_nid_no_patch(rightRzAddr, kAllocaRedzoneSize, kAsanAllocaRightMagic);
}

void APS5_VABI __asan_allocas_unpoison(std::uintptr_t top, std::uintptr_t bottom) {
    if (top == 0 || top > bottom) return;
    const auto shadowSize = (bottom - top) >> 3;
    if (shadowSize == 0) return;
    if (bottom > (1ull << 47) ||
        (top < AsanRuntime::ShadowBase + AsanRuntime::ShadowSize && bottom > AsanRuntime::ShadowBase)) {
        InvalidArgument_nid_no_patch(__func__);
    }
    RequireShadow_nid_no_patch(top, bottom - top, __func__);
    std::memset(reinterpret_cast<void*>(AsanRuntime::ShadowBase + (top >> 3)), 0, shadowSize);
}

int APS5_VABI __asan_before_dynamic_init() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI __asan_calloc() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI __asan_current_error_report() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI __asan_current_error_report_size() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI __asan_describe_address() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI __asan_destroy_fake_stack() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

void APS5_VABI __asan_exp_load1(std::uintptr_t address, std::uint32_t) {
    CheckAccess_nid_no_patch(address, 1, false, reinterpret_cast<std::uintptr_t>(__builtin_return_address(0)), reinterpret_cast<std::uintptr_t>(__builtin_frame_address(0)), __func__);
}

void APS5_VABI __asan_exp_load16(std::uintptr_t address, std::uint32_t) {
    CheckAccess_nid_no_patch(address, 16, false, reinterpret_cast<std::uintptr_t>(__builtin_return_address(0)), reinterpret_cast<std::uintptr_t>(__builtin_frame_address(0)), __func__);
}

void APS5_VABI __asan_exp_load16_noabort(std::uintptr_t address, std::uint32_t) {
    CheckAccess_nid_no_patch(address, 16, false, reinterpret_cast<std::uintptr_t>(__builtin_return_address(0)), reinterpret_cast<std::uintptr_t>(__builtin_frame_address(0)), __func__);
}

void APS5_VABI __asan_exp_load1_noabort(std::uintptr_t address, std::uint32_t) {
    CheckAccess_nid_no_patch(address, 1, false, reinterpret_cast<std::uintptr_t>(__builtin_return_address(0)), reinterpret_cast<std::uintptr_t>(__builtin_frame_address(0)), __func__);
}

void APS5_VABI __asan_exp_load2(std::uintptr_t address, std::uint32_t) {
    CheckAccess_nid_no_patch(address, 2, false, reinterpret_cast<std::uintptr_t>(__builtin_return_address(0)), reinterpret_cast<std::uintptr_t>(__builtin_frame_address(0)), __func__);
}

void APS5_VABI __asan_exp_load2_noabort(std::uintptr_t address, std::uint32_t) {
    CheckAccess_nid_no_patch(address, 2, false, reinterpret_cast<std::uintptr_t>(__builtin_return_address(0)), reinterpret_cast<std::uintptr_t>(__builtin_frame_address(0)), __func__);
}

void APS5_VABI __asan_exp_load4(std::uintptr_t address, std::uint32_t) {
    CheckAccess_nid_no_patch(address, 4, false, reinterpret_cast<std::uintptr_t>(__builtin_return_address(0)), reinterpret_cast<std::uintptr_t>(__builtin_frame_address(0)), __func__);
}

void APS5_VABI __asan_exp_load4_noabort(std::uintptr_t address, std::uint32_t) {
    CheckAccess_nid_no_patch(address, 4, false, reinterpret_cast<std::uintptr_t>(__builtin_return_address(0)), reinterpret_cast<std::uintptr_t>(__builtin_frame_address(0)), __func__);
}

void APS5_VABI __asan_exp_load8(std::uintptr_t address, std::uint32_t) {
    CheckAccess_nid_no_patch(address, 8, false, reinterpret_cast<std::uintptr_t>(__builtin_return_address(0)), reinterpret_cast<std::uintptr_t>(__builtin_frame_address(0)), __func__);
}

void APS5_VABI __asan_exp_load8_noabort(std::uintptr_t address, std::uint32_t) {
    CheckAccess_nid_no_patch(address, 8, false, reinterpret_cast<std::uintptr_t>(__builtin_return_address(0)), reinterpret_cast<std::uintptr_t>(__builtin_frame_address(0)), __func__);
}

void APS5_VABI __asan_exp_loadN(std::uintptr_t address, std::uintptr_t size, std::uint32_t) {
    CheckRegion_nid_no_patch(address, size, false, reinterpret_cast<std::uintptr_t>(__builtin_return_address(0)), reinterpret_cast<std::uintptr_t>(__builtin_frame_address(0)));
}

void APS5_VABI __asan_exp_loadN_noabort(std::uintptr_t address, std::uintptr_t size, std::uint32_t) {
    CheckRegion_nid_no_patch(address, size, false, reinterpret_cast<std::uintptr_t>(__builtin_return_address(0)), reinterpret_cast<std::uintptr_t>(__builtin_frame_address(0)));
}

void APS5_VABI __asan_exp_store1(std::uintptr_t address, std::uint32_t) {
    CheckAccess_nid_no_patch(address, 1, true, reinterpret_cast<std::uintptr_t>(__builtin_return_address(0)), reinterpret_cast<std::uintptr_t>(__builtin_frame_address(0)), __func__);
}

void APS5_VABI __asan_exp_store16(std::uintptr_t address, std::uint32_t) {
    CheckAccess_nid_no_patch(address, 16, true, reinterpret_cast<std::uintptr_t>(__builtin_return_address(0)), reinterpret_cast<std::uintptr_t>(__builtin_frame_address(0)), __func__);
}

void APS5_VABI __asan_exp_store16_noabort(std::uintptr_t address, std::uint32_t) {
    CheckAccess_nid_no_patch(address, 16, true, reinterpret_cast<std::uintptr_t>(__builtin_return_address(0)), reinterpret_cast<std::uintptr_t>(__builtin_frame_address(0)), __func__);
}

void APS5_VABI __asan_exp_store1_noabort(std::uintptr_t address, std::uint32_t) {
    CheckAccess_nid_no_patch(address, 1, true, reinterpret_cast<std::uintptr_t>(__builtin_return_address(0)), reinterpret_cast<std::uintptr_t>(__builtin_frame_address(0)), __func__);
}

void APS5_VABI __asan_exp_store2(std::uintptr_t address, std::uint32_t) {
    CheckAccess_nid_no_patch(address, 2, true, reinterpret_cast<std::uintptr_t>(__builtin_return_address(0)), reinterpret_cast<std::uintptr_t>(__builtin_frame_address(0)), __func__);
}

void APS5_VABI __asan_exp_store2_noabort(std::uintptr_t address, std::uint32_t) {
    CheckAccess_nid_no_patch(address, 2, true, reinterpret_cast<std::uintptr_t>(__builtin_return_address(0)), reinterpret_cast<std::uintptr_t>(__builtin_frame_address(0)), __func__);
}

void APS5_VABI __asan_exp_store4(std::uintptr_t address, std::uint32_t) {
    CheckAccess_nid_no_patch(address, 4, true, reinterpret_cast<std::uintptr_t>(__builtin_return_address(0)), reinterpret_cast<std::uintptr_t>(__builtin_frame_address(0)), __func__);
}

void APS5_VABI __asan_exp_store4_noabort(std::uintptr_t address, std::uint32_t) {
    CheckAccess_nid_no_patch(address, 4, true, reinterpret_cast<std::uintptr_t>(__builtin_return_address(0)), reinterpret_cast<std::uintptr_t>(__builtin_frame_address(0)), __func__);
}

void APS5_VABI __asan_exp_store8(std::uintptr_t address, std::uint32_t) {
    CheckAccess_nid_no_patch(address, 8, true, reinterpret_cast<std::uintptr_t>(__builtin_return_address(0)), reinterpret_cast<std::uintptr_t>(__builtin_frame_address(0)), __func__);
}

void APS5_VABI __asan_exp_store8_noabort(std::uintptr_t address, std::uint32_t) {
    CheckAccess_nid_no_patch(address, 8, true, reinterpret_cast<std::uintptr_t>(__builtin_return_address(0)), reinterpret_cast<std::uintptr_t>(__builtin_frame_address(0)), __func__);
}

void APS5_VABI __asan_exp_storeN(std::uintptr_t address, std::uintptr_t size, std::uint32_t) {
    CheckRegion_nid_no_patch(address, size, true, reinterpret_cast<std::uintptr_t>(__builtin_return_address(0)), reinterpret_cast<std::uintptr_t>(__builtin_frame_address(0)));
}

void APS5_VABI __asan_exp_storeN_noabort(std::uintptr_t address, std::uintptr_t size, std::uint32_t) {
    CheckRegion_nid_no_patch(address, size, true, reinterpret_cast<std::uintptr_t>(__builtin_return_address(0)), reinterpret_cast<std::uintptr_t>(__builtin_frame_address(0)));
}

int APS5_VABI __asan_free() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI __asan_get_alloc_stack() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI __asan_get_current_fake_stack() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI __asan_get_free_stack() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

std::size_t APS5_VABI __asan_get_report_access_size() {
    if (!AsanRuntime::CurrentErrorReport_nid_no_patch()->generic) {
        return 0;
    }
    return AsanRuntime::CurrentErrorReport_nid_no_patch()->accessSize;
}

int APS5_VABI __asan_get_report_access_type() {
    if (!AsanRuntime::CurrentErrorReport_nid_no_patch()->generic) {
        return 0;
    }
    return AsanRuntime::CurrentErrorReport_nid_no_patch()->isWrite ? 1 : 0;
}

void* APS5_VABI __asan_get_report_address() {
    if (!AsanRuntime::CurrentErrorReport_nid_no_patch()->generic) {
        return nullptr;
    }
    return reinterpret_cast<void*>(AsanRuntime::CurrentErrorReport_nid_no_patch()->address);
}

void* APS5_VABI __asan_get_report_bp() {
    if (!AsanRuntime::CurrentErrorReport_nid_no_patch()->generic) {
        return nullptr;
    }
    return reinterpret_cast<void*>(AsanRuntime::CurrentErrorReport_nid_no_patch()->bp);
}

const char* APS5_VABI __asan_get_report_description() {
    if (!AsanRuntime::CurrentErrorReport_nid_no_patch()->generic) {
        return nullptr;
    }
    return AsanRuntime::CurrentErrorReport_nid_no_patch()->description;
}

void* APS5_VABI __asan_get_report_pc() {
    if (!AsanRuntime::CurrentErrorReport_nid_no_patch()->generic) {
        return nullptr;
    }
    return reinterpret_cast<void*>(AsanRuntime::CurrentErrorReport_nid_no_patch()->pc);
}

void* APS5_VABI __asan_get_report_sp() {
    if (!AsanRuntime::CurrentErrorReport_nid_no_patch()->generic) {
        return nullptr;
    }
    return reinterpret_cast<void*>(AsanRuntime::CurrentErrorReport_nid_no_patch()->sp);
}

void APS5_VABI __asan_get_shadow_mapping(std::size_t* scale, std::size_t* offset) {
    if (scale) *scale = 3;
    if (offset) *offset = AsanRuntime::ShadowBase;
}

int APS5_VABI __asan_handle_no_return() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI __asan_init() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI __asan_init_v5() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

void APS5_VABI __asan_load1(std::uintptr_t address) {
    CheckAccess_nid_no_patch(address, 1, false, reinterpret_cast<std::uintptr_t>(__builtin_return_address(0)), reinterpret_cast<std::uintptr_t>(__builtin_frame_address(0)), __func__);
}

void APS5_VABI __asan_load16(std::uintptr_t address) {
    CheckAccess_nid_no_patch(address, 16, false, reinterpret_cast<std::uintptr_t>(__builtin_return_address(0)), reinterpret_cast<std::uintptr_t>(__builtin_frame_address(0)), __func__);
}

void APS5_VABI __asan_load16_noabort(std::uintptr_t address) {
    CheckAccess_nid_no_patch(address, 16, false, reinterpret_cast<std::uintptr_t>(__builtin_return_address(0)), reinterpret_cast<std::uintptr_t>(__builtin_frame_address(0)), __func__);
}

void APS5_VABI __asan_load1_noabort(std::uintptr_t address) {
    CheckAccess_nid_no_patch(address, 1, false, reinterpret_cast<std::uintptr_t>(__builtin_return_address(0)), reinterpret_cast<std::uintptr_t>(__builtin_frame_address(0)), __func__);
}

void APS5_VABI __asan_load2(std::uintptr_t address) {
    CheckAccess_nid_no_patch(address, 2, false, reinterpret_cast<std::uintptr_t>(__builtin_return_address(0)), reinterpret_cast<std::uintptr_t>(__builtin_frame_address(0)), __func__);
}

void APS5_VABI __asan_load2_noabort(std::uintptr_t address) {
    CheckAccess_nid_no_patch(address, 2, false, reinterpret_cast<std::uintptr_t>(__builtin_return_address(0)), reinterpret_cast<std::uintptr_t>(__builtin_frame_address(0)), __func__);
}

void APS5_VABI __asan_load4(std::uintptr_t address) {
    CheckAccess_nid_no_patch(address, 4, false, reinterpret_cast<std::uintptr_t>(__builtin_return_address(0)), reinterpret_cast<std::uintptr_t>(__builtin_frame_address(0)), __func__);
}

void APS5_VABI __asan_load4_noabort(std::uintptr_t address) {
    CheckAccess_nid_no_patch(address, 4, false, reinterpret_cast<std::uintptr_t>(__builtin_return_address(0)), reinterpret_cast<std::uintptr_t>(__builtin_frame_address(0)), __func__);
}

void APS5_VABI __asan_load8(std::uintptr_t address) {
    CheckAccess_nid_no_patch(address, 8, false, reinterpret_cast<std::uintptr_t>(__builtin_return_address(0)), reinterpret_cast<std::uintptr_t>(__builtin_frame_address(0)), __func__);
}

void APS5_VABI __asan_load8_noabort(std::uintptr_t address) {
    CheckAccess_nid_no_patch(address, 8, false, reinterpret_cast<std::uintptr_t>(__builtin_return_address(0)), reinterpret_cast<std::uintptr_t>(__builtin_frame_address(0)), __func__);
}

void APS5_VABI __asan_loadN(std::uintptr_t address, std::uintptr_t size) {
    CheckRegion_nid_no_patch(address, size, false, reinterpret_cast<std::uintptr_t>(__builtin_return_address(0)), reinterpret_cast<std::uintptr_t>(__builtin_frame_address(0)));
}

void APS5_VABI __asan_loadN_noabort(std::uintptr_t address, std::uintptr_t size) {
    CheckRegion_nid_no_patch(address, size, false, reinterpret_cast<std::uintptr_t>(__builtin_return_address(0)), reinterpret_cast<std::uintptr_t>(__builtin_frame_address(0)));
}

std::uintptr_t APS5_VABI __asan_load_cxx_array_cookie(std::uintptr_t* p) {
    std::uintptr_t address = reinterpret_cast<std::uintptr_t>(p);
    if (address + 8 < address || address + 8 > (1ULL << 47) ||
        (address < AsanRuntime::ShadowBase + AsanRuntime::ShadowSize && address + 8 > AsanRuntime::ShadowBase)) {
        InvalidArgument_nid_no_patch(__func__);
    }
    RequireShadow_nid_no_patch(address, 8, __func__);
    std::uint8_t sval = *reinterpret_cast<std::uint8_t*>(AsanRuntime::ShadowBase + (address >> 3));
    if (sval == kAsanArrayCookieMagic) {
        return *p;
    }
    if (sval == kAsanHeapFreeMagic) {
        APS5_LOG_CHARS_ERR("AddressSanitizer: loaded array cookie from free-d memory; expect a double-free report");
        return 0;
    }
    return *p;
}

int APS5_VABI __asan_locate_address() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI __asan_malloc() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI __asan_malloc_stats() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI __asan_malloc_usable_size() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI __asan_memalign() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

void* APS5_VABI __asan_memcpy(void* to, const void* from, std::uintptr_t size) {
    const auto destination = reinterpret_cast<std::uintptr_t>(to);
    const auto source = reinterpret_cast<std::uintptr_t>(from);
    if (to != from && RangesOverlap_nid_no_patch(destination, size, source, size)) {
        ReportRangesOverlap_nid_no_patch("memcpy", destination, size, source, size);
    }
    AccessRange_nid_no_patch(source, size, false, reinterpret_cast<std::uintptr_t>(__builtin_return_address(0)), reinterpret_cast<std::uintptr_t>(__builtin_frame_address(0)));
    AccessRange_nid_no_patch(destination, size, true, reinterpret_cast<std::uintptr_t>(__builtin_return_address(0)), reinterpret_cast<std::uintptr_t>(__builtin_frame_address(0)));
    return std::memcpy(to, from, size);
}

void* APS5_VABI __asan_memmove(void* to, const void* from, std::uintptr_t size) {
    AccessRange_nid_no_patch(reinterpret_cast<std::uintptr_t>(from), size, false, reinterpret_cast<std::uintptr_t>(__builtin_return_address(0)), reinterpret_cast<std::uintptr_t>(__builtin_frame_address(0)));
    AccessRange_nid_no_patch(reinterpret_cast<std::uintptr_t>(to), size, true, reinterpret_cast<std::uintptr_t>(__builtin_return_address(0)), reinterpret_cast<std::uintptr_t>(__builtin_frame_address(0)));
    return std::memmove(to, from, size);
}

void* APS5_VABI __asan_memset(void* block, int value, std::uintptr_t size) {
    AccessRange_nid_no_patch(reinterpret_cast<std::uintptr_t>(block), size, true, reinterpret_cast<std::uintptr_t>(__builtin_return_address(0)), reinterpret_cast<std::uintptr_t>(__builtin_frame_address(0)));
    return std::memset(block, value, size);
}

int APS5_VABI __asan_option_detect_stack_use_after_return() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

void APS5_VABI __asan_poison_cxx_array_cookie(std::uintptr_t p) {
    if (p + 1 < p || p + 1 > (1ULL << 47) ||
        (p < AsanRuntime::ShadowBase + AsanRuntime::ShadowSize && p + 1 > AsanRuntime::ShadowBase)) {
        InvalidArgument_nid_no_patch(__func__);
    }
    RequireShadow_nid_no_patch(p, 1, __func__);
    if (!kPoisonArrayCookie) {
        return;
    }
    *reinterpret_cast<std::int8_t*>(AsanRuntime::ShadowBase + (p >> 3)) = static_cast<std::int8_t>(kAsanArrayCookieMagic);
}

void APS5_VABI __asan_poison_intra_object_redzone(std::uintptr_t address, std::uintptr_t size) {
    UpdateIntraObjectShadow_nid_no_patch(address, size, true, __func__);
}

void APS5_VABI __asan_poison_memory_region(const volatile void* addr, std::uintptr_t size) {
    if (!kAllowUserPoisoning || size == 0) {
        return;
    }
    std::uintptr_t begAddr = reinterpret_cast<std::uintptr_t>(addr);
    std::uintptr_t endAddr = begAddr + size;
    if (endAddr < begAddr || endAddr > (1ULL << 47) ||
        (begAddr < AsanRuntime::ShadowBase + AsanRuntime::ShadowSize && endAddr > AsanRuntime::ShadowBase)) {
        InvalidArgument_nid_no_patch(__func__);
    }
    RequireShadow_nid_no_patch(begAddr, size + 1, __func__);
    ShadowSegmentEndpoint beg(begAddr);
    ShadowSegmentEndpoint end(endAddr);
    if (beg.chunk == end.chunk) {
        std::int8_t value = beg.value;
        if (value > 0 && value <= end.offset) {
            if (beg.offset > 0) {
                *beg.chunk = std::min(value, beg.offset);
            } else {
                *beg.chunk = kAsanUserPoisonedMemoryMagic;
            }
        }
        return;
    }
    if (beg.offset > 0) {
        if (beg.value == 0) {
            *beg.chunk = beg.offset;
        } else {
            *beg.chunk = std::min(beg.value, beg.offset);
        }
        beg.chunk++;
    }
    std::memset(beg.chunk, static_cast<unsigned char>(kAsanUserPoisonedMemoryMagic), end.chunk - beg.chunk);
    if (end.value > 0 && end.value <= end.offset) {
        *end.chunk = kAsanUserPoisonedMemoryMagic;
    }
}

void APS5_VABI __asan_poison_stack_memory(std::uintptr_t address, std::uintptr_t size) {
    if (size == 0) {
        return;
    }
    if ((address & 7) != 0 || address + size < address || address + size > (1ULL << 47) ||
        (address < AsanRuntime::ShadowBase + AsanRuntime::ShadowSize && address + size > AsanRuntime::ShadowBase)) {
        InvalidArgument_nid_no_patch(__func__);
    }
    RequireShadow_nid_no_patch(address, size, __func__);
    UpdateStackShadow_nid_no_patch(reinterpret_cast<std::int8_t*>(AsanRuntime::ShadowBase), AsanRuntime::ShadowSize, address, size, true);
}

int APS5_VABI __asan_posix_memalign() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI __asan_print_accumulated_stats() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI __asan_realloc() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI __asan_reallocalign() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

std::uintptr_t APS5_VABI __asan_region_is_poisoned(std::uintptr_t address, std::uintptr_t size) {
    if (size == 0) return 0;
    const auto end = address + size;
    if (end < address || end >= (1ull << 47) ||
        (address < AsanRuntime::ShadowBase + AsanRuntime::ShadowSize && end > AsanRuntime::ShadowBase)) {
        InvalidArgument_nid_no_patch(__func__);
    }
    RequireShadow_nid_no_patch(address, size, __func__);
    return FindFirstPoisoned_nid_no_patch(reinterpret_cast<const std::int8_t*>(AsanRuntime::ShadowBase), AsanRuntime::ShadowSize, address, size).value_or(0);
}

int APS5_VABI __asan_register_elf_globals() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI __asan_register_globals() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI __asan_register_image_globals() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

void APS5_VABI __asan_report_error(void* pc, void* bp, void* sp, void* address, int isWrite, std::size_t accessSize, std::uint32_t) {
    ReportGenericError_nid_no_patch(reinterpret_cast<std::uintptr_t>(pc), reinterpret_cast<std::uintptr_t>(bp), reinterpret_cast<std::uintptr_t>(sp), reinterpret_cast<std::uintptr_t>(address), isWrite != 0, accessSize);
}

void APS5_VABI __asan_report_exp_load1(std::uintptr_t address, std::uint32_t) {
    std::uintptr_t local = 0;
    ReportGenericError_nid_no_patch(reinterpret_cast<std::uintptr_t>(__builtin_return_address(0)), reinterpret_cast<std::uintptr_t>(__builtin_frame_address(0)), reinterpret_cast<std::uintptr_t>(&local), address, false, 1);
}

void APS5_VABI __asan_report_exp_load16(std::uintptr_t address, std::uint32_t) {
    std::uintptr_t local = 0;
    ReportGenericError_nid_no_patch(reinterpret_cast<std::uintptr_t>(__builtin_return_address(0)), reinterpret_cast<std::uintptr_t>(__builtin_frame_address(0)), reinterpret_cast<std::uintptr_t>(&local), address, false, 16);
}

void APS5_VABI __asan_report_exp_load16_noabort(std::uintptr_t address, std::uint32_t) {
    std::uintptr_t local = 0;
    ReportGenericError_nid_no_patch(reinterpret_cast<std::uintptr_t>(__builtin_return_address(0)), reinterpret_cast<std::uintptr_t>(__builtin_frame_address(0)), reinterpret_cast<std::uintptr_t>(&local), address, false, 16);
}

void APS5_VABI __asan_report_exp_load1_noabort(std::uintptr_t address, std::uint32_t) {
    std::uintptr_t local = 0;
    ReportGenericError_nid_no_patch(reinterpret_cast<std::uintptr_t>(__builtin_return_address(0)), reinterpret_cast<std::uintptr_t>(__builtin_frame_address(0)), reinterpret_cast<std::uintptr_t>(&local), address, false, 1);
}

void APS5_VABI __asan_report_exp_load2(std::uintptr_t address, std::uint32_t) {
    std::uintptr_t local = 0;
    ReportGenericError_nid_no_patch(reinterpret_cast<std::uintptr_t>(__builtin_return_address(0)), reinterpret_cast<std::uintptr_t>(__builtin_frame_address(0)), reinterpret_cast<std::uintptr_t>(&local), address, false, 2);
}

void APS5_VABI __asan_report_exp_load2_noabort(std::uintptr_t address, std::uint32_t) {
    std::uintptr_t local = 0;
    ReportGenericError_nid_no_patch(reinterpret_cast<std::uintptr_t>(__builtin_return_address(0)), reinterpret_cast<std::uintptr_t>(__builtin_frame_address(0)), reinterpret_cast<std::uintptr_t>(&local), address, false, 2);
}

void APS5_VABI __asan_report_exp_load4(std::uintptr_t address, std::uint32_t) {
    std::uintptr_t local = 0;
    ReportGenericError_nid_no_patch(reinterpret_cast<std::uintptr_t>(__builtin_return_address(0)), reinterpret_cast<std::uintptr_t>(__builtin_frame_address(0)), reinterpret_cast<std::uintptr_t>(&local), address, false, 4);
}

void APS5_VABI __asan_report_exp_load4_noabort(std::uintptr_t address, std::uint32_t) {
    std::uintptr_t local = 0;
    ReportGenericError_nid_no_patch(reinterpret_cast<std::uintptr_t>(__builtin_return_address(0)), reinterpret_cast<std::uintptr_t>(__builtin_frame_address(0)), reinterpret_cast<std::uintptr_t>(&local), address, false, 4);
}

void APS5_VABI __asan_report_exp_load8(std::uintptr_t address, std::uint32_t) {
    std::uintptr_t local = 0;
    ReportGenericError_nid_no_patch(reinterpret_cast<std::uintptr_t>(__builtin_return_address(0)), reinterpret_cast<std::uintptr_t>(__builtin_frame_address(0)), reinterpret_cast<std::uintptr_t>(&local), address, false, 8);
}

void APS5_VABI __asan_report_exp_load8_noabort(std::uintptr_t address, std::uint32_t) {
    std::uintptr_t local = 0;
    ReportGenericError_nid_no_patch(reinterpret_cast<std::uintptr_t>(__builtin_return_address(0)), reinterpret_cast<std::uintptr_t>(__builtin_frame_address(0)), reinterpret_cast<std::uintptr_t>(&local), address, false, 8);
}

void APS5_VABI __asan_report_exp_load_n(std::uintptr_t address, std::uintptr_t size, std::uint32_t) {
    std::uintptr_t local = 0;
    ReportGenericError_nid_no_patch(reinterpret_cast<std::uintptr_t>(__builtin_return_address(0)), reinterpret_cast<std::uintptr_t>(__builtin_frame_address(0)), reinterpret_cast<std::uintptr_t>(&local), address, false, size);
}

void APS5_VABI __asan_report_exp_load_n_noabort(std::uintptr_t address, std::uintptr_t size, std::uint32_t) {
    std::uintptr_t local = 0;
    ReportGenericError_nid_no_patch(reinterpret_cast<std::uintptr_t>(__builtin_return_address(0)), reinterpret_cast<std::uintptr_t>(__builtin_frame_address(0)), reinterpret_cast<std::uintptr_t>(&local), address, false, size);
}

void APS5_VABI __asan_report_exp_store1(std::uintptr_t address, std::uint32_t) {
    std::uintptr_t local = 0;
    ReportGenericError_nid_no_patch(reinterpret_cast<std::uintptr_t>(__builtin_return_address(0)), reinterpret_cast<std::uintptr_t>(__builtin_frame_address(0)), reinterpret_cast<std::uintptr_t>(&local), address, true, 1);
}

void APS5_VABI __asan_report_exp_store16(std::uintptr_t address, std::uint32_t) {
    std::uintptr_t local = 0;
    ReportGenericError_nid_no_patch(reinterpret_cast<std::uintptr_t>(__builtin_return_address(0)), reinterpret_cast<std::uintptr_t>(__builtin_frame_address(0)), reinterpret_cast<std::uintptr_t>(&local), address, true, 16);
}

void APS5_VABI __asan_report_exp_store16_noabort(std::uintptr_t address, std::uint32_t) {
    std::uintptr_t local = 0;
    ReportGenericError_nid_no_patch(reinterpret_cast<std::uintptr_t>(__builtin_return_address(0)), reinterpret_cast<std::uintptr_t>(__builtin_frame_address(0)), reinterpret_cast<std::uintptr_t>(&local), address, true, 16);
}

void APS5_VABI __asan_report_exp_store1_noabort(std::uintptr_t address, std::uint32_t) {
    std::uintptr_t local = 0;
    ReportGenericError_nid_no_patch(reinterpret_cast<std::uintptr_t>(__builtin_return_address(0)), reinterpret_cast<std::uintptr_t>(__builtin_frame_address(0)), reinterpret_cast<std::uintptr_t>(&local), address, true, 1);
}

void APS5_VABI __asan_report_exp_store2(std::uintptr_t address, std::uint32_t) {
    std::uintptr_t local = 0;
    ReportGenericError_nid_no_patch(reinterpret_cast<std::uintptr_t>(__builtin_return_address(0)), reinterpret_cast<std::uintptr_t>(__builtin_frame_address(0)), reinterpret_cast<std::uintptr_t>(&local), address, true, 2);
}

void APS5_VABI __asan_report_exp_store2_noabort(std::uintptr_t address, std::uint32_t) {
    std::uintptr_t local = 0;
    ReportGenericError_nid_no_patch(reinterpret_cast<std::uintptr_t>(__builtin_return_address(0)), reinterpret_cast<std::uintptr_t>(__builtin_frame_address(0)), reinterpret_cast<std::uintptr_t>(&local), address, true, 2);
}

void APS5_VABI __asan_report_exp_store4(std::uintptr_t address, std::uint32_t) {
    std::uintptr_t local = 0;
    ReportGenericError_nid_no_patch(reinterpret_cast<std::uintptr_t>(__builtin_return_address(0)), reinterpret_cast<std::uintptr_t>(__builtin_frame_address(0)), reinterpret_cast<std::uintptr_t>(&local), address, true, 4);
}

void APS5_VABI __asan_report_exp_store4_noabort(std::uintptr_t address, std::uint32_t) {
    std::uintptr_t local = 0;
    ReportGenericError_nid_no_patch(reinterpret_cast<std::uintptr_t>(__builtin_return_address(0)), reinterpret_cast<std::uintptr_t>(__builtin_frame_address(0)), reinterpret_cast<std::uintptr_t>(&local), address, true, 4);
}

void APS5_VABI __asan_report_exp_store8(std::uintptr_t address, std::uint32_t) {
    std::uintptr_t local = 0;
    ReportGenericError_nid_no_patch(reinterpret_cast<std::uintptr_t>(__builtin_return_address(0)), reinterpret_cast<std::uintptr_t>(__builtin_frame_address(0)), reinterpret_cast<std::uintptr_t>(&local), address, true, 8);
}

void APS5_VABI __asan_report_exp_store8_noabort(std::uintptr_t address, std::uint32_t) {
    std::uintptr_t local = 0;
    ReportGenericError_nid_no_patch(reinterpret_cast<std::uintptr_t>(__builtin_return_address(0)), reinterpret_cast<std::uintptr_t>(__builtin_frame_address(0)), reinterpret_cast<std::uintptr_t>(&local), address, true, 8);
}

void APS5_VABI __asan_report_exp_store_n(std::uintptr_t address, std::uintptr_t size, std::uint32_t) {
    std::uintptr_t local = 0;
    ReportGenericError_nid_no_patch(reinterpret_cast<std::uintptr_t>(__builtin_return_address(0)), reinterpret_cast<std::uintptr_t>(__builtin_frame_address(0)), reinterpret_cast<std::uintptr_t>(&local), address, true, size);
}

void APS5_VABI __asan_report_exp_store_n_noabort(std::uintptr_t address, std::uintptr_t size, std::uint32_t) {
    std::uintptr_t local = 0;
    ReportGenericError_nid_no_patch(reinterpret_cast<std::uintptr_t>(__builtin_return_address(0)), reinterpret_cast<std::uintptr_t>(__builtin_frame_address(0)), reinterpret_cast<std::uintptr_t>(&local), address, true, size);
}

void APS5_VABI __asan_report_load1(std::uintptr_t address) {
    std::uintptr_t local = 0;
    ReportGenericError_nid_no_patch(reinterpret_cast<std::uintptr_t>(__builtin_return_address(0)), reinterpret_cast<std::uintptr_t>(__builtin_frame_address(0)), reinterpret_cast<std::uintptr_t>(&local), address, false, 1);
}

void APS5_VABI __asan_report_load16(std::uintptr_t address) {
    std::uintptr_t local = 0;
    ReportGenericError_nid_no_patch(reinterpret_cast<std::uintptr_t>(__builtin_return_address(0)), reinterpret_cast<std::uintptr_t>(__builtin_frame_address(0)), reinterpret_cast<std::uintptr_t>(&local), address, false, 16);
}

void APS5_VABI __asan_report_load16_noabort(std::uintptr_t address) {
    std::uintptr_t local = 0;
    ReportGenericError_nid_no_patch(reinterpret_cast<std::uintptr_t>(__builtin_return_address(0)), reinterpret_cast<std::uintptr_t>(__builtin_frame_address(0)), reinterpret_cast<std::uintptr_t>(&local), address, false, 16);
}

void APS5_VABI __asan_report_load1_noabort(std::uintptr_t address) {
    std::uintptr_t local = 0;
    ReportGenericError_nid_no_patch(reinterpret_cast<std::uintptr_t>(__builtin_return_address(0)), reinterpret_cast<std::uintptr_t>(__builtin_frame_address(0)), reinterpret_cast<std::uintptr_t>(&local), address, false, 1);
}

void APS5_VABI __asan_report_load2(std::uintptr_t address) {
    std::uintptr_t local = 0;
    ReportGenericError_nid_no_patch(reinterpret_cast<std::uintptr_t>(__builtin_return_address(0)), reinterpret_cast<std::uintptr_t>(__builtin_frame_address(0)), reinterpret_cast<std::uintptr_t>(&local), address, false, 2);
}

void APS5_VABI __asan_report_load2_noabort(std::uintptr_t address) {
    std::uintptr_t local = 0;
    ReportGenericError_nid_no_patch(reinterpret_cast<std::uintptr_t>(__builtin_return_address(0)), reinterpret_cast<std::uintptr_t>(__builtin_frame_address(0)), reinterpret_cast<std::uintptr_t>(&local), address, false, 2);
}

void APS5_VABI __asan_report_load4(std::uintptr_t address) {
    std::uintptr_t local = 0;
    ReportGenericError_nid_no_patch(reinterpret_cast<std::uintptr_t>(__builtin_return_address(0)), reinterpret_cast<std::uintptr_t>(__builtin_frame_address(0)), reinterpret_cast<std::uintptr_t>(&local), address, false, 4);
}

void APS5_VABI __asan_report_load4_noabort(std::uintptr_t address) {
    std::uintptr_t local = 0;
    ReportGenericError_nid_no_patch(reinterpret_cast<std::uintptr_t>(__builtin_return_address(0)), reinterpret_cast<std::uintptr_t>(__builtin_frame_address(0)), reinterpret_cast<std::uintptr_t>(&local), address, false, 4);
}

void APS5_VABI __asan_report_load8(std::uintptr_t address) {
    std::uintptr_t local = 0;
    ReportGenericError_nid_no_patch(reinterpret_cast<std::uintptr_t>(__builtin_return_address(0)), reinterpret_cast<std::uintptr_t>(__builtin_frame_address(0)), reinterpret_cast<std::uintptr_t>(&local), address, false, 8);
}

void APS5_VABI __asan_report_load8_noabort(std::uintptr_t address) {
    std::uintptr_t local = 0;
    ReportGenericError_nid_no_patch(reinterpret_cast<std::uintptr_t>(__builtin_return_address(0)), reinterpret_cast<std::uintptr_t>(__builtin_frame_address(0)), reinterpret_cast<std::uintptr_t>(&local), address, false, 8);
}

void APS5_VABI __asan_report_load_n(std::uintptr_t address, std::uintptr_t size) {
    std::uintptr_t local = 0;
    ReportGenericError_nid_no_patch(reinterpret_cast<std::uintptr_t>(__builtin_return_address(0)), reinterpret_cast<std::uintptr_t>(__builtin_frame_address(0)), reinterpret_cast<std::uintptr_t>(&local), address, false, size);
}

void APS5_VABI __asan_report_load_n_noabort(std::uintptr_t address, std::uintptr_t size) {
    std::uintptr_t local = 0;
    ReportGenericError_nid_no_patch(reinterpret_cast<std::uintptr_t>(__builtin_return_address(0)), reinterpret_cast<std::uintptr_t>(__builtin_frame_address(0)), reinterpret_cast<std::uintptr_t>(&local), address, false, size);
}

int APS5_VABI __asan_report_present() {
    const auto* report = AsanRuntime::CurrentErrorReport_nid_no_patch();
    return report->generic || report->other ? 1 : 0;
}

void APS5_VABI __asan_report_store1(std::uintptr_t address) {
    std::uintptr_t local = 0;
    ReportGenericError_nid_no_patch(reinterpret_cast<std::uintptr_t>(__builtin_return_address(0)), reinterpret_cast<std::uintptr_t>(__builtin_frame_address(0)), reinterpret_cast<std::uintptr_t>(&local), address, true, 1);
}

void APS5_VABI __asan_report_store16(std::uintptr_t address) {
    std::uintptr_t local = 0;
    ReportGenericError_nid_no_patch(reinterpret_cast<std::uintptr_t>(__builtin_return_address(0)), reinterpret_cast<std::uintptr_t>(__builtin_frame_address(0)), reinterpret_cast<std::uintptr_t>(&local), address, true, 16);
}

void APS5_VABI __asan_report_store16_noabort(std::uintptr_t address) {
    std::uintptr_t local = 0;
    ReportGenericError_nid_no_patch(reinterpret_cast<std::uintptr_t>(__builtin_return_address(0)), reinterpret_cast<std::uintptr_t>(__builtin_frame_address(0)), reinterpret_cast<std::uintptr_t>(&local), address, true, 16);
}

void APS5_VABI __asan_report_store1_noabort(std::uintptr_t address) {
    std::uintptr_t local = 0;
    ReportGenericError_nid_no_patch(reinterpret_cast<std::uintptr_t>(__builtin_return_address(0)), reinterpret_cast<std::uintptr_t>(__builtin_frame_address(0)), reinterpret_cast<std::uintptr_t>(&local), address, true, 1);
}

void APS5_VABI __asan_report_store2(std::uintptr_t address) {
    std::uintptr_t local = 0;
    ReportGenericError_nid_no_patch(reinterpret_cast<std::uintptr_t>(__builtin_return_address(0)), reinterpret_cast<std::uintptr_t>(__builtin_frame_address(0)), reinterpret_cast<std::uintptr_t>(&local), address, true, 2);
}

void APS5_VABI __asan_report_store2_noabort(std::uintptr_t address) {
    std::uintptr_t local = 0;
    ReportGenericError_nid_no_patch(reinterpret_cast<std::uintptr_t>(__builtin_return_address(0)), reinterpret_cast<std::uintptr_t>(__builtin_frame_address(0)), reinterpret_cast<std::uintptr_t>(&local), address, true, 2);
}

void APS5_VABI __asan_report_store4(std::uintptr_t address) {
    std::uintptr_t local = 0;
    ReportGenericError_nid_no_patch(reinterpret_cast<std::uintptr_t>(__builtin_return_address(0)), reinterpret_cast<std::uintptr_t>(__builtin_frame_address(0)), reinterpret_cast<std::uintptr_t>(&local), address, true, 4);
}

void APS5_VABI __asan_report_store4_noabort(std::uintptr_t address) {
    std::uintptr_t local = 0;
    ReportGenericError_nid_no_patch(reinterpret_cast<std::uintptr_t>(__builtin_return_address(0)), reinterpret_cast<std::uintptr_t>(__builtin_frame_address(0)), reinterpret_cast<std::uintptr_t>(&local), address, true, 4);
}

void APS5_VABI __asan_report_store8(std::uintptr_t address) {
    std::uintptr_t local = 0;
    ReportGenericError_nid_no_patch(reinterpret_cast<std::uintptr_t>(__builtin_return_address(0)), reinterpret_cast<std::uintptr_t>(__builtin_frame_address(0)), reinterpret_cast<std::uintptr_t>(&local), address, true, 8);
}

void APS5_VABI __asan_report_store8_noabort(std::uintptr_t address) {
    std::uintptr_t local = 0;
    ReportGenericError_nid_no_patch(reinterpret_cast<std::uintptr_t>(__builtin_return_address(0)), reinterpret_cast<std::uintptr_t>(__builtin_frame_address(0)), reinterpret_cast<std::uintptr_t>(&local), address, true, 8);
}

void APS5_VABI __asan_report_store_n(std::uintptr_t address, std::uintptr_t size) {
    std::uintptr_t local = 0;
    ReportGenericError_nid_no_patch(reinterpret_cast<std::uintptr_t>(__builtin_return_address(0)), reinterpret_cast<std::uintptr_t>(__builtin_frame_address(0)), reinterpret_cast<std::uintptr_t>(&local), address, true, size);
}

void APS5_VABI __asan_report_store_n_noabort(std::uintptr_t address, std::uintptr_t size) {
    std::uintptr_t local = 0;
    ReportGenericError_nid_no_patch(reinterpret_cast<std::uintptr_t>(__builtin_return_address(0)), reinterpret_cast<std::uintptr_t>(__builtin_frame_address(0)), reinterpret_cast<std::uintptr_t>(&local), address, true, size);
}

void APS5_VABI __asan_set_error_report_callback(void (APS5_VABI* callback)(const char*)) {
    AsanRuntime::errorReportCallback.store(callback, std::memory_order_release);
}

void APS5_VABI __asan_set_shadow_00(std::uintptr_t address, std::uintptr_t size) {
    if (address == 0 || size > std::numeric_limits<std::uintptr_t>::max() - address) InvalidArgument_nid_no_patch(__func__);
    if (address + size > AsanRuntime::ShadowBase && address < AsanRuntime::ShadowBase + AsanRuntime::ShadowSize && !AsanRuntime::ShadowRangeAvailable_nid_no_patch(std::max(address, AsanRuntime::ShadowBase), std::min(address + size, AsanRuntime::ShadowBase + AsanRuntime::ShadowSize))) ShadowUnavailable_nid_no_patch(__func__);
    if (size) std::memset(reinterpret_cast<void*>(address), 0x00, size);
}

void APS5_VABI __asan_set_shadow_01(std::uintptr_t address, std::uintptr_t size) {
    if (address == 0 || size > std::numeric_limits<std::uintptr_t>::max() - address) InvalidArgument_nid_no_patch(__func__);
    if (address + size > AsanRuntime::ShadowBase && address < AsanRuntime::ShadowBase + AsanRuntime::ShadowSize && !AsanRuntime::ShadowRangeAvailable_nid_no_patch(std::max(address, AsanRuntime::ShadowBase), std::min(address + size, AsanRuntime::ShadowBase + AsanRuntime::ShadowSize))) ShadowUnavailable_nid_no_patch(__func__);
    if (size) std::memset(reinterpret_cast<void*>(address), 0x01, size);
}

void APS5_VABI __asan_set_shadow_02(std::uintptr_t address, std::uintptr_t size) {
    if (address == 0 || size > std::numeric_limits<std::uintptr_t>::max() - address) InvalidArgument_nid_no_patch(__func__);
    if (address + size > AsanRuntime::ShadowBase && address < AsanRuntime::ShadowBase + AsanRuntime::ShadowSize && !AsanRuntime::ShadowRangeAvailable_nid_no_patch(std::max(address, AsanRuntime::ShadowBase), std::min(address + size, AsanRuntime::ShadowBase + AsanRuntime::ShadowSize))) ShadowUnavailable_nid_no_patch(__func__);
    if (size) std::memset(reinterpret_cast<void*>(address), 0x02, size);
}

void APS5_VABI __asan_set_shadow_03(std::uintptr_t address, std::uintptr_t size) {
    if (address == 0 || size > std::numeric_limits<std::uintptr_t>::max() - address) InvalidArgument_nid_no_patch(__func__);
    if (address + size > AsanRuntime::ShadowBase && address < AsanRuntime::ShadowBase + AsanRuntime::ShadowSize && !AsanRuntime::ShadowRangeAvailable_nid_no_patch(std::max(address, AsanRuntime::ShadowBase), std::min(address + size, AsanRuntime::ShadowBase + AsanRuntime::ShadowSize))) ShadowUnavailable_nid_no_patch(__func__);
    if (size) std::memset(reinterpret_cast<void*>(address), 0x03, size);
}

void APS5_VABI __asan_set_shadow_04(std::uintptr_t address, std::uintptr_t size) {
    if (address == 0 || size > std::numeric_limits<std::uintptr_t>::max() - address) InvalidArgument_nid_no_patch(__func__);
    if (address + size > AsanRuntime::ShadowBase && address < AsanRuntime::ShadowBase + AsanRuntime::ShadowSize && !AsanRuntime::ShadowRangeAvailable_nid_no_patch(std::max(address, AsanRuntime::ShadowBase), std::min(address + size, AsanRuntime::ShadowBase + AsanRuntime::ShadowSize))) ShadowUnavailable_nid_no_patch(__func__);
    if (size) std::memset(reinterpret_cast<void*>(address), 0x04, size);
}

void APS5_VABI __asan_set_shadow_05(std::uintptr_t address, std::uintptr_t size) {
    if (address == 0 || size > std::numeric_limits<std::uintptr_t>::max() - address) InvalidArgument_nid_no_patch(__func__);
    if (address + size > AsanRuntime::ShadowBase && address < AsanRuntime::ShadowBase + AsanRuntime::ShadowSize && !AsanRuntime::ShadowRangeAvailable_nid_no_patch(std::max(address, AsanRuntime::ShadowBase), std::min(address + size, AsanRuntime::ShadowBase + AsanRuntime::ShadowSize))) ShadowUnavailable_nid_no_patch(__func__);
    if (size) std::memset(reinterpret_cast<void*>(address), 0x05, size);
}

void APS5_VABI __asan_set_shadow_06(std::uintptr_t address, std::uintptr_t size) {
    if (address == 0 || size > std::numeric_limits<std::uintptr_t>::max() - address) InvalidArgument_nid_no_patch(__func__);
    if (address + size > AsanRuntime::ShadowBase && address < AsanRuntime::ShadowBase + AsanRuntime::ShadowSize && !AsanRuntime::ShadowRangeAvailable_nid_no_patch(std::max(address, AsanRuntime::ShadowBase), std::min(address + size, AsanRuntime::ShadowBase + AsanRuntime::ShadowSize))) ShadowUnavailable_nid_no_patch(__func__);
    if (size) std::memset(reinterpret_cast<void*>(address), 0x06, size);
}

void APS5_VABI __asan_set_shadow_07(std::uintptr_t address, std::uintptr_t size) {
    if (address == 0 || size > std::numeric_limits<std::uintptr_t>::max() - address) InvalidArgument_nid_no_patch(__func__);
    if (address + size > AsanRuntime::ShadowBase && address < AsanRuntime::ShadowBase + AsanRuntime::ShadowSize && !AsanRuntime::ShadowRangeAvailable_nid_no_patch(std::max(address, AsanRuntime::ShadowBase), std::min(address + size, AsanRuntime::ShadowBase + AsanRuntime::ShadowSize))) ShadowUnavailable_nid_no_patch(__func__);
    if (size) std::memset(reinterpret_cast<void*>(address), 0x07, size);
}

void APS5_VABI __asan_set_shadow_f1(std::uintptr_t address, std::uintptr_t size) {
    if (address == 0 || size > std::numeric_limits<std::uintptr_t>::max() - address) InvalidArgument_nid_no_patch(__func__);
    if (address + size > AsanRuntime::ShadowBase && address < AsanRuntime::ShadowBase + AsanRuntime::ShadowSize && !AsanRuntime::ShadowRangeAvailable_nid_no_patch(std::max(address, AsanRuntime::ShadowBase), std::min(address + size, AsanRuntime::ShadowBase + AsanRuntime::ShadowSize))) ShadowUnavailable_nid_no_patch(__func__);
    if (size) std::memset(reinterpret_cast<void*>(address), 0xf1, size);
}

void APS5_VABI __asan_set_shadow_f2(std::uintptr_t address, std::uintptr_t size) {
    if (address == 0 || size > std::numeric_limits<std::uintptr_t>::max() - address) InvalidArgument_nid_no_patch(__func__);
    if (address + size > AsanRuntime::ShadowBase && address < AsanRuntime::ShadowBase + AsanRuntime::ShadowSize && !AsanRuntime::ShadowRangeAvailable_nid_no_patch(std::max(address, AsanRuntime::ShadowBase), std::min(address + size, AsanRuntime::ShadowBase + AsanRuntime::ShadowSize))) ShadowUnavailable_nid_no_patch(__func__);
    if (size) std::memset(reinterpret_cast<void*>(address), 0xf2, size);
}

void APS5_VABI __asan_set_shadow_f3(std::uintptr_t address, std::uintptr_t size) {
    if (address == 0 || size > std::numeric_limits<std::uintptr_t>::max() - address) InvalidArgument_nid_no_patch(__func__);
    if (address + size > AsanRuntime::ShadowBase && address < AsanRuntime::ShadowBase + AsanRuntime::ShadowSize && !AsanRuntime::ShadowRangeAvailable_nid_no_patch(std::max(address, AsanRuntime::ShadowBase), std::min(address + size, AsanRuntime::ShadowBase + AsanRuntime::ShadowSize))) ShadowUnavailable_nid_no_patch(__func__);
    if (size) std::memset(reinterpret_cast<void*>(address), 0xf3, size);
}

void APS5_VABI __asan_set_shadow_f5(std::uintptr_t address, std::uintptr_t size) {
    if (address == 0 || size > std::numeric_limits<std::uintptr_t>::max() - address) InvalidArgument_nid_no_patch(__func__);
    if (address + size > AsanRuntime::ShadowBase && address < AsanRuntime::ShadowBase + AsanRuntime::ShadowSize && !AsanRuntime::ShadowRangeAvailable_nid_no_patch(std::max(address, AsanRuntime::ShadowBase), std::min(address + size, AsanRuntime::ShadowBase + AsanRuntime::ShadowSize))) ShadowUnavailable_nid_no_patch(__func__);
    if (size) std::memset(reinterpret_cast<void*>(address), 0xf5, size);
}

void APS5_VABI __asan_set_shadow_f8(std::uintptr_t address, std::uintptr_t size) {
    if (address == 0 || size > std::numeric_limits<std::uintptr_t>::max() - address) InvalidArgument_nid_no_patch(__func__);
    if (address + size > AsanRuntime::ShadowBase && address < AsanRuntime::ShadowBase + AsanRuntime::ShadowSize && !AsanRuntime::ShadowRangeAvailable_nid_no_patch(std::max(address, AsanRuntime::ShadowBase), std::min(address + size, AsanRuntime::ShadowBase + AsanRuntime::ShadowSize))) ShadowUnavailable_nid_no_patch(__func__);
    if (size) std::memset(reinterpret_cast<void*>(address), 0xf8, size);
}

int APS5_VABI __asan_stack_free_0() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI __asan_stack_free_1() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI __asan_stack_free_10() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI __asan_stack_free_2() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI __asan_stack_free_3() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI __asan_stack_free_4() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI __asan_stack_free_5() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI __asan_stack_free_6() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI __asan_stack_free_7() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI __asan_stack_free_8() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI __asan_stack_free_9() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI __asan_stack_malloc_0() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI __asan_stack_malloc_1() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI __asan_stack_malloc_10() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI __asan_stack_malloc_2() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI __asan_stack_malloc_3() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI __asan_stack_malloc_4() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI __asan_stack_malloc_5() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI __asan_stack_malloc_6() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI __asan_stack_malloc_7() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI __asan_stack_malloc_8() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI __asan_stack_malloc_9() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI __asan_stack_malloc_always_0() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI __asan_stack_malloc_always_1() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI __asan_stack_malloc_always_10() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI __asan_stack_malloc_always_2() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI __asan_stack_malloc_always_3() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI __asan_stack_malloc_always_4() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI __asan_stack_malloc_always_5() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI __asan_stack_malloc_always_6() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI __asan_stack_malloc_always_7() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI __asan_stack_malloc_always_8() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI __asan_stack_malloc_always_9() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

void APS5_VABI __asan_store1(std::uintptr_t address) {
    CheckAccess_nid_no_patch(address, 1, true, reinterpret_cast<std::uintptr_t>(__builtin_return_address(0)), reinterpret_cast<std::uintptr_t>(__builtin_frame_address(0)), __func__);
}

void APS5_VABI __asan_store16(std::uintptr_t address) {
    CheckAccess_nid_no_patch(address, 16, true, reinterpret_cast<std::uintptr_t>(__builtin_return_address(0)), reinterpret_cast<std::uintptr_t>(__builtin_frame_address(0)), __func__);
}

void APS5_VABI __asan_store16_noabort(std::uintptr_t address) {
    CheckAccess_nid_no_patch(address, 16, true, reinterpret_cast<std::uintptr_t>(__builtin_return_address(0)), reinterpret_cast<std::uintptr_t>(__builtin_frame_address(0)), __func__);
}

void APS5_VABI __asan_store1_noabort(std::uintptr_t address) {
    CheckAccess_nid_no_patch(address, 1, true, reinterpret_cast<std::uintptr_t>(__builtin_return_address(0)), reinterpret_cast<std::uintptr_t>(__builtin_frame_address(0)), __func__);
}

void APS5_VABI __asan_store2(std::uintptr_t address) {
    CheckAccess_nid_no_patch(address, 2, true, reinterpret_cast<std::uintptr_t>(__builtin_return_address(0)), reinterpret_cast<std::uintptr_t>(__builtin_frame_address(0)), __func__);
}

void APS5_VABI __asan_store2_noabort(std::uintptr_t address) {
    CheckAccess_nid_no_patch(address, 2, true, reinterpret_cast<std::uintptr_t>(__builtin_return_address(0)), reinterpret_cast<std::uintptr_t>(__builtin_frame_address(0)), __func__);
}

void APS5_VABI __asan_store4(std::uintptr_t address) {
    CheckAccess_nid_no_patch(address, 4, true, reinterpret_cast<std::uintptr_t>(__builtin_return_address(0)), reinterpret_cast<std::uintptr_t>(__builtin_frame_address(0)), __func__);
}

void APS5_VABI __asan_store4_noabort(std::uintptr_t address) {
    CheckAccess_nid_no_patch(address, 4, true, reinterpret_cast<std::uintptr_t>(__builtin_return_address(0)), reinterpret_cast<std::uintptr_t>(__builtin_frame_address(0)), __func__);
}

void APS5_VABI __asan_store8(std::uintptr_t address) {
    CheckAccess_nid_no_patch(address, 8, true, reinterpret_cast<std::uintptr_t>(__builtin_return_address(0)), reinterpret_cast<std::uintptr_t>(__builtin_frame_address(0)), __func__);
}

void APS5_VABI __asan_store8_noabort(std::uintptr_t address) {
    CheckAccess_nid_no_patch(address, 8, true, reinterpret_cast<std::uintptr_t>(__builtin_return_address(0)), reinterpret_cast<std::uintptr_t>(__builtin_frame_address(0)), __func__);
}

void APS5_VABI __asan_storeN(std::uintptr_t address, std::uintptr_t size) {
    CheckRegion_nid_no_patch(address, size, true, reinterpret_cast<std::uintptr_t>(__builtin_return_address(0)), reinterpret_cast<std::uintptr_t>(__builtin_frame_address(0)));
}

void APS5_VABI __asan_storeN_noabort(std::uintptr_t address, std::uintptr_t size) {
    CheckRegion_nid_no_patch(address, size, true, reinterpret_cast<std::uintptr_t>(__builtin_return_address(0)), reinterpret_cast<std::uintptr_t>(__builtin_frame_address(0)));
}

void APS5_VABI __asan_unpoison_intra_object_redzone(std::uintptr_t address, std::uintptr_t size) {
    UpdateIntraObjectShadow_nid_no_patch(address, size, false, __func__);
}

void APS5_VABI __asan_unpoison_memory_region(const volatile void* addr, std::uintptr_t size) {
    if (!kAllowUserPoisoning || size == 0) {
        return;
    }
    std::uintptr_t begAddr = reinterpret_cast<std::uintptr_t>(addr);
    std::uintptr_t endAddr = begAddr + size;
    if (endAddr < begAddr || endAddr > (1ULL << 47) ||
        (begAddr < AsanRuntime::ShadowBase + AsanRuntime::ShadowSize && endAddr > AsanRuntime::ShadowBase)) {
        InvalidArgument_nid_no_patch(__func__);
    }
    RequireShadow_nid_no_patch(begAddr, size + 1, __func__);
    ShadowSegmentEndpoint beg(begAddr);
    ShadowSegmentEndpoint end(endAddr);
    if (beg.chunk == end.chunk) {
        std::int8_t value = beg.value;
        if (value != 0) {
            *beg.chunk = std::max(value, end.offset);
        }
        return;
    }
    std::memset(beg.chunk, 0, end.chunk - beg.chunk);
    if (end.offset > 0 && end.value != 0) {
        *end.chunk = std::max(end.value, end.offset);
    }
}

void APS5_VABI __asan_unpoison_stack_memory(std::uintptr_t address, std::uintptr_t size) {
    if (size == 0) {
        return;
    }
    if ((address & 7) != 0 || address + size < address || address + size > (1ULL << 47) ||
        (address < AsanRuntime::ShadowBase + AsanRuntime::ShadowSize && address + size > AsanRuntime::ShadowBase)) {
        InvalidArgument_nid_no_patch(__func__);
    }
    RequireShadow_nid_no_patch(address, size, __func__);
    UpdateStackShadow_nid_no_patch(reinterpret_cast<std::int8_t*>(AsanRuntime::ShadowBase), AsanRuntime::ShadowSize, address, size, false);
}

int APS5_VABI __asan_unregister_elf_globals() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI __asan_unregister_globals() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI __asan_unregister_image_globals() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI __asan_update_allocation_context() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

void APS5_VABI __asan_version_mismatch_check_v6() {}

void APS5_VABI __asan_version_mismatch_check_v7() {}

void APS5_VABI __asan_version_mismatch_check_v8() {}

const char* APS5_VABI __sancov_default_options() {
    return "";
}

void APS5_VABI __sanitizer_annotate_contiguous_container(const void* beg, const void* end, const void* oldMid, const void* newMid) {
    const auto storageBeg = reinterpret_cast<std::uintptr_t>(beg);
    const auto storageEnd = reinterpret_cast<std::uintptr_t>(end);
    const auto oldEnd = reinterpret_cast<std::uintptr_t>(oldMid);
    const auto newEnd = reinterpret_cast<std::uintptr_t>(newMid);
    if (!(storageBeg <= oldEnd && storageBeg <= newEnd && oldEnd <= storageEnd && newEnd <= storageEnd)) {
        ReportBadContainerParams_nid_no_patch(storageBeg, storageEnd, oldEnd, newEnd);
    }
    if (storageEnd - storageBeg > AsanRuntime::kMaxContainerStorage) {
        InvalidArgument_nid_no_patch(__func__);
    }
    RequireGuestRange_nid_no_patch(storageBeg, storageEnd - storageBeg + 8, __func__);
    AsanRuntime::AnnotateContiguous_nid_no_patch(storageBeg, storageEnd, oldEnd, newEnd);
}

void APS5_VABI __sanitizer_annotate_double_ended_contiguous_container(const void* storageBeg, const void* storageEnd, const void* oldContainerBeg, const void* oldContainerEnd, const void* newContainerBeg, const void* newContainerEnd) {
    const auto storageBegin = reinterpret_cast<std::uintptr_t>(storageBeg);
    const auto storageLimit = reinterpret_cast<std::uintptr_t>(storageEnd);
    const auto oldBegin = reinterpret_cast<std::uintptr_t>(oldContainerBeg);
    const auto oldLimit = reinterpret_cast<std::uintptr_t>(oldContainerEnd);
    const auto newBegin = reinterpret_cast<std::uintptr_t>(newContainerBeg);
    const auto newLimit = reinterpret_cast<std::uintptr_t>(newContainerEnd);
    if (!(oldBegin <= oldLimit && newBegin <= newLimit) || !(storageBegin <= newBegin && newLimit <= storageLimit) || !(storageBegin <= oldBegin && oldLimit <= storageLimit)) {
        ReportBadDoubleEndedParams_nid_no_patch(storageBegin, storageLimit, oldBegin, oldLimit, newBegin, newLimit);
    }
    if (storageLimit - storageBegin > AsanRuntime::kMaxContainerStorage) {
        InvalidArgument_nid_no_patch(__func__);
    }
    RequireGuestRange_nid_no_patch(storageBegin, storageLimit - storageBegin + 8, __func__);
    AsanRuntime::AnnotateDoubleEnded_nid_no_patch(storageBegin, storageLimit, oldBegin, oldLimit, newBegin, newLimit);
}

const void* APS5_VABI __sanitizer_contiguous_container_find_bad_address(const void* beg, const void* mid, const void* end) {
    const auto first = reinterpret_cast<std::uintptr_t>(beg);
    const auto middle = reinterpret_cast<std::uintptr_t>(mid);
    const auto last = reinterpret_cast<std::uintptr_t>(end);
    if (!(first <= middle && middle <= last) || last - first > AsanRuntime::kMaxContainerStorage) {
        InvalidArgument_nid_no_patch(__func__);
    }
    RequireGuestRange_nid_no_patch(first, last - first + 8, __func__);
    return AsanRuntime::ContiguousFindBad_nid_no_patch(first, middle, last);
}

int APS5_VABI __sanitizer_cov_dump() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI __sanitizer_cov_trace_pc_guard() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI __sanitizer_cov_trace_pc_guard_init() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

const void* APS5_VABI __sanitizer_double_ended_contiguous_container_find_bad_address(const void* storageBeg, const void* containerBeg, const void* containerEnd, const void* storageEnd) {
    const auto storageBegin = reinterpret_cast<std::uintptr_t>(storageBeg);
    const auto storageLimit = reinterpret_cast<std::uintptr_t>(storageEnd);
    const auto begin = reinterpret_cast<std::uintptr_t>(containerBeg);
    const auto limit = reinterpret_cast<std::uintptr_t>(containerEnd);
    if (!(storageBegin <= begin && begin <= limit && limit <= storageLimit) || storageLimit - storageBegin > AsanRuntime::kMaxContainerStorage) {
        InvalidArgument_nid_no_patch(__func__);
    }
    RequireGuestRange_nid_no_patch(storageBegin, storageLimit - storageBegin + 8, __func__);
    return AsanRuntime::DoubleEndedFindBad_nid_no_patch(storageBegin, begin, limit, storageLimit);
}

int APS5_VABI __sanitizer_dump_coverage() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI __sanitizer_dump_trace_pc_guard_coverage() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI __sanitizer_finish_switch_fiber() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

void APS5_VABI __sanitizer_free_hook(void*) {
}

int APS5_VABI __sanitizer_get_allocated_begin() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI __sanitizer_get_allocated_size() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI __sanitizer_get_allocated_size_fast() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI __sanitizer_get_current_allocated_bytes() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

std::size_t APS5_VABI __sanitizer_get_estimated_allocated_size(std::size_t size) {
    return size;
}

int APS5_VABI __sanitizer_get_free_bytes() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI __sanitizer_get_heap_size() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI __sanitizer_get_ownership() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI __sanitizer_get_report_path() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

std::size_t APS5_VABI __sanitizer_get_unmapped_bytes() {
    return 0;
}

void APS5_VABI __sanitizer_malloc_hook(void*, std::size_t) {
}

int APS5_VABI __sanitizer_print_stack_trace() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

void APS5_VABI __sanitizer_ptr_cmp(void*, void*) {

}

void APS5_VABI __sanitizer_ptr_sub(void*, void*) {

}

int APS5_VABI __sanitizer_purge_allocator() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI __sanitizer_sandbox_on_notify() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

void APS5_VABI __sanitizer_set_death_callback(void (APS5_VABI* callback)()) {
    deathCallback.store(callback, std::memory_order_release);
}

int APS5_VABI __sanitizer_set_report_fd() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI __sanitizer_set_report_path() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI __sanitizer_start_switch_fiber() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI __sanitizer_symbolize_global() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI __sanitizer_symbolize_pc() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

std::uint16_t APS5_VABI __sanitizer_unaligned_load16(const void* ptr) {
    const auto addr = reinterpret_cast<std::uintptr_t>(ptr);
    if (!addr || addr + sizeof(std::uint16_t) < addr) {
        InvalidArgument_nid_no_patch(__func__);
    }
    CheckSmallRegion_nid_no_patch(addr, sizeof(std::uint16_t), false, reinterpret_cast<std::uintptr_t>(__builtin_return_address(0)), reinterpret_cast<std::uintptr_t>(__builtin_frame_address(0)));
    std::uint16_t val;
    std::memcpy(&val, ptr, sizeof(val));
    return val;
}

std::uint32_t APS5_VABI __sanitizer_unaligned_load32(const void* ptr) {
    const auto addr = reinterpret_cast<std::uintptr_t>(ptr);
    if (!addr || addr + sizeof(std::uint32_t) < addr) {
        InvalidArgument_nid_no_patch(__func__);
    }
    CheckSmallRegion_nid_no_patch(addr, sizeof(std::uint32_t), false, reinterpret_cast<std::uintptr_t>(__builtin_return_address(0)), reinterpret_cast<std::uintptr_t>(__builtin_frame_address(0)));
    std::uint32_t val;
    std::memcpy(&val, ptr, sizeof(val));
    return val;
}

std::uint64_t APS5_VABI __sanitizer_unaligned_load64(const void* ptr) {
    const auto addr = reinterpret_cast<std::uintptr_t>(ptr);
    if (!addr || addr + sizeof(std::uint64_t) < addr) {
        InvalidArgument_nid_no_patch(__func__);
    }
    CheckSmallRegion_nid_no_patch(addr, sizeof(std::uint64_t), false, reinterpret_cast<std::uintptr_t>(__builtin_return_address(0)), reinterpret_cast<std::uintptr_t>(__builtin_frame_address(0)));
    std::uint64_t val;
    std::memcpy(&val, ptr, sizeof(val));
    return val;
}

void APS5_VABI __sanitizer_unaligned_store16(void* ptr, std::uint16_t val) {
    const auto addr = reinterpret_cast<std::uintptr_t>(ptr);
    if (!addr || addr + sizeof(val) < addr) {
        InvalidArgument_nid_no_patch(__func__);
    }
    CheckSmallRegion_nid_no_patch(addr, sizeof(val), true, reinterpret_cast<std::uintptr_t>(__builtin_return_address(0)), reinterpret_cast<std::uintptr_t>(__builtin_frame_address(0)));
    std::memcpy(ptr, &val, sizeof(val));
}

void APS5_VABI __sanitizer_unaligned_store32(void* ptr, std::uint32_t val) {
    const auto addr = reinterpret_cast<std::uintptr_t>(ptr);
    if (!addr || addr + sizeof(val) < addr) {
        InvalidArgument_nid_no_patch(__func__);
    }
    CheckSmallRegion_nid_no_patch(addr, sizeof(val), true, reinterpret_cast<std::uintptr_t>(__builtin_return_address(0)), reinterpret_cast<std::uintptr_t>(__builtin_frame_address(0)));
    std::memcpy(ptr, &val, sizeof(val));
}

void APS5_VABI __sanitizer_unaligned_store64(void* ptr, std::uint64_t val) {
    const auto addr = reinterpret_cast<std::uintptr_t>(ptr);
    if (!addr || addr + sizeof(val) < addr) {
        InvalidArgument_nid_no_patch(__func__);
    }
    CheckSmallRegion_nid_no_patch(addr, sizeof(val), true, reinterpret_cast<std::uintptr_t>(__builtin_return_address(0)), reinterpret_cast<std::uintptr_t>(__builtin_frame_address(0)));
    std::memcpy(ptr, &val, sizeof(val));
}

int APS5_VABI __sanitizer_verify_contiguous_container(const void* beg, const void* mid, const void* end) {
    return __sanitizer_contiguous_container_find_bad_address(beg, mid, end) == nullptr ? 1 : 0;
}

int APS5_VABI __sanitizer_verify_double_ended_contiguous_container(const void* storageBeg, const void* containerBeg, const void* containerEnd, const void* storageEnd) {
    return __sanitizer_double_ended_contiguous_container_find_bad_address(storageBeg, containerBeg, containerEnd, storageEnd) == nullptr ? 1 : 0;
}

int APS5_VABI __ubsan_current_error_report() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI __ubsan_current_error_report_size() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

const char* APS5_VABI __ubsan_default_options() {
    return "";
}

void APS5_VABI __ubsan_get_current_report_data(const char** issueKind, const char** message, const char** filename, unsigned* line, unsigned* column, char** memoryAddress) {
    AsanRuntime::Ubsan::GetCurrentReportData_nid_no_patch(issueKind, message, filename, line, column, memoryAddress);
}

void APS5_VABI __ubsan_handle_add_overflow(void* data, std::uintptr_t lhs, std::uintptr_t rhs) {
    AsanRuntime::Ubsan::Handle_nid_no_patch(AsanRuntime::Ubsan::Check::AddOverflow, data, lhs, rhs, 0, false, reinterpret_cast<std::uintptr_t>(__builtin_return_address(0)), reinterpret_cast<std::uintptr_t>(__builtin_frame_address(0)));
}

void APS5_VABI __ubsan_handle_add_overflow_abort(void* data, std::uintptr_t lhs, std::uintptr_t rhs) {
    AsanRuntime::Ubsan::Handle_nid_no_patch(AsanRuntime::Ubsan::Check::AddOverflow, data, lhs, rhs, 0, true, reinterpret_cast<std::uintptr_t>(__builtin_return_address(0)), reinterpret_cast<std::uintptr_t>(__builtin_frame_address(0)));
    Die_nid_no_patch();
}

void APS5_VABI __ubsan_handle_alignment_assumption(void* data, std::uintptr_t pointer, std::uintptr_t alignment, std::uintptr_t offset) {
    AsanRuntime::Ubsan::Handle_nid_no_patch(AsanRuntime::Ubsan::Check::AlignmentAssumption, data, pointer, alignment, offset, false, reinterpret_cast<std::uintptr_t>(__builtin_return_address(0)), reinterpret_cast<std::uintptr_t>(__builtin_frame_address(0)));
}

void APS5_VABI __ubsan_handle_alignment_assumption_abort(void* data, std::uintptr_t pointer, std::uintptr_t alignment, std::uintptr_t offset) {
    AsanRuntime::Ubsan::Handle_nid_no_patch(AsanRuntime::Ubsan::Check::AlignmentAssumption, data, pointer, alignment, offset, true, reinterpret_cast<std::uintptr_t>(__builtin_return_address(0)), reinterpret_cast<std::uintptr_t>(__builtin_frame_address(0)));
    Die_nid_no_patch();
}

void APS5_VABI __ubsan_handle_builtin_unreachable(void* data) {
    AsanRuntime::Ubsan::Handle_nid_no_patch(AsanRuntime::Ubsan::Check::BuiltinUnreachable, data, 0, 0, 0, true, reinterpret_cast<std::uintptr_t>(__builtin_return_address(0)), reinterpret_cast<std::uintptr_t>(__builtin_frame_address(0)));
    Die_nid_no_patch();
}

void APS5_VABI __ubsan_handle_divrem_overflow(void* data, std::uintptr_t lhs, std::uintptr_t rhs) {
    AsanRuntime::Ubsan::Handle_nid_no_patch(AsanRuntime::Ubsan::Check::DivremOverflow, data, lhs, rhs, 0, false, reinterpret_cast<std::uintptr_t>(__builtin_return_address(0)), reinterpret_cast<std::uintptr_t>(__builtin_frame_address(0)));
}

void APS5_VABI __ubsan_handle_divrem_overflow_abort(void* data, std::uintptr_t lhs, std::uintptr_t rhs) {
    AsanRuntime::Ubsan::Handle_nid_no_patch(AsanRuntime::Ubsan::Check::DivremOverflow, data, lhs, rhs, 0, true, reinterpret_cast<std::uintptr_t>(__builtin_return_address(0)), reinterpret_cast<std::uintptr_t>(__builtin_frame_address(0)));
    Die_nid_no_patch();
}

int APS5_VABI __ubsan_handle_dynamic_type_cache_miss() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI __ubsan_handle_dynamic_type_cache_miss_abort() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

void APS5_VABI __ubsan_handle_float_cast_overflow(void* data, std::uintptr_t from) {
    AsanRuntime::Ubsan::Handle_nid_no_patch(AsanRuntime::Ubsan::Check::FloatCastOverflow, data, from, 0, 0, false, reinterpret_cast<std::uintptr_t>(__builtin_return_address(0)), reinterpret_cast<std::uintptr_t>(__builtin_frame_address(0)));
}

void APS5_VABI __ubsan_handle_float_cast_overflow_abort(void* data, std::uintptr_t from) {
    AsanRuntime::Ubsan::Handle_nid_no_patch(AsanRuntime::Ubsan::Check::FloatCastOverflow, data, from, 0, 0, true, reinterpret_cast<std::uintptr_t>(__builtin_return_address(0)), reinterpret_cast<std::uintptr_t>(__builtin_frame_address(0)));
    Die_nid_no_patch();
}

void APS5_VABI __ubsan_handle_function_type_mismatch(void* data, std::uintptr_t function) {
    AsanRuntime::Ubsan::Handle_nid_no_patch(AsanRuntime::Ubsan::Check::FunctionTypeMismatch, data, function, 0, 0, false, reinterpret_cast<std::uintptr_t>(__builtin_return_address(0)), reinterpret_cast<std::uintptr_t>(__builtin_frame_address(0)));
}

void APS5_VABI __ubsan_handle_function_type_mismatch_abort(void* data, std::uintptr_t function) {
    AsanRuntime::Ubsan::Handle_nid_no_patch(AsanRuntime::Ubsan::Check::FunctionTypeMismatch, data, function, 0, 0, true, reinterpret_cast<std::uintptr_t>(__builtin_return_address(0)), reinterpret_cast<std::uintptr_t>(__builtin_frame_address(0)));
    Die_nid_no_patch();
}

int APS5_VABI __ubsan_handle_function_type_mismatch_v1() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI __ubsan_handle_function_type_mismatch_v1_abort() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

void APS5_VABI __ubsan_handle_implicit_conversion(void* data, std::uintptr_t source, std::uintptr_t destination) {
    AsanRuntime::Ubsan::Handle_nid_no_patch(AsanRuntime::Ubsan::Check::ImplicitConversion, data, source, destination, 0, false, reinterpret_cast<std::uintptr_t>(__builtin_return_address(0)), reinterpret_cast<std::uintptr_t>(__builtin_frame_address(0)));
}

void APS5_VABI __ubsan_handle_implicit_conversion_abort(void* data, std::uintptr_t source, std::uintptr_t destination) {
    AsanRuntime::Ubsan::Handle_nid_no_patch(AsanRuntime::Ubsan::Check::ImplicitConversion, data, source, destination, 0, true, reinterpret_cast<std::uintptr_t>(__builtin_return_address(0)), reinterpret_cast<std::uintptr_t>(__builtin_frame_address(0)));
    Die_nid_no_patch();
}

void APS5_VABI __ubsan_handle_invalid_builtin(void* data) {
    AsanRuntime::Ubsan::Handle_nid_no_patch(AsanRuntime::Ubsan::Check::InvalidBuiltin, data, 0, 0, 0, true, reinterpret_cast<std::uintptr_t>(__builtin_return_address(0)), reinterpret_cast<std::uintptr_t>(__builtin_frame_address(0)));
}

void APS5_VABI __ubsan_handle_invalid_builtin_abort(void* data) {
    AsanRuntime::Ubsan::Handle_nid_no_patch(AsanRuntime::Ubsan::Check::InvalidBuiltin, data, 0, 0, 0, true, reinterpret_cast<std::uintptr_t>(__builtin_return_address(0)), reinterpret_cast<std::uintptr_t>(__builtin_frame_address(0)));
    Die_nid_no_patch();
}

void APS5_VABI __ubsan_handle_load_invalid_value(void* data, std::uintptr_t value) {
    AsanRuntime::Ubsan::Handle_nid_no_patch(AsanRuntime::Ubsan::Check::LoadInvalidValue, data, value, 0, 0, false, reinterpret_cast<std::uintptr_t>(__builtin_return_address(0)), reinterpret_cast<std::uintptr_t>(__builtin_frame_address(0)));
}

void APS5_VABI __ubsan_handle_load_invalid_value_abort(void* data, std::uintptr_t value) {
    AsanRuntime::Ubsan::Handle_nid_no_patch(AsanRuntime::Ubsan::Check::LoadInvalidValue, data, value, 0, 0, true, reinterpret_cast<std::uintptr_t>(__builtin_return_address(0)), reinterpret_cast<std::uintptr_t>(__builtin_frame_address(0)));
    Die_nid_no_patch();
}

void APS5_VABI __ubsan_handle_missing_return(void* data) {
    AsanRuntime::Ubsan::Handle_nid_no_patch(AsanRuntime::Ubsan::Check::MissingReturn, data, 0, 0, 0, true, reinterpret_cast<std::uintptr_t>(__builtin_return_address(0)), reinterpret_cast<std::uintptr_t>(__builtin_frame_address(0)));
    Die_nid_no_patch();
}

void APS5_VABI __ubsan_handle_mul_overflow(void* data, std::uintptr_t lhs, std::uintptr_t rhs) {
    AsanRuntime::Ubsan::Handle_nid_no_patch(AsanRuntime::Ubsan::Check::MulOverflow, data, lhs, rhs, 0, false, reinterpret_cast<std::uintptr_t>(__builtin_return_address(0)), reinterpret_cast<std::uintptr_t>(__builtin_frame_address(0)));
}

void APS5_VABI __ubsan_handle_mul_overflow_abort(void* data, std::uintptr_t lhs, std::uintptr_t rhs) {
    AsanRuntime::Ubsan::Handle_nid_no_patch(AsanRuntime::Ubsan::Check::MulOverflow, data, lhs, rhs, 0, true, reinterpret_cast<std::uintptr_t>(__builtin_return_address(0)), reinterpret_cast<std::uintptr_t>(__builtin_frame_address(0)));
    Die_nid_no_patch();
}

void APS5_VABI __ubsan_handle_negate_overflow(void* data, std::uintptr_t oldValue) {
    AsanRuntime::Ubsan::Handle_nid_no_patch(AsanRuntime::Ubsan::Check::NegateOverflow, data, oldValue, 0, 0, false, reinterpret_cast<std::uintptr_t>(__builtin_return_address(0)), reinterpret_cast<std::uintptr_t>(__builtin_frame_address(0)));
}

void APS5_VABI __ubsan_handle_negate_overflow_abort(void* data, std::uintptr_t oldValue) {
    AsanRuntime::Ubsan::Handle_nid_no_patch(AsanRuntime::Ubsan::Check::NegateOverflow, data, oldValue, 0, 0, true, reinterpret_cast<std::uintptr_t>(__builtin_return_address(0)), reinterpret_cast<std::uintptr_t>(__builtin_frame_address(0)));
    Die_nid_no_patch();
}

void APS5_VABI __ubsan_handle_nonnull_arg(void* data) {
    AsanRuntime::Ubsan::Handle_nid_no_patch(AsanRuntime::Ubsan::Check::NonNullArg, data, 0, 0, 0, false, reinterpret_cast<std::uintptr_t>(__builtin_return_address(0)), reinterpret_cast<std::uintptr_t>(__builtin_frame_address(0)));
}

void APS5_VABI __ubsan_handle_nonnull_arg_abort(void* data) {
    AsanRuntime::Ubsan::Handle_nid_no_patch(AsanRuntime::Ubsan::Check::NonNullArg, data, 0, 0, 0, true, reinterpret_cast<std::uintptr_t>(__builtin_return_address(0)), reinterpret_cast<std::uintptr_t>(__builtin_frame_address(0)));
    Die_nid_no_patch();
}

int APS5_VABI __ubsan_handle_nonnull_return() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI __ubsan_handle_nonnull_return_abort() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

void APS5_VABI __ubsan_handle_nonnull_return_v1(void* data, void* location) {
    AsanRuntime::Ubsan::Handle_nid_no_patch(AsanRuntime::Ubsan::Check::NonNullReturn, data, reinterpret_cast<std::uintptr_t>(location), 0, 0, false, reinterpret_cast<std::uintptr_t>(__builtin_return_address(0)), reinterpret_cast<std::uintptr_t>(__builtin_frame_address(0)));
}

void APS5_VABI __ubsan_handle_nonnull_return_v1_abort(void* data, void* location) {
    AsanRuntime::Ubsan::Handle_nid_no_patch(AsanRuntime::Ubsan::Check::NonNullReturn, data, reinterpret_cast<std::uintptr_t>(location), 0, 0, true, reinterpret_cast<std::uintptr_t>(__builtin_return_address(0)), reinterpret_cast<std::uintptr_t>(__builtin_frame_address(0)));
    Die_nid_no_patch();
}

void APS5_VABI __ubsan_handle_nullability_arg(void* data) {
    AsanRuntime::Ubsan::Handle_nid_no_patch(AsanRuntime::Ubsan::Check::NullabilityArg, data, 0, 0, 0, false, reinterpret_cast<std::uintptr_t>(__builtin_return_address(0)), reinterpret_cast<std::uintptr_t>(__builtin_frame_address(0)));
}

void APS5_VABI __ubsan_handle_nullability_arg_abort(void* data) {
    AsanRuntime::Ubsan::Handle_nid_no_patch(AsanRuntime::Ubsan::Check::NullabilityArg, data, 0, 0, 0, true, reinterpret_cast<std::uintptr_t>(__builtin_return_address(0)), reinterpret_cast<std::uintptr_t>(__builtin_frame_address(0)));
    Die_nid_no_patch();
}

void APS5_VABI __ubsan_handle_nullability_return_v1(void* data, void* location) {
    AsanRuntime::Ubsan::Handle_nid_no_patch(AsanRuntime::Ubsan::Check::NullabilityReturn, data, reinterpret_cast<std::uintptr_t>(location), 0, 0, false, reinterpret_cast<std::uintptr_t>(__builtin_return_address(0)), reinterpret_cast<std::uintptr_t>(__builtin_frame_address(0)));
}

void APS5_VABI __ubsan_handle_nullability_return_v1_abort(void* data, void* location) {
    AsanRuntime::Ubsan::Handle_nid_no_patch(AsanRuntime::Ubsan::Check::NullabilityReturn, data, reinterpret_cast<std::uintptr_t>(location), 0, 0, true, reinterpret_cast<std::uintptr_t>(__builtin_return_address(0)), reinterpret_cast<std::uintptr_t>(__builtin_frame_address(0)));
    Die_nid_no_patch();
}

void APS5_VABI __ubsan_handle_out_of_bounds(void* data, std::uintptr_t index) {
    AsanRuntime::Ubsan::Handle_nid_no_patch(AsanRuntime::Ubsan::Check::OutOfBounds, data, index, 0, 0, false, reinterpret_cast<std::uintptr_t>(__builtin_return_address(0)), reinterpret_cast<std::uintptr_t>(__builtin_frame_address(0)));
}

void APS5_VABI __ubsan_handle_out_of_bounds_abort(void* data, std::uintptr_t index) {
    AsanRuntime::Ubsan::Handle_nid_no_patch(AsanRuntime::Ubsan::Check::OutOfBounds, data, index, 0, 0, true, reinterpret_cast<std::uintptr_t>(__builtin_return_address(0)), reinterpret_cast<std::uintptr_t>(__builtin_frame_address(0)));
    Die_nid_no_patch();
}

void APS5_VABI __ubsan_handle_pointer_overflow(void* data, std::uintptr_t base, std::uintptr_t result) {
    AsanRuntime::Ubsan::Handle_nid_no_patch(AsanRuntime::Ubsan::Check::PointerOverflow, data, base, result, 0, false, reinterpret_cast<std::uintptr_t>(__builtin_return_address(0)), reinterpret_cast<std::uintptr_t>(__builtin_frame_address(0)));
}

void APS5_VABI __ubsan_handle_pointer_overflow_abort(void* data, std::uintptr_t base, std::uintptr_t result) {
    AsanRuntime::Ubsan::Handle_nid_no_patch(AsanRuntime::Ubsan::Check::PointerOverflow, data, base, result, 0, true, reinterpret_cast<std::uintptr_t>(__builtin_return_address(0)), reinterpret_cast<std::uintptr_t>(__builtin_frame_address(0)));
    Die_nid_no_patch();
}

void APS5_VABI __ubsan_handle_shift_out_of_bounds(void* data, std::uintptr_t lhs, std::uintptr_t rhs) {
    AsanRuntime::Ubsan::Handle_nid_no_patch(AsanRuntime::Ubsan::Check::ShiftOutOfBounds, data, lhs, rhs, 0, false, reinterpret_cast<std::uintptr_t>(__builtin_return_address(0)), reinterpret_cast<std::uintptr_t>(__builtin_frame_address(0)));
}

void APS5_VABI __ubsan_handle_shift_out_of_bounds_abort(void* data, std::uintptr_t lhs, std::uintptr_t rhs) {
    AsanRuntime::Ubsan::Handle_nid_no_patch(AsanRuntime::Ubsan::Check::ShiftOutOfBounds, data, lhs, rhs, 0, true, reinterpret_cast<std::uintptr_t>(__builtin_return_address(0)), reinterpret_cast<std::uintptr_t>(__builtin_frame_address(0)));
    Die_nid_no_patch();
}

void APS5_VABI __ubsan_handle_sub_overflow(void* data, std::uintptr_t lhs, std::uintptr_t rhs) {
    AsanRuntime::Ubsan::Handle_nid_no_patch(AsanRuntime::Ubsan::Check::SubOverflow, data, lhs, rhs, 0, false, reinterpret_cast<std::uintptr_t>(__builtin_return_address(0)), reinterpret_cast<std::uintptr_t>(__builtin_frame_address(0)));
}

void APS5_VABI __ubsan_handle_sub_overflow_abort(void* data, std::uintptr_t lhs, std::uintptr_t rhs) {
    AsanRuntime::Ubsan::Handle_nid_no_patch(AsanRuntime::Ubsan::Check::SubOverflow, data, lhs, rhs, 0, true, reinterpret_cast<std::uintptr_t>(__builtin_return_address(0)), reinterpret_cast<std::uintptr_t>(__builtin_frame_address(0)));
    Die_nid_no_patch();
}

int APS5_VABI __ubsan_handle_type_mismatch() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI __ubsan_handle_type_mismatch_abort() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

void APS5_VABI __ubsan_handle_type_mismatch_v1(void* data, std::uintptr_t pointer) {
    AsanRuntime::Ubsan::Handle_nid_no_patch(AsanRuntime::Ubsan::Check::TypeMismatch, data, pointer, 0, 0, false, reinterpret_cast<std::uintptr_t>(__builtin_return_address(0)), reinterpret_cast<std::uintptr_t>(__builtin_frame_address(0)));
}

void APS5_VABI __ubsan_handle_type_mismatch_v1_abort(void* data, std::uintptr_t pointer) {
    AsanRuntime::Ubsan::Handle_nid_no_patch(AsanRuntime::Ubsan::Check::TypeMismatch, data, pointer, 0, 0, true, reinterpret_cast<std::uintptr_t>(__builtin_return_address(0)), reinterpret_cast<std::uintptr_t>(__builtin_frame_address(0)));
    Die_nid_no_patch();
}

void APS5_VABI __ubsan_handle_vla_bound_not_positive(void* data, std::uintptr_t bound) {
    AsanRuntime::Ubsan::Handle_nid_no_patch(AsanRuntime::Ubsan::Check::VlaBound, data, bound, 0, 0, false, reinterpret_cast<std::uintptr_t>(__builtin_return_address(0)), reinterpret_cast<std::uintptr_t>(__builtin_frame_address(0)));
}

void APS5_VABI __ubsan_handle_vla_bound_not_positive_abort(void* data, std::uintptr_t bound) {
    AsanRuntime::Ubsan::Handle_nid_no_patch(AsanRuntime::Ubsan::Check::VlaBound, data, bound, 0, 0, true, reinterpret_cast<std::uintptr_t>(__builtin_return_address(0)), reinterpret_cast<std::uintptr_t>(__builtin_frame_address(0)));
    Die_nid_no_patch();
}

int APS5_VABI __ubsan_vptr_type_cache() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

}
