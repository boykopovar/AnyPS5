#include "prx/libSceFios2/include/Api.hpp"
#include "Runtime.hpp"
#include "prx/libc/include/Api.hpp"
#include "prx/libc/include/General.hpp"
#include "prx/libc/include/HostThreadLocal.hpp"
#include "prx/libkernel/include/Api.hpp"
#include <algorithm>
#include <array>
#include <chrono>
#include <cstdio>
#include <ctime>
#include <mutex>
#include <string>
#include <vector>

namespace {

struct Timestamp {
    std::string tag;
    std::int32_t value = 0;
    std::int64_t time = 0;
    std::uint64_t thread = 0;
};

std::mutex g_timestampMutex;
std::vector<Timestamp> g_timestamps;
bool g_timestampOverflow = false;
std::uint32_t g_dllOverlayCount = 0;

const char* CopyDump(const std::string& text, char* output, std::size_t capacity) {
    if (output != nullptr) {
        if (capacity == 0) return output;
        const auto count = std::min(text.size(), capacity - 1);
        if (count != 0 && !Fios::CopyOut(output, text.data(), count)) return nullptr;
        const char terminator = 0;
        if (!Fios::CopyOut(output + count, &terminator, 1)) return nullptr;
        return output;
    }
    struct DumpBufferStorage {};
    auto& buffer = HostThreadLocal<std::string, DumpBufferStorage>();
    buffer = text;
    return buffer.c_str();
}

const char* ErrorName(std::int32_t error) {
    if (error >= 0) return error == 0 ? "SCE_FIOS_OK" : "SCE_FIOS_IN_PROGRESS";
    switch (static_cast<std::uint32_t>(error) - 0x80820000u) {
    case 0x00: return "SCE_FIOS_ERROR_UNIMPLEMENTED";
    case 0x01: return "SCE_FIOS_ERROR_CANT_ALLOCATE_OP";
    case 0x02: return "SCE_FIOS_ERROR_CANT_ALLOCATE_FH";
    case 0x03: return "SCE_FIOS_ERROR_CANT_ALLOCATE_DH";
    case 0x04: return "SCE_FIOS_ERROR_CANT_ALLOCATE_CHUNK";
    case 0x05: return "SCE_FIOS_ERROR_BAD_PATH";
    case 0x06: return "SCE_FIOS_ERROR_BAD_PTR";
    case 0x07: return "SCE_FIOS_ERROR_BAD_OFFSET";
    case 0x08: return "SCE_FIOS_ERROR_BAD_SIZE";
    case 0x09: return "SCE_FIOS_ERROR_BAD_IOVCNT";
    case 0x0a: return "SCE_FIOS_ERROR_BAD_OP";
    case 0x0b: return "SCE_FIOS_ERROR_BAD_FH";
    case 0x0c: return "SCE_FIOS_ERROR_BAD_DH";
    case 0x0d: return "SCE_FIOS_ERROR_BAD_ALIGNMENT";
    case 0x0e: return "SCE_FIOS_ERROR_NOT_A_FILE";
    case 0x0f: return "SCE_FIOS_ERROR_NOT_A_DIRECTORY";
    case 0x10: return "SCE_FIOS_ERROR_EOF";
    case 0x11: return "SCE_FIOS_ERROR_TIMEOUT";
    case 0x12: return "SCE_FIOS_ERROR_CANCELLED";
    case 0x13: return "SCE_FIOS_ERROR_ACCESS";
    case 0x14: return "SCE_FIOS_ERROR_DECOMPRESSION";
    case 0x15: return "SCE_FIOS_ERROR_READ_ONLY";
    case 0x16: return "SCE_FIOS_ERROR_WRITE_ONLY";
    case 0x17: return "SCE_FIOS_ERROR_MEDIA_GONE";
    case 0x18: return "SCE_FIOS_ERROR_PATH_TOO_LONG";
    case 0x19: return "SCE_FIOS_ERROR_TOO_MANY_OVERLAYS";
    case 0x1a: return "SCE_FIOS_ERROR_BAD_OVERLAY";
    case 0x1b: return "SCE_FIOS_ERROR_BAD_ORDER";
    case 0x1c: return "SCE_FIOS_ERROR_BAD_INDEX";
    case 0x1d: return "SCE_FIOS_ERROR_EVENT_NOT_HANDLED";
    case 0x1e: return "SCE_FIOS_ERROR_BUSY";
    case 0x1f: return "SCE_FIOS_ERROR_BAD_ARCHIVE";
    case 0x21: return "SCE_FIOS_ERROR_BAD_FLAGS";
    case 0x22: return "SCE_FIOS_ERROR_UNKNOWN";
    case 0x23: return "SCE_FIOS_ERROR_ALREADY_EXISTS";
    case 0x24: return "SCE_FIOS_ERROR_IN_CALLBACK";
    default: return nullptr;
    }
}

int Vprintf(const char* format, VaList* args) {
    std::string copied;
    if (format == nullptr || args == nullptr || CopyGuestString_nid_no_patch(format, SCE_FIOS_PATH_MAX, copied) != 0) return -1;
    return vprintf_nid_postfix(copied.c_str(), args);
}

}

