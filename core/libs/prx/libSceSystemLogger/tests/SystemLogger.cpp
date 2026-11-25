#include "prx/libSceSystemLogger/include/Api.hpp"
#include "prx/libc/include/GuestHeap.hpp"
#include "prx/libc/include/Shutdown.hpp"
#include "prx/libkernel/include/Api.hpp"
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <stdexcept>
#include <string>
#include <string_view>
#include <type>
#include <vector>

namespace {

void check(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}

template<typename TAction>
std::string expectFailure(TAction action) {
    try {
        action();
    } catch (const std::runtime_error& error) {
        return error.what();
    }
    throw std::runtime_error("expected exception");
}

void testInitializeTerminate() {
    check(sceSystemLoggerInitialize() == 0, "initialization failed");
    expectFailure([] { sceSystemLoggerInitialize(); });
    check(sceSystemLoggerTerminate() == 0, "termination failed");
    expectFailure([] { sceSystemLoggerTerminate(); });
    check(sceSystemLoggerInitialize() == 0, "reinitialization failed");
    check(sceSystemLoggerTerminate() == 0, "second termination failed");
}

void testInvalidArguments() {
    expectFailure([] { sceSystemLoggerSetLogData(nullptr, 0, nullptr); });
    expectFailure([] { sceSystemLoggerSetLogData("", 1, nullptr); });
    expectFailure([] { sceSystemLoggerSetLogData("ABC", 1, nullptr); });
    expectFailure([] { sceSystemLoggerSetLogData("ABC", 0, nullptr); });
    expectFailure([] { sceSystemLoggerSetLogData("ABC", 0, reinterpret_cast<const SystemLoggerLogData*>(1)); });
    expectFailure([] { sceSystemLoggerSetLogData("ABC", 1, reinterpret_cast<const SystemLoggerLogData*>(1)); });
    expectFailure([] { sceSystemLoggerSetLogData("ABC", 0x21, nullptr); });
    expectFailure([] { sceSystemLoggerSetLogData("ABC", 0x21, reinterpret_cast<const SystemLoggerLogData*>(1)); });
    expectFailure([] { sceSystemLoggerSetLogData("ABC", 0x21, reinterpret_cast<const SystemLoggerLogData*>(0x100000000u)); });
}

void testTagValidation() {
    expectFailure([] { sceSystemLoggerSetLogData("", 0, nullptr); });
    expectFailure([] { sceSystemLoggerSetLogData(" ", 0, nullptr); });
    expectFailure([] { sceSystemLoggerSetLogData("!@#", 0, nullptr); });
    expectFailure([] { sceSystemLoggerSetLogData("ABCDEFGHIJKLMNO", 0, nullptr); });
    expectFailure([] { sceSystemLoggerSetLogData("ABCDEFGHIJKLMNOP", 0, nullptr); });
    check(sceSystemLoggerSetLogData("ABC", 0, nullptr) == 0, "valid tag rejected");
    check(sceSystemLoggerSetLogData("123", 0, nullptr) == 0, "numeric tag rejected");
    check(sceSystemLoggerSetLogData("ABC123", 0, nullptr) == 0, "alphanumeric tag rejected");
}

void testEntryValidation() {
    SystemLoggerLogData entry{};
    entry.size = 0x41;
    expectFailure([&] { sceSystemLoggerSetLogData("ABC", 1, &entry); });
    entry.size = 0;
    entry.reserved[0] = 1;
    expectFailure([&] { sceSystemLoggerSetLogData("ABC", 1, &entry); });
    entry.reserved[0] = 0;
    entry.reserved[15] = 1;
    expectFailure([&] { sceSystemLoggerSetLogData("ABC", 1, &entry); });
    entry.reserved[15] = 0;
    check(sceSystemLoggerSetLogData("ABC", 1, &entry) == 0, "valid entry rejected");
}

void testLogSubmission() {
    SystemLoggerLogData entry{};
    entry.size = 5;
    std::copy_n("Hello", 5, entry.data);
    check(sceSystemLoggerSetLogData("TEST", 1, &entry) == 0, "log submission failed");
    
    std::array<SystemLoggerLogData, 5> entries{};
    for (std::uint32_t i = 0; i < entries.size(); ++i) {
        entries[i].size = 3;
        std::copy_n("ABC", 3, entries[i].data);
    }
    check(sceSystemLoggerSetLogData("MULTI", 5, entries.data()) == 0, "multiple log submission failed");
}

void testNotInitialized() {
    expectFailure([] { sceSystemLoggerSetLogData("ABC", 0, nullptr); });
    check(sceSystemLoggerInitialize() == 0, "initialization failed");
    check(sceSystemLoggerSetLogData("ABC", 0, nullptr) == 0, "log submission failed after initialization");
    check(sceSystemLoggerTerminate() == 0, "termination failed");
}

} // namespace

int main() {
    try {
        testInitializeTerminate();
        testInvalidArguments();
        testTagValidation();
        testEntryValidation();
        testLogSubmission();
        testNotInitialized();
        std::fprintf(stderr, "All tests passed\n");
        return 0;
    } catch (const std::exception& error) {
        std::fprintf(stderr, "Test failed: %s\n", error.what());
        return 1;
    }
}
