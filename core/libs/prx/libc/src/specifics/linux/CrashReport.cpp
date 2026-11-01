#if defined(__linux__) && defined(__x86_64__)
#include "prx/libc/include/CallTrace.hpp"
#include "prx/libc/include/LogFile.hpp"

#include <atomic>
#include <cerrno>
#include <csignal>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <fcntl.h>
#include <sys/syscall.h>
#include <sys/uio.h>
#include <ucontext.h>
#include <unistd.h>

namespace {

constexpr int FatalSignals[] = {SIGSEGV, SIGBUS, SIGILL, SIGFPE};
constexpr std::size_t MappingCapacity = 4096;
constexpr std::size_t NameBytes = 64;
constexpr int FrameLimit = 32;
constexpr int CandidateLimit = 24;
constexpr std::uint64_t CandidateSpan = 32 * 1024;

struct Mapping {
    std::uint64_t start;
    std::uint64_t end;
    std::uint64_t offset;
    bool executable;
    char name[NameBytes];
};

struct sigaction g_previous[std::size(FatalSignals)]{};
Mapping g_mappings[MappingCapacity];
std::size_t g_mappingCount = 0;
std::atomic<bool> g_reporting{false};
std::atomic<bool> g_reported{false};
alignas(16) unsigned char g_alternateStack[64 * 1024];

class ReportLine {
public:
    ReportLine& Text(const char* text) {
        while (*text != '\0' && _length < sizeof(_buffer)) _buffer[_length++] = *text++;
        return *this;
    }

    ReportLine& Decimal(std::uint64_t value) {
        char digits[20];
        std::size_t count = 0;
        do {
            digits[count++] = static_cast<char>('0' + value % 10);
            value /= 10;
        } while (value != 0);
        while (count > 0 && _length < sizeof(_buffer)) _buffer[_length++] = digits[--count];
        return *this;
    }

    ReportLine& Hex(std::uint64_t value, std::size_t digits = 1) {
        while (digits < 16 && (value >> (digits * 4)) != 0) ++digits;
        Text("0x");
        for (std::size_t index = digits; index > 0 && _length < sizeof(_buffer); --index) _buffer[_length++] = "0123456789abcdef"[(value >> ((index - 1) * 4)) & 0xf];
        return *this;
    }

