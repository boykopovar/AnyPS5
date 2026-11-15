#include "prx/libScePad/include/Pad.hpp"
#include <cerrno>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#ifdef _WIN32
#include <windows.h>
#else
#include <sys/mman.h>
#include <unistd.h>
#endif

static void Require(bool condition) { if (!condition) std::abort(); }

int main() {
    auto* const unreadableDeviceId = reinterpret_cast<std::int32_t*>(0x10);
    auto* const unwritableStatus = reinterpret_cast<std::uint16_t*>(0x10);
    std::int32_t deviceId = 7;
    std::uint16_t status = 7;

    Require(scePadGetDeviceId(12345, unreadableDeviceId) == PAD_ERROR_NOT_INITIALIZED);
    Require(scePadControllerUpdateIsUpdateNeeded2(12345, nullptr, unwritableStatus) == PAD_ERROR_INVALID_ARG);
    Require(scePadControllerUpdateIsUpdateNeeded2(12345, nullptr, &status) == PAD_ERROR_INVALID_ARG && status == 0);
    Require(scePadInit_nid_postfix() == PAD_OK);

    errno = 71;
    Require(scePadGetDeviceId(12345, unreadableDeviceId) == PAD_ERROR_INVALID_ARG);
    Require(errno == 71);
    Require(scePadGetDeviceId(12345, &deviceId) == PAD_ERROR_INVALID_HANDLE && deviceId == -1);
    Require(scePadControllerUpdateIsUpdateNeeded2(12345, nullptr, unwritableStatus) == PAD_ERROR_INVALID_ARG);
    Require(errno == 71);
    status = 7;
    Require(scePadControllerUpdateIsUpdateNeeded2(12345, nullptr, &status) == PAD_ERROR_INVALID_ARG && status == 0);

#ifdef _WIN32
    SYSTEM_INFO info{};
    GetSystemInfo(&info);
    void* page = VirtualAlloc(nullptr, info.dwPageSize, MEM_RESERVE | MEM_COMMIT, PAGE_READONLY);
    Require(page != nullptr);
#else
    const long size = sysconf(_SC_PAGESIZE);
    Require(size > 0);
    void* page = mmap(nullptr, static_cast<std::size_t>(size), PROT_READ, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    Require(page != MAP_FAILED);
#endif
    Require(scePadGetDeviceId(12345, static_cast<std::int32_t*>(page)) == PAD_ERROR_INVALID_ARG);
    Require(scePadControllerUpdateIsUpdateNeeded2(12345, nullptr, static_cast<std::uint16_t*>(page)) == PAD_ERROR_INVALID_ARG);
    Require(errno == 71);
    const int special = scePadOpen_nid_postfix(200, PAD_PORT_TYPE_SPECIAL, 0, nullptr);
    Require(special > 0);
    Require(scePadGetExtControllerInformation(special, reinterpret_cast<void*>(0x10)) == PAD_ERROR_INVALID_ARG);
    Require(errno == 71);
    Require(scePadGetExtControllerInformation(special, page) == PAD_ERROR_INVALID_ARG);
    Require(errno == 71);
    unsigned char extInfo[0x30]{};
    for (unsigned char& byte : extInfo) byte = 0x5a;
    Require(scePadGetExtControllerInformation(special, extInfo) == PAD_OK);
    for (std::size_t i = 0; i < 0x2c; ++i) Require(extInfo[i] == 0);
    for (std::size_t i = 0x2c; i < sizeof(extInfo); ++i) Require(extInfo[i] == 0x5a);
    Require(scePadClose_nid_postfix(special) == PAD_OK);
#ifdef _WIN32
    Require(VirtualFree(page, 0, MEM_RELEASE) != 0);
#else
    Require(munmap(page, static_cast<std::size_t>(size)) == 0);
#endif
}
