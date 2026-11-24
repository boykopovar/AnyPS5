#include "prx/libkernel_web/include/Api.hpp"
#include "prx/libkernel_sys/include/AdditionalExports.hpp"
#include "prx/libkernel/include/Api.hpp"
#include "prx/libc/include/Api.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>

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

void TestWait4() {
    int status = -1;
    RequireFailure(_wait4_nid_disambig1_nid_postfix(-1, &status, GuestWnohang, nullptr), GuestEchild,
                   "_wait4 without children did not fail with ECHILD");
    RequireFailure(_wait4_nid_postfix(-1, &status, GuestWnohang, nullptr), GuestEchild, "libkernel_sys _wait4 did not fail with ECHILD");
    RequireFailure(_wait4_nid_disambig1_nid_postfix(-1, &status, 0x100, nullptr), GuestEinval,
                   "_wait4 with an unknown option did not fail with EINVAL");
    Require(status == -1, "_wait4 wrote a status on failure");
}

} // namespace

int main() {
    TestDup2();
    TestExec();
    TestDevelopmentMode();
    TestGetGPI();
    TestNamedObject();
    TestWait4();
    return 0;
}
