#if defined(_WIN32) || defined(__linux__)
#ifdef _WIN32
#include <windows.h>
#else
#include <cerrno>
#include <csignal>
#include <ctime>
#include <link.h>
#include <sys/mman.h>
#include <unistd.h>
#endif
#include "prx/libc/include/CallTrace.hpp"
#include "prx/libc/include/general/AsmMacros.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include <algorithm>
#include <atomic>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <mutex>
#include <stdexcept>
#include <string>
#include <string_view>
#include <system_error>
#include <unordered_map>

extern "C" void CallTraceEnter_nid_no_patch();

namespace {

struct ImportRecord {
    void* target = nullptr;
    std::string name;
    std::string library;
    mutable std::atomic<std::uint64_t> calls{0};
};

struct RingSlot {
    std::atomic<std::uint64_t> sequence{0};
    std::atomic<std::uint64_t> thread{0};
    std::atomic<std::uint64_t> returnAddress{0};
    std::atomic<const ImportRecord*> record{nullptr};
};

constexpr std::size_t RingCapacity = 65536;
constexpr std::size_t ThunkBytes = 24;
constexpr std::size_t ThunkAreaBytes = 1024 * 1024;
constexpr std::size_t RecordCapacity = ThunkAreaBytes / ThunkBytes;

std::mutex g_mutex;
std::unordered_map<void*, void*> g_thunkByTarget;
std::atomic<ImportRecord*> g_records{nullptr};
std::atomic<std::size_t> g_recordCount{0};
std::atomic<RingSlot*> g_ring{nullptr};
std::atomic<std::uint64_t> g_sequence{0};
std::uint8_t* g_writable = nullptr;
std::uint8_t* g_executable = nullptr;

bool Selected(std::string_view library) {
    const char* value = std::getenv("APS5_TRACE_CALLS");
    if (value == nullptr || *value == '\0') return false;
    std::string_view rest(value);
    if (library.ends_with(".prx")) library.remove_suffix(4);
    while (!rest.empty()) {
        const auto comma = rest.find(',');
        auto token = rest.substr(0, comma);
        rest = comma == std::string_view::npos ? std::string_view{} : rest.substr(comma + 1);
        if (token.ends_with(".prx")) token.remove_suffix(4);
        if (token == "all" || token == library) return true;
    }
    return false;
}

std::uint64_t CurrentThread() {
#ifdef _WIN32
    return GetCurrentThreadId();
#else
    return static_cast<std::uint64_t>(gettid());
#endif
}

#ifdef _WIN32
void MapThunkArea() {
    const HANDLE section = CreateFileMappingW(INVALID_HANDLE_VALUE, nullptr, PAGE_EXECUTE_READWRITE, 0, static_cast<DWORD>(ThunkAreaBytes), nullptr);
    if (section == nullptr) throw std::system_error(static_cast<int>(GetLastError()), std::system_category(), "call trace: CreateFileMappingW");
    g_writable = static_cast<std::uint8_t*>(MapViewOfFile(section, FILE_MAP_WRITE, 0, 0, ThunkAreaBytes));
    g_executable = static_cast<std::uint8_t*>(MapViewOfFile(section, FILE_MAP_READ | FILE_MAP_EXECUTE, 0, 0, ThunkAreaBytes));
    const DWORD code = GetLastError();
    CloseHandle(section);
    if (g_writable == nullptr || g_executable == nullptr) throw std::system_error(static_cast<int>(code), std::system_category(), "call trace: MapViewOfFile");
}
#else
void MapThunkArea() {
    const int file = memfd_create("aps5-call-trace", MFD_CLOEXEC);
    if (file < 0) throw std::system_error(errno, std::generic_category(), "call trace: memfd_create");
    void* writable = MAP_FAILED;
    void* executable = MAP_FAILED;
    if (ftruncate(file, ThunkAreaBytes) == 0) {
        writable = mmap(nullptr, ThunkAreaBytes, PROT_READ | PROT_WRITE, MAP_SHARED, file, 0);
        if (writable != MAP_FAILED) executable = mmap(nullptr, ThunkAreaBytes, PROT_READ | PROT_EXEC, MAP_SHARED, file, 0);
    }
    const int code = errno;
    close(file);
    if (writable == MAP_FAILED || executable == MAP_FAILED) {
        if (writable != MAP_FAILED) munmap(writable, ThunkAreaBytes);
        throw std::system_error(code, std::generic_category(), "call trace: mmap");
    }
    g_writable = static_cast<std::uint8_t*>(writable);
    g_executable = static_cast<std::uint8_t*>(executable);
}

struct ModuleSegment {
    std::uint64_t start;
    std::uint64_t end;
    std::uint64_t base;
    char name[64];
};

struct ModuleMap {
    std::size_t count;
    std::uint64_t pageSize;
    ModuleSegment segments[256];
};

struct sigaction g_previousAbort{};
std::atomic<const ModuleMap*> g_modules{nullptr};
std::atomic<bool> g_reporting{false};
std::atomic<bool> g_reported{false};
unsigned long long g_moduleChanges = ~0ull;

void SnapshotModules() {
    unsigned long long changes = 0;
    dl_iterate_phdr([](dl_phdr_info* info, std::size_t, void* data) {
        *static_cast<unsigned long long*>(data) = info->dlpi_adds + info->dlpi_subs;
        return 1;
    }, &changes);
    if (changes == g_moduleChanges) return;
    g_moduleChanges = changes;
    auto* map = new ModuleMap{};
    map->pageSize = static_cast<std::uint64_t>(sysconf(_SC_PAGESIZE));
    dl_iterate_phdr([](dl_phdr_info* info, std::size_t, void* data) {
        auto& modules = *static_cast<ModuleMap*>(data);
        const char* path = info->dlpi_name != nullptr && *info->dlpi_name != '\0' ? info->dlpi_name : program_invocation_short_name;
        const char* slash = std::strrchr(path, '/');
        const char* name = slash == nullptr ? path : slash + 1;
        std::uint64_t first = std::numeric_limits<std::uint64_t>::max();
        for (int index = 0; index < info->dlpi_phnum; ++index)
            if (info->dlpi_phdr[index].p_type == PT_LOAD) first = std::min<std::uint64_t>(first, info->dlpi_phdr[index].p_vaddr & ~(modules.pageSize - 1));
        for (int index = 0; index < info->dlpi_phnum && modules.count < std::size(modules.segments); ++index) {
            const auto& header = info->dlpi_phdr[index];
            if (header.p_type != PT_LOAD || header.p_memsz == 0) continue;
            auto& segment = modules.segments[modules.count++];
            segment.base = info->dlpi_addr + first;
            segment.start = info->dlpi_addr + header.p_vaddr;
            segment.end = segment.start + header.p_memsz;
            std::strncpy(segment.name, name, sizeof(segment.name) - 1);
        }
        return 0;
    }, map);
    g_modules.store(map, std::memory_order_release);
}

class ReportLine {
public:
    ReportLine& Text(const char* text) {
        while (*text != '\0' && _length < sizeof(_buffer)) _buffer[_length++] = *text++;
        return *this;
    }

