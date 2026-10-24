#ifdef _WIN32
#include <windows.h>
#include <tlhelp32.h>
#include "prx/libc/include/CallTrace.hpp"
#include "prx/libc/include/FailureReport.hpp"
#include "prx/libc/include/GuestArena.hpp"
#include "prx/libc/include/HostFault.hpp"
#include "prx/libc/include/Shutdown.hpp"
#include "prx/libc/include/CrashText.hpp"
#include <algorithm>
#include <atomic>
#include <chrono>
#include <csignal>
#include <cstdint>
#include <cstdio>
#include <cstdarg>
#include <cstdlib>
#include <cstring>
#include <iterator>
#include <utility>
#include <mutex>
#include "Sse4aEmulation.hpp"

namespace {

// The reporter runs on a faulting thread that may itself hold the CRT stream lock (a fault inside
// printf), so it writes straight to the stderr handle.
void Report(const char* format, ...) {
    char buffer[1024];
    va_list args;
    va_start(args, format);
    int length = std::vsnprintf(buffer, sizeof(buffer), format, args);
    va_end(args);
    if (length <= 0) return;
    if (length > static_cast<int>(sizeof(buffer)) - 1) length = sizeof(buffer) - 1;
    DWORD written = 0;
    WriteFile(GetStdHandle(STD_ERROR_HANDLE), buffer, static_cast<DWORD>(length), &written, nullptr);
}

void ReportAllThreads();
void ReportStackCandidates(std::uint64_t rsp, int limit, std::uint64_t span);
void ReportCallTrace();

void DescribeAddress(std::uint64_t address, char* buffer, std::size_t size) {
    HMODULE module = nullptr;
    char path[MAX_PATH] = "?";
    if (GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT, reinterpret_cast<LPCSTR>(address), &module) && module) {
        GetModuleFileNameA(module, path, sizeof(path));
        const char* name = path;
        for (const char* cursor = path; *cursor; ++cursor) {
            if (*cursor == '\\' || *cursor == '/') name = cursor + 1;
        }
        std::snprintf(buffer, size, "0x%016llx %s+0x%llx", static_cast<unsigned long long>(address), name, static_cast<unsigned long long>(address - reinterpret_cast<std::uint64_t>(module)));
        return;
    }
    std::snprintf(buffer, size, "0x%016llx", static_cast<unsigned long long>(address));
}

bool IsExecutable(std::uint64_t address) {
    MEMORY_BASIC_INFORMATION memory{};
    if (VirtualQuery(reinterpret_cast<const void*>(address), &memory, sizeof(memory)) != sizeof(memory)) return false;
    if (memory.State != MEM_COMMIT) return false;
    const DWORD protection = memory.Protect & 0xff;
    return protection == PAGE_EXECUTE || protection == PAGE_EXECUTE_READ || protection == PAGE_EXECUTE_READWRITE || protection == PAGE_EXECUTE_WRITECOPY;
}

bool IsReadable(std::uint64_t address) {
    MEMORY_BASIC_INFORMATION memory{};
    if (VirtualQuery(reinterpret_cast<const void*>(address), &memory, sizeof(memory)) != sizeof(memory)) return false;
    return memory.State == MEM_COMMIT && (memory.Protect & (PAGE_NOACCESS | PAGE_GUARD)) == 0;
}

bool IsFatal(DWORD code) {
    switch (code) {
    case EXCEPTION_ACCESS_VIOLATION:
    case EXCEPTION_ILLEGAL_INSTRUCTION:
    case EXCEPTION_PRIV_INSTRUCTION:
    case EXCEPTION_STACK_OVERFLOW:
    case EXCEPTION_INT_DIVIDE_BY_ZERO:
    case EXCEPTION_ARRAY_BOUNDS_EXCEEDED:
    case EXCEPTION_IN_PAGE_ERROR:
        return true;
    default:
        return false;
    }
}

// Debug aid: APS5_WATCH_WRITE=<guest ELF vaddr> reports every write to that 8-byte location by
// making its page read-only and single-stepping each trapped write.
std::uintptr_t g_watchAddress = 0;
std::uintptr_t g_watchPage = 0;
thread_local bool t_watchStepping = false;
thread_local bool t_watchHit = false;
thread_local unsigned long long t_watchBefore = 0;
thread_local std::uintptr_t t_watchFault = 0;
thread_local char t_watchWhere[MAX_PATH + 64];

void ProtectWatchPage(DWORD protection) {
    DWORD old;
    VirtualProtect(reinterpret_cast<void*>(g_watchPage), 0x1000, protection, &old);
}

