#include "prx/libkernel/Pthread/include/Cancel.hpp"

#include <cerrno>
#include <cstdlib>
#include <optional>
#include <stdexcept>
#include <system_error>

#ifdef _WIN32
#include <windows.h>
#else
#include <csignal>
#include <pthread.h>
#endif

namespace ThreadCancel {

struct NativeIo::State {
#ifdef _WIN32
    HANDLE thread = nullptr;
#else
    pthread_t thread = pthread_self();
    sigset_t previous{};
    sigset_t signal{};
#endif
    std::optional<IoWait> wait;

    static void Interrupt(void* argument) noexcept {
        auto& state = *static_cast<State*>(argument);
#ifdef _WIN32
        CancelSynchronousIo(state.thread);
#else
        pthread_kill(state.thread, SIGUSR2);
#endif
    }

    State() {
#ifdef _WIN32
        if (!DuplicateHandle(GetCurrentProcess(), GetCurrentThread(), GetCurrentProcess(), &thread,
            THREAD_TERMINATE, FALSE, 0))
            throw std::system_error(GetLastError(), std::system_category(), "cancellable I/O: retaining thread handle");
#else
        static const bool installed = [] {
            struct sigaction previous{};
            if (sigaction(SIGUSR2, nullptr, &previous) != 0)
                throw std::system_error(errno, std::generic_category(), "cancellable I/O: querying interrupt signal");
            if (previous.sa_handler != SIG_DFL)
                throw std::runtime_error("cancellable I/O: SIGUSR2 is already in use");
            struct sigaction action{};
            action.sa_handler = [](int) {};
            sigemptyset(&action.sa_mask);
            if (sigaction(SIGUSR2, &action, nullptr) != 0)
                throw std::system_error(errno, std::generic_category(), "cancellable I/O: installing interrupt signal");
            return true;
        }();
        static_cast<void>(installed);
        sigemptyset(&signal);
        sigaddset(&signal, SIGUSR2);
        if (const int error = pthread_sigmask(SIG_UNBLOCK, &signal, &previous); error != 0)
            throw std::system_error(error, std::generic_category(), "cancellable I/O: unblocking interrupt signal");
#endif
        try {
            wait.emplace(Interrupt, this);
        } catch (...) {
            release();
            throw;
        }
    }

    ~State() { release(); }

private:
    void release() {
#ifdef _WIN32
        const DWORD error = GetLastError();
#endif
        const int saved = errno;
        wait.reset();
#ifdef _WIN32
        CloseHandle(thread);
#else
        pthread_sigmask(SIG_BLOCK, &signal, nullptr);
        sigset_t pending{};
        while (sigpending(&pending) == 0 && sigismember(&pending, SIGUSR2) == 1) {
            int received = 0;
            sigwait(&signal, &received);
        }
        pthread_sigmask(SIG_SETMASK, &previous, nullptr);
#endif
        errno = saved;
#ifdef _WIN32
        SetLastError(error);
#endif
    }
};

NativeIo::NativeIo() {
    if (IoActive()) state = new State;
}

void NativeIo::InterruptError() {
    errno = EINTR;
#ifdef _WIN32
    _doserrno = ERROR_OPERATION_ABORTED;
    SetLastError(ERROR_OPERATION_ABORTED);
#endif
}

NativeIo::~NativeIo() {
#ifdef _WIN32
    const DWORD error = GetLastError();
#endif
    const int saved = errno;
    delete state;
    errno = saved;
#ifdef _WIN32
    SetLastError(error);
#endif
}

}
