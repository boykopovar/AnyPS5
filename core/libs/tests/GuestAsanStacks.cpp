#include "prx/libSceAddressSanitizer/include/Api.hpp"
#include "prx/libkernel/include/Api.hpp"
#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"

#include <csignal>
#include <dlfcn.h>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <functional>
#include <initializer_list>
#include <sstream>
#include <string>
#include <system_error>
#include <sys/wait.h>
#include <unistd.h>
#include <vector>

#if defined(__APPLE__)
#include <mach-o/dyld.h>
#endif

extern "C" {

alignas(64) unsigned char AsanStacksBuffer[128];
alignas(64) unsigned char AsanStacksData[64];

[[gnu::noinline]] void AsanStacksLeaf() {
    __asan_load4(reinterpret_cast<std::uintptr_t>(AsanStacksBuffer + 32));
    asm volatile("" ::: "memory");
}

[[gnu::noinline]] void AsanStacksMiddle() {
    AsanStacksLeaf();
    asm volatile("" ::: "memory");
}

[[gnu::noinline]] void AsanStacksOuter() {
    AsanStacksMiddle();
    asm volatile("" ::: "memory");
}

[[gnu::noinline]] void* AsanStacksAllocate() {
    void* const pointer = __asan_malloc(16);
    asm volatile("" ::: "memory");
    return pointer;
}

[[gnu::noinline]] void AsanStacksPrint() {
    __sanitizer_print_stack_trace();
    asm volatile("" ::: "memory");
}

std::uint32_t AsanStacksGuards[3];

[[gnu::noinline]] void AsanStacksCovered() {
    __sanitizer_cov_trace_pc_guard(&AsanStacksGuards[1]);
    asm volatile("" ::: "memory");
}

[[gnu::noinline]] void AsanStacksCoveredLater() {
    asm volatile("nop; nop; nop; nop; nop; nop; nop; nop" ::: "memory");
    __sanitizer_cov_trace_pc_guard(&AsanStacksGuards[1]);
    asm volatile("" ::: "memory");
}

}