bool HandleWatch(EXCEPTION_POINTERS* info) {
    if (g_watchPage == 0) return false;
    const auto* record = info->ExceptionRecord;
    auto* context = info->ContextRecord;
    if (record->ExceptionCode == EXCEPTION_SINGLE_STEP && t_watchStepping) {
        t_watchStepping = false;
        ProtectWatchPage(PAGE_READONLY);
        const auto current = *reinterpret_cast<const unsigned long long*>(g_watchAddress);
        if (t_watchHit || current != t_watchBefore) {
            if (!t_watchHit) Report("[watch] thread %lu changed the value with a write starting at 0x%llx from %s\n", GetCurrentThreadId(), static_cast<unsigned long long>(t_watchFault), t_watchWhere);
            Report("[watch]   new value 0x%016llx at %lld ms\n", current, static_cast<long long>(std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now().time_since_epoch()).count()));
            t_watchHit = false;
        }
        return true;
    }
    if (record->ExceptionCode != EXCEPTION_ACCESS_VIOLATION || record->NumberParameters < 2 || record->ExceptionInformation[0] != 1) return false;
    const auto address = static_cast<std::uintptr_t>(record->ExceptionInformation[1]);
    if (address < g_watchPage || address >= g_watchPage + 0x1000) return false;
    // Block writes (memset, memcpy) fault on their first byte, so compare the value after the step
    // as well as matching the fault address.
    t_watchBefore = *reinterpret_cast<const unsigned long long*>(g_watchAddress);
    t_watchFault = address;
    DescribeAddress(context->Rip, t_watchWhere, sizeof(t_watchWhere));
    if (address + 32 > g_watchAddress && address < g_watchAddress + 8) {
        char line[MAX_PATH + 64];
        DescribeAddress(context->Rip, line, sizeof(line));
        Report("[watch] thread %lu writes 0x%llx at %s rcx=%llx rdx=%llx r8=%llx\n", GetCurrentThreadId(), static_cast<unsigned long long>(address), line, context->Rcx, context->Rdx, context->R8);
        for (std::uint64_t slot = context->Rsp; slot < context->Rsp + 0x80 && IsReadable(slot); slot += 8) {
            const auto value = *reinterpret_cast<const std::uint64_t*>(slot);
            if (!IsExecutable(value)) continue;
            DescribeAddress(value, line, sizeof(line));
            Report("[watch]     [rsp+0x%llx] %s\n", static_cast<unsigned long long>(slot - context->Rsp), line);
        }
        std::uint64_t frame = context->Rbp;
        for (int depth = 0; depth < 6 && frame != 0 && (frame & 7) == 0 && IsReadable(frame) && IsReadable(frame + 8); ++depth) {
            const auto returnAddress = reinterpret_cast<const std::uint64_t*>(frame)[1];
            if (!IsExecutable(returnAddress)) break;
            DescribeAddress(returnAddress, line, sizeof(line));
            Report("[watch]     #%d %s\n", depth, line);
            const auto next = reinterpret_cast<const std::uint64_t*>(frame)[0];
            if (next <= frame) break;
            frame = next;
        }
        t_watchHit = true;
    }
    ProtectWatchPage(PAGE_READWRITE);
    context->EFlags |= 0x100;
    t_watchStepping = true;
    return true;
}

void InstallWatch() {
    // APS5_WATCH_ADDR=<absolute hex address> watches guest data that is mapped later: the page is
    // protected once it has been committed.
    if (const char* absolute = std::getenv("APS5_WATCH_ADDR")) {
        g_watchAddress = std::strtoull(absolute, nullptr, 16);
        const auto page = g_watchAddress & ~static_cast<std::uintptr_t>(0xfff);
        CreateThread(nullptr, 0, [](void* parameter) -> DWORD {
            const auto page = reinterpret_cast<std::uintptr_t>(parameter);
            for (;;) {
                MEMORY_BASIC_INFORMATION memory{};
                if (VirtualQuery(reinterpret_cast<const void*>(page), &memory, sizeof(memory)) == sizeof(memory) && memory.State == MEM_COMMIT) break;
                Sleep(1);
            }
            g_watchPage = page;
            ProtectWatchPage(PAGE_READONLY);
            Report("[watch] watching 0x%llx (page 0x%llx)\n", static_cast<unsigned long long>(g_watchAddress), static_cast<unsigned long long>(page));
            return 0;
        }, reinterpret_cast<void*>(page), 0, nullptr);
        return;
    }
    const char* value = std::getenv("APS5_WATCH_WRITE");
    if (!value) return;
    const auto elfAddress = std::strtoull(value, nullptr, 0);
    g_watchAddress = reinterpret_cast<std::uintptr_t>(GetModuleHandleW(nullptr)) + 0x10000 + elfAddress;
    g_watchPage = g_watchAddress & ~static_cast<std::uintptr_t>(0xfff);
    ProtectWatchPage(PAGE_READONLY);
    Report("[watch] watching 0x%llx (page 0x%llx)\n", static_cast<unsigned long long>(g_watchAddress), static_cast<unsigned long long>(g_watchPage));
}

// SSE4a (EXTRQ / INSERTQ) is AMD-only: the PS5's Zen 2 has it and titles use it, so on an Intel host the
// instruction raises STATUS_ILLEGAL_INSTRUCTION and is emulated on the faulting thread's CONTEXT.
// APS5_NO_SSE4A_EMULATION=1 disables this (the fault is then reported as fatal as before) and
// APS5_TRACE_SSE4A=1 prints each emulated instruction once per rip.
bool g_sse4aEmulation = true;
bool g_sse4aTrace = false;
std::atomic<unsigned long long> g_sse4aEmulated{0};

bool IsEnvironmentSet(const char* name) {
    const char* value = std::getenv(name);
    return value != nullptr && *value != '\0' && !(value[0] == '0' && value[1] == '\0');
}