    void Write() {
        Text("\n");
        log_file::CrashWrite(STDERR_FILENO, _buffer, _length);
        _length = 0;
    }

private:
    char _buffer[512];
    std::size_t _length = 0;
};

bool ParseHex(const char*& cursor, const char* end, std::uint64_t& value) {
    value = 0;
    const char* first = cursor;
    while (cursor < end) {
        const char c = *cursor;
        const int digit = c >= '0' && c <= '9' ? c - '0' : c >= 'a' && c <= 'f' ? c - 'a' + 10 : -1;
        if (digit < 0) break;
        value = value * 16 + static_cast<std::uint64_t>(digit);
        ++cursor;
    }
    return cursor != first;
}

void SkipField(const char*& cursor, const char* end) {
    while (cursor < end && *cursor != ' ') ++cursor;
    while (cursor < end && *cursor == ' ') ++cursor;
}

void ParseMapsLine(const char* line, const char* end) {
    if (g_mappingCount == MappingCapacity) return;
    Mapping mapping{};
    const char* cursor = line;
    if (!ParseHex(cursor, end, mapping.start) || cursor == end || *cursor++ != '-' || !ParseHex(cursor, end, mapping.end)) return;
    while (cursor < end && *cursor == ' ') ++cursor;
    if (end - cursor < 4) return;
    mapping.executable = cursor[2] == 'x';
    SkipField(cursor, end);
    if (!ParseHex(cursor, end, mapping.offset)) return;
    SkipField(cursor, end);
    SkipField(cursor, end);
    SkipField(cursor, end);
    const char* name = cursor;
    for (const char* scan = cursor; scan < end; ++scan)
        if (*scan == '/') name = scan + 1;
    std::size_t length = 0;
    while (name + length < end && length + 1 < NameBytes) {
        mapping.name[length] = name[length];
        ++length;
    }
    g_mappings[g_mappingCount++] = mapping;
}

void ReadMappings() {
    g_mappingCount = 0;
    const int file = open("/proc/self/maps", O_RDONLY | O_CLOEXEC);
    if (file < 0) return;
    static char buffer[8192];
    std::size_t kept = 0;
    for (;;) {
        const ssize_t got = read(file, buffer + kept, sizeof(buffer) - kept);
        if (got <= 0) break;
        const char* end = buffer + kept + got;
        const char* line = buffer;
        for (const char* scan = buffer; scan < end; ++scan) {
            if (*scan != '\n') continue;
            ParseMapsLine(line, scan);
            line = scan + 1;
        }
        kept = static_cast<std::size_t>(end - line);
        if (kept == sizeof(buffer)) kept = 0;
        std::memmove(buffer, line, kept);
    }
    close(file);
}

const Mapping* MappingOf(std::uint64_t address) {
    for (std::size_t index = 0; index < g_mappingCount; ++index)
        if (address >= g_mappings[index].start && address < g_mappings[index].end) return &g_mappings[index];
    return nullptr;
}

void DescribeAddress(ReportLine& line, std::uint64_t address) {
    line.Hex(address, 16);
    const Mapping* mapping = MappingOf(address);
    if (mapping == nullptr || mapping->name[0] == '\0') return;
    line.Text(" ").Text(mapping->name).Text("+").Hex(address - mapping->start + mapping->offset);
}

bool ReadWord(std::uint64_t address, std::uint64_t& value) {
    iovec local{&value, sizeof(value)};
    iovec remote{reinterpret_cast<void*>(address), sizeof(value)};
    return process_vm_readv(getpid(), &local, 1, &remote, 1, 0) == static_cast<ssize_t>(sizeof(value));
}

bool IsCode(std::uint64_t address) {
    const Mapping* mapping = MappingOf(address);
    return mapping != nullptr && mapping->executable;
}

const char* SignalName(int signal) {
    switch (signal) {
    case SIGSEGV: return "SIGSEGV";
    case SIGBUS: return "SIGBUS";
    case SIGILL: return "SIGILL";
    case SIGFPE: return "SIGFPE";
    default: return "signal";
    }
}

void Report(int signal, const siginfo_t* info, const ucontext_t* context) {
    ReadMappings();
    ReportLine line;
    line.Text("[libc] fatal ").Text(SignalName(signal)).Text(" (code ").Decimal(static_cast<std::uint64_t>(static_cast<std::uint32_t>(info->si_code))).Text(") at address ");
    line.Hex(reinterpret_cast<std::uint64_t>(info->si_addr), 16).Text(" in thread ").Decimal(static_cast<std::uint64_t>(syscall(SYS_gettid))).Write();
    if (context == nullptr) return;
    const greg_t* registers = context->uc_mcontext.gregs;
    const auto rip = static_cast<std::uint64_t>(registers[REG_RIP]);
    const auto rsp = static_cast<std::uint64_t>(registers[REG_RSP]);
    auto rbp = static_cast<std::uint64_t>(registers[REG_RBP]);
    line.Text("  rip ");
    DescribeAddress(line, rip);
    line.Write();
    static constexpr struct {
        const char* name;
        int index;
    } Registers[] = {{"rax", REG_RAX}, {"rbx", REG_RBX}, {"rcx", REG_RCX}, {"rdx", REG_RDX}, {"rsi", REG_RSI}, {"rdi", REG_RDI}, {"rbp", REG_RBP}, {"rsp", REG_RSP},
        {"r8", REG_R8}, {"r9", REG_R9}, {"r10", REG_R10}, {"r11", REG_R11}, {"r12", REG_R12}, {"r13", REG_R13}, {"r14", REG_R14}, {"r15", REG_R15}};
    for (std::size_t index = 0; index < std::size(Registers); index += 4) {
        line.Text(" ");
        for (std::size_t column = index; column < index + 4; ++column) line.Text(" ").Text(Registers[column].name).Text(" ").Hex(static_cast<std::uint64_t>(registers[Registers[column].index]), 16);
        line.Write();
    }
    line.Text("  frame chain from rbp:").Write();
    for (int depth = 1; depth <= FrameLimit && rbp != 0 && rbp % 8 == 0; ++depth) {
        std::uint64_t next = 0;
        std::uint64_t returnAddress = 0;
        if (!ReadWord(rbp, next) || !ReadWord(rbp + 8, returnAddress) || !IsCode(returnAddress)) break;
        line.Text("    #").Decimal(static_cast<std::uint64_t>(depth)).Text(" ");
        DescribeAddress(line, returnAddress);
        line.Write();
        if (next <= rbp) break;
        rbp = next;
    }
    line.Text("  stack words that point at code, from rsp:").Write();
    int candidates = 0;
    for (std::uint64_t address = rsp & ~std::uint64_t{7}; address < rsp + CandidateSpan && candidates < CandidateLimit; address += 8) {
        std::uint64_t word = 0;
        if (!ReadWord(address, word)) break;
        if (!IsCode(word)) continue;
        line.Text("    [rsp+").Hex(address - rsp).Text("] ");
        DescribeAddress(line, word);
        line.Write();
        ++candidates;
    }
}

void ReportCallTrace() {
    static CallTraceEntry entries[64];
    static CallTraceEntry own[32];
    static CallTraceCount counts[32];
    ReportLine line;
    const std::size_t count = CopyCallTrace_nid_no_patch(entries, std::size(entries));
    if (count == 0) return;
    line.Text("  last traced system library calls (APS5_TRACE_CALLS):").Write();
    for (std::size_t index = 0; index < count; ++index) {
        line.Text("    #").Decimal(entries[index].sequence).Text(" thread ").Decimal(entries[index].thread);
        line.Text(" ").Text(entries[index].library).Text(" ").Text(entries[index].name).Text(" from ");
        DescribeAddress(line, entries[index].returnAddress);
        line.Write();
    }
    const auto thread = static_cast<std::uint64_t>(syscall(SYS_gettid));
    const std::size_t owned = CopyThreadCallTrace_nid_no_patch(thread, own, std::size(own));
    if (owned != 0) {
        line.Text("  last traced calls of thread ").Decimal(thread).Text(":").Write();
        for (std::size_t index = 0; index < owned; ++index) {
            line.Text("    #").Decimal(own[index].sequence).Text(" ").Text(own[index].library).Text(" ").Text(own[index].name).Text(" from ");
            DescribeAddress(line, own[index].returnAddress);
            line.Write();
        }
    }
    std::uint64_t total = 0;
    const std::size_t counted = CopyCallCounts_nid_no_patch(counts, std::size(counts), &total);
    if (counted == 0) return;
    line.Text("  most called traced imports (").Decimal(total).Text(" calls in total):").Write();
    for (std::size_t index = 0; index < counted; ++index) line.Text("    ").Decimal(counts[index].calls).Text(" ").Text(counts[index].library).Text(" ").Text(counts[index].name).Write();
}

void OnFatalSignal(int signal, siginfo_t* info, void* context) {
    const int savedErrno = errno;
    if (!g_reporting.exchange(true, std::memory_order_acq_rel)) {
        Report(signal, info, static_cast<const ucontext_t*>(context));
        ReportCallTrace();
        g_reported.store(true, std::memory_order_release);
    } else {
        const timespec pause{0, 1000000};
        for (int waited = 0; waited < 2000 && !g_reported.load(std::memory_order_acquire); ++waited) nanosleep(&pause, nullptr);
    }
    errno = savedErrno;
    std::size_t slot = 0;
    while (slot < std::size(FatalSignals) && FatalSignals[slot] != signal) ++slot;
    const struct sigaction& previous = g_previous[slot];
    if ((previous.sa_flags & SA_SIGINFO) != 0 && previous.sa_sigaction != nullptr) {
        previous.sa_sigaction(signal, info, context);
        return;
    }
    if (previous.sa_handler != SIG_DFL && previous.sa_handler != SIG_IGN && previous.sa_handler != nullptr) {
        previous.sa_handler(signal);
        return;
    }
    struct sigaction fallback{};
    fallback.sa_handler = SIG_DFL;
    sigemptyset(&fallback.sa_mask);
    sigaction(signal, &fallback, nullptr);
    if (info->si_code <= 0) raise(signal);
}

const bool g_installed = [] {
    stack_t alternate{};
    alternate.ss_sp = g_alternateStack;
    alternate.ss_size = sizeof(g_alternateStack);
    sigaltstack(&alternate, nullptr);
    struct sigaction action{};
    action.sa_sigaction = OnFatalSignal;
    action.sa_flags = SA_SIGINFO | SA_ONSTACK;
    sigemptyset(&action.sa_mask);
    for (std::size_t slot = 0; slot < std::size(FatalSignals); ++slot) sigaction(FatalSignals[slot], &action, &g_previous[slot]);
    return true;
}();

}
#endif
