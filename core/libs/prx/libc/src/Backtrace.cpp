#include "prx/libc/include/Api.hpp"
#include <cstddef>
#include <cstdint>
#include <stdexcept>
#include <string>
#include <vector>

#include "prx/libc/include/General.hpp"

#if defined(__linux__) || defined(__APPLE__)
#include <dlfcn.h>
#include <execinfo.h>
#elif defined(_WIN32)
#include <windows.h>
#include "prx/libc/include/exceptions/Unwind.hpp"
extern "C" {

std::string BacktraceModulePath_nid_no_patch(HMODULE handle, std::size_t initialCapacity) {
    std::string module;
    std::size_t capacity = initialCapacity == 0 ? 1 : initialCapacity;
    while (true) {
        module.resize(capacity);
        SetLastError(ERROR_SUCCESS);
        const DWORD length = GetModuleFileNameA(handle, module.data(), static_cast<DWORD>(module.size()));
        if (length == 0) return {};
        if (length < capacity && GetLastError() != ERROR_INSUFFICIENT_BUFFER) {
            module.resize(length);
            return module;
        }
        if (capacity >= 32768) return {};
        capacity *= 2;
    }
}
}
#endif

#if defined(__linux__) || defined(__APPLE__) || defined(_WIN32)

namespace {

constexpr int MaxFrames = 256;
constexpr std::size_t NameOffset = 0x10;
constexpr char GuestSuffix[] = ".guest.prx";

struct Frame {
    std::uint32_t offset;
    std::string module;
};

struct FrameRecord {
    FrameRecord* next;
    std::uint32_t offset;
    std::uint32_t reserved;
};
static_assert(sizeof(FrameRecord) == NameOffset);

[[gnu::noinline]] std::vector<Frame> CaptureFrames(int limit, const void* caller) {
    void* addresses[MaxFrames];
#if defined(__linux__) || defined(__APPLE__)
    const int captured = backtrace(addresses, MaxFrames);
#elif defined(_WIN32)
    struct TraceState {
        void** addrs;
        int max;
        int count;
    };
    static const auto traceCallback = [](_Unwind_Context* ctx, void* arg) APS5_VABI -> _Unwind_Reason_Code {
        auto* s = static_cast<TraceState*>(arg);
        if (s->count < s->max) {
            s->addrs[s->count++] = reinterpret_cast<void*>(_Unwind_GetIP_nid_postfix(ctx));
        }
        return (s->count >= s->max) ? _URC_NORMAL_STOP : _URC_NO_REASON;
    };
    TraceState state{addresses, MaxFrames, 0};
    _Unwind_Backtrace_nid_postfix(traceCallback, &state);

    const int captured = state.count;
#endif
    int first = 2;
    for (int index = 0; index < captured; ++index) {
        if (addresses[index] != caller) continue;
        first = index;
        break;
    }
    std::vector<Frame> frames;
    for (int index = first; index < captured && (limit < 0 || static_cast<int>(frames.size()) < limit); ++index) {
#if defined(__linux__) || defined(__APPLE__)
        Dl_info info{};
        if (dladdr(addresses[index], &info) == 0 || info.dli_fbase == nullptr || info.dli_fname == nullptr) continue;
        const auto offset = reinterpret_cast<std::uintptr_t>(addresses[index]) - reinterpret_cast<std::uintptr_t>(info.dli_fbase);
        if (offset > UINT32_MAX) continue;
        std::string module = info.dli_fname;
#elif defined(_WIN32)
        HMODULE handle = nullptr;
        if (!GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                                static_cast<LPCSTR>(addresses[index]), &handle) || handle == nullptr) {
            continue;
        }
        const auto offset = reinterpret_cast<std::uintptr_t>(addresses[index]) - reinterpret_cast<std::uintptr_t>(handle);
        if (offset > UINT32_MAX) continue;
        std::string module = BacktraceModulePath_nid_no_patch(handle, MAX_PATH);
        if (module.empty()) continue;
#endif
        if (const auto slash = module.find_last_of("/\\"); slash != std::string::npos) module.erase(0, slash + 1);
        if (module.size() > sizeof(GuestSuffix) - 1 && module.compare(module.size() - (sizeof(GuestSuffix) - 1), std::string::npos, GuestSuffix) == 0)
            module.resize(module.size() - (sizeof(GuestSuffix) - 1));
        frames.push_back({static_cast<std::uint32_t>(offset), std::move(module)});
    }
    return frames;
}

std::size_t RecordSize(const Frame& frame) {
    return (NameOffset + frame.module.size() + 1 + 7) & ~std::size_t{7};
}

void WriteBacktraceOutput(void* destination, const void* source, std::size_t bytes, const char* function) {
    if (!GuestCopyOut_nid_no_patch(destination, source, bytes))
        throw std::invalid_argument(std::string(function) + ": unwritable output");
}

}  // namespace

extern "C" {

int APS5_VABI sceLibcBacktraceGetBufferSize_nid_postfix(int limit, int* count, std::size_t* size) {
    if (count == nullptr || size == nullptr) throw std::invalid_argument("sceLibcBacktraceGetBufferSize: null output");
    const auto frames = CaptureFrames(limit, __builtin_return_address(0));
    std::size_t total = 0;
    for (const auto& frame : frames) total += RecordSize(frame);
    const int frameCount = static_cast<int>(frames.size());
    WriteBacktraceOutput(count, &frameCount, sizeof(frameCount), __func__);
    WriteBacktraceOutput(size, &total, sizeof(total), __func__);
    return 0;
}

int APS5_VABI sceLibcBacktraceSelf_nid_postfix(int limit, void* buffer, std::size_t size, int* count) {
    if (buffer == nullptr || count == nullptr) throw std::invalid_argument("sceLibcBacktraceSelf: null argument");
    const auto frames = CaptureFrames(limit, __builtin_return_address(0));
    const auto base = reinterpret_cast<std::uintptr_t>(buffer);
    std::size_t used = 0;
    FrameRecord* previous = nullptr;
    int written = 0;
    for (const auto& frame : frames) {
        const auto bytes = RecordSize(frame);
        if (bytes > size - used) break;
        if (used > UINTPTR_MAX - base || bytes > UINTPTR_MAX - base - used)
            throw std::invalid_argument("sceLibcBacktraceSelf: output range wraps");
        auto* record = reinterpret_cast<FrameRecord*>(base + used);
        const FrameRecord value{nullptr, frame.offset, 0};
        WriteBacktraceOutput(record, &value, sizeof(value), __func__);
        WriteBacktraceOutput(reinterpret_cast<void*>(base + used + NameOffset), frame.module.c_str(),
                             frame.module.size() + 1, __func__);
        if (previous != nullptr) WriteBacktraceOutput(previous, &record, sizeof(record), __func__);
        previous = record;
        used += bytes;
        ++written;
    }
    WriteBacktraceOutput(count, &written, sizeof(written), __func__);
    return 0;
}

}

#else

extern "C" {

int APS5_VABI sceLibcBacktraceGetBufferSize_nid_postfix(int limit, int* count, std::size_t* size) {
    (void)limit;
    (void)count;
    (void)size;
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceLibcBacktraceSelf_nid_postfix(int limit, void* buffer, std::size_t size, int* count) {
    (void)limit;
    (void)buffer;
    (void)size;
    (void)count;
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

}

#endif