// Copies the bytes at `address` that are mapped; the read stays within readable pages.
std::size_t ReadCode(std::uint64_t address, std::uint8_t* buffer, std::size_t size) {
    std::size_t count = 0;
    while (count < size) {
        const auto cursor = address + count;
        if ((cursor & 0xfff) == 0 || count == 0) {
            if (!IsReadable(cursor)) break;
        }
        buffer[count++] = *reinterpret_cast<const std::uint8_t*>(cursor);
    }
    return count;
}

void TraceSse4a(std::uint64_t rip, const sse4a::Instruction& instruction, const sse4a::Field& field, unsigned long long count) {
    static std::mutex mutex;
    static std::uint64_t seen[128];
    static std::size_t seenCount = 0;
    {
        std::lock_guard lock(mutex);
        for (std::size_t i = 0; i < seenCount; ++i) {
            if (seen[i] == rip) return;
        }
        if (seenCount < sizeof(seen) / sizeof(seen[0])) seen[seenCount++] = rip;
    }
    char line[MAX_PATH + 64];
    DescribeAddress(rip, line, sizeof(line));
    const char* mnemonic = instruction.op == sse4a::Op::Extrq ? "extrq" : "insertq";
    char operands[64];
    if (instruction.op == sse4a::Op::Extrq && !instruction.registerForm) {
        std::snprintf(operands, sizeof(operands), "xmm%u, %u, %u", instruction.destination, instruction.length, instruction.index);
    } else if (!instruction.registerForm) {
        std::snprintf(operands, sizeof(operands), "xmm%u, xmm%u, %u, %u", instruction.destination, instruction.source, instruction.length, instruction.index);
    } else {
        std::snprintf(operands, sizeof(operands), "xmm%u, xmm%u", instruction.destination, instruction.source);
    }
    Report("[sse4a] #%llu %s %s (length %u, index %u, %u bytes) at %s on thread %lu\n", count, mnemonic, operands, field.length, field.index, static_cast<unsigned>(instruction.size), line, GetCurrentThreadId());
}

constexpr DWORD GccExceptionMask = 0x20ffffffu;
constexpr DWORD GccException = 0x20474343u;
constexpr std::uint64_t GccExceptionClass = 0x474e5543432b2b00ull;
constexpr std::uint64_t CxaExceptionTypeOffset = 80;

PWSTR ThreadDescription(HANDLE thread) {
    using GetDescription = HRESULT (WINAPI *)(HANDLE, PWSTR*);
    static const auto getDescription = reinterpret_cast<GetDescription>(GetProcAddress(GetModuleHandleW(L"kernel32.dll"), "GetThreadDescription"));
    PWSTR description = nullptr;
    if (!getDescription || FAILED(getDescription(thread, &description))) return nullptr;
    return description;
}

std::uint64_t FrameWithoutUnwindData(const CONTEXT& start) {
    const HMODULE runtime[] = {GetModuleHandleA("KERNELBASE.dll"), GetModuleHandleA("libgcc_s_seh-1.dll"), GetModuleHandleA("libstdc++-6.dll")};
    CONTEXT context = start;
    for (int depth = 0; depth < 64 && context.Rip != 0; ++depth) {
        HMODULE module = nullptr;
        GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT, reinterpret_cast<LPCSTR>(context.Rip), &module);
        DWORD64 imageBase = 0;
        const auto entry = RtlLookupFunctionEntry(context.Rip, &imageBase, nullptr);
        if (module == nullptr || std::find(std::begin(runtime), std::end(runtime), module) == std::end(runtime)) return entry == nullptr ? context.Rip : 0;
        if (entry == nullptr) return 0;
        PVOID handlerData = nullptr;
        DWORD64 establisher = 0;
        RtlVirtualUnwind(UNW_FLAG_NHANDLER, imageBase, context.Rip, entry, &context, &handlerData, &establisher, nullptr);
    }
    return 0;
}

bool ReadWord(std::uint64_t address, std::uint64_t* value) {
    return ReadCode(address, reinterpret_cast<std::uint8_t*>(value), sizeof(*value)) == sizeof(*value);
}

std::uint64_t EnvironmentNumber(const char* name) {
    char text[32];
    const DWORD length = GetEnvironmentVariableA(name, text, sizeof(text));
    if (length == 0 || length >= sizeof(text)) return 0;
    return std::strtoull(text, nullptr, 0);
}

