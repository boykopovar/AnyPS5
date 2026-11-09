#include "prx/libSceSystemLogger/include/Api.hpp"
#include "SceTypes.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <source_location>

static void Require(bool value, std::source_location location = std::source_location::current()) {
    if (value) return;
    std::fprintf(stderr, "%s:%u: system logger assertion failed\n", location.file_name(), location.line());
    std::abort();
}

int main() {
    constexpr int notInitialized = static_cast<int>(0x81080004);
    constexpr int alreadyInitialized = static_cast<int>(0x81080005);
    constexpr int invalidArgument = static_cast<int>(0x81080002);

    Require(sceSystemLoggerTerminate() == notInitialized);
    Require(sceSystemLoggerSetLogData("tag", 0, nullptr) == notInitialized);
    Require(sceSystemLoggerInitialize() == 0);
    Require(sceSystemLoggerInitialize() == alreadyInitialized);

    SystemLoggerLogData entry{};
    std::memcpy(entry.data, "hello", 5);
    entry.size = 5;
    Require(sceSystemLoggerSetLogData("abc123", 1, &entry) == 0);
    Require(sceSystemLoggerSetLogData("", 0, nullptr) == 0);

    Require(sceSystemLoggerSetLogData("abc-123", 0, nullptr) == invalidArgument);
    Require(sceSystemLoggerSetLogData("0123456789abcdef", 0, nullptr) == invalidArgument);
    Require(sceSystemLoggerSetLogData(nullptr, 0, nullptr) == invalidArgument);
    Require(sceSystemLoggerSetLogData("tag", 0x21, nullptr) == invalidArgument);
    Require(sceSystemLoggerSetLogData("tag", 1, nullptr) == invalidArgument);

    SystemLoggerLogData invalid = entry;
    invalid.size = 0x41;
    Require(sceSystemLoggerSetLogData("tag", 1, &invalid) == invalidArgument);
    invalid = entry;
    invalid.reserved[0] = 1;
    Require(sceSystemLoggerSetLogData("tag", 1, &invalid) == invalidArgument);

    Require(sceSystemLoggerTerminate() == 0);
    Require(sceSystemLoggerTerminate() == notInitialized);
}