extern "C" {

void* APS5_VABI sceFiosDLLInitialize() {
    return &g_dllOverlayCount;
}

std::int64_t APS5_VABI sceFiosDLLTerminate() {
    g_dllOverlayCount = 0;
    return 0;
}

std::int64_t APS5_VABI sceFiosTimeGetCurrent() {
    return std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now().time_since_epoch()).count();
}

std::int64_t APS5_VABI sceFiosTraceTimestamp(char* buffer, std::uint64_t size) {
    if (buffer == nullptr || size == 0) return SCE_FIOS_ERROR_BAD_PTR;
    const auto now = (sceFiosTimeGetCurrent() + 999) / 1000;
    std::array<char, 32> text{};
    std::snprintf(text.data(), text.size(), "%lld.%06lld", static_cast<long long>(now / 1000000), static_cast<long long>(now % 1000000));
    const auto count = std::min<std::uint64_t>(size - 1, text.size() - 1);
    if (!Fios::CopyOut(buffer, text.data(), count)) return SCE_FIOS_ERROR_BAD_PTR;
    const char terminator = 0;
    if (!Fios::CopyOut(buffer + count, &terminator, 1)) return SCE_FIOS_ERROR_BAD_PTR;
    return SCE_FIOS_OK;
}

std::int64_t APS5_VABI sceFiosSaveTimeStamp(const char* tag, std::int32_t value) {
    std::string copiedTag;
    if (tag == nullptr) return SCE_FIOS_ERROR_BAD_PTR;
    const int copyError = CopyGuestString_nid_no_patch(tag, 256, copiedTag);
    if (copyError == 14) return SCE_FIOS_ERROR_BAD_PTR;
    if (copyError != 0) return SCE_FIOS_ERROR_PATH_TOO_LONG;
    const auto thread = static_cast<std::uint64_t>(scePthreadGetthreadid());
    const std::lock_guard lock(g_timestampMutex);
    if (g_timestamps.size() == 0x8000) {
        g_timestamps.erase(g_timestamps.begin());
        g_timestampOverflow = true;
    }
    g_timestamps.push_back({std::move(copiedTag), value, sceFiosTimeGetCurrent(), thread});
    return static_cast<std::int64_t>(thread);
}

void APS5_VABI sceFiosClearTimeStamps() {
    const std::lock_guard lock(g_timestampMutex);
    g_timestamps.clear();
    g_timestampOverflow = false;
}

std::int64_t APS5_VABI sceFiosPrintTimeStamps() {
    std::vector<Timestamp> timestamps;
    bool overflow = false;
    {
        const std::lock_guard lock(g_timestampMutex);
        timestamps = g_timestamps;
        overflow = g_timestampOverflow;
        g_timestampOverflow = false;
    }
    sceFiosPrintf("--------------------------------------------------------------------------------\n");
    if (overflow) sceFiosPrintf("Overflowed while capturing timings\n");
    if (timestamps.empty()) return sceFiosPrintf("Time stamp dump: no entries\n");
    sceFiosPrintf("Time stamp dump:\n");
    sceFiosPrintf("%-16s %-32s %8s %-16s %-6s\n", "    Thread", "    Tag", " Value", "   Absolute us", "Delta");
    std::int64_t previous = timestamps.front().time;
    for (const auto& timestamp : timestamps) {
        const auto absolute = (timestamp.time + 999) / 1000;
        const auto delta = (timestamp.time - previous + 999) / 1000;
        sceFiosPrintf("%016llx %-32s %8x %16lld %6lld\n", static_cast<unsigned long long>(timestamp.thread), timestamp.tag.c_str(), static_cast<unsigned int>(timestamp.value), static_cast<long long>(absolute), static_cast<long long>(delta));
        previous = timestamp.time;
    }
    return sceFiosPrintf("--------------------------------------------------------------------------------\n");
}

