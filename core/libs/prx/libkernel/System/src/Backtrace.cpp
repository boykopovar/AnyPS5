#include "prx/libkernel/include/Api.hpp"
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <string>
#include <vector>

#include "prx/libc/include/general/VabiMacros.hpp"

#if defined(__linux__) || defined(__APPLE__)
#include <dlfcn.h>
#include <execinfo.h>
#elif defined(_WIN32)
#include <windows.h>
#include "prx/libc/include/Api.hpp"
#include "prx/libc/include/exceptions/Unwind.hpp"
extern "C" {
std::string BacktraceModulePath_nid_no_patch(HMODULE handle, std::size_t initialCapacity);
}
#endif

namespace {

constexpr int MaxFrames = 128;
constexpr int FramesToSkip = 2;
constexpr char GuestSuffix[] = ".guest.prx";

struct Frame {
    std::uintptr_t address;
    std::uintptr_t moduleBase;
    std::string module;
    std::string symbol;
};

#if defined(_WIN32)
const char* NearestExportName(HMODULE module, std::uintptr_t address) {
    const auto base = reinterpret_cast<const std::uint8_t*>(module);
    const auto* dos = reinterpret_cast<const IMAGE_DOS_HEADER*>(base);
    if (dos->e_magic != IMAGE_DOS_SIGNATURE) return nullptr;
    const auto* nt = reinterpret_cast<const IMAGE_NT_HEADERS64*>(base + dos->e_lfanew);
    if (nt->Signature != IMAGE_NT_SIGNATURE) return nullptr;
    const auto& directory = nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_EXPORT];
    if (directory.VirtualAddress == 0 || directory.Size == 0) return nullptr;
    const auto* exports = reinterpret_cast<const IMAGE_EXPORT_DIRECTORY*>(base + directory.VirtualAddress);
    const auto* functions = reinterpret_cast<const DWORD*>(base + exports->AddressOfFunctions);
    const auto* names = reinterpret_cast<const DWORD*>(base + exports->AddressOfNames);
    const auto* ordinals = reinterpret_cast<const WORD*>(base + exports->AddressOfNameOrdinals);
    const auto rva = address - reinterpret_cast<std::uintptr_t>(module);
    const char* name = nullptr;
    DWORD nearest = 0;
    for (DWORD index = 0; index < exports->NumberOfNames; ++index) {
        if (ordinals[index] >= exports->NumberOfFunctions) continue;
        const DWORD target = functions[ordinals[index]];
        if (target - directory.VirtualAddress < directory.Size || target > rva || (name != nullptr && target <= nearest)) continue;
        name = reinterpret_cast<const char*>(base + names[index]);
        nearest = target;
    }
    return name;
}
#endif

void TrimModule(std::string& module) {
    if (const auto slash = module.find_last_of("/\\"); slash != std::string::npos) module.erase(0, slash + 1);
    if (module.size() > sizeof(GuestSuffix) - 1
        && module.compare(module.size() - (sizeof(GuestSuffix) - 1), std::string::npos, GuestSuffix) == 0) {
        module.resize(module.size() - (sizeof(GuestSuffix) - 1));
    }
}

std::vector<Frame> CaptureFrames(void* caller) {
    void* addresses[MaxFrames] = {};
#if defined(__linux__) || defined(__APPLE__)
    const int captured = backtrace(addresses, MaxFrames);
#elif defined(_WIN32)
    struct TraceState {
        void** addresses;
        int max;
        int count;
    };
    static const auto trace = [](_Unwind_Context* context, void* argument) APS5_VABI -> _Unwind_Reason_Code {
        auto* state = static_cast<TraceState*>(argument);
        if (state->count < state->max) state->addresses[state->count++] = reinterpret_cast<void*>(_Unwind_GetIP_nid_postfix(context));
        return state->count >= state->max ? _URC_NORMAL_STOP : _URC_NO_REASON;
    };
    TraceState state{addresses, MaxFrames, 0};
    _Unwind_Backtrace_nid_postfix(trace, &state);
    const int captured = state.count;
#else
    const int captured = 0;
#endif
    int first = FramesToSkip;
    for (int index = 0; index < captured; ++index) {
        if (addresses[index] != caller) continue;
        first = index;
        break;
    }
    std::vector<Frame> frames;
    for (int index = first; index < captured; ++index) {
        Frame frame{reinterpret_cast<std::uintptr_t>(addresses[index]), 0, {}, "NoName"};
#if defined(__linux__) || defined(__APPLE__)
        Dl_info info{};
        if (dladdr(addresses[index], &info) == 0 || info.dli_fbase == nullptr || info.dli_fname == nullptr) continue;
        frame.moduleBase = reinterpret_cast<std::uintptr_t>(info.dli_fbase);
        frame.module = info.dli_fname;
        if (info.dli_sname != nullptr) frame.symbol = info.dli_sname;
#elif defined(_WIN32)
        HMODULE module = nullptr;
        if (GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                               static_cast<LPCSTR>(addresses[index]), &module) == 0 || module == nullptr) {
            continue;
        }
        frame.moduleBase = reinterpret_cast<std::uintptr_t>(module);
        frame.module = BacktraceModulePath_nid_no_patch(module, MAX_PATH);
        if (frame.module.empty()) continue;
        if (const char* name = NearestExportName(module, frame.address); name != nullptr) frame.symbol = name;
#endif
        TrimModule(frame.module);
        frames.push_back(std::move(frame));
    }
    return frames;
}

void PrintFrames(const std::vector<Frame>& frames) {
    std::fprintf(stderr, "backtrace:\n");
    int index = 0;
    for (const auto& frame : frames) {
        const std::uintptr_t offset = frame.address - frame.moduleBase;
        std::fprintf(stderr, "[%d]    0x%llx => %s%s+0x%llx\n", index++, static_cast<unsigned long long>(frame.address),
                     frame.symbol.c_str(), frame.module.c_str(), static_cast<unsigned long long>(offset));
    }
    std::fprintf(stderr, "[%d]    0x0 => NoName+0x0\n", index);
    std::fflush(stderr);
}

}

extern "C" {

int APS5_VABI sceKernelPrintBacktraceWithModuleInfo(void) {
    PrintFrames(CaptureFrames(__builtin_return_address(0)));
    return 0;
}

}
