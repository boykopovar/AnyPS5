#include "prx/libc/include/Api.hpp"
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#ifdef _WIN32
#include <io.h>
#else
#include <unistd.h>
#endif

namespace {

void Require(bool condition, const char* message) {
    if (condition) return;
    std::fprintf(stderr, "daemon: %s\n", message);
    std::abort();
}

void TestNoChdirNoClose() {
    char initialDir[1024]{};
    Require(getcwd_nid_postfix(initialDir, sizeof(initialDir)) != nullptr, "initial getcwd");
    Require(daemon_nid_postfix(1, 1) == 0, "daemon(1, 1)");
    char currentDir[1024]{};
    Require(getcwd_nid_postfix(currentDir, sizeof(currentDir)) != nullptr, "post daemon(1, 1) getcwd");
    Require(std::strcmp(initialDir, currentDir) == 0, "working directory unchanged when nochdir is set");
}

void TestChdirNoClose() {
    Require(daemon_nid_postfix(0, 1) == 0, "daemon(0, 1)");
    char currentDir[1024]{};
    Require(getcwd_nid_postfix(currentDir, sizeof(currentDir)) != nullptr, "post daemon(0, 1) getcwd");
    Require(std::strcmp(currentDir, "/") == 0, "working directory is root when nochdir is 0");
}

void TestNoChdirClose() {
    Require(daemon_nid_postfix(1, 0) == 0, "daemon(1, 0)");
    const char message[] = "test output\n";
    char input[16]{};
#ifdef _WIN32
    Require(::_write(1, message, sizeof(message) - 1) == static_cast<int>(sizeof(message) - 1), "write to redirected stdout");
    Require(::_write(2, message, sizeof(message) - 1) == static_cast<int>(sizeof(message) - 1), "write to redirected stderr");
    Require(::_read(0, input, sizeof(input)) == 0, "read from redirected stdin returns eof");
#else
    Require(::write(1, message, sizeof(message) - 1) == static_cast<int>(sizeof(message) - 1), "write to redirected stdout");
    Require(::write(2, message, sizeof(message) - 1) == static_cast<int>(sizeof(message) - 1), "write to redirected stderr");
    Require(::read(0, input, sizeof(input)) == 0, "read from redirected stdin returns eof");
#endif
}

void TestChdirClose() {
    Require(daemon_nid_postfix(0, 0) == 0, "daemon(0, 0)");
    char currentDir[1024]{};
    Require(getcwd_nid_postfix(currentDir, sizeof(currentDir)) != nullptr, "post daemon(0, 0) getcwd");
    Require(std::strcmp(currentDir, "/") == 0, "working directory is root when nochdir is 0 with close");
}

} // namespace

int main() {
    TestNoChdirNoClose();
    TestChdirNoClose();
    TestNoChdirClose();
    TestChdirClose();
    return 0;
}
