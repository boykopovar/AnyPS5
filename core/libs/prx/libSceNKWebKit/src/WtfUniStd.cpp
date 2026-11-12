#include "prx/libc/include/general/VabiMacros.hpp"
#include "prx/libc/include/GuestErrno.hpp"
#include "prx/libSceNKWebKit/include/WtfExports.hpp"
#include "prx/libkernel/include/Api.hpp"

#include <algorithm>
#include <chrono>
#include <thread>

namespace {

constexpr int GuestGetDescriptorFlags = 1;
constexpr int GuestSetDescriptorFlags = 2;
constexpr int GuestGetStatusFlags = 3;
constexpr int GuestSetStatusFlags = 4;
constexpr int GuestDuplicateCloseOnExec = 17;
constexpr int GuestCloseOnExec = 1;
constexpr int GuestNonBlock = 4;
constexpr int GuestEintr = 4;
constexpr double LongestSleepSeconds = 3600.0;

bool Interrupted(int result) {
    return result == -1 && *__error_nid_postfix() == GuestEintr;
}

bool SetCloseOnExec(int descriptor) {
    int result = -1;
    do {
        const int flags = fcntl_nid_postfix(descriptor, GuestGetDescriptorFlags);
        if (flags != -1) result = fcntl_nid_postfix(descriptor, GuestSetDescriptorFlags, flags | GuestCloseOnExec);
    } while (Interrupted(result));
    return result != -1;
}

void CloseWithRetry(int descriptor) {
    while (Interrupted(close_nid_postfix(descriptor))) {
    }
}

}  // namespace

extern "C" {

int APS5_VABI _ZN3WTF14dupCloseOnExecEi(int fileDescriptor) {
    int duplicate = -1;
    while (Interrupted(duplicate = fcntl_nid_postfix(fileDescriptor, GuestDuplicateCloseOnExec, 0))) {
    }
    if (duplicate != -1) return duplicate;
    while (Interrupted(duplicate = dup_nid_postfix(fileDescriptor))) {
    }
    if (duplicate == -1) return -1;
    if (!SetCloseOnExec(duplicate)) {
        CloseWithRetry(duplicate);
        return -1;
    }
    return duplicate;
}

bool APS5_VABI _ZN3WTF11setNonBlockEi(int fileDescriptor) {
    const int flags = fcntl_nid_postfix(fileDescriptor, GuestGetStatusFlags, 0);
    int result = -1;
    while (Interrupted(result = fcntl_nid_postfix(fileDescriptor, GuestSetStatusFlags, flags | GuestNonBlock))) {
    }
    return result != -1;
}

void APS5_VABI _ZN3WTF5sleepENS_7SecondsE(WTF::Seconds value) {
    const double deadline = _ZN3WTF13MonotonicTime3nowEv().m_value + value.m_value;
    for (double now = _ZN3WTF13MonotonicTime3nowEv().m_value; now < deadline; now = _ZN3WTF13MonotonicTime3nowEv().m_value) {
        std::this_thread::sleep_for(std::chrono::duration<double>(std::min(deadline - now, LongestSleepSeconds)));
    }
}

}
