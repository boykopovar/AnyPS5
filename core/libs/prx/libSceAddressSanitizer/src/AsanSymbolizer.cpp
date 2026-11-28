/* Derived from LLVM compiler-rt lib/sanitizer_common (sanitizer_stacktrace.cpp, sanitizer_stacktrace_libcdep.cpp, sanitizer_stacktrace_printer.cpp, sanitizer_symbolizer_report.cpp) and lib/asan (asan_stack.h, asan_thread.cpp) at llvmorg-16.0.6 (af2ed9f794c422923414d162dd1f48d986e2d6e3) */
/* Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
 * See https://llvm.org/LICENSE.txt for license information.
 * SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception */
#include "prx/libSceAddressSanitizer/src/AsanRuntime.hpp"
#include "prx/libSceAddressSanitizer/include/Api.hpp"
#include "prx/libc/include/General.hpp"
#include "prx/libkernel/include/Api.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <map>
#include <mutex>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <vector>

#if !defined(_WIN32)
#include <cxxabi.h>
#include <dlfcn.h>
#endif

namespace Asan::Runtime {

namespace {

constexpr uptr PageSize = 4096;
constexpr uptr FrameSize = 2 * sizeof(uptr);
constexpr std::size_t ThreadNameCapacity = 32;

bool IsValidFrame(uptr frame, uptr stackTop, uptr stackBottom) {
    return frame > stackBottom && frame < stackTop - FrameSize;
}

std::string Demangle(const char* name) {
#if !defined(_WIN32)
    if (name[0] == '_' && name[1] == 'Z') {
        int status = 0;
        char* const demangled = abi::__cxa_demangle(name, nullptr, nullptr, &status);
        if (status == 0 && demangled != nullptr) {
            std::string text(demangled);
            std::free(demangled);
            return text;
        }
        std::free(demangled);
    }
#endif
    return name;
}

std::string StripPathPrefix(const std::string& path) {
    const std::string& prefix = GetFlags().stripPathPrefix;
    if (prefix.empty()) return path;
    const std::size_t found = path.find(prefix);
    if (found == std::string::npos) return path;
    return path.substr(found + prefix.size());
}

std::string StripModuleName(const std::string& module) {
    const std::size_t slash = module.find_last_of("/\\");
    return slash == std::string::npos ? module : module.substr(slash + 1);
}

struct FrameInfo {
    uptr address = 0;
    bool hasModule = false;
    std::string module;
    uptr moduleOffset = 0;
    bool hasFunction = false;
    std::string function;
    uptr functionOffset = 0;
};

bool ModuleForAddress(uptr address, std::string* name, uptr* base) {
    ModuleInfoEx info{};
    info.st_size = sizeof(info);
    if (sceKernelGetModuleInfoFromAddr(address, 1, &info) != 0) return false;
    if (info.segment_count == 0 || info.name[0] == '\0') return false;
    uptr lowest = ~uptr{0};
    for (std::uint32_t index = 0; index < info.segment_count && index < std::size(info.segments); ++index)
        lowest = std::min<uptr>(lowest, info.segments[index].address);
    *name = std::string(info.name, strnlen(info.name, sizeof(info.name)));
    *base = lowest;
    return true;
}

FrameInfo SymbolizePc(uptr pc) {
    FrameInfo frame;
    frame.address = pc;
    std::string module;
    uptr base = 0;
    try {
        if (ModuleForAddress(pc, &module, &base) && pc >= base) {
            frame.hasModule = true;
            frame.module = module;
            frame.moduleOffset = pc - base;
        }
    } catch (const std::exception&) {
        frame.hasModule = false;
    }
#if !defined(_WIN32)
    if (GetFlags().symbolize) {
        Dl_info symbol{};
        if (::dladdr(reinterpret_cast<const void*>(pc), &symbol) != 0 && symbol.dli_sname != nullptr && symbol.dli_saddr != nullptr) {
            frame.hasFunction = true;
            frame.function = Demangle(symbol.dli_sname);
            frame.functionOffset = pc - reinterpret_cast<uptr>(symbol.dli_saddr);
        }
    }
#endif
    return frame;
}

std::string RenderModuleLocation(const std::string& module, uptr offset) {
    return Format("(%s+0x%zx)", StripPathPrefix(module).c_str(), static_cast<std::size_t>(offset));
}

std::string RenderFrame(const char* format, unsigned number, const FrameInfo& info) {
    if (std::strcmp(format, "DEFAULT") == 0) format = "    #%n %p %F %L";
    std::string text;
    for (const char* cursor = format; *cursor != '\0'; ++cursor) {
        if (*cursor != '%') {
            text.push_back(*cursor);
            continue;
        }
        ++cursor;
        switch (*cursor) {
        case '%': text.push_back('%'); break;
        case 'n': text += Format("%u", number); break;
        case 'p': text += Format("0x%zx", static_cast<std::size_t>(info.address)); break;
        case 'm': text += StripPathPrefix(info.module); break;
        case 'o': text += Format("0x%zx", static_cast<std::size_t>(info.moduleOffset)); break;
        case 'f': text += info.function; break;
        case 'q': text += Format("0x%zx", static_cast<std::size_t>(info.hasFunction ? info.functionOffset : 0)); break;
        case 's': break;
        case 'l': text += "0"; break;
        case 'c': text += "0"; break;
        case 'F':
            if (info.hasFunction) text += "in " + info.function;
            break;
        case 'S': break;
        case 'L':
            if (info.hasModule) text += RenderModuleLocation(info.module, info.moduleOffset);
            else text += "(<unknown module>)";
            break;
        case 'M':
            if (info.hasModule) text += RenderModuleLocation(StripModuleName(info.module), info.moduleOffset);
            else text += "(" + Pointer(info.address) + ")";
            break;
        default: throw std::invalid_argument(std::string("guest AddressSanitizer: unsupported specifier in stack frame format: ") + *cursor);
        }
    }
    return text;
}

std::mutex depotMutex;
std::vector<Trace> depotTraces;
std::map<Trace, std::uint32_t> depotIds;

std::mutex threadNamesMutex;
std::unordered_map<int, std::string> threadNames;

}

bool ModuleAndOffsetForPc(uptr pc, std::string* module, uptr* offset) {
    uptr base = 0;
    if (!ModuleForAddress(pc, module, &base) || pc < base) return false;
    *offset = pc - base;
    return true;
}

uptr PreviousInstructionPc(uptr pc) {
    return pc - 1;
}

Trace UnwindFrom(uptr pc, uptr bp, uptr maxDepth) {
    Trace trace;
    if (maxDepth == 0) return trace;
    trace.push_back(pc);
    if (maxDepth < 2) return trace;
    uptr stackBottom = 0;
    uptr stackTop = 0;
    if (!CurrentStackBounds(&stackBottom, &stackTop) || stackTop < PageSize) return trace;
    uptr frame = bp;
    uptr bottom = stackBottom;
    while (IsValidFrame(frame, stackTop, bottom) && (frame & (sizeof(uptr) - 1)) == 0 && trace.size() < maxDepth) {
        const uptr* const words = reinterpret_cast<const uptr*>(frame);
        const uptr returnAddress = words[1];
        if (returnAddress < PageSize) break;
        if (returnAddress != pc) trace.push_back(returnAddress);
        bottom = frame;
        frame = words[0];
    }
    return trace;
}

Trace MallocTrace(uptr here, uptr caller, uptr bp) {
    const int depth = GetFlags().mallocContextSize;
    if (depth <= 0) return {};
    if (depth == 1) return {here};
    if (depth == 2) return {here, caller};
    return UnwindFrom(here, bp, static_cast<uptr>(depth));
}

std::string RenderStack(const Trace& trace) {
    if (trace.empty()) return "    <empty stack>\n\n";
    std::string text;
    unsigned number = 0;
    for (const uptr address : trace) {
        if (address == 0) break;
        text += RenderFrame(GetFlags().stackTraceFormat.c_str(), number++, SymbolizePc(PreviousInstructionPc(address))) + "\n";
    }
    return text + "\n";
}

std::string SummaryFor(const char* bugType, const Trace& trace) {
    if (!GetFlags().printSummary) return {};
    if (trace.empty()) return Format("SUMMARY: AddressSanitizer: %s\n", bugType);
    const FrameInfo frame = SymbolizePc(PreviousInstructionPc(trace.front()));
    return Format("SUMMARY: AddressSanitizer: %s %s\n", bugType, RenderFrame("%L %F", 0, frame).c_str());
}

std::uint32_t StackDepotPut(const Trace& trace) {
    if (trace.empty()) return 0;
    std::lock_guard lock(depotMutex);
    const auto found = depotIds.find(trace);
    if (found != depotIds.end()) return found->second;
    depotTraces.push_back(trace);
    const auto id = static_cast<std::uint32_t>(depotTraces.size());
    depotIds.emplace(trace, id);
    return id;
}

Trace StackDepotGet(std::uint32_t id) {
    if (id == 0) return {};
    std::lock_guard lock(depotMutex);
    if (id > depotTraces.size()) throw std::logic_error("guest AddressSanitizer: unknown stack depot id");
    return depotTraces[id - 1];
}

std::size_t StackDepotSize() {
    std::lock_guard lock(depotMutex);
    return depotTraces.size();
}

int CurrentThreadId() {
    const Pthread self = scePthreadSelf();
    if (self == nullptr) return -1;
    const int tid = scePthreadGetthreadid();
    char name[ThreadNameCapacity] = {};
    if (scePthreadGetname(self, name) == 0) {
        name[ThreadNameCapacity - 1] = '\0';
        std::lock_guard lock(threadNamesMutex);
        threadNames[tid] = name;
    }
    return tid;
}

std::string ThreadIdAndName(int tid) {
    std::string name;
    {
        std::lock_guard lock(threadNamesMutex);
        const auto found = threadNames.find(tid);
        if (found != threadNames.end()) name = found->second;
    }
    if (name.empty()) return Format("T%d", tid);
    return Format("T%d (%s)", tid, name.c_str());
}

std::string SymbolizePcWith(uptr pc, const char* format) {
    return RenderFrame(format, 0, SymbolizePc(PreviousInstructionPc(pc)));
}

std::string RenderGlobalData(uptr address, const char* format, bool* found) {
    std::string name;
    std::string file;
    uptr line = 0;
    *found = false;
    char globalName[256] = {};
    uptr begin = 0;
    uptr size = 0;
    if (GlobalRegionForAddress(address, globalName, sizeof(globalName), &begin, &size)) {
        name = globalName;
        *found = true;
    }
#if !defined(_WIN32)
    if (!*found && GetFlags().symbolize) {
        Dl_info symbol{};
        if (::dladdr(reinterpret_cast<const void*>(address), &symbol) != 0 && symbol.dli_sname != nullptr) {
            name = Demangle(symbol.dli_sname);
            *found = true;
        }
    }
#endif
    if (!*found) return {};
    std::string text;
    for (const char* cursor = format; *cursor != '\0'; ++cursor) {
        if (*cursor != '%') {
            text.push_back(*cursor);
            continue;
        }
        ++cursor;
        switch (*cursor) {
        case '%': text += "%%"; break;
        case 's': text += StripPathPrefix(file); break;
        case 'l': text += Format("%zu", static_cast<std::size_t>(line)); break;
        case 'g': text += name; break;
        default: throw std::invalid_argument(std::string("guest AddressSanitizer: unsupported specifier in stack frame format: ") + *cursor);
        }
    }
    return text;
}

}

