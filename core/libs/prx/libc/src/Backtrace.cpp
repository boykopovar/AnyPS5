#include <charconv>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <string>
#include <vector>
#include "prx/libc/include/exceptions/Unwind.hpp"
#include "prx/libc/include/General.hpp"
#if defined(__linux__) || defined(__APPLE__)
#include <dlfcn.h>
#endif
#ifdef _WIN32
#include <windows.h>
#endif

extern "C" {
_Unwind_Reason_Code APS5_VABI _Unwind_Backtrace_nid_postfix(_Unwind_Trace_Fn, void*);
_Unwind_Ptr APS5_VABI _Unwind_GetIP_nid_postfix(_Unwind_Context*);
}

namespace {

struct CapturedFrame {
    std::uintptr_t address;
    std::string text;
};

struct CaptureState {
    std::vector<CapturedFrame>* frames;
    std::uint32_t limit;
    unsigned skip;
};

std::string Hex(std::uintptr_t value) {
    std::string result("0x");
    char buffer[2 + sizeof(value) * 2];
    const auto conversion = std::to_chars(buffer, buffer + sizeof(buffer), value, 16);
    result.append(buffer, conversion.ptr);
    return result;
}

std::string FrameText(std::uintptr_t address) {
#if defined(__linux__) || defined(__APPLE__)
    Dl_info info{};
    if (dladdr(reinterpret_cast<void*>(address), &info) != 0 && info.dli_fname != nullptr) {
        const char* name = std::strrchr(info.dli_fname, '/');
        std::string result(name != nullptr ? name + 1 : info.dli_fname);
        if (info.dli_sname != nullptr) {
            result += '!';
            result += info.dli_sname;
            if (info.dli_saddr != nullptr) result += '+' + Hex(address - reinterpret_cast<std::uintptr_t>(info.dli_saddr));
        } else if (info.dli_fbase != nullptr) {
            result += '+' + Hex(address - reinterpret_cast<std::uintptr_t>(info.dli_fbase));
        }
        if (result.size() > 255) result.resize(255);
        return result;
    }
#endif
#ifdef _WIN32
    HMODULE module = nullptr;
    if (GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
            reinterpret_cast<LPCSTR>(address), &module)) {
        char path[MAX_PATH] = {};
        const auto length = GetModuleFileNameA(module, path, sizeof(path));
        if (length != 0 && length < sizeof(path)) {
            const char* name = std::strrchr(path, '\\');
            std::string result(name != nullptr ? name + 1 : path);
            result += '+' + Hex(address - reinterpret_cast<std::uintptr_t>(module));
            if (result.size() > 255) result.resize(255);
            return result;
        }
    }
#endif
    return Hex(address);
}

_Unwind_Reason_Code CaptureFrame(_Unwind_Context* context, void* argument) {
    auto& state = *static_cast<CaptureState*>(argument);
    if (state.skip != 0) {
        --state.skip;
        return _URC_NO_REASON;
    }
    if (state.frames->size() == state.limit) return _URC_NORMAL_STOP;
    const auto address = _Unwind_GetIP_nid_postfix(context);
    if (address != 0) state.frames->push_back({address, FrameText(address)});
    return _URC_NO_REASON;
}

bool CaptureFrames(std::uint32_t limit, std::vector<CapturedFrame>& frames) {
    if (limit == 0) return true;
    CaptureState state{&frames, limit, 2};
    const auto result = _Unwind_Backtrace_nid_postfix(CaptureFrame, &state);
    return result == _URC_END_OF_STACK || result == _URC_NORMAL_STOP;
}

std::size_t RecordSize(const CapturedFrame& frame) {
    return 0x18 + ((frame.text.size() + 7) & ~std::size_t(7));
}

int Capture(std::uint32_t limit, int* count, std::size_t* size) {
    std::vector<CapturedFrame> frames;
    if (!CaptureFrames(limit, frames)) return 1;
    std::size_t required = 0;
    for (const auto& frame : frames) required += RecordSize(frame);
    *count = static_cast<int>(frames.size());
    *size = required;
    return 0;
}

}

extern "C" {

int APS5_VABI sceLibcBacktraceGetBufferSize_nid_postfix(int maxFrames, int* count, std::size_t* size) {
    if (count == nullptr || size == nullptr) return 1;
    return Capture(static_cast<std::uint32_t>(maxFrames), count, size);
}

int APS5_VABI sceLibcBacktraceSelf_nid_postfix(int maxFrames, void* buffer, std::size_t bufferSize, int* count) {
    if (buffer == nullptr || count == nullptr) return 1;
    std::vector<CapturedFrame> frames;
    if (!CaptureFrames(static_cast<std::uint32_t>(maxFrames), frames)) return 1;
    auto* current = static_cast<std::byte*>(buffer);
    std::size_t remaining = bufferSize;
    int written = 0;
    for (const auto& frame : frames) {
        const auto recordSize = RecordSize(frame);
        if (recordSize > remaining) break;
        const auto next = recordSize < remaining ? current + recordSize : nullptr;
        std::memset(current, 0, recordSize);
        std::memcpy(current, &next, sizeof(next));
        std::memcpy(current + 0x10, frame.text.c_str(), frame.text.size() + 1);
        current += recordSize;
        remaining -= recordSize;
        ++written;
    }
    *count = written;
    return 0;
}

}
