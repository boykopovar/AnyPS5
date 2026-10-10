#include "prx/libc/include/Shutdown.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include <cstdio>
#include <cstdlib>
#include <sys/wait.h>
#include <unistd.h>

extern "C" int APS5_VABI __cxa_atexit_nid_postfix(void (APS5_VABI *func)(void*), void* arg, void* dsoHandle);

namespace {

int events[2];

void Record(char value) {
    static_cast<void>(!write(events[1], &value, 1));
}

void APS5_VABI GuestHandler(void*) { Record('G'); }

void HostHandler() { Record('H'); }

bool GuestHandlersRunFirst() {
    if (pipe(events) != 0) return false;
    const pid_t child = fork();
    if (child < 0) return false;
    if (child == 0) {
        close(events[0]);
        __cxa_atexit_nid_postfix(GuestHandler, nullptr, nullptr);
        std::atexit(HostHandler);
        LibcExit_nid_no_patch(0);
    }
    close(events[1]);
    char order[3] = {};
    ssize_t length = 0;
    while (length < 2) {
        const ssize_t count = read(events[0], order + length, 2 - length);
        if (count <= 0) break;
        length += count;
    }
    close(events[0]);
    int status = 0;
    return waitpid(child, &status, 0) == child && WIFEXITED(status) && WEXITSTATUS(status) == 0 && length == 2 && order[0] == 'G' && order[1] == 'H';
}

}

int main() {
    if (!GuestHandlersRunFirst()) {
        std::fputs("guest exit handlers must run before the host exit handlers\n", stderr);
        return 1;
    }
    return 0;
}