using namespace Asan::Runtime;

namespace {

std::string GuestFormat(const char* format) {
    Require(format != nullptr, "guest AddressSanitizer: null symbolization format");
    std::string text;
    if (CopyGuestString_nid_no_patch(format, GuestStringMaximum, text) != 0) throw std::invalid_argument("guest AddressSanitizer: symbolization format is not readable");
    return text;
}

void CopyToGuestBuffer(const std::string& text, char* buffer, std::uintptr_t size) {
    if (size == 0) return;
    const std::size_t count = std::min<std::size_t>(text.size(), size - 1);
    std::string copy = text.substr(0, count);
    copy.push_back('\0');
    if (!GuestCopyOut_nid_no_patch(buffer, copy.data(), copy.size())) throw std::invalid_argument("guest AddressSanitizer: symbolization buffer is not writable");
}

}

extern "C" {

void APS5_VABI __sanitizer_symbolize_pc(std::uintptr_t pc, const char* format, char* buffer, std::uintptr_t size) {
    if (size == 0) return;
    CopyToGuestBuffer(SymbolizePcWith(pc, GuestFormat(format).c_str()), buffer, size);
}

void APS5_VABI __sanitizer_symbolize_global(std::uintptr_t address, const char* format, char* buffer, std::uintptr_t size) {
    if (size == 0) return;
    CopyToGuestBuffer("", buffer, size);
    bool found = false;
    const std::string text = RenderGlobalData(address, GuestFormat(format).c_str(), &found);
    if (found) CopyToGuestBuffer(text, buffer, size);
}

}