    ReportLine& Decimal(std::uint64_t value, std::size_t width = 0) {
        char digits[20];
        std::size_t count = 0;
        do {
            digits[count++] = static_cast<char>('0' + value % 10);
            value /= 10;
        } while (value != 0);
        for (std::size_t pad = count; pad < width; ++pad) Text(" ");
        while (count > 0 && _length < sizeof(_buffer)) _buffer[_length++] = digits[--count];
        return *this;
    }

    ReportLine& Hex(std::uint64_t value, std::size_t digits) {
        while (digits < 16 && (value >> (digits * 4)) != 0) ++digits;
        Text("0x");
        for (std::size_t index = digits; index > 0 && _length < sizeof(_buffer); --index) {
            _buffer[_length++] = "0123456789abcdef"[(value >> ((index - 1) * 4)) & 0xf];
        }
        return *this;
    }

    void Write() {
        Text("\n");
        std::size_t written = 0;
        while (written < _length) {
            const ssize_t result = ::write(STDERR_FILENO, _buffer + written, _length - written);
            if (result < 0 && errno == EINTR) continue;
            if (result <= 0) break;
            written += static_cast<std::size_t>(result);
        }
        _length = 0;
    }

private:
    char _buffer[512];
    std::size_t _length = 0;
};

void DescribeAddress(ReportLine& line, std::uint64_t address) {
    line.Hex(address, 16);
    const ModuleMap* modules = g_modules.load(std::memory_order_acquire);
    if (modules == nullptr) return;
    for (std::size_t index = 0; index < modules->count; ++index) {
        const auto& segment = modules->segments[index];
        if (address < segment.start || address >= segment.end) continue;
        line.Text(" ").Text(segment.name).Text("+");
        line.Hex(address - segment.base, 1);
        return;
    }
}

void ReportCallTrace() {
    static CallTraceEntry entries[64];
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
    static CallTraceEntry own[32];
    const std::uint64_t thread = CurrentThread();
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
    for (std::size_t index = 0; index < counted; ++index) {
        line.Text("    ").Decimal(counts[index].calls, 12).Text(" ").Text(counts[index].library).Text(" ").Text(counts[index].name).Write();
    }
}

void ReportOnAbort(int number, siginfo_t* info, void* context) {
    const int savedErrno = errno;
    if (!g_reporting.exchange(true, std::memory_order_acq_rel)) {
        ReportLine line;
        line.Text("[libc] SIGABRT").Write();
        ReportCallTrace();
        g_reported.store(true, std::memory_order_release);
    } else {
        const timespec pause{0, 1000000};
        for (int waited = 0; waited < 2000 && !g_reported.load(std::memory_order_acquire); ++waited) nanosleep(&pause, nullptr);
    }
    errno = savedErrno;
    if ((g_previousAbort.sa_flags & SA_SIGINFO) != 0) {
        if (g_previousAbort.sa_sigaction != nullptr) g_previousAbort.sa_sigaction(number, info, context);
        return;
    }
    if (g_previousAbort.sa_handler == SIG_IGN) return;
    if (g_previousAbort.sa_handler != SIG_DFL) {
        g_previousAbort.sa_handler(number);
        return;
    }
    struct sigaction fallback{};
    fallback.sa_handler = SIG_DFL;
    sigemptyset(&fallback.sa_mask);
    sigaction(number, &fallback, nullptr);
    raise(number);
}

void InstallAbortReport() {
    struct sigaction action{};
    action.sa_sigaction = ReportOnAbort;
    action.sa_flags = SA_SIGINFO;
    sigemptyset(&action.sa_mask);
    if (sigaction(SIGABRT, &action, &g_previousAbort) != 0) throw std::system_error(errno, std::generic_category(), "call trace: sigaction");
}
#endif

void* WriteThunk(const ImportRecord& record, const std::size_t index) {
    const std::size_t offset = index * ThunkBytes;
    const std::uint8_t prefix[] = {0x49, 0xbb};
    const std::uint8_t jump[] = {0xff, 0x25, 0, 0, 0, 0};
    const auto recordAddress = reinterpret_cast<std::uint64_t>(&record);
    const auto handler = reinterpret_cast<std::uint64_t>(&CallTraceEnter_nid_no_patch);
    std::uint8_t* thunk = g_writable + offset;
    std::memcpy(thunk, prefix, sizeof(prefix));
    std::memcpy(thunk + 2, &recordAddress, sizeof(recordAddress));
    std::memcpy(thunk + 10, jump, sizeof(jump));
    std::memcpy(thunk + 16, &handler, sizeof(handler));
#ifdef _WIN32
    FlushInstructionCache(GetCurrentProcess(), g_executable + offset, ThunkBytes);
#else
    __builtin___clear_cache(reinterpret_cast<char*>(g_executable + offset), reinterpret_cast<char*>(g_executable + offset + ThunkBytes));
#endif
    return g_executable + offset;
}

}