namespace {

std::string ThreadTag() {
    return "T" + std::to_string(scePthreadGetthreadid());
}

void Require(bool value, const char* what) {
    if (value) return;
    std::fprintf(stdout, "failed: %s\n", what);
    std::fflush(stdout);
    std::abort();
}

struct Outcome {
    std::string output;
    int status = 0;
};

Outcome Run(const char* options, const std::function<void()>& body, const char* threadName = nullptr) {
    int channel[2] = {-1, -1};
    Require(::pipe(channel) == 0, "pipe");
    std::fflush(nullptr);
    const pid_t child = ::fork();
    Require(child >= 0, "fork");
    if (child == 0) {
        ::close(channel[0]);
        if (::dup2(channel[1], 2) != 2) ::_exit(70);
        ::close(channel[1]);
        if (options != nullptr) ::setenv("APS5_GUEST_ASAN_OPTIONS", options, 1);
        else ::unsetenv("APS5_GUEST_ASAN_OPTIONS");
        if (threadName != nullptr) Require(scePthreadRename(scePthreadSelf(), threadName) == 0, "scePthreadRename");
        __asan_init();
        body();
        std::fflush(nullptr);
        ::_exit(0);
    }
    ::close(channel[1]);
    Outcome outcome;
    char buffer[512];
    for (ssize_t count; (count = ::read(channel[0], buffer, sizeof(buffer))) > 0;) outcome.output.append(buffer, static_cast<std::size_t>(count));
    ::close(channel[0]);
    Require(::waitpid(child, &outcome.status, 0) == child, "waitpid");
    return outcome;
}

void Fail(const Outcome& outcome, const char* what, const std::string& detail) {
    std::fprintf(stdout, "%s: %s; status %d, output:\n%s---\n", what, detail.c_str(), outcome.status, outcome.output.c_str());
    std::fflush(stdout);
    std::abort();
}

void Expect(const Outcome& outcome, int code, std::initializer_list<std::string> fragments, const char* what) {
    if (!WIFEXITED(outcome.status) || WEXITSTATUS(outcome.status) != code) Fail(outcome, what, "wrong exit status, expected " + std::to_string(code));
    for (const auto& fragment : fragments)
        if (outcome.output.find(fragment) == std::string::npos) Fail(outcome, what, "missing \"" + fragment + "\"");
}

void ExpectAbsent(const Outcome& outcome, const std::string& fragment, const char* what) {
    if (outcome.output.find(fragment) != std::string::npos) Fail(outcome, what, "unexpected \"" + fragment + "\"");
}

void ExpectOrder(const Outcome& outcome, std::initializer_list<std::string> fragments, const char* what) {
    std::size_t at = 0;
    for (const auto& fragment : fragments) {
        const std::size_t found = outcome.output.find(fragment, at);
        if (found == std::string::npos) Fail(outcome, what, "missing in order \"" + fragment + "\"");
        at = found + fragment.size();
    }
}

void ExpectClean(const Outcome& outcome, const char* what);

void ExpectAbort(const Outcome& outcome, const char* what) {
    if (!WIFSIGNALED(outcome.status) || WTERMSIG(outcome.status) != SIGABRT) Fail(outcome, what, "expected an abort");
}

std::string ProgramName() {
    char path[4096] = {};
#if defined(__APPLE__)
    std::uint32_t size = sizeof(path);
    Require(_NSGetExecutablePath(path, &size) == 0, "_NSGetExecutablePath");
#else
    Require(::readlink("/proc/self/exe", path, sizeof(path) - 1) > 0, "readlink");
#endif
    const std::string full = path;
    return full.substr(full.find_last_of('/') + 1);
}

void PoisonTarget() {
    __asan_poison_memory_region(AsanStacksBuffer + 32, 8);
}

void Unwinding() {
    const auto outcome = Run(nullptr, [] {
        PoisonTarget();
        AsanStacksOuter();
    });
    Expect(outcome, 1, {"READ of size 4 at ", " thread " + ThreadTag(), "    #0 0x", " in AsanStacksLeaf (", "    #1 0x", " in AsanStacksMiddle (", "    #2 0x", " in AsanStacksOuter (",
                        "SUMMARY: AddressSanitizer: use-after-poison (", ") in AsanStacksLeaf\n"},
           "unwinding through frame pointers");
    ExpectOrder(outcome, {"#0 0x", "in AsanStacksLeaf", "#1 0x", "in AsanStacksMiddle", "#2 0x", "in AsanStacksOuter", "\n\n"}, "frames are numbered from the faulting call");
    Expect(outcome, 1, {ProgramName() + "+0x"}, "frames name the module and offset");
}

void CraftedFrames() {
    const auto backwards = Run(nullptr, [] {
        alignas(16) static std::uintptr_t unused[1];
        static_cast<void>(unused);
        alignas(16) volatile std::uintptr_t frames[8] = {};
        frames[4] = reinterpret_cast<std::uintptr_t>(&frames[0]);
        frames[5] = 0x5000;
        frames[0] = reinterpret_cast<std::uintptr_t>(&frames[4]);
        frames[1] = 0x6000;
        __asan_report_error(0x4000, reinterpret_cast<std::uintptr_t>(&frames[4]), 0, reinterpret_cast<std::uintptr_t>(AsanStacksBuffer), 0, 1, 0);
    });
    Expect(backwards, 1, {"    #0 0x3fff  (<unknown module>)\n    #1 0x4fff  (<unknown module>)\n\n"}, "a frame pointer that goes backwards stops the walk");
    const auto low = Run(nullptr, [] {
        alignas(16) volatile std::uintptr_t frames[8] = {};
        frames[0] = reinterpret_cast<std::uintptr_t>(&frames[2]);
        frames[1] = 0x5000;
        frames[2] = reinterpret_cast<std::uintptr_t>(&frames[4]);
        frames[3] = 0x10;
        frames[4] = 0;
        frames[5] = 0x7000;
        __asan_report_error(0x4000, reinterpret_cast<std::uintptr_t>(&frames[0]), 0, reinterpret_cast<std::uintptr_t>(AsanStacksBuffer), 0, 1, 0);
    });
    Expect(low, 1, {"    #0 0x3fff  (<unknown module>)\n    #1 0x4fff  (<unknown module>)\n\n"}, "a return address in the zero page stops the walk");
    ExpectAbsent(low, "0xf  (", "the zero-page return address is not printed");
}

void Formatting() {
    const auto custom = Run("stack_trace_format=[%n]%f@%M", [] {
        PoisonTarget();
        AsanStacksOuter();
    });
    Expect(custom, 1, {"[0]AsanStacksLeaf@(" + ProgramName() + "+0x", "[1]AsanStacksMiddle@(", "[2]AsanStacksOuter@("}, "stack_trace_format");
    const auto bare = Run("symbolize=0", [] {
        PoisonTarget();
        AsanStacksOuter();
    });
    Expect(bare, 1, {"  (", "SUMMARY: AddressSanitizer: use-after-poison (", ") \n"}, "symbolize=0");
    ExpectAbsent(bare, "in AsanStacksLeaf", "symbolize=0 drops function names");
    const auto stripped = Run("strip_path_prefix=/", [] {
        PoisonTarget();
        AsanStacksOuter();
    });
    ExpectAbsent(stripped, "(/", "strip_path_prefix removes the leading slash");
    ExpectAbort(Run("stack_trace_format=%Z", [] {
                    PoisonTarget();
                    AsanStacksOuter();
                }),
                "an unknown specifier aborts");
    Expect(Run(nullptr, [] {
               PoisonTarget();
               AsanStacksOuter();
           },
               "worker"),
           1, {" thread " + ThreadTag() + " (worker)\n"}, "thread name");
}

void MallocContext() {
    const auto full = Run(nullptr, [] {
        auto* p = static_cast<unsigned char*>(AsanStacksAllocate());
        __asan_load1(reinterpret_cast<std::uintptr_t>(p + 16));
    });
    Expect(full, 1, {"allocated by thread " + ThreadTag() + " here:\n    #0 0x", " in __asan_malloc (", "    #1 0x", " in AsanStacksAllocate ("}, "allocation stack");
    const auto one = Run("malloc_context_size=1", [] {
        auto* p = static_cast<unsigned char*>(AsanStacksAllocate());
        __asan_load1(reinterpret_cast<std::uintptr_t>(p + 16));
    });
    Expect(one, 1, {"allocated by thread " + ThreadTag() + " here:\n    #0 0x"}, "malloc_context_size=1");
    const std::size_t oneAllocated = one.output.find("allocated by thread " + ThreadTag() + " here:\n");
    const std::string oneBlock = one.output.substr(oneAllocated, one.output.find("\n\n", oneAllocated) - oneAllocated);
    if (oneBlock.find("    #1 0x") != std::string::npos || oneBlock.find("in __asan_malloc") == std::string::npos) Fail(one, "one frame keeps only the allocator", oneBlock);
    const auto two = Run("malloc_context_size=2", [] {
        auto* p = static_cast<unsigned char*>(AsanStacksAllocate());
        __asan_load1(reinterpret_cast<std::uintptr_t>(p + 16));
    });
    Expect(two, 1, {"in __asan_malloc (", "    #1 0x", " in AsanStacksAllocate ("}, "malloc_context_size=2");
    const std::size_t allocated = two.output.find("allocated by thread " + ThreadTag() + " here:\n");
    Require(allocated != std::string::npos, "allocation block");
    const std::string block = two.output.substr(allocated, two.output.find("\n\n", allocated) - allocated);
    if (block.find("    #2 0x") != std::string::npos || block.find("    #1 0x") == std::string::npos) Fail(two, "two frames stop at the caller", block);
    Expect(Run("malloc_context_size=0", [] {
               auto* p = static_cast<unsigned char*>(AsanStacksAllocate());
               __asan_load1(reinterpret_cast<std::uintptr_t>(p + 16));
           }),
           1, {"allocated by thread " + ThreadTag() + " here:\n    <empty stack>\n\n"}, "malloc_context_size=0");
    ExpectAbort(Run("malloc_context_size=300", [] { AsanStacksAllocate(); }), "malloc_context_size is bounded");
}

void PrintStack() {
    Expect(Run(nullptr, [] {
               __asan_poison_memory_region(AsanStacksBuffer + 64, 8);
               __asan_memset(AsanStacksBuffer + 60, 0, 8);
           }),
           1, {"    #0 0x", " in __asan_memset (", "SUMMARY: AddressSanitizer: use-after-poison (", ") in __asan_memset\n"}, "intrinsic reports start in the intrinsic");
    const auto outcome = Run(nullptr, [] { AsanStacksPrint(); });
    Expect(outcome, 0, {"    #0 0x", " in __sanitizer_print_stack_trace (", "    #1 0x", " in AsanStacksPrint ("}, "print_stack_trace");
}

void Symbolize() {
    ExpectClean(Run(nullptr, [] {
                    char text[256] = {};
                    const auto leaf = reinterpret_cast<std::uintptr_t>(&AsanStacksLeaf);
                    __sanitizer_symbolize_pc(leaf + 2, "%f|%q|%M|%n|%%", text, sizeof(text));
                    const std::string rendered = text;
                    Require(rendered.rfind("AsanStacksLeaf|0x1|(" + ProgramName() + "+0x", 0) == 0 && rendered.size() > 4 && rendered.substr(rendered.size() - 4) == "|0|%", "symbolize_pc");
                    char tiny[6] = {};
                    __sanitizer_symbolize_pc(leaf + 2, "%f", tiny, sizeof(tiny));
                    Require(std::strcmp(tiny, "AsanS") == 0, "symbolize_pc truncates");
                    char untouched[4] = {'x', 'y', 'z', 0};
                    __sanitizer_symbolize_pc(leaf + 2, "%f", untouched, 0);
                    Require(untouched[0] == 'x', "a zero-size buffer is left alone");
                    char wild[64] = {};
                    __sanitizer_symbolize_pc(0x10, "%L", wild, sizeof(wild));
                    Require(std::strcmp(wild, "(<unknown module>)") == 0, "unknown pc");
                    char global[64] = "stale";
                    __sanitizer_symbolize_global(reinterpret_cast<std::uintptr_t>(AsanStacksData), "%g:%l", global, sizeof(global));
                    Require(std::strcmp(global, "AsanStacksData:0") == 0, "symbolize_global from the symbol table");
                    struct GlobalDescriptor {
                        std::uintptr_t beg, size, sizeWithRedzone;
                        const char* name;
                        const char* moduleName;
                        std::uintptr_t hasDynamicInit, location, odrIndicator;
                    };
                    alignas(64) static GlobalDescriptor descriptor;
                    descriptor = GlobalDescriptor{reinterpret_cast<std::uintptr_t>(AsanStacksData), 40, 64, "registered_name", "game.elf", 0, 0, 0};
                    __asan_register_globals(&descriptor, 1);
                    __sanitizer_symbolize_global(reinterpret_cast<std::uintptr_t>(AsanStacksData + 4), "[%g]", global, sizeof(global));
                    Require(std::strcmp(global, "[registered_name]") == 0, "symbolize_global prefers the registered global");
                    char none[16] = "stale";
                    __sanitizer_symbolize_global(0x10, "%g", none, sizeof(none));
                    Require(none[0] == 0, "an unknown global clears the buffer");
                }),
                "symbolize");
    ExpectAbort(Run(nullptr, [] {
                    char text[16] = {};
                    __sanitizer_symbolize_pc(reinterpret_cast<std::uintptr_t>(&AsanStacksLeaf), "%Z", text, sizeof(text));
                }),
                "symbolize_pc rejects an unknown specifier");
}

std::string ReadFile(const std::filesystem::path& path) {
    std::ifstream file(path, std::ios::binary);
    std::stringstream contents;
    contents << file.rdbuf();
    return contents.str();
}

std::uint64_t Word(const std::string& bytes, std::size_t index) {
    std::uint64_t value = 0;
    std::memcpy(&value, bytes.data() + index * 8, 8);
    return value;
}

std::string coverageDirectory;

void Coverage() {
    coverageDirectory = "/tmp/asan-coverage-" + std::to_string(::getpid());
    const std::filesystem::path hostDirectory = ResolvePath_nid_no_patch(coverageDirectory.c_str());
    std::error_code created;
    std::filesystem::create_directories(hostDirectory, created);
    Require(!created, "create the coverage directory");
    static std::string options;
    options = "coverage=1:coverage_dir=" + coverageDirectory;
    const auto outcome = Run(options.c_str(), [] {
        __sanitizer_cov_trace_pc_guard_init(AsanStacksGuards, AsanStacksGuards + 3);
        Require(AsanStacksGuards[0] == 1 && AsanStacksGuards[1] == 2 && AsanStacksGuards[2] == 3, "guards are numbered from 1");
        __sanitizer_cov_trace_pc_guard_init(AsanStacksGuards, AsanStacksGuards + 3);
        Require(AsanStacksGuards[2] == 3, "an initialized range is not renumbered");
        std::uint32_t more[2] = {};
        __sanitizer_cov_trace_pc_guard_init(more, more + 2);
        Require(more[0] == 4 && more[1] == 5, "a second module continues the numbering");
        __sanitizer_cov_trace_pc_guard_init(more, more);
        AsanStacksCovered();
        AsanStacksCoveredLater();
        std::uint32_t disabled = 0;
        __sanitizer_cov_trace_pc_guard(&disabled);
        __sanitizer_cov_dump();
        Require(std::strcmp(__sancov_default_options(), "") == 0, "default sancov options");
    });
    Expect(outcome, 0, {"SanitizerCoverage: " + coverageDirectory + "/" + ProgramName() + ".", ".sancov: 1 PCs written\n"}, "coverage dump");
    const std::size_t begin = outcome.output.find(coverageDirectory);
    const std::size_t end = outcome.output.find(':', begin);
    const std::string bytes = ReadFile(ResolvePath_nid_no_patch(outcome.output.substr(begin, end - begin).c_str()));
    Require(bytes.size() == 16 && Word(bytes, 0) == 0xC0BFFFFFFFFFFF64ull, "sancov file holds the magic and one module offset");
    Dl_info module{};
    Require(::dladdr(reinterpret_cast<const void*>(&AsanStacksCovered), &module) != 0 && module.dli_fbase != nullptr, "dladdr");
    const std::uint64_t base = reinterpret_cast<std::uintptr_t>(module.dli_fbase);
    const std::uint64_t function = reinterpret_cast<std::uintptr_t>(&AsanStacksCovered) - base;
    const std::uint64_t later = reinterpret_cast<std::uintptr_t>(&AsanStacksCoveredLater) - base;
    const std::uint64_t recorded = Word(bytes, 1);
    const std::uint64_t firstEnd = later > function ? later : function + 64;
    Require(recorded > function && recorded < firstEnd, "the module offset is the first call site's, relative to the module");
    const auto off = Run(nullptr, [] {
        __sanitizer_cov_trace_pc_guard_init(AsanStacksGuards, AsanStacksGuards + 3);
        AsanStacksCovered();
        __sanitizer_cov_dump();
        __sanitizer_dump_trace_pc_guard_coverage();
    });
    ExpectClean(off, "coverage=0 writes nothing");
    static std::string directOptions;
    directOptions = "coverage_dir=" + coverageDirectory;
    const auto direct = Run(directOptions.c_str(), [] {
        const std::uintptr_t pcs[4] = {reinterpret_cast<std::uintptr_t>(&AsanStacksLeaf) + 1, 0, reinterpret_cast<std::uintptr_t>(::dlsym(RTLD_DEFAULT, "puts")) + 1,
                                       reinterpret_cast<std::uintptr_t>(&AsanStacksMiddle) + 1};
        __sanitizer_dump_coverage(pcs, 4);
        __sanitizer_dump_coverage(pcs, 0);
    });
    Expect(direct, 0, {"/" + ProgramName() + ".", ".sancov: 2 PCs written\n", ".sancov: 1 PCs written\n"}, "dump_coverage writes one file per module regardless of the flag");
    ExpectAbsent(direct, "unknown pc", "a zero PC is skipped");
    const auto onDeath = Run(options.c_str(), [] {
        __sanitizer_cov_trace_pc_guard_init(AsanStacksGuards, AsanStacksGuards + 3);
        AsanStacksCovered();
        PoisonTarget();
        AsanStacksOuter();
    });
    Expect(onDeath, 1, {"ABORTING\n", ".sancov: 1 PCs written\n"}, "a fatal report dumps coverage");
    ExpectOrder(onDeath, {"ABORTING\n", "SanitizerCoverage: "}, "coverage is written after the report");
    std::error_code removed;
    std::filesystem::remove_all(hostDirectory, removed);
}

void ExpectClean(const Outcome& outcome, const char* what) {
    if (!WIFEXITED(outcome.status) || WEXITSTATUS(outcome.status) != 0 || !outcome.output.empty()) Fail(outcome, what, "expected a silent clean exit");
}

}

int main() {
#if defined(ANYPS5_ENABLE_ASAN) || defined(__SANITIZE_ADDRESS__)
    std::fputs("skipped: the host AddressSanitizer shadow covers the PS guest shadow range at 1 << 40\n", stderr);
    return 77;
#endif
    Unwinding();
    CraftedFrames();
    Formatting();
    MallocContext();
    PrintStack();
    Symbolize();
    Coverage();
    std::puts("guest_asan_stacks passed");
    return 0;
}
