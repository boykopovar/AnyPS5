#include "prx/libkernel_web/include/Api.hpp"
#include "prx/libkernel_sys/include/AdditionalExports.hpp"
#include "prx/libkernel/include/Api.hpp"
#include "prx/libc/include/Api.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <stdexcept>

namespace {

constexpr int GuestEnoent = 2;
constexpr int GuestEsrch = 3;
constexpr int GuestEbadf = 9;
constexpr int GuestEchild = 10;
constexpr int GuestEfault = 14;
constexpr int GuestEinval = 22;
constexpr int GuestWnohang = 1;
constexpr std::uint32_t MutexType = 0x201;

void Require(bool condition, const char* message) {
    if (!condition) {
        std::fprintf(stderr, "KERNEL WEB FORWARDS: %s\n", message);
        std::abort();
    }
}

void RequireFailure(int result, int error, const char* message) {
    Require(result == -1 && *__error_nid_postfix() == error, message);
}

void TestDup2() {
    RequireFailure(_dup2_nid_disambig1_nid_postfix(-1, 0), GuestEbadf, "_dup2(-1, 0) did not fail with EBADF");
    RequireFailure(_dup2_nid_disambig1_nid_postfix(1000000, 1000001), GuestEbadf, "_dup2 on unopened descriptors did not fail with EBADF");

    int first[2]{-1, -1};
    int second[2]{-1, -1};
    Require(pipe_nid_postfix(first) == 0 && pipe_nid_postfix(second) == 0, "pipe creation failed");
    Require(_dup2_nid_disambig1_nid_postfix(first[0], first[0]) == first[0], "_dup2 to the same descriptor failed");
    Require(_dup2_nid_disambig1_nid_postfix(first[1], second[1]) == second[1], "_dup2 replacing a descriptor failed");
    Require(close_nid_postfix(first[1]) == 0, "closing the original write end failed");

    const char message[] = "kernel_web_dup2";
    const auto length = static_cast<std::int64_t>(sizeof(message));
    Require(write_nid_postfix(second[1], message, length) == length, "write through the duplicate failed");
    char buffer[32]{};
    Require(read_nid_postfix(first[0], buffer, sizeof(buffer)) == length, "read from the original read end failed");
    Require(std::memcmp(buffer, message, sizeof(message)) == 0, "payload mismatch");

    Require(close_nid_postfix(second[1]) == 0 && close_nid_postfix(second[0]) == 0 && close_nid_postfix(first[0]) == 0,
            "closing the pipes failed");
}

void TestExec() {
    char argument[] = "kernel_web_missing.bin";
    char* argv[] = {argument, nullptr};
    char* const emptyArgv[] = {nullptr};
    RequireFailure(_execve_nid_disambig1_nid_postfix("", nullptr, nullptr), GuestEfault, "_execve with a null argv did not fail with EFAULT");
    RequireFailure(_execve_nid_disambig1_nid_postfix("", emptyArgv, nullptr), GuestEnoent, "_execve with an empty path did not fail with ENOENT");
    RequireFailure(_execve_nid_disambig1_nid_postfix("kernel_web_missing.bin", argv, nullptr), GuestEnoent,
                   "_execve of a missing file did not fail with ENOENT");
    RequireFailure(_execvpe_nid_disambig1_nid_postfix(nullptr, argv, nullptr), GuestEfault, "_execvpe with a null name did not fail with EFAULT");
    RequireFailure(_execvpe_nid_disambig1_nid_postfix("", argv, nullptr), GuestEnoent, "_execvpe with an empty name did not fail with ENOENT");
    RequireFailure(_execvpe_nid_disambig1_nid_postfix("kernel_web_absent.bin", argv, nullptr), GuestEnoent,
                   "_execvpe of a missing program did not fail with ENOENT");
}

void TestSocketClose() {
    RequireFailure(__sys_socketclose_nid_disambig1_nid_postfix(-1), GuestEbadf,
                   "__sys_socketclose(-1) did not fail with EBADF");
    RequireFailure(__sys_socketclose_nid_postfix(-1), GuestEbadf,
                   "libkernel_sys __sys_socketclose(-1) did not fail with EBADF");

    int pipeFds[2]{-1, -1};
    Require(pipe_nid_postfix(pipeFds) == 0, "pipe creation failed");
    Require(__sys_socketclose_nid_disambig1_nid_postfix(pipeFds[0]) == 0,
            "__sys_socketclose failed to close descriptor");
    RequireFailure(__sys_socketclose_nid_disambig1_nid_postfix(pipeFds[0]), GuestEbadf,
                   "__sys_socketclose closed descriptor twice");
    Require(__sys_socketclose_nid_postfix(pipeFds[1]) == 0,
            "libkernel_sys __sys_socketclose failed to close descriptor");
    RequireFailure(__sys_socketclose_nid_postfix(pipeFds[1]), GuestEbadf,
                   "libkernel_sys __sys_socketclose closed descriptor twice");
}

void TestDevelopmentMode() {
    Require(__sys_is_development_mode_nid_disambig1_nid_postfix() == 0, "__sys_is_development_mode did not report development mode off");
    Require(__sys_is_development_mode_nid_disambig1_nid_postfix() == __sys_is_development_mode_nid_postfix(),
            "__sys_is_development_mode differs from libkernel_sys");
}

void TestGetGPI() {
    sceKernelSetGPO(0);
    Require(__sceKernelGetGPI_nid_postfix() == 0, "__sceKernelGetGPI in libkernel_sys did not return 0");
    Require(__sceKernelGetGPI_nid_disambig1_nid_postfix() == 0, "__sceKernelGetGPI in libkernel_web did not return 0");

    constexpr std::uint32_t Pattern = 0xA5A5A5A5;
    sceKernelSetGPO(Pattern);
    Require(sceKernelGetGPI() == Pattern, "sceKernelGetGPI did not reflect sceKernelSetGPO");
    Require(__sceKernelGetGPI_nid_postfix() == Pattern, "__sceKernelGetGPI in libkernel_sys did not reflect sceKernelSetGPO");
    Require(__sceKernelGetGPI_nid_disambig1_nid_postfix() == Pattern, "__sceKernelGetGPI in libkernel_web did not reflect sceKernelSetGPO");

    sceKernelSetGPO(0);
}

void TestNamedObject() {
    int object = 0;
    RequireFailure(__sys_namedobj_create_nid_disambig1_nid_postfix(nullptr, &object, MutexType), GuestEinval,
                   "__sys_namedobj_create with a null name did not fail with EINVAL");
    RequireFailure(__sys_namedobj_create_nid_disambig1_nid_postfix(reinterpret_cast<const char*>(0x10), &object, MutexType), GuestEfault,
                   "__sys_namedobj_create with an unreadable name did not fail with EFAULT");
    const int id = __sys_namedobj_create_nid_disambig1_nid_postfix("kernel_web_named", &object, MutexType);
    Require(id > 0, "__sys_namedobj_create did not return an id");
    Require(__sys_namedobj_delete_nid_postfix(static_cast<std::uint32_t>(id), MutexType) == 0,
            "libkernel_sys did not delete the id created through libkernel_web");
    RequireFailure(__sys_namedobj_delete_nid_postfix(static_cast<std::uint32_t>(id), MutexType), GuestEsrch,
                   "a deleted id is still live");
    const int reused = __sys_namedobj_create_nid_postfix("kernel_web_named", &object, MutexType);
    Require(reused == id, "libkernel_sys did not reuse the id released through the shared table");
    RequireFailure(__sys_namedobj_delete_nid_disambig1_nid_postfix(static_cast<std::uint32_t>(reused), MutexType + 1), GuestEsrch,
                   "__sys_namedobj_delete with another type did not fail with ESRCH");
    Require(__sys_namedobj_delete_nid_disambig1_nid_postfix(static_cast<std::uint32_t>(reused), MutexType) == 0,
            "libkernel_web did not delete the id created through libkernel_sys");
    RequireFailure(__sys_namedobj_delete_nid_disambig1_nid_postfix(static_cast<std::uint32_t>(reused), MutexType), GuestEsrch,
                   "libkernel_web deleted an id twice");
    RequireFailure(__sys_namedobj_delete_nid_postfix(static_cast<std::uint32_t>(reused), MutexType), GuestEsrch,
                   "libkernel_sys still sees the id deleted through libkernel_web");
}

void TestRandomizedPath() {
    constexpr char Expected[] = "gkbHtXc3Wq";
    constexpr std::size_t ExpectedLength = sizeof(Expected) - 1;

    char full[32]{};
    std::size_t fullLength = sizeof(full);
    Require(__sys_randomized_path_nid_postfix(nullptr, full, &fullLength) == 0, "libkernel_sys randomized-path query failed");
    Require(fullLength == ExpectedLength && std::strcmp(full, Expected) == 0, "libkernel_sys randomized-path output mismatch");

    char shortBuffer[8];
    std::memset(shortBuffer, 'x', sizeof(shortBuffer));
    std::size_t shortLength = 5;
    Require(__sys_randomized_path_nid_disambig1_nid_postfix(nullptr, shortBuffer, &shortLength) == 0, "libkernel_web short randomized-path query failed");
    Require(shortLength == ExpectedLength && std::memcmp(shortBuffer, "gkbH\0", 5) == 0 && shortBuffer[5] == 'x',
            "libkernel_web short randomized-path result mismatch");

    char one[2] = {'x', 'y'};
    std::size_t oneLength = 1;
    Require(__sys_randomized_path_nid_postfix(nullptr, one, &oneLength) == 0 && oneLength == ExpectedLength && one[0] == '\0' && one[1] == 'y',
            "capacity-one randomized-path query mismatch");

    char zero[2] = {'x', 'y'};
    std::size_t zeroLength = 0;
    Require(__sys_randomized_path_nid_postfix(nullptr, zero, &zeroLength) == 0 && zeroLength == ExpectedLength && zero[0] == 'x' && zero[1] == 'y',
            "capacity-zero randomized-path query wrote the buffer");

    std::size_t untouchedLength = 77;
    Require(__sys_randomized_path_nid_postfix(nullptr, nullptr, &untouchedLength) == 0 && untouchedLength == 77,
            "null randomized-path output changed the length");
    char untouched[4] = {'a', 'b', 'c', '\0'};
    Require(__sys_randomized_path_nid_postfix(nullptr, untouched, nullptr) == 0 && std::strcmp(untouched, "abc") == 0,
            "null randomized-path length changed the output");

    std::size_t badLength = sizeof(full);
    RequireFailure(__sys_randomized_path_nid_postfix(nullptr, reinterpret_cast<char*>(0x10), &badLength), GuestEfault,
                   "unwritable randomized-path output did not fail with EFAULT");
    Require(badLength == ExpectedLength, "randomized-path output length was not written before the buffer fault");
    RequireFailure(__sys_randomized_path_nid_postfix(nullptr, full, reinterpret_cast<std::size_t*>(0x10)), GuestEfault,
                   "unreadable randomized-path length did not fail with EFAULT");

    constexpr std::size_t Page = 0x4000;
    void* const failed = reinterpret_cast<void*>(static_cast<std::uintptr_t>(-1));
    auto* const memory = static_cast<char*>(mmap_nid_postfix(nullptr, Page * 2, 3, 0x1002, -1, 0));
    Require(memory != failed, "randomized-path boundary mapping failed");
    char* const boundary = memory + Page - ExpectedLength;
    std::memset(boundary, 'x', ExpectedLength);
    Require(sceKernelMprotect(memory + Page, Page, 0) == 0, "randomized-path boundary protection failed");
    std::size_t boundaryLength = ExpectedLength + 1;
    RequireFailure(__sys_randomized_path_nid_postfix(nullptr, boundary, &boundaryLength), GuestEfault,
                   "randomized-path terminator crossing an inaccessible page did not fail with EFAULT");
    Require(boundaryLength == ExpectedLength, "randomized-path boundary failure did not preserve length-first output");
    Require(std::memcmp(boundary, "xxxxxxxxxx", ExpectedLength) == 0,
            "randomized-path boundary failure partially modified the unterminated output");
    Require(sceKernelMprotect(memory + Page, Page, 3) == 0, "randomized-path boundary protection restore failed");
    Require(munmap_nid_postfix(memory, Page * 2) == 0, "randomized-path boundary mapping release failed");
    bool threw = false;
    try {
        __sys_randomized_path_nid_postfix("replacement", full, &fullLength);
    } catch (const std::runtime_error&) {
        threw = true;
    }
    Require(threw, "setting the randomized path did not stay unimplemented");
}

void TestWait4() {
    int status = -1;
    RequireFailure(_wait4_nid_disambig1_nid_postfix(-1, &status, GuestWnohang, nullptr), GuestEchild,
                   "_wait4 without children did not fail with ECHILD");
    RequireFailure(_wait4_nid_postfix(-1, &status, GuestWnohang, nullptr), GuestEchild, "libkernel_sys _wait4 did not fail with ECHILD");
    RequireFailure(_wait4_nid_disambig1_nid_postfix(-1, &status, 0x100, nullptr), GuestEinval,
                   "_wait4 with an unknown option did not fail with EINVAL");
    Require(status == -1, "_wait4 wrote a status on failure");
}

void TestRdup() {
    RequireFailure(__sys_rdup_nid_disambig1_nid_postfix(0, -1), GuestEbadf,
                   "__sys_rdup(-1) did not fail with EBADF");
    RequireFailure(__sys_rdup_nid_disambig1_nid_postfix(getpid_nid_postfix(), -1), GuestEbadf,
                   "__sys_rdup(own_pid, -1) did not fail with EBADF");
    RequireFailure(__sys_rdup_nid_disambig1_nid_postfix(0, 1000000), GuestEbadf,
                   "__sys_rdup on unopened descriptor did not fail with EBADF");
    bool threw = false;
    try {
        __sys_rdup_nid_disambig1_nid_postfix(1234567, 0);
    } catch (...) {
        threw = true;
    }
    Require(threw, "__sys_rdup for another process did not throw");

    int pipeFds[2]{-1, -1};
    Require(pipe_nid_postfix(pipeFds) == 0, "pipe creation failed");
    const int copy = __sys_rdup_nid_disambig1_nid_postfix(0, pipeFds[0]);
    Require(copy >= 0 && copy != pipeFds[0], "__sys_rdup failed to duplicate");

    const char msg[] = "web_rdup";
    const auto len = static_cast<std::int64_t>(sizeof(msg));
    Require(write_nid_postfix(pipeFds[1], msg, len) == len, "write through pipe write-end failed");
    char buf[16]{};
    Require(read_nid_postfix(copy, buf, sizeof(buf)) == len, "read through rdup duplicate failed");
    Require(std::memcmp(buf, msg, sizeof(msg)) == 0, "payload mismatch");

    Require(close_nid_postfix(copy) == 0, "close on duplicate failed");
    Require(close_nid_postfix(pipeFds[0]) == 0, "close on pipe 0 failed");
    Require(close_nid_postfix(pipeFds[1]) == 0, "close on pipe 1 failed");
}

} // namespace

int main() {
    TestDup2();
    TestExec();
    TestSocketClose();
    TestDevelopmentMode();
    TestGetGPI();
    TestNamedObject();
    TestRandomizedPath();
    TestWait4();
    TestRdup();
    return 0;
}
