#include "prx/libSceLibcInternal/include/RuntimeInternals.hpp"
#include "prx/libc/include/Api.hpp"

#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace {

int g_calls = 0;
const char* g_message = nullptr;
void* g_pointer = nullptr;
int g_error = 0;

void APS5_VABI TestHandler(const char* message, void* pointer, int error) {
    ++g_calls;
    g_message = message;
    g_pointer = pointer;
    g_error = error;
}

void Require(bool condition, const char* message) {
    if (condition) return;
    std::fprintf(stderr, "libc internal fail_s test failure: %s\n", message);
    std::abort();
}

void TestFailS() {
    void* const previous = set_constraint_handler_s_nid_postfix(reinterpret_cast<void*>(&TestHandler));

    g_calls = 0;
    g_message = nullptr;
    g_pointer = nullptr;
    g_error = 0;

    int dummy = 42;
    _Fail_s_nid_postfix("strcpy_s: buffer overflow", &dummy, 34);
    Require(g_calls == 1, "handler called once");
    Require(g_message != nullptr && std::strcmp(g_message, "strcpy_s: buffer overflow") == 0, "message matches");
    Require(g_pointer == &dummy, "pointer matches");
    Require(g_error == 34, "error code matches");

    _Fail_s_nid_postfix(nullptr, nullptr, 0);
    Require(g_calls == 2, "handler called second time");
    Require(g_message == nullptr, "null message preserved");
    Require(g_pointer == nullptr, "null pointer preserved");
    Require(g_error == 0, "zero error code preserved");

    set_constraint_handler_s_nid_postfix(reinterpret_cast<void*>(&ignore_handler_s_nid_postfix));
    _Fail_s_nid_postfix("ignored violation", &dummy, 22);
    Require(g_calls == 2, "ignore handler does not call custom handler");

    set_constraint_handler_s_nid_postfix(previous);
}

}

int main() {
    TestFailS();
    return 0;
}
