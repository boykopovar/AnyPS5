#include "prx/libc/include/Api.hpp"
#include "prx/libkernel/KernelErrors.hpp"
#include "prx/libkernel/include/Api.hpp"
#include "prx/libkernel/Semaphore/include/Semaphore.hpp"
#include "SceTypes.hpp"
#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace {

constexpr int SceOk = 0;

void Fail(const char* what) {
    std::fprintf(stderr, "FAIL %s\n", what);
    std::exit(1);
}

void Require(bool value, const char* what) {
    if (!value) Fail(what);
}

KernelSema CreateDeleted(const char* name) {
    KernelSema sem = nullptr;
    Require(sceKernelCreateSema(&sem, name, 0, 1, 4, nullptr) == SceOk, "create");
    Require(sem != nullptr, "create returned no handle");
    Require(sceKernelPollSema(sem, 1) == SceOk, "fixture poll on the live semaphore");
    Require(sceKernelSignalSema(sem, 1) == SceOk, "fixture signal on the live semaphore");
    Require(sceKernelDeleteSema(sem) == SceOk, "delete");
    return sem;
}

int Report(const char* name, int expected, int observed) {
    std::printf("case=%s expected=%#x observed=%#x result=%s\n", name, expected, observed, observed == expected ? "match" : "mismatch");
    return observed == expected ? 0 : 1;
}

int PollAfterDelete() {
    const KernelSema sem = CreateDeleted("deleted_poll");
    const int observed = sceKernelPollSema(sem, 1);
    return Report("poll-after-delete", SCE_KERNEL_ERROR_ESRCH, observed);
}

int SignalAfterDelete() {
    const KernelSema sem = CreateDeleted("deleted_signal");
    const int observed = sceKernelSignalSema(sem, 1);
    return Report("signal-after-delete", SCE_KERNEL_ERROR_ESRCH, observed);
}

int DeleteTwice() {
    const KernelSema sem = CreateDeleted("deleted_delete");
    const int observed = sceKernelDeleteSema(sem);
    return Report("delete-twice", SCE_KERNEL_ERROR_ESRCH, observed);
}

int WaitAfterDelete() {
    const KernelSema sem = CreateDeleted("deleted_wait");
    KernelUseconds remaining = 0;
    const int observed = sceKernelWaitSema(sem, 1, &remaining);
    std::printf("case=wait-after-delete contract=unmeasured observed=%#x remaining=%u\n", observed, static_cast<unsigned>(remaining));
    return 0;
}

struct Case {
    const char* name;
    int (*run)();
};

constexpr Case Cases[] = {
    {"poll-after-delete", PollAfterDelete},
    {"signal-after-delete", SignalAfterDelete},
    {"delete-twice", DeleteTwice},
    {"wait-after-delete", WaitAfterDelete},
};

void List() {
    for (const auto& entry : Cases) std::printf("%s\n", entry.name);
}

int Run(const char* name) {
    for (const auto& entry : Cases) {
        if (std::strcmp(entry.name, name) == 0) return entry.run();
    }
    std::fprintf(stderr, "unknown case %s\n", name);
    return 2;
}

}

int main(int argc, char** argv) {
    if (argc == 1) {
        List();
        return 2;
    }
    if (std::strcmp(argv[1], "--list") == 0) {
        List();
        return 0;
    }
    if (std::strcmp(argv[1], "--case") == 0) {
        if (argc != 3) {
            std::fprintf(stderr, "--case needs one case name\n");
            return 2;
        }
        return Run(argv[2]);
    }
    return Run(argv[1]);
}