void ReportStackWords(std::uint64_t rsp) {
    const auto bytes = std::min<std::uint64_t>(EnvironmentNumber("APS5_CRASH_STACK_BYTES"), std::uint64_t{1} << 20);
    if (bytes == 0) return;
    const auto* tib = reinterpret_cast<const NT_TIB*>(NtCurrentTeb());
    const auto stackLow = reinterpret_cast<std::uint64_t>(tib->StackLimit);
    const auto stackHigh = reinterpret_cast<std::uint64_t>(tib->StackBase);
    const auto first = rsp & ~std::uint64_t{7};
    Report("  stack words from rsp 0x%016llx (APS5_CRASH_STACK_BYTES):\n", static_cast<unsigned long long>(rsp));
    const auto limit = std::min(first + bytes, stackHigh);
    std::uint64_t end = first;
    std::uint64_t readablePage = 0;
    for (std::uint64_t at = first; at < limit; at += 32) {
        std::uint64_t words[4] = {};
        int count = 0;
        for (; count < 4; ++count) {
            const auto slot = at + static_cast<std::uint64_t>(count) * 8;
            const auto page = slot & ~std::uint64_t{0xfff};
            if (page != readablePage) {
                if (!IsReadable(slot)) break;
                readablePage = page;
            }
            words[count] = *reinterpret_cast<const std::uint64_t*>(slot);
        }
        if (count == 0) break;
        static char text[192];
        int used = std::snprintf(text, sizeof(text), "    +0x%04llx:", static_cast<unsigned long long>(at - first));
        for (int k = 0; k < count && used > 0 && used < static_cast<int>(sizeof(text)); ++k) used += std::snprintf(text + used, sizeof(text) - static_cast<std::size_t>(used), " %016llx", static_cast<unsigned long long>(words[k]));
        Report("%s\n", text);
        end = at + static_cast<std::uint64_t>(count) * 8;
        if (count < 4) break;
    }
    static std::uint64_t seen[256];
    std::size_t seenCount = 0;
    Report("  data at stack words that point outside the stack (8 words each):\n");
    for (std::uint64_t slot = first; slot + 8 <= end && seenCount < std::size(seen); slot += 8) {
        const auto value = *reinterpret_cast<const std::uint64_t*>(slot);
        if (value < 0x10000 || (value & 7) != 0 || value > 0x7fffffffffffull || (value >= stackLow && value < stackHigh)) continue;
        if (std::find(seen, seen + seenCount, value) != seen + seenCount) continue;
        if (IsExecutable(value) || !IsReadable(value) || !IsReadable(value + 0x38)) continue;
        seen[seenCount++] = value;
        const auto* words = reinterpret_cast<const std::uint64_t*>(value);
        Report("    [+0x%04llx] 0x%016llx: %016llx %016llx %016llx %016llx %016llx %016llx %016llx %016llx\n", static_cast<unsigned long long>(slot - first), static_cast<unsigned long long>(value), words[0], words[1], words[2], words[3], words[4], words[5], words[6], words[7]);
    }
}

void ReportUndeliverableException(const EXCEPTION_POINTERS* info, std::uint64_t frame) {
    const auto* record = info->ExceptionRecord;
    char type[256] = "of unknown type";
    const auto header = record->NumberParameters >= 1 ? static_cast<std::uint64_t>(record->ExceptionInformation[0]) : 0;
    std::uint64_t exceptionClass = 0;
    std::uint64_t typeInfo = 0;
    std::uint64_t namePointer = 0;
    if (ReadWord(header, &exceptionClass) && exceptionClass == GccExceptionClass && ReadWord(header - CxaExceptionTypeOffset, &typeInfo) && ReadWord(typeInfo + 8, &namePointer)) {
        std::uint8_t name[sizeof(type)];
        const auto copied = ReadCode(namePointer, name, sizeof(name));
        if (const auto* end = static_cast<const std::uint8_t*>(std::memchr(name, 0, copied))) std::memcpy(type, name, static_cast<std::size_t>(end - name) + 1);
    }
    char line[MAX_PATH + 64];
    DescribeAddress(frame, line, sizeof(line));
    Report("\nFATAL: C++ exception %s raised on thread %lu cannot be delivered: %s has no unwind data\n", type, GetCurrentThreadId(), line);
    ReportStackCandidates(info->ContextRecord->Rsp, 32, 0x4000);
}

std::atomic<LibcHostFaultHandler> g_hostFaultHandler{nullptr};

bool HandleHostFault(EXCEPTION_POINTERS* info) {
    const auto handler = g_hostFaultHandler.load(std::memory_order_acquire);
    return handler != nullptr && handler(info);
}

bool HandleSse4a(EXCEPTION_POINTERS* info) {
    if (!g_sse4aEmulation) return false;
    const auto* record = info->ExceptionRecord;
    if (record->ExceptionCode != EXCEPTION_ILLEGAL_INSTRUCTION) return false;
    auto* context = info->ContextRecord;
    const auto rip = context->Rip;
    std::uint8_t bytes[sse4a::kMaxInstructionSize];
    const std::size_t available = ReadCode(rip, bytes, sizeof(bytes));
    sse4a::Instruction instruction;
    sse4a::Field field;
    if (!sse4a::Emulate(bytes, available, *context, &instruction, &field)) return false;
    const auto count = ++g_sse4aEmulated;
    if (g_sse4aTrace) TraceSse4a(rip, instruction, field, count);
    return true;
}