int APS5_VABI sceFiosPrintf(const char* format, ...) {
#ifdef _WIN32
    __builtin_sysv_va_list args;
    __builtin_sysv_va_start(args, format);
#else
    std::va_list args;
    va_start(args, format);
#endif
    const int result = Vprintf(format, reinterpret_cast<VaList*>(args));
#ifdef _WIN32
    __builtin_sysv_va_end(args);
#else
    va_end(args);
#endif
    return result;
}

int APS5_VABI sceFiosVprintf(const char* format, VaList* args) {
    return Vprintf(format, args);
}

const char* APS5_VABI sceFiosDebugDumpError(std::int32_t error, char* buffer, std::size_t size) {
    const char* name = ErrorName(error);
    std::array<char, 96> text{};
    if (name != nullptr) std::snprintf(text.data(), text.size(), "%s", name);
    else std::snprintf(text.data(), text.size(), "Unknown FIOS error 0x%08x", static_cast<std::uint32_t>(error));
    return CopyDump(text.data(), buffer, size);
}

const char* APS5_VABI sceFiosDebugDumpDate(SceFiosDate date, char* buffer, std::size_t size) {
    SceFiosTm components{};
    if (sceFiosDateToComponents(date, &components) == nullptr) return nullptr;
    std::array<char, 96> text{};
    std::snprintf(text.data(), text.size(), "%04d-%02d-%02d %02d:%02d:%02d.%09llu", components.tm_year + 1900, components.tm_mon + 1, components.tm_mday, components.tm_hour, components.tm_min, components.tm_sec, static_cast<unsigned long long>(date % 1000000000ull));
    return CopyDump(text.data(), buffer, size);
}

const char* APS5_VABI sceFiosDebugDumpFH(SceFiosFH handle, char* buffer, std::size_t size) {
    const auto file = Fios::FindFile(handle);
    std::array<char, 256> text{};
    if (!file) std::snprintf(text.data(), text.size(), "Invalid FH %d", handle);
    else {
        const std::lock_guard lock(file->mutex);
        std::snprintf(text.data(), text.size(), "FH %d descriptor=%d path=%s", handle, file->descriptor, file->path.c_str());
    }
    return CopyDump(text.data(), buffer, size);
}

const char* APS5_VABI sceFiosDebugDumpDH(SceFiosDH handle, char* buffer, std::size_t size) {
    const auto directory = Fios::FindDirectory(handle);
    std::array<char, 256> text{};
    if (!directory) std::snprintf(text.data(), text.size(), "Invalid DH %d", handle);
    else {
        const std::lock_guard lock(directory->mutex);
        std::snprintf(text.data(), text.size(), "DH %d path=%s entry=%zu/%zu", handle, directory->path.c_str(), directory->next, directory->names.size());
    }
    return CopyDump(text.data(), buffer, size);
}

const char* APS5_VABI sceFiosDebugDumpOp(SceFiosOp op, char* buffer, std::size_t size) {
    const auto path = Fios::OpPath(op);
    std::int32_t error = SCE_FIOS_ERROR_BAD_OP;
    const bool exists = Fios::OpError(op, error);
    std::array<char, 256> text{};
    if (!exists) std::snprintf(text.data(), text.size(), "Invalid op %d", op);
    else std::snprintf(text.data(), text.size(), "Op %d error=0x%08x path=%s", op, static_cast<std::uint32_t>(error), path == nullptr ? "" : path);
    return CopyDump(text.data(), buffer, size);
}

}