static_assert(offsetof(ImportRecord, target) == 0);

extern "C" {

void APS5_VABI CallTraceRecord_nid_no_patch(const void* record, std::uint64_t returnAddress) {
    static_cast<const ImportRecord*>(record)->calls.fetch_add(1, std::memory_order_relaxed);
    const std::uint64_t sequence = g_sequence.fetch_add(1, std::memory_order_relaxed) + 1;
    RingSlot& slot = g_ring.load(std::memory_order_relaxed)[sequence % RingCapacity];
    slot.sequence.store(0, std::memory_order_release);
    slot.thread.store(CurrentThread(), std::memory_order_relaxed);
    slot.returnAddress.store(returnAddress, std::memory_order_relaxed);
    slot.record.store(static_cast<const ImportRecord*>(record), std::memory_order_relaxed);
    slot.sequence.store(sequence, std::memory_order_release);
}

void* TraceImport_nid_no_patch(void* target, const char* name, const char* library) {
    if (target == nullptr || name == nullptr || library == nullptr || !Selected(library)) return target;
    std::lock_guard lock(g_mutex);
    if (g_ring.load(std::memory_order_relaxed) == nullptr) {
        MapThunkArea();
        g_records.store(new ImportRecord[RecordCapacity], std::memory_order_release);
        g_ring.store(new RingSlot[RingCapacity], std::memory_order_release);
#ifdef __linux__
        InstallAbortReport();
#endif
    }
#ifdef __linux__
    SnapshotModules();
#endif
    if (const auto existing = g_thunkByTarget.find(target); existing != g_thunkByTarget.end()) return existing->second;
    const std::size_t index = g_recordCount.load(std::memory_order_relaxed);
    if (index == RecordCapacity) throw std::runtime_error("call trace: thunk area exhausted");
    ImportRecord& record = g_records.load(std::memory_order_relaxed)[index];
    record.target = target;
    record.name = name;
    record.library = library;
    void* thunk = WriteThunk(record, index);
    g_thunkByTarget.emplace(target, thunk);
    g_recordCount.store(index + 1, std::memory_order_release);
    return thunk;
}

std::size_t CopyCallTrace_nid_no_patch(CallTraceEntry* entries, std::size_t capacity) {
    const RingSlot* ring = g_ring.load(std::memory_order_acquire);
    if (entries == nullptr || capacity == 0 || ring == nullptr) return 0;
    const std::uint64_t last = g_sequence.load(std::memory_order_acquire);
    const std::uint64_t span = std::min<std::uint64_t>({last, capacity, RingCapacity});
    std::size_t count = 0;
    for (std::uint64_t sequence = last - span + 1; sequence <= last; ++sequence) {
        const RingSlot& slot = ring[sequence % RingCapacity];
        if (slot.sequence.load(std::memory_order_acquire) != sequence) continue;
        const auto* record = slot.record.load(std::memory_order_relaxed);
        const CallTraceEntry entry{sequence, slot.thread.load(std::memory_order_relaxed), slot.returnAddress.load(std::memory_order_relaxed), record->name.c_str(), record->library.c_str()};
        if (slot.sequence.load(std::memory_order_acquire) != sequence) continue;
        entries[count++] = entry;
    }
    return count;
}

std::size_t CopyThreadCallTrace_nid_no_patch(std::uint64_t thread, CallTraceEntry* entries, std::size_t capacity) {
    const RingSlot* ring = g_ring.load(std::memory_order_acquire);
    if (entries == nullptr || capacity == 0 || ring == nullptr) return 0;
    const std::uint64_t last = g_sequence.load(std::memory_order_acquire);
    const std::uint64_t span = std::min<std::uint64_t>(last, RingCapacity);
    std::size_t count = 0;
    for (std::uint64_t sequence = last; sequence > last - span && count < capacity; --sequence) {
        const RingSlot& slot = ring[sequence % RingCapacity];
        if (slot.sequence.load(std::memory_order_acquire) != sequence || slot.thread.load(std::memory_order_relaxed) != thread) continue;
        const auto* record = slot.record.load(std::memory_order_relaxed);
        const CallTraceEntry entry{sequence, thread, slot.returnAddress.load(std::memory_order_relaxed), record->name.c_str(), record->library.c_str()};
        if (slot.sequence.load(std::memory_order_acquire) != sequence) continue;
        entries[count++] = entry;
    }
    std::reverse(entries, entries + count);
    return count;
}

std::size_t CopyCallCounts_nid_no_patch(CallTraceCount* entries, std::size_t capacity, std::uint64_t* total) {
    if (total != nullptr) *total = g_sequence.load(std::memory_order_acquire);
    if (entries == nullptr || capacity == 0) return 0;
    const std::size_t recorded = g_recordCount.load(std::memory_order_acquire);
    const ImportRecord* records = g_records.load(std::memory_order_acquire);
    std::size_t count = 0;
    for (std::size_t index = 0; index < recorded; ++index) {
        const ImportRecord& record = records[index];
        const std::uint64_t calls = record.calls.load(std::memory_order_relaxed);
        if (calls == 0 || (count == capacity && calls <= entries[count - 1].calls)) continue;
        std::size_t position = count < capacity ? count++ : count - 1;
        while (position > 0 && entries[position - 1].calls < calls) {
            entries[position] = entries[position - 1];
            --position;
        }
        entries[position] = {calls, record.name.c_str(), record.library.c_str()};
    }
    return count;
}

}