LONG WINAPI ReportCrash(EXCEPTION_POINTERS* info) {
    static std::atomic<bool> reported{false};
    const auto* fault = info->ExceptionRecord;
    if (fault->ExceptionCode == EXCEPTION_ACCESS_VIOLATION && fault->NumberParameters >= 2 && fault->ExceptionInformation[0] == 1 && GuestArena::GuestArenaHandleWrite_nid_postfix(fault->ExceptionInformation[1])) return EXCEPTION_CONTINUE_EXECUTION;
    if (fault->ExceptionCode == EXCEPTION_ACCESS_VIOLATION && fault->NumberParameters >= 2 && fault->ExceptionInformation[0] == 0 && GuestArena::GuestArenaHandleRead_nid_postfix(fault->ExceptionInformation[1])) return EXCEPTION_CONTINUE_EXECUTION;
    if (HandleWatch(info)) return EXCEPTION_CONTINUE_EXECUTION;
    if (HandleSse4a(info)) return EXCEPTION_CONTINUE_EXECUTION;
    if (HandleHostFault(info)) return EXCEPTION_CONTINUE_EXECUTION;
    const auto* record = info->ExceptionRecord;
    if ((record->ExceptionCode & GccExceptionMask) == GccException) {
        if (const auto frame = FrameWithoutUnwindData(*info->ContextRecord)) ReportUndeliverableException(info, frame);
        return EXCEPTION_CONTINUE_SEARCH;
    }
    if (!IsFatal(record->ExceptionCode)) return EXCEPTION_CONTINUE_SEARCH;
    if (reported.exchange(true)) {
        // Another thread is already reporting; let it finish before this fault ends the process.
        Sleep(INFINITE);
    }
    LibcRunEndFlush_nid_no_patch();
    const auto* context = info->ContextRecord;
    char line[MAX_PATH + 64];
    char threadName[128] = "";
    if (PWSTR description = ThreadDescription(GetCurrentThread())) {
        WideCharToMultiByte(CP_UTF8, 0, description, -1, threadName, sizeof(threadName), nullptr, nullptr);
        LocalFree(description);
    }
    Report("\nFATAL: unhandled exception 0x%08lx on thread %lu '%s'\n", record->ExceptionCode, GetCurrentThreadId(), threadName);
    DescribeAddress(context->Rip, line, sizeof(line));
    Report("  rip %s\n", line);
    if (record->ExceptionCode == EXCEPTION_ILLEGAL_INSTRUCTION) {
        std::uint8_t bytes[8];
        const std::size_t available = ReadCode(context->Rip, bytes, sizeof(bytes));
        char hex[3 * sizeof(bytes) + 1] = "";
        for (std::size_t i = 0; i < available; ++i) std::snprintf(hex + 3 * i, sizeof(hex) - 3 * i, "%02x ", bytes[i]);
        Report("  bytes %s(sse4a emulation %s, %llu emulated so far)\n", hex, g_sse4aEmulation ? "on" : "off", g_sse4aEmulated.load());
    }
    if (record->ExceptionCode == EXCEPTION_ACCESS_VIOLATION && record->NumberParameters >= 2) {
        const char* kind = record->ExceptionInformation[0] == 0 ? "read" : record->ExceptionInformation[0] == 1 ? "write" : "execute";
        Report("  %s of 0x%016llx\n", kind, static_cast<unsigned long long>(record->ExceptionInformation[1]));
        std::uint8_t trap[2];
        if (record->ExceptionInformation[0] == 0 && record->ExceptionInformation[1] == ~0ull && ReadCode(context->Rip, trap, sizeof(trap)) == sizeof(trap) && trap[0] == 0xcd) {
            if (trap[1] == 0x45) Report("  guest abort() (int 0x45)\n");
            else if (trap[1] == 0x41) Report("  guest assert/break (int 0x41)\n");
        }
    }
    Report("  rax %016llx rbx %016llx rcx %016llx rdx %016llx\n", context->Rax, context->Rbx, context->Rcx, context->Rdx);
    Report("  rsi %016llx rdi %016llx rbp %016llx rsp %016llx\n", context->Rsi, context->Rdi, context->Rbp, context->Rsp);
    Report("  r8  %016llx r9  %016llx r10 %016llx r11 %016llx\n", context->R8, context->R9, context->R10, context->R11);
    Report("  r12 %016llx r13 %016llx r14 %016llx r15 %016llx\n", context->R12, context->R13, context->R14, context->R15);
    // The words each register points at, to recognise which structure a bad pointer came from.
    {
        const DWORD indirectLength = GetEnvironmentVariableA("APS5_CRASH_INDIRECT_MEMORY", nullptr, 0);
        const bool indirectMemory = indirectLength > 1;
        const std::pair<const char*, std::uint64_t> pointers[] = {{"rax", context->Rax}, {"rbx", context->Rbx}, {"rcx", context->Rcx}, {"rdx", context->Rdx}, {"rsi", context->Rsi}, {"rdi", context->Rdi}, {"r12", context->R12}, {"r13", context->R13}, {"r14", context->R14}, {"r15", context->R15}};
        for (const auto& [name, value] : pointers) {
            if (value < 0x10000 || (value & 7) != 0 || !IsReadable(value) || !IsReadable(value + 0x38)) continue;
            const auto* words = reinterpret_cast<const std::uint64_t*>(value);
            Report("  [%s] %016llx %016llx %016llx %016llx %016llx %016llx %016llx %016llx\n", name, words[0], words[1], words[2], words[3], words[4], words[5], words[6], words[7]);
            if (!indirectMemory) continue;
            for (std::size_t i = 0; i < 8; ++i) {
                const auto address = words[i];
                if (address < 0x10000 || IsExecutable(address)) continue;
                std::uint64_t content[8]{};
                SIZE_T copied = 0;
                if (!ReadProcessMemory(GetCurrentProcess(), reinterpret_cast<const void*>(address), content, sizeof(content), &copied) || copied != sizeof(content)) continue;
                Report("  [[%s+0x%llx]] @%016llx %016llx %016llx %016llx %016llx %016llx %016llx %016llx %016llx\n",
                    name, static_cast<unsigned long long>(i * sizeof(*words)), static_cast<unsigned long long>(address),
                    content[0], content[1], content[2], content[3], content[4], content[5], content[6], content[7]);
            }
        }
    }
    {
        const std::pair<const char*, std::uint64_t> registers[] = {{"rax", context->Rax}, {"rbx", context->Rbx}, {"rcx", context->Rcx}, {"rdx", context->Rdx}, {"rsi", context->Rsi}, {"rdi", context->Rdi}, {"rbp", context->Rbp}, {"r8", context->R8}, {"r9", context->R9}, {"r10", context->R10}, {"r11", context->R11}, {"r12", context->R12}, {"r13", context->R13}, {"r14", context->R14}, {"r15", context->R15}};
        static std::uint8_t bytes[crash_text::MaximumBytes];
        static char text[960];
        for (const auto& [name, value] : registers) {
            if (value < 0x10000) continue;
            if (crash_text::Format(bytes, ReadCode(value, bytes, sizeof(bytes)), text, sizeof(text))) Report("  [%s] %s\n", name, text);
        }
    }
    Report("  frame pointer chain:\n");
    std::uint64_t frame = context->Rbp;
    for (int depth = 0; depth < 32 && frame != 0 && (frame & 7) == 0 && IsReadable(frame) && IsReadable(frame + 8); ++depth) {
        const auto returnAddress = reinterpret_cast<const std::uint64_t*>(frame)[1];
        if (!IsExecutable(returnAddress)) break;
        DescribeAddress(returnAddress, line, sizeof(line));
        Report("    #%d %s\n", depth, line);
        const auto next = reinterpret_cast<const std::uint64_t*>(frame)[0];
        if (next <= frame) break;
        frame = next;
    }
    ReportStackCandidates(context->Rsp, 16, 0x2000);
    ReportStackWords(context->Rsp);
    if (context->Rip == 0 ||!IsExecutable(context->Rip)) {
        // A jump into nothing usually follows a return through a clobbered frame; the frames that just
        // returned are still below rsp.
        Report("  recently popped return address candidates:\n");
        for (std::uint64_t slot = context->Rsp - 8; slot >= context->Rsp - 0x800; slot -= 8) {
            if (!IsReadable(slot)) break;
            const auto value = *reinterpret_cast<const std::uint64_t*>(slot);
            if (!IsExecutable(value)) continue;
            DescribeAddress(value, line, sizeof(line));
            Report("    [rsp-0x%llx] %s\n", static_cast<unsigned long long>(context->Rsp - slot), line);
        }
        // Registers often still point into the stack the code ran on before the bad return.
        const std::pair<const char*, std::uint64_t> registers[] = {{"rdi", context->Rdi}, {"rsi", context->Rsi}, {"rbx", context->Rbx}, {"r12", context->R12}, {"r13", context->R13}, {"r14", context->R14}, {"r15", context->R15}};
        for (const auto& [name, value] : registers) {
            if (value < 0x10000 || (value & 7) != 0 || !IsReadable(value)) continue;
            Report("  code addresses above %s (0x%llx):\n", name, static_cast<unsigned long long>(value));
            int found = 0;
            for (std::uint64_t slot = value; found < 24 && slot < value + 0x1000; slot += 8) {
                if (!IsReadable(slot)) break;
                const auto candidate = *reinterpret_cast<const std::uint64_t*>(slot);
                if (!IsExecutable(candidate)) continue;
                DescribeAddress(candidate, line, sizeof(line));
                Report("    [+0x%llx] %s\n", static_cast<unsigned long long>(slot - value), line);
                ++found;
            }
        }
    }
    ReportCallTrace();
    std::fflush(stderr);
    ReportAllThreads();
    LibcRunFailureReports_nid_no_patch();
    std::fflush(stderr);
    return EXCEPTION_CONTINUE_SEARCH;
}

