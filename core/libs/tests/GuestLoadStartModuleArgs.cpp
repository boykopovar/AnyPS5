#include "prx/libc/include/general/VabiMacros.hpp"
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <source_location>
#include <string>
#include <thread>
#include <vector>
extern "C" {
int APS5_VABI sceKernelLoadStartModule(const char*, std::size_t, const void*, std::uint32_t, const void*, int*);
int APS5_VABI sceKernelStopUnloadModule(int, std::size_t, const void*, std::uint32_t, const void*, int*);
}
static void Require(bool value, const std::source_location& location = std::source_location::current()) {
    if (value) return;
    std::fprintf(stderr, "guest_load_start_module_args: failed at %s:%u\n", location.file_name(), location.line());
    std::abort();
}
struct Seen {
    std::size_t args = 0;
    std::size_t argp = 0;
    std::size_t sum = 0;
};
static Seen Load(const char* module, const char* events, std::size_t args, const void* argp) {
    std::remove(events);
    const int handle = sceKernelLoadStartModule(module, args, argp, 0, nullptr, nullptr);
    Require(handle > 0);
    Seen seen;
    std::ifstream file(events);
    std::string line;
    Require(static_cast<bool>(std::getline(file, line)));
    Require(std::sscanf(line.c_str(), "args=%zu argp=%zx sum=%zu", &seen.args, &seen.argp, &seen.sum) == 3);
    Require(sceKernelStopUnloadModule(handle, 0, nullptr, 0, nullptr, nullptr) == 0);
    return seen;
}
static std::size_t Address(const void* pointer) {
    return static_cast<std::size_t>(reinterpret_cast<std::uintptr_t>(pointer));
}
int main(int argc, char** argv) {
    Require(argc == 3);
    ::setenv("ANYPS5_GUEST_MODULE_ARGS", argv[2], 1);
    const char* module = argv[1];
    const char* events = argv[2];

    Seen seen = Load(module, events, 0, nullptr);
    Require(seen.args == 0 && seen.argp == 0);

    std::uint8_t stackBlock[24] = {};
    seen = Load(module, events, sizeof(stackBlock), stackBlock);
    Require(seen.args == sizeof(stackBlock) && seen.argp == Address(stackBlock));
    seen = Load(module, events, 0, nullptr);
    Require(seen.args == 0 && seen.argp == 0);

    std::vector<std::uint8_t> engine(52, 0);
    const std::size_t engineAddress = Address(engine.data());
    seen = Load(module, events, engine.size(), engine.data());
    Require(seen.args == 52 && seen.argp == engineAddress && seen.sum == 0);

    std::memset(engine.data(), 0xAB, engine.size());
    seen = Load(module, events, 0, nullptr);
    Require(seen.args == 52 && seen.argp == engineAddress && seen.sum == 52 * 0xAB);

    const std::vector<std::uint8_t> later(16, 0x11);
    seen = Load(module, events, later.size(), later.data());
    Require(seen.args == 16 && seen.argp == Address(later.data()) && seen.sum == 16 * 0x11);
    seen = Load(module, events, 0, nullptr);
    Require(seen.args == 52 && seen.argp == engineAddress && seen.sum == 52 * 0xAB);

    seen = Load(module, events, 0, reinterpret_cast<const void*>(0x2000));
    Require(seen.args == 0 && seen.argp == 0x2000);
    seen = Load(module, events, 8, nullptr);
    Require(seen.args == 8 && seen.argp == 0);

    Seen fromThread;
    std::thread other([&] { fromThread = Load(module, events, 0, nullptr); });
    other.join();
    Require(fromThread.args == 52 && fromThread.argp == engineAddress && fromThread.sum == 52 * 0xAB);
    std::remove(events);
    return 0;
}