asm(".text\n"
    APS5_ASM_FUNCTION("CallTraceEnter_nid_no_patch")
    "    push %rdi\n"
    "    push %rsi\n"
    "    push %rdx\n"
    "    push %rcx\n"
    "    push %r8\n"
    "    push %r9\n"
    "    push %rax\n"
    "    push %r10\n"
    "    push %r11\n"
    "    sub $128, %rsp\n"
    "    movdqu %xmm0, 0(%rsp)\n"
    "    movdqu %xmm1, 16(%rsp)\n"
    "    movdqu %xmm2, 32(%rsp)\n"
    "    movdqu %xmm3, 48(%rsp)\n"
    "    movdqu %xmm4, 64(%rsp)\n"
    "    movdqu %xmm5, 80(%rsp)\n"
    "    movdqu %xmm6, 96(%rsp)\n"
    "    movdqu %xmm7, 112(%rsp)\n"
    "    mov %r11, %rdi\n"
    "    mov 200(%rsp), %rsi\n"
    "    call CallTraceRecord_nid_no_patch\n"
    "    movdqu 0(%rsp), %xmm0\n"
    "    movdqu 16(%rsp), %xmm1\n"
    "    movdqu 32(%rsp), %xmm2\n"
    "    movdqu 48(%rsp), %xmm3\n"
    "    movdqu 64(%rsp), %xmm4\n"
    "    movdqu 80(%rsp), %xmm5\n"
    "    movdqu 96(%rsp), %xmm6\n"
    "    movdqu 112(%rsp), %xmm7\n"
    "    add $128, %rsp\n"
    "    pop %r11\n"
    "    pop %r10\n"
    "    pop %rax\n"
    "    pop %r9\n"
    "    pop %r8\n"
    "    pop %rcx\n"
    "    pop %rdx\n"
    "    pop %rsi\n"
    "    pop %rdi\n"
    "    jmp *(%r11)\n"
    APS5_ASM_FUNCTION_END("CallTraceEnter_nid_no_patch"));
#endif