void ReportCallTrace() {
    static CallTraceEntry entries[64];
    const std::size_t count = CopyCallTrace_nid_no_patch(entries, std::size(entries));
    if (count == 0) return;
    char line[MAX_PATH + 64];
    Report("  last traced system library calls (APS5_TRACE_CALLS):\n");
    for (std::size_t index = 0; index < count; ++index) {
        DescribeAddress(entries[index].returnAddress, line, sizeof(line));
        Report("    #%llu thread %llu %s %s from %s\n", static_cast<unsigned long long>(entries[index].sequence),
            static_cast<unsigned long long>(entries[index].thread), entries[index].library, entries[index].name, line);
    }
    static CallTraceEntry own[32];
    const std::uint64_t thread = GetCurrentThreadId();
    const std::size_t owned = CopyThreadCallTrace_nid_no_patch(thread, own, std::size(own));
    if (owned != 0) {
        Report("  last traced calls of thread %llu:\n", static_cast<unsigned long long>(thread));
        for (std::size_t index = 0; index < owned; ++index) {
            DescribeAddress(own[index].returnAddress, line, sizeof(line));
            Report("    #%llu %s %s from %s\n", static_cast<unsigned long long>(own[index].sequence), own[index].library, own[index].name, line);
        }
    }
    static CallTraceCount counts[32];
    std::uint64_t total = 0;
    const std::size_t counted = CopyCallCounts_nid_no_patch(counts, std::size(counts), &total);
    if (counted == 0) return;
    Report("  most called traced imports (%llu calls in total):\n", static_cast<unsigned long long>(total));
    for (std::size_t index = 0; index < counted; ++index) {
        Report("    %12llu %s %s\n", static_cast<unsigned long long>(counts[index].calls), counts[index].library, counts[index].name);
    }
}

