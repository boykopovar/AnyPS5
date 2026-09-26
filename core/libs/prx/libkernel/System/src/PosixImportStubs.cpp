#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"
#include <cstddef>
#include <cstdint>

struct GuestSignalSet;
struct GuestResourceUsage;

extern "C" {

int APS5_VABI _close_nid_postfix(int descriptor) {
    (void)descriptor;
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

void APS5_VABI _exit_nid_postfix(int status) {
    (void)status;
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _nanosleep_nid_postfix(const KernelTimespec* requested, KernelTimespec* remaining) {
    (void)requested;
    (void)remaining;
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI _open_nid_postfix(const char* path, int flags, ...) {
    (void)path;
    (void)flags;
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

std::int64_t APS5_VABI _read_nid_postfix(int descriptor, void* buffer, std::size_t count) {
    (void)descriptor;
    (void)buffer;
    (void)count;
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI _sigprocmask_nid_postfix(int how, const GuestSignalSet* set, GuestSignalSet* previousSet) {
    (void)how;
    (void)set;
    (void)previousSet;
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

std::int64_t APS5_VABI _write_nid_postfix(int descriptor, const void* buffer, std::size_t count) {
    (void)descriptor;
    (void)buffer;
    (void)count;
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI getrusage_nid_postfix(int who, GuestResourceUsage* usage) {
    (void)who;
    (void)usage;
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI pthread_rwlock_rdlock_nid_postfix(PthreadRwlock* rwlock) {
    (void)rwlock;
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI pthread_rwlock_unlock_nid_postfix(PthreadRwlock* rwlock) {
    (void)rwlock;
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI rmdir_nid_postfix(const char* path) {
    (void)path;
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI unlink_nid_postfix(const char* path) {
    (void)path;
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

}
