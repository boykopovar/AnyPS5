#include "prx/libc/include/Shutdown.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"

#include <Testing/Test.hpp>

#include <cstdlib>
#include <string>
#include <sys/wait.h>
#include <unistd.h>

extern "C" int APS5_VABI __cxa_atexit_nid_postfix(void (APS5_VABI *func)(void*), void* arg, void* dsoHandle);

namespace {

using Testing::Case;
using Testing::Require;
using Testing::RequireEqual;

int events[2];

void Record(char value) {
    static_cast<void>(!write(events[1], &value, 1));
}

void APS5_VABI GuestHandler(void*) { Record('G'); }

void HostHandler() { Record('H'); }

class EventPipe {
public:
    EventPipe() {
        Require(pipe(events) == 0, "create the event pipe");
    }

    ~EventPipe() {
        if (events[0] >= 0) close(events[0]);
        if (events[1] >= 0) close(events[1]);
    }

    EventPipe(const EventPipe&) = delete;
    EventPipe& operator=(const EventPipe&) = delete;

    void CloseWriter() {
        close(events[1]);
        events[1] = -1;
    }

    std::string ReadAll() {
        char order[3] = {};
        ssize_t length = 0;
        while (length < 2) {
            const ssize_t count = read(events[0], order + length, 2 - length);
            if (count <= 0) break;
            length += count;
        }
        return std::string(order, static_cast<std::size_t>(length));
    }
};

const Case exitOrder{"LibcExit_GuestAndHostHandlers_RunsGuestHandlersFirst", [] {
    EventPipe pipe;
    const pid_t child = fork();
    Require(child >= 0, "fork");
    if (child == 0) {
        close(events[0]);
        __cxa_atexit_nid_postfix(GuestHandler, nullptr, nullptr);
        std::atexit(HostHandler);
        LibcExit_nid_no_patch(0);
    }
    pipe.CloseWriter();
    const std::string order = pipe.ReadAll();
    int status = 0;
    RequireEqual(waitpid(child, &status, 0), child, "wait for the child");
    Require(WIFEXITED(status), "child exits normally");
    RequireEqual(WEXITSTATUS(status), 0, "child exit status");
    RequireEqual(order, std::string("GH"), "guest exit handlers run before host exit handlers");
}};

} // namespace