std::size_t CaptureFrames(const CONTEXT* context, std::uint64_t* frames, std::size_t capacity) {
    std::size_t count = 0;
    std::uint64_t frame = context->Rbp;
    while (count < capacity && frame != 0 && (frame & 7) == 0 && IsReadable(frame) && IsReadable(frame + 8)) {
        const auto returnAddress = reinterpret_cast<const std::uint64_t*>(frame)[1];
        if (!IsExecutable(returnAddress)) break;
        frames[count++] = returnAddress;
        const auto next = reinterpret_cast<const std::uint64_t*>(frame)[0];
        if (next <= frame) break;
        frame = next;
    }
    return count;
}

std::size_t CaptureStackCandidates(std::uint64_t rsp, std::uint64_t* candidates, std::size_t capacity) {
    std::size_t count = 0;
    for (std::uint64_t slot = rsp; count < capacity && slot < rsp + 0x8000; slot += 8) {
        if (!IsReadable(slot)) break;
        const auto value = *reinterpret_cast<const std::uint64_t*>(slot);
        if (IsExecutable(value)) candidates[count++] = value;
    }
    return count;
}

void ReportThreadContext(const CONTEXT* context, DWORD threadId, const char* name, const std::uint64_t* frames, std::size_t frameCount, const std::uint64_t* candidates, std::size_t candidateCount) {
    char line[MAX_PATH + 64];
    Report("  thread %lu '%s': rip ", static_cast<unsigned long>(threadId), name ? name : "");
    DescribeAddress(context->Rip, line, sizeof(line));
    Report("%s rsp 0x%016llx rbp 0x%016llx\n", line, static_cast<unsigned long long>(context->Rsp), static_cast<unsigned long long>(context->Rbp));
    for (std::size_t depth = 0; depth < frameCount; ++depth) {
        DescribeAddress(frames[depth], line, sizeof(line));
        Report("    #%llu %s\n", static_cast<unsigned long long>(depth), line);
    }
    if (candidateCount != 0) Report("    stack return address candidates:\n");
    for (std::size_t index = 0; index < candidateCount; ++index) {
        DescribeAddress(candidates[index], line, sizeof(line));
        Report("      %s\n", line);
    }
}

void ReportAllThreads() {
    const DWORD current = GetCurrentThreadId();
    const DWORD pid = GetCurrentProcessId();
    HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPTHREAD, 0);
    if (snapshot == INVALID_HANDLE_VALUE) return;
    THREADENTRY32 entry{};
    entry.dwSize = sizeof(entry);
    struct CapturedThread {
        DWORD id;
        char name[128];
        CONTEXT context;
        std::uint64_t frames[12];
        std::size_t frameCount;
        std::uint64_t candidates[24];
        std::size_t candidateCount;
    };
    static CapturedThread captured[128];
    std::size_t capturedCount = 0;
    std::size_t skippedCount = 0;
    if (Thread32First(snapshot, &entry)) {
        do {
            if (entry.th32OwnerProcessID != pid || entry.th32ThreadID == current) continue;
            if (capturedCount >= sizeof(captured) / sizeof(captured[0])) {
                ++skippedCount;
                continue;
            }
            HANDLE thread = OpenThread(THREAD_SUSPEND_RESUME | THREAD_GET_CONTEXT | THREAD_QUERY_INFORMATION, FALSE, entry.th32ThreadID);
            if (!thread) continue;
            if (SuspendThread(thread) == static_cast<DWORD>(-1)) {
                CloseHandle(thread);
                continue;
            }
            CONTEXT context{};
            context.ContextFlags = CONTEXT_FULL;
            const BOOL haveContext = GetThreadContext(thread, &context);
            std::uint64_t frames[12] = {};
            const std::size_t frameCount = haveContext ? CaptureFrames(&context, frames, sizeof(frames) / sizeof(frames[0])) : 0;
            std::uint64_t candidates[24] = {};
            const std::size_t candidateCount = haveContext && frameCount < 2 ? CaptureStackCandidates(context.Rsp, candidates, sizeof(candidates) / sizeof(candidates[0])) : 0;
            ResumeThread(thread);
            char name[128] = "";
            if (PWSTR description = ThreadDescription(thread)) {
                WideCharToMultiByte(CP_UTF8, 0, description, -1, name, sizeof(name), nullptr, nullptr);
                LocalFree(description);
            }
            CloseHandle(thread);
            if (!haveContext) continue;
            captured[capturedCount].id = entry.th32ThreadID;
            std::memcpy(captured[capturedCount].name, name, sizeof(captured[capturedCount].name));
            captured[capturedCount].context = context;
            std::memcpy(captured[capturedCount].frames, frames, sizeof(captured[capturedCount].frames));
            captured[capturedCount].frameCount = frameCount;
            std::memcpy(captured[capturedCount].candidates, candidates, sizeof(captured[capturedCount].candidates));
            captured[capturedCount].candidateCount = candidateCount;
            ++capturedCount;
        } while (Thread32Next(snapshot, &entry));
    }
    CloseHandle(snapshot);
    Report("  all threads:\n");
    for (std::size_t i = 0; i < capturedCount; ++i) {
        ReportThreadContext(&captured[i].context, captured[i].id, captured[i].name, captured[i].frames, captured[i].frameCount, captured[i].candidates, captured[i].candidateCount);
    }
    if (skippedCount != 0) Report("  %llu further thread(s) left out\n", static_cast<unsigned long long>(skippedCount));
}

