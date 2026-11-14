#include "prx/libkernel/include/Api.hpp"
#include <array>
#include <cstdint>
#include <cstddef>
#include <atomic>
#include <cstdio>
#include <cstring>
#include <limits>
#include <stdexcept>
#include <string>
#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"
#include "prx/libc/include/ApplicationHeap.hpp"

// The handler is recorded but never invoked: host crashes are not turned into guest core dumps.
static std::atomic<uint64_t> g_coredumpHandler{0};
static std::atomic<uint64_t> g_coredumpContext{0};
static std::atomic<std::uint32_t> coredumpSdkVersion{0};

static constexpr int SCE_COREDUMP_ERROR_INVALID_ARGUMENT = static_cast<int>(0x81180000u);
static constexpr int SCE_COREDUMP_ERROR_NOT_IN_COREDUMP_HANDLER = static_cast<int>(0x81180003u);

static bool validUserFileFlags(std::uint32_t userValue) {
    return coredumpSdkVersion.load(std::memory_order_relaxed) < 0x06500000 || (userValue & 0xc0000000u) != 0x40000000u;
}

extern "C" {

int APS5_VABI sceCoredumpRegisterCoredumpHandler(uint64_t handler, size_t stack_size, uint64_t context) {
    if (g_coredumpHandler.load(std::memory_order_relaxed)) return static_cast<int>(0x81180002u);
    if (!handler || stack_size < 0x4000 || stack_size >= 0x20000000) return SCE_COREDUMP_ERROR_INVALID_ARGUMENT;
    const auto* parameters = static_cast<const std::byte*>(ApplicationProcessParameters_nid_no_patch());
    std::uint32_t sdkVersion;
    std::memcpy(&sdkVersion, parameters + 16, sizeof(sdkVersion));
    coredumpSdkVersion.store(sdkVersion, std::memory_order_relaxed);
    g_coredumpHandler.store(handler, std::memory_order_relaxed);
    g_coredumpContext.store(context, std::memory_order_relaxed);
    return 0;
}

int APS5_VABI sceCoredumpUnregisterCoredumpHandler(void) {
    if (!g_coredumpHandler.load(std::memory_order_relaxed)) return static_cast<int>(0x81180001u);
    g_coredumpHandler.store(0, std::memory_order_relaxed);
    g_coredumpContext.store(0, std::memory_order_relaxed);
    return 0;
}

int APS5_VABI sceKernelDebugWriteCppExceptionInfo(const void* exception, uint64_t unknown, const char* typeName, const char* what) {
    (void)unknown;
    std::fprintf(stderr, "[coredump] uncaught C++ exception %p of type %s%s%s\n", exception, typeName ? typeName : "(unknown)", what ? ", what(): " : "", what ? what : "");
    return 0;
}

int APS5_VABI sceCoredumpAttachUserFile(std::uint32_t userValue, const char* path) {
    if (!path || !validUserFileFlags(userValue)) return SCE_COREDUMP_ERROR_INVALID_ARGUMENT;
    std::string guestPath;
    if (CopyGuestPath_nid_no_patch(path, guestPath) != 0) return SCE_COREDUMP_ERROR_INVALID_ARGUMENT;
    return SCE_COREDUMP_ERROR_NOT_IN_COREDUMP_HANDLER;
}

int APS5_VABI sceCoredumpGetStopInfoGpu_Agc(void* info, size_t size) {
    (void)size;
    if (info == nullptr) return SCE_COREDUMP_ERROR_INVALID_ARGUMENT;
    return SCE_COREDUMP_ERROR_NOT_IN_COREDUMP_HANDLER;
}

int APS5_VABI sceCoredumpAttachMemoryRegionAsUserFile(std::uint32_t userValue, const void* mem, std::int32_t size, const char* name) {
    (void)mem;
    if (!name || !validUserFileFlags(userValue)) return SCE_COREDUMP_ERROR_INVALID_ARGUMENT;
    return size < 0 ? 0 : SCE_COREDUMP_ERROR_NOT_IN_COREDUMP_HANDLER;
}

int APS5_VABI sceCoredumpSetUserDataType(std::int32_t type) {
    (void)type;
    return 0;
}

void APS5_VABI sceCoredumpDebugTextOut(const char* str, int len) {
    if (len < 0 || (str == nullptr && len != 0))
        throw std::runtime_error(std::string(__func__) + ": null text or negative length");
    if (len == 0) return;
    const auto address = reinterpret_cast<std::uintptr_t>(str);
    const auto lastOffset = static_cast<std::size_t>(len - 1);
    if (address > std::numeric_limits<std::uintptr_t>::max() - lastOffset)
        throw std::runtime_error(std::string(__func__) + ": unreadable text");
    char last{};
    if (!GuestCopyIn_nid_no_patch(&last, reinterpret_cast<const void*>(address + lastOffset), 1))
        throw std::runtime_error(std::string(__func__) + ": unreadable text");
    const auto shown = last == '\n' ? lastOffset : static_cast<std::size_t>(len);
    std::string guestText;
    std::array<char, 256> part{};
    for (std::size_t offset = 0; offset < shown;) {
        const auto remaining = shown - offset;
        const auto count = remaining < part.size() ? remaining : part.size();
        const auto* source = reinterpret_cast<const void*>(address + offset);
        if (GuestCopyIn_nid_no_patch(part.data(), source, count)) {
            const auto* end = static_cast<const char*>(std::memchr(part.data(), 0, count));
            guestText.append(part.data(), end ? static_cast<std::size_t>(end - part.data()) : count);
            if (end) break;
            offset += count;
            continue;
        }
        char byte{};
        if (!GuestCopyIn_nid_no_patch(&byte, source, 1))
            throw std::runtime_error(std::string(__func__) + ": unreadable text");
        if (byte == 0) break;
        guestText.push_back(byte);
        ++offset;
    }
    std::fprintf(stderr, "[coredump] %s\n", guestText.c_str());
}

int64_t APS5_VABI sceCoredumpWriteUserData(const void* data, size_t size) {
    if (!data || size > INT32_MAX) return SCE_COREDUMP_ERROR_INVALID_ARGUMENT;
    return SCE_COREDUMP_ERROR_NOT_IN_COREDUMP_HANDLER;
}

int APS5_VABI sceCoredumpGetStopInfoCpu(void* info, size_t size) {
    if (!info || (size != 8 && size != 16)) return SCE_COREDUMP_ERROR_INVALID_ARGUMENT;
    return SCE_COREDUMP_ERROR_NOT_IN_COREDUMP_HANDLER;
}

int APS5_VABI sceCoredumpGetThreadContextInfo(void* threadContextInfo, size_t threadContextInfoSize) {
    if (!threadContextInfo || threadContextInfoSize != 144) return SCE_COREDUMP_ERROR_INVALID_ARGUMENT;
    return SCE_COREDUMP_ERROR_NOT_IN_COREDUMP_HANDLER;
}

int APS5_VABI sceCoredumpWriteUserString(uint32_t first, const char* text, int third) {
    if (text == nullptr || first > 15 || third < 0 || third > 256) return SCE_COREDUMP_ERROR_INVALID_ARGUMENT;
    return 0;
}

int APS5_VABI sceCoredumpConfigDumpMode(int mode) {
    return mode == 1 ? 0 : SCE_COREDUMP_ERROR_INVALID_ARGUMENT;
}

int APS5_VABI sceCoredumpAttachUserMemoryFile(std::uint32_t userValue, const void* mem, std::int32_t size) {
    (void)userValue;
    (void)mem;
    return size < 0 ? 0 : SCE_COREDUMP_ERROR_NOT_IN_COREDUMP_HANDLER;
}

int APS5_VABI sceCoredumpAttachMemoryRegion(std::uint32_t userValue, const void* mem, std::int32_t size) {
    (void)userValue;
    (void)mem;
    return size < 0 ? 0 : SCE_COREDUMP_ERROR_NOT_IN_COREDUMP_HANDLER;
}
}
