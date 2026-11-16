#include "prx/libSceHidControl/libSceHidControl.hpp"
#include "prx/libkernel/include/Api.hpp"
#include <array>
#include <cerrno>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <initializer_list>
#ifdef _WIN32
#include <windows.h>
#else
#include <sys/mman.h>
#include <unistd.h>
#endif

namespace {

constexpr int InvalidArgument = static_cast<int>(0x803b3001);
constexpr int NotInitialized = static_cast<int>(0x803b3002);
constexpr std::size_t OutputSize = 0x40;
constexpr std::uint32_t Device = 19;
int result = 0;
int calls = 0;

void Require(bool condition, const char* message) {
    if (condition) return;
    std::fprintf(stderr, "HID telemetry: %s\n", message);
    std::abort();
}

struct Request {
    std::uint32_t device;
    std::uint32_t reserved0;
    void* reply;
    std::uint32_t mode;
    std::uint32_t reserved1;
};
static_assert(sizeof(Request) == 0x18);

void CheckReply(const std::uint8_t* output) {
    for (std::size_t index = 0; index < OutputSize; ++index)
        Require(output[index] == (index < 4 ? 0x91 + index : 0), "reply layout");
}

}

extern "C" {

int APS5_VABI open_nid_postfix(const char* path, int flags, int mode) {
    Require(std::strcmp(path, "/dev/hid") == 0 && flags == 0 && mode == 0, "open request");
    return 37;
}

int APS5_VABI ioctl_nid_postfix(int descriptor, std::uint64_t command, void* data) {
    Require(descriptor == 37 && command == 0x8018486d, "ioctl command");
    const auto& request = *static_cast<Request*>(data);
    Require(request.device == Device && request.mode == 1, "ioctl arguments");
    Require(request.reserved0 == 0 && request.reserved1 == 0 && request.reply != nullptr, "ioctl storage");
    ++calls;
    for (std::size_t index = 0; index < OutputSize; ++index)
        static_cast<std::uint8_t*>(request.reply)[index] = static_cast<std::uint8_t>(0x91 + index);
    errno = 72;
    return result;
}

}

int main() {
    Require(sceHidControlPswUpdateGetDeviceFirmwareUpdateTelemetryData(Device, reinterpret_cast<void*>(0x10)) == NotInitialized, "initialization precedence");
    Require(calls == 0 && sceHidControlInit() == 0, "initialize");
    Require(sceHidControlPswUpdateGetDeviceFirmwareUpdateTelemetryData(Device, nullptr) == InvalidArgument && calls == 0, "null output precedence");
    std::array<std::uint8_t, OutputSize + 2> buffer{};
    buffer.fill(0x5a);
    result = -9;
    Require(sceHidControlPswUpdateGetDeviceFirmwareUpdateTelemetryData(Device, buffer.data() + 1) == -9 && errno == 72, "ioctl error");
    for (auto byte : buffer) Require(byte == 0x5a, "error preserves output");
    Require(sceHidControlPswUpdateGetDeviceFirmwareUpdateTelemetryData(Device, reinterpret_cast<void*>(0x10)) == -9, "error skips output access");
    for (int status : {0, 7}) {
        result = status;
        Require(sceHidControlPswUpdateGetDeviceFirmwareUpdateTelemetryData(Device, buffer.data() + 1) == status && errno == 72, "successful ioctl status and errno");
        CheckReply(buffer.data() + 1);
        Require(buffer.front() == 0x5a && buffer.back() == 0x5a, "output sentinels");
    }
#ifdef _WIN32
    SYSTEM_INFO info{};
    GetSystemInfo(&info);
    const std::size_t pageSize = info.dwPageSize;
    auto* memory = static_cast<std::uint8_t*>(VirtualAlloc(nullptr, pageSize * 2, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE));
    Require(memory != nullptr, "allocate pages");
    DWORD previous = 0;
    Require(VirtualProtect(memory + pageSize, pageSize, PAGE_NOACCESS, &previous) != 0, "protect guard");
#else
    const long hostPageSize = sysconf(_SC_PAGESIZE);
    Require(hostPageSize >= static_cast<long>(OutputSize), "page size");
    const auto pageSize = static_cast<std::size_t>(hostPageSize);
    auto* memory = static_cast<std::uint8_t*>(mmap(nullptr, pageSize * 2, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0));
    Require(memory != MAP_FAILED, "allocate pages");
    Require(mprotect(memory + pageSize, pageSize, PROT_NONE) == 0, "protect guard");
#endif
    std::memset(memory, 0x5a, pageSize);
    for (void* output : {reinterpret_cast<void*>(0x10), static_cast<void*>(memory + pageSize), static_cast<void*>(memory + pageSize - OutputSize / 2)}) {
        Require(sceHidControlPswUpdateGetDeviceFirmwareUpdateTelemetryData(Device, output) == InvalidArgument && errno == 72, "inaccessible output");
    }
    std::memset(memory, 0x5a, pageSize);
#ifdef _WIN32
    Require(VirtualProtect(memory, pageSize, PAGE_READONLY, &previous) != 0, "read-only page");
#else
    Require(mprotect(memory, pageSize, PROT_READ) == 0, "read-only page");
#endif
    auto* output = memory + pageSize - OutputSize;
    Require(sceHidControlPswUpdateGetDeviceFirmwareUpdateTelemetryData(Device, output) == InvalidArgument && errno == 72, "read-only output");
    for (std::size_t index = 0; index < pageSize; ++index) Require(memory[index] == 0x5a, "read-only bytes unchanged");
#ifdef _WIN32
    Require(VirtualProtect(memory, pageSize, PAGE_READWRITE, &previous) != 0, "restore writes");
#else
    Require(mprotect(memory, pageSize, PROT_READ | PROT_WRITE) == 0, "restore writes");
#endif
    Require(sceHidControlPswUpdateGetDeviceFirmwareUpdateTelemetryData(Device, output) == result && errno == 72, "exact output extent and recovery");
    CheckReply(output);
    Require(output[-1] == 0x5a, "exact extent sentinel");
    Require(calls == 9, "ioctl call count");
#ifdef _WIN32
    Require(VirtualFree(memory, 0, MEM_RELEASE) != 0, "free pages");
#else
    Require(munmap(memory, pageSize * 2) == 0, "free pages");
#endif
    std::puts("HID telemetry checks passed");
}
