#include "prx/libc/include/Api.hpp"
#include <array>
#include <bit>
#include <cerrno>
#include <csignal>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <exception>
#include <limits>
#include <string>
#ifndef _WIN32
#include <sys/mman.h>
#include <sys/resource.h>
#include <sys/wait.h>
#include <unistd.h>
#include <dlfcn.h>
#else
#include <windows.h>
#endif

namespace {

void Require(bool condition, const char* message) {
    if (condition) return;
    std::fprintf(stderr, "exception report: %s\n", message);
    std::exit(1);
}

using Report = void (APS5_VABI *)(const LibcException::ExceptionObject*);
Report report;

using What = const char* (APS5_VABI *)(const void*);
const void* expectedSelf;
const char* diagnostic;
int calls;

const char* APS5_VABI GetMessage(const void* self) {
    Require(self == expectedSelf && ++calls == 1, "what receives this exactly once");
    Require(fwrite_nid_postfix("callback;", 1, 9, &_Stdout_nid_postfix) == 9, "buffer callback output");
    return diagnostic;
}

void Reject(const LibcException::ExceptionObject* object) {
    bool rejected = false;
    try { report(object); }
    catch (const std::exception&) { rejected = true; }
    Require(rejected, "unreadable input is explicitly refused");
}

#ifndef _WIN32
void CheckReport(const std::string& message, bool boundary) {
    Require(std::fflush(nullptr) == 0, "flush parent streams");
    int output[2];
    Require(pipe(output) == 0, "pipe");
    const pid_t child = fork();
    Require(child >= 0, "fork");
    if (child == 0) {
        const rlimit noCore{0, 0};
        setrlimit(RLIMIT_CORE, &noCore);
        close(output[0]);
        Require(dup2(output[1], 1) == 1 && dup2(output[1], 2) == 2, "redirect streams");
        close(output[1]);
        char buffer[4096];
        Require(setvbuf_nid_postfix(&_Stdout_nid_postfix, buffer, 0, sizeof(buffer)) == 0, "buffer stdout");
        const std::array<const void*, 3> table{nullptr, nullptr, std::bit_cast<const void*>(static_cast<What>(GetMessage))};
        LibcException::ExceptionObject object{table.data(), "wrong nonvirtual message"};
        const LibcException::ExceptionObject* pointer = &object;
        if (boundary) {
            const auto page = static_cast<std::size_t>(sysconf(_SC_PAGESIZE));
            auto* memory = static_cast<char*>(mmap(nullptr, page * 2, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0));
            Require(memory != MAP_FAILED && mprotect(memory + page, page, PROT_NONE) == 0, "guard page");
            auto** slot = reinterpret_cast<const void**>(memory + page - sizeof(void*));
            *slot = table.data();
            pointer = reinterpret_cast<const LibcException::ExceptionObject*>(slot);
        }
        expectedSelf = pointer;
        diagnostic = message.c_str();
        calls = 0;
        try { report(pointer); }
        catch (...) { _exit(87); }
        _exit(88);
    }
    close(output[1]);
    std::string captured;
    std::array<char, 4096> buffer;
    for (;;) {
        const auto count = read(output[0], buffer.data(), buffer.size());
        if (count < 0 && errno == EINTR) continue;
        Require(count >= 0, "read child output");
        if (count == 0) break;
        captured.append(buffer.data(), static_cast<std::size_t>(count));
    }
    close(output[0]);
    int status = 0;
    Require(waitpid(child, &status, 0) == child, "waitpid");
    Require(WIFSIGNALED(status) && WTERMSIG(status) == SIGABRT, "reports by aborting instead of throwing or returning");
    const std::string expected = "callback;" + message + "(0) : error : exception\n";
    Require(captured.starts_with(expected), "virtual diagnostic, flush-before-report order and complete message");
}
#endif

}

int main(int argc, char** argv) {
    Require(argc == 2, "library path argument");
#ifdef _WIN32
    auto module = LoadLibraryA(argv[1]);
    Require(module != nullptr, "load library");
    report = reinterpret_cast<Report>(GetProcAddress(module, "libSceLibcInternalInventory_245_nid_no_patch"));
#else
    auto* module = dlopen(argv[1], RTLD_NOW | RTLD_LOCAL);
    Require(module != nullptr, "load library");
    report = reinterpret_cast<Report>(dlsym(module, "libSceLibcInternalInventory_245_nid_no_patch"));
#endif
    Require(report != nullptr, "resolve exception report entry");
    Reject(nullptr);
    const std::array<const void*, 3> empty{};
    LibcException::ExceptionObject object{empty.data(), nullptr};
    Reject(&object);
    object.vtable = reinterpret_cast<const void*>(std::numeric_limits<std::uintptr_t>::max() - sizeof(void*));
    Reject(&object);
#ifndef _WIN32
    CheckReport("guest failure", false);
    CheckReport("", false);
    CheckReport("100% complete: %s\nsecond line", false);
    CheckReport(std::string(12000, 'x'), false);
    CheckReport("minimal object", true);
    std::puts("exception report: 3 input refusals and 5 fatal diagnostic cases passed");
#else
    std::puts("exception report: input refusals passed; fatal subprocess coverage is POSIX-only");
#endif
}
