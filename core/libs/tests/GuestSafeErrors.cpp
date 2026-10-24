#include "prx/libc/include/general/VabiMacros.hpp"
#include <array>
#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <string>
#include <thread>

extern "C" {
int APS5_VABI strerror_s_nid_postfix(char*, std::size_t, int);
std::size_t APS5_VABI strerrorlen_s_nid_postfix(int);
int APS5_VABI strerror_r_nid_postfix(int, char*, std::size_t);
int* APS5_VABI __error_nid_postfix();
void* APS5_VABI set_constraint_handler_s_nid_postfix(void*);
void APS5_VABI ignore_handler_s_nid_postfix(const char*, void*, int);
}

namespace {

void Require(bool condition, const char* message) {
    if (!condition) {
        std::fprintf(stderr, "safe error messages: %s\n", message);
        std::abort();
    }
}

thread_local int violations = 0;
thread_local void* violationBuffer = nullptr;

void APS5_VABI Constraint(const char* message, void* buffer, int error) {
    Require(message && std::strcmp(message, "strerror_s") == 0, "constraint callback message and ABI");
    Require(error == 22, "constraint callback error");
    violationBuffer = buffer;
    ++violations;
    auto previous = set_constraint_handler_s_nid_postfix(reinterpret_cast<void*>(Constraint));
    Require(previous == reinterpret_cast<void*>(Constraint), "handler is shared and callable without its lock");
    char nested[64];
    Require(strerror_s_nid_postfix(nested, sizeof(nested), 2) == 0, "handler can reenter valid conversion");
}

void CheckMessage(int error, const char* expected) {
    const std::string full(expected);
    *__error_nid_postfix() = 1234;
    Require(strerrorlen_s_nid_postfix(error) == full.size(), "full message length excludes null");
    for (std::size_t capacity = 1; capacity <= full.size() + 2; ++capacity) {
        std::array<char, 160> buffer;
        buffer.fill('#');
        const auto result = strerror_s_nid_postfix(buffer.data() + 1, capacity, error);
        std::string wanted = full;
        if (capacity <= full.size()) {
            wanted.resize(capacity - 1);
            if (capacity > 3) wanted.replace(capacity - 4, 3, "...");
        }
        Require(result == (capacity <= full.size() ? 34 : 0), "complete versus truncated return");
        Require(std::strcmp(buffer.data() + 1, wanted.c_str()) == 0, "truncated text, ellipsis and terminator");
        Require(buffer[0] == '#', "leading canary");
        for (std::size_t index = wanted.size() + 2; index < buffer.size(); ++index)
            Require(buffer[index] == '#', "bytes after the terminator remain untouched");
        Require(*__error_nid_postfix() == 1234, "preserve guest errno");
    }
}

void CheckMessages() {
    CheckMessage(0, "No error: 0");
    CheckMessage(2, "No such file or directory");
    CheckMessage(11, "Resource deadlock avoided");
    CheckMessage(35, "Resource temporarily unavailable");
    CheckMessage(96, "Previous owner died");
    CheckMessage(-1, "Unknown error: -1");
    CheckMessage(97, "Unknown error: 97");
    CheckMessage(std::numeric_limits<int>::min(), "Unknown error: -2147483648");
    CheckMessage(std::numeric_limits<int>::max(), "Unknown error: 2147483647");
    for (int error = 0; error < 97; ++error) {
        char expected[128];
        Require(strerror_r_nid_postfix(error, expected, sizeof(expected)) == 0, "existing guest error table");
        CheckMessage(error, expected);
    }
    Require(violations == 0, "truncation is not a constraint violation");
}

void CheckConstraints() {
    std::array<char, 16> buffer;
    buffer.fill('!');
    constexpr auto maximum = std::numeric_limits<std::size_t>::max();
    for (const auto capacity : {std::size_t{0}, (maximum >> 1) + 1, maximum}) {
        const int before = violations;
        Require(strerror_s_nid_postfix(buffer.data(), capacity, 2) == 22, "reject invalid capacity");
        Require(violations == before + 1 && violationBuffer == buffer.data(), "one callback with destination");
        for (const char value : buffer) Require(value == '!', "constraint failure leaves destination untouched");
    }
    for (const auto capacity : {std::size_t{0}, std::size_t{1}, maximum}) {
        const int before = violations;
        Require(strerror_s_nid_postfix(nullptr, capacity, 2) == 22, "reject null destination");
        Require(violations == before + 1 && violationBuffer == nullptr, "one callback for null destination");
    }
    set_constraint_handler_s_nid_postfix(reinterpret_cast<void*>(ignore_handler_s_nid_postfix));
    Require(strerror_s_nid_postfix(nullptr, 1, 2) == 22, "installed ignore handler returns to caller");
    set_constraint_handler_s_nid_postfix(reinterpret_cast<void*>(Constraint));
}

void CheckThreads() {
    const auto worker = [](int error, const char* message) {
        for (int iteration = 0; iteration < 100; ++iteration) {
            CheckMessage(error, message);
            Require(strerror_s_nid_postfix(nullptr, 1, error) == 22, "thread constraint callback");
        }
        Require(violations == 100, "callbacks execute in the calling thread");
    };
    std::thread first(worker, 35, "Resource temporarily unavailable");
    std::thread second(worker, -1, "Unknown error: -1");
    first.join();
    second.join();
}

}

int main() {
    auto previous = set_constraint_handler_s_nid_postfix(reinterpret_cast<void*>(Constraint));
    CheckMessages();
    CheckConstraints();
    CheckThreads();
    set_constraint_handler_s_nid_postfix(previous);
    std::puts("safe error message checks passed");
}
