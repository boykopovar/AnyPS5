#include "prx/libkernel/include/Api.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include <cstdio>
#include <cstring>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>

namespace {

constexpr int GuestGetFlags = 3;

int Child(const char* closed) {
    struct stat null{};
    if (::stat("/dev/null", &null) != 0) return 70;
    for (int descriptor = 0; descriptor <= 2; ++descriptor) {
        struct stat status{};
        if (::fstat(descriptor, &status) != 0) return 71 + descriptor;
        if (closed[descriptor] != '1') continue;
        if (!S_ISCHR(status.st_mode) || status.st_rdev != null.st_rdev) return 74 + descriptor;
        if ((::fcntl(descriptor, F_GETFL) & O_ACCMODE) != (descriptor == 0 ? O_RDONLY : O_WRONLY)) return 77 + descriptor;
    }
    if (closed[0] == '1' && fcntl_nid_postfix(0, GuestGetFlags) < 0) return 80;
    return 0;
}

int RunClosed(const char* self, bool closeInput, bool closeOutput, bool closeError) {
    const pid_t child = ::fork();
    if (child < 0) return -1;
    if (child == 0) {
        if (closeInput) ::close(0);
        if (closeOutput) ::close(1);
        if (closeError) ::close(2);
        char closed[] = {closeInput ? '1' : '0', closeOutput ? '1' : '0', closeError ? '1' : '0', '\0'};
        char* const arguments[] = {const_cast<char*>(self), const_cast<char*>("--closed-child"), closed, nullptr};
        ::execv(self, arguments);
        ::_exit(69);
    }
    int status = 0;
    if (::waitpid(child, &status, 0) != child) return -1;
    return WIFEXITED(status) ? WEXITSTATUS(status) : 128 + WTERMSIG(status);
}

}

int main(int argc, char** argv) {
    if (argc > 2 && std::strcmp(argv[1], "--closed-child") == 0 && std::strlen(argv[2]) == 3) return Child(argv[2]);
    const struct { bool input, output, error; const char* name; } cases[] = {
        {true, false, false, "stdin"},
        {false, true, false, "stdout"},
        {true, true, true, "all three"},
    };
    for (const auto& test : cases) {
        const int result = RunClosed(argv[0], test.input, test.output, test.error);
        if (result != 0) {
            std::fprintf(stderr, "closed standard descriptors, %s: child exited with %d\n", test.name, result);
            return 1;
        }
    }
    std::puts("closed standard descriptors are reopened on /dev/null");
    return 0;
}
