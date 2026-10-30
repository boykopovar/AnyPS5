#include "prx/libSceAddressSanitizer/src/ModuleLookup.hpp"
#include <cstring>

#if defined(_WIN32)
#include <process.h>
#include <windows.h>

namespace AsanRuntime {

bool ModuleForAddress_nid_no_patch(std::uintptr_t address, std::string& name, std::uintptr_t& offset) {
    HMODULE module = nullptr;
    if (!GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT, reinterpret_cast<LPCWSTR>(address), &module)) {
        return false;
    }
    wchar_t wide[4096];
    const DWORD length = GetModuleFileNameW(module, wide, 4096);
    if (length == 0 || length >= 4096) {
        return false;
    }
    char narrow[4 * 4096];
    const int written = WideCharToMultiByte(CP_UTF8, 0, wide, static_cast<int>(length), narrow, sizeof(narrow), nullptr, nullptr);
    if (written <= 0) {
        return false;
    }
    name.assign(narrow, static_cast<std::size_t>(written));
    offset = address - reinterpret_cast<std::uintptr_t>(module);
    return true;
}

std::size_t CaptureBacktrace_nid_no_patch(std::uintptr_t* frames, std::size_t maxFrames, std::uintptr_t pc, std::uintptr_t framePointer) {
    if (maxFrames == 0) {
        return 0;
    }
    std::size_t count = 0;
    frames[count++] = pc;
    CONTEXT context{};
    context.Rip = pc;
    context.Rsp = framePointer + 2 * sizeof(std::uintptr_t);
    context.Rbp = *reinterpret_cast<const std::uintptr_t*>(framePointer);
    while (count < maxFrames) {
        DWORD64 imageBase = 0;
        PRUNTIME_FUNCTION function = RtlLookupFunctionEntry(context.Rip, &imageBase, nullptr);
        if (!function) {
            break;
        }
        void* handlerData = nullptr;
        DWORD64 establisher = 0;
        RtlVirtualUnwind(UNW_FLAG_NHANDLER, imageBase, context.Rip, function, &context, &handlerData, &establisher, nullptr);
        if (!context.Rip) {
            break;
        }
        frames[count++] = context.Rip;
    }
    return count;
}

std::uintptr_t HostProcessId_nid_no_patch() {
    return static_cast<std::uintptr_t>(_getpid());
}

}  // namespace AsanRuntime
#elif defined(__linux__)
#include <dlfcn.h>
#include <unistd.h>
#if defined(__GLIBC__)
#include <execinfo.h>
#endif

namespace AsanRuntime {

bool ModuleForAddress_nid_no_patch(std::uintptr_t address, std::string& name, std::uintptr_t& offset) {
    Dl_info info{};
    if (!dladdr(reinterpret_cast<void*>(address), &info) || !info.dli_fname || !info.dli_fbase) {
        return false;
    }
    name = info.dli_fname;
    offset = address - reinterpret_cast<std::uintptr_t>(info.dli_fbase);
    return true;
}

std::size_t CaptureBacktrace_nid_no_patch(std::uintptr_t* frames, std::size_t maxFrames, std::uintptr_t pc, std::uintptr_t framePointer) {
    static_cast<void>(framePointer);
    if (maxFrames == 0) {
        return 0;
    }
    std::size_t written = 0;
    frames[written++] = pc;
#if defined(__GLIBC__)
    void* captured[256];
    const std::size_t limit = maxFrames + 2 < 256 ? maxFrames + 2 : 256;
    const int count = backtrace(captured, static_cast<int>(limit));
    for (int i = 2; i < count && written < maxFrames; ++i) {
        frames[written++] = reinterpret_cast<std::uintptr_t>(captured[i]);
    }
#endif
    return written;
}

std::uintptr_t HostProcessId_nid_no_patch() {
    return static_cast<std::uintptr_t>(getpid());
}

}  // namespace AsanRuntime
#endif

namespace AsanRuntime {

const char* StripModuleName_nid_no_patch(const char* module) {
    if (!module) {
        return nullptr;
    }
#if defined(_WIN32)
    if (const char* backslash = std::strrchr(module, '\\')) {
        return StripModuleName_nid_no_patch(backslash + 1);
    }
#endif
    if (const char* slash = std::strrchr(module, '/')) {
        return slash + 1;
    }
    return module;
}

}  // namespace AsanRuntime
