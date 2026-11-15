#include "prx/libc/include/GuestHeap.hpp"
#include "prx/libc/include/GuestArena.hpp"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <exception>
#include <thread>
#include <sys/wait.h>
#include <unistd.h>

namespace {

int events[2];
void* retained;

void CheckHeapDuringExit() {
    bool passed = false;
    try {
        std::thread worker([&] {
            try {
                const auto* bytes = static_cast<const unsigned char*>(retained);
                for (std::size_t index = 0; index < 32; ++index) {
                    if (bytes[index] != 0x5a) return;
                }
                if (GuestHeap::GuestHeapUsableSize_nid_postfix(retained) != 32) return;
                for (std::size_t iteration = 0; iteration < 256; ++iteration) {
                    void* pointer = GuestHeap::GuestHeapAllocate_nid_postfix(32);
                    if (GuestHeap::GuestHeapUsableSize_nid_postfix(pointer) != 32) return;
                    std::memset(pointer, 0xa5, 32);
                    GuestHeap::GuestHeapFree_nid_postfix(pointer);
                }
                GuestHeap::GuestHeapFree_nid_postfix(retained);
                passed = true;
            } catch (const std::exception& error) {
                std::fprintf(stderr, "guest heap during exit: %s\n", error.what());
            } catch (...) {}
        });
        worker.join();
    } catch (...) {}
    const char result = passed ? 'P' : 'F';
    static_cast<void>(!write(events[1], &result, 1));
}

bool HeapSurvivesExit() {
    if (pipe(events) != 0) return false;
    const pid_t child = fork();
    if (child < 0) {
        close(events[0]);
        close(events[1]);
        return false;
    }
    if (child == 0) {
        close(events[0]);
        if (std::atexit(CheckHeapDuringExit) != 0) std::_Exit(2);
        retained = GuestHeap::GuestHeapAllocate_nid_postfix(32);
        if (!GuestArena::GuestArenaContains_nid_postfix(retained, 32)) std::_Exit(2);
        std::memset(retained, 0x5a, 32);
        std::exit(0);
    }
    close(events[1]);
    char result = 0;
    const ssize_t count = read(events[0], &result, 1);
    close(events[0]);
    int status = 0;
    return waitpid(child, &status, 0) == child && WIFEXITED(status) &&
        WEXITSTATUS(status) == 0 && count == 1 && result == 'P';
}

}

int main() {
    if (!HeapSurvivesExit()) {
        std::fputs("guest small-block heap must remain available during static finalization\n", stderr);
        return 1;
    }
    return 0;
}