DWORD WINAPI HangWatchdog(LPVOID param) {
    const auto seconds = static_cast<unsigned long long>(reinterpret_cast<std::uintptr_t>(param));
    Sleep(static_cast<DWORD>(seconds * 1000ull));
    Report("\nFATAL: timed dump after %llu s (APS5_HANG_DUMP_SECS); aborting\n", seconds);
    std::fflush(stderr);
    std::abort();
    return 0;
}

void ReportStackCandidates(std::uint64_t rsp, int limit, std::uint64_t span) {
    char line[256];
    Report("  stack return address candidates:\n");
    int printed = 0;
    for (std::uint64_t slot = rsp; printed < limit && slot < rsp + span; slot += 8) {
        if (!IsReadable(slot)) break;
        const auto value = *reinterpret_cast<const std::uint64_t*>(slot);
        if (!IsExecutable(value)) continue;
        DescribeAddress(value, line, sizeof(line));
        Report("    [rsp+0x%llx] %s\n", static_cast<unsigned long long>(slot - rsp), line);
        ++printed;
    }
}

// abort() and the UCRT's invalid-parameter path end the process with a silent fast fail
// (0xc0000409); both are reported with the calling thread's stack first.
void ReportBacktrace(const char* what, bool reportThreads) {
    void* frames[48];
    const auto count = RtlCaptureStackBackTrace(0, 48, frames, nullptr);
    Report("FATAL: %s on thread %lu\n", what, static_cast<unsigned long>(GetCurrentThreadId()));
    char line[256];
    for (USHORT i = 0; i < count; ++i) {
        DescribeAddress(reinterpret_cast<std::uint64_t>(frames[i]), line, sizeof(line));
        Report("    #%u %s\n", static_cast<unsigned>(i), line);
    }
    ReportStackCandidates(reinterpret_cast<std::uint64_t>(__builtin_frame_address(0)), 32, 0x4000);
    ReportCallTrace();
    if (reportThreads) ReportAllThreads();
    std::fflush(stderr);
}

void AbortSignalHandler(int) {
    ReportBacktrace("abort() (SIGABRT)", true);
    LibcRunFailureReports_nid_no_patch();
}

void InvalidParameterHandler(const wchar_t*, const wchar_t*, const wchar_t*, unsigned, std::uintptr_t) {
    ReportBacktrace("UCRT invalid parameter", true);
    LibcRunFailureReports_nid_no_patch();
}

// Static destruction runs on the thread that ends the process (ExitProcess -> DLL_PROCESS_DETACH),
// so its stack names the exit's caller when no libc exit/abort path was taken.
struct ExitReporter {
    ~ExitReporter() {
        if (std::getenv("APS5_TRACE_EXIT") != nullptr) ReportBacktrace("process exit (static destruction)", false);
    }
} g_exitReporter;

const bool g_crashReportInstalled = [] {
    g_sse4aEmulation = !IsEnvironmentSet("APS5_NO_SSE4A_EMULATION");
    g_sse4aTrace = IsEnvironmentSet("APS5_TRACE_SSE4A");
    AddVectoredExceptionHandler(1, ReportCrash);
    std::signal(SIGABRT, AbortSignalHandler);
    _set_invalid_parameter_handler(InvalidParameterHandler);
    InstallWatch();
    if (const char* hang = std::getenv("APS5_HANG_DUMP_SECS")) {
        unsigned long long seconds = 0;
        if (hang[0] >= '0' && hang[0] <= '9') {
            char* end = nullptr;
            const auto value = std::strtoull(hang, &end, 10);
            if (end && *end == 0) seconds = value;
        }
        if (seconds > 0) CreateThread(nullptr, 0, HangWatchdog, reinterpret_cast<LPVOID>(static_cast<std::uintptr_t>(seconds)), 0, nullptr);
    }
    return true;
}();

}

extern "C" void LibcSetHostFaultHandler_nid_no_patch(LibcHostFaultHandler handler) noexcept {
    g_hostFaultHandler.store(handler, std::memory_order_release);
}
#endif
