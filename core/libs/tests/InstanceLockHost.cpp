#include "prx/libkernel/System/include/InstanceLockHost.hpp"
#include <cerrno>
#include <filesystem>
#include <iostream>
#include <stdexcept>
#include <string>
#ifdef _WIN32
#include <windows.h>
#else
#include <csignal>
#include <fcntl.h>
#include <sys/wait.h>
#include <unistd.h>
#endif

static void Require(bool condition, const std::string& message) {
    if (!condition) throw std::runtime_error(message);
}

namespace {

class ScratchDirectory {
public:
    ScratchDirectory() {
#ifdef _WIN32
        const auto id = std::to_string(GetCurrentProcessId());
#else
        const auto id = std::to_string(getpid());
#endif
        path = std::filesystem::temp_directory_path() / ("anyps5-instance-lock-host-" + id);
        std::filesystem::remove_all(path);
        std::filesystem::create_directories(path);
    }
    ~ScratchDirectory() {
        std::error_code error;
        std::filesystem::remove_all(path, error);
    }
    std::filesystem::path path;
};

}

static void CheckSecondHandleIsRefused() {
    ScratchDirectory scratch;
    const auto file = InstanceLockPath(scratch.path);
    auto first = MakeHostInstanceLockBackend();
    auto second = MakeHostInstanceLockBackend();
    Require(first->TryLock(file), "the first lock must be taken");
    Require(std::filesystem::exists(file), "the lock file must exist while held");
    Require(!second->TryLock(file), "a second handle must be refused while the first is held");
    Require(!second->TryLock(file), "a refused handle must be refused again");
}

static void CheckReleaseOnDestruction() {
    ScratchDirectory scratch;
    const auto file = InstanceLockPath(scratch.path);
    auto first = MakeHostInstanceLockBackend();
    Require(first->TryLock(file), "the first lock must be taken");
    first.reset();
    auto second = MakeHostInstanceLockBackend();
    Require(second->TryLock(file), "the lock must be free once its holder is gone, with the stale file left in place");
}

static void CheckRootsAreIndependent() {
    ScratchDirectory scratch;
    const auto left = scratch.path / "left";
    const auto right = scratch.path / "right";
    std::filesystem::create_directories(left);
    std::filesystem::create_directories(right);
    auto first = MakeHostInstanceLockBackend();
    auto second = MakeHostInstanceLockBackend();
    Require(first->TryLock(InstanceLockPath(left)), "the left root must lock");
    Require(second->TryLock(InstanceLockPath(right)), "a different root must not conflict");
}

static void CheckUnwritableRootThrows() {
    ScratchDirectory scratch;
    const auto file = InstanceLockPath(scratch.path / "missing");
    auto backend = MakeHostInstanceLockBackend();
    bool thrown = false;
    try {
        backend->TryLock(file);
    } catch (const std::runtime_error& error) {
        const std::string message = error.what();
        thrown = true;
        Require(message.find(file.string()) != std::string::npos, "the error must name the lock file: " + message);
        Require(message.find("ANYPS5_INSTANCE_LOCK=off") != std::string::npos, "the error must name the variable: " + message);
    }
    Require(thrown, "a lock file that cannot be created must throw, not pass silently");
}

static void CheckLockedTwiceThrows() {
    ScratchDirectory scratch;
    const auto file = InstanceLockPath(scratch.path);
    auto backend = MakeHostInstanceLockBackend();
    Require(backend->TryLock(file), "the lock must be taken");
    bool thrown = false;
    try {
        backend->TryLock(file);
    } catch (const std::logic_error&) {
        thrown = true;
    }
    Require(thrown, "locking twice with one backend must throw");
}

#ifndef _WIN32
static void CheckExecDoesNotInherit() {
    ScratchDirectory scratch;
    const auto file = InstanceLockPath(scratch.path);
    auto first = MakeHostInstanceLockBackend();
    Require(first->TryLock(file), "the lock must be taken");
    int ready[2];
    Require(pipe2(ready, O_CLOEXEC) == 0, "pipe2 failed");
    const pid_t child = fork();
    Require(child >= 0, "fork failed");
    if (child == 0) {
        execl("/bin/sleep", "sleep", "60", static_cast<char*>(nullptr));
        _exit(127);
    }
    close(ready[1]);
    char byte;
    while (read(ready[0], &byte, 1) < 0 && errno == EINTR) {
    }
    close(ready[0]);
    first.reset();
    auto second = MakeHostInstanceLockBackend();
    const bool taken = second->TryLock(file);
    kill(child, SIGKILL);
    waitpid(child, nullptr, 0);
    Require(taken, "a program started by exec must not keep the lock alive");
}
#endif

int main() {
    try {
        CheckSecondHandleIsRefused();
        CheckReleaseOnDestruction();
        CheckRootsAreIndependent();
        CheckUnwritableRootThrows();
        CheckLockedTwiceThrows();
#ifndef _WIN32
        CheckExecDoesNotInherit();
#endif
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
    return 0;
}
