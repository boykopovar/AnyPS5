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
    auto* const unreadableLeonardoData = reinterpret_cast<const PadLeonardoData*>(0x10);
    auto* const unwritableLeonardoUniqueData = reinterpret_cast<PadLeonardoUniqueData*>(0x10);
    std::int32_t deviceId = 7;
    std::uint16_t status = 7;
    std::uint8_t leonardoData[128]{};
    std::uint8_t leonardoUniqueData[24]{};
    for (std::uint8_t& byte : leonardoUniqueData) byte = 0xaa;
    const auto* const localData = reinterpret_cast<const PadLeonardoData*>(leonardoData);
    auto* const localUniqueData = reinterpret_cast<PadLeonardoUniqueData*>(leonardoUniqueData);

    Require(scePadGetDeviceId(12345, unreadableDeviceId) == PAD_ERROR_NOT_INITIALIZED);
    Require(scePadControllerUpdateIsUpdateNeeded2(12345, nullptr, unwritableStatus) == PAD_ERROR_INVALID_ARG);
    Require(scePadControllerUpdateIsUpdateNeeded2(12345, nullptr, &status) == PAD_ERROR_INVALID_ARG && status == 0);
    Require(scePadParseUniqueDataLeonardo(12345, unreadableLeonardoData, unwritableLeonardoUniqueData) == PAD_ERROR_NOT_INITIALIZED);
    Require(scePadParseUniqueDataLeonardo(12345, unreadableLeonardoData, localUniqueData) == PAD_ERROR_NOT_INITIALIZED);
    Require(scePadParseUniqueDataLeonardo(12345, localData, unwritableLeonardoUniqueData) == PAD_ERROR_NOT_INITIALIZED);
    Require(scePadParseUniqueDataLeonardo(12345, localData, localUniqueData) == PAD_ERROR_NOT_INITIALIZED);
    for (std::uint8_t byte : leonardoUniqueData) Require(byte == 0xaa);
    Require(scePadInit_nid_postfix() == PAD_OK);

    errno = 71;
    Require(scePadGetDeviceId(12345, unreadableDeviceId) == PAD_ERROR_INVALID_ARG);
    Require(errno == 71);
    Require(scePadGetDeviceId(12345, &deviceId) == PAD_ERROR_INVALID_HANDLE && deviceId == -1);
    Require(scePadControllerUpdateIsUpdateNeeded2(12345, nullptr, unwritableStatus) == PAD_ERROR_INVALID_ARG);
    Require(errno == 71);
    status = 7;
    Require(scePadControllerUpdateIsUpdateNeeded2(12345, nullptr, &status) == PAD_ERROR_INVALID_ARG && status == 0);
    Require(scePadParseUniqueDataLeonardo(12345, unreadableLeonardoData, unwritableLeonardoUniqueData) == PAD_ERROR_INVALID_ARG);
    Require(errno == 71);
    for (std::uint8_t& byte : leonardoUniqueData) byte = 0xaa;
    Require(scePadParseUniqueDataLeonardo(12345, unreadableLeonardoData, localUniqueData) == PAD_ERROR_INVALID_ARG);
    Require(errno == 71);
    for (std::uint8_t byte : leonardoUniqueData) Require(byte == 0);
    Require(scePadParseUniqueDataLeonardo(12345, localData, unwritableLeonardoUniqueData) == PAD_ERROR_INVALID_ARG);
    Require(errno == 71);

#ifdef _WIN32
    SYSTEM_INFO info{};
    GetSystemInfo(&info);
    const std::size_t pageSize = info.dwPageSize;
    void* page = VirtualAlloc(nullptr, pageSize, MEM_RESERVE | MEM_COMMIT, PAGE_READONLY);
    Require(page != nullptr);
#else
    const long size = sysconf(_SC_PAGESIZE);
    Require(size > 0);
    const std::size_t pageSize = static_cast<std::size_t>(size);
    void* page = mmap(nullptr, pageSize, PROT_READ, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    Require(page != MAP_FAILED);
#endif
    Require(scePadGetDeviceId(12345, static_cast<std::int32_t*>(page)) == PAD_ERROR_INVALID_ARG);
    Require(scePadControllerUpdateIsUpdateNeeded2(12345, nullptr, static_cast<std::uint16_t*>(page)) == PAD_ERROR_INVALID_ARG);
    Require(errno == 71);
    Require(scePadParseUniqueDataLeonardo(12345, static_cast<const PadLeonardoData*>(page), static_cast<PadLeonardoUniqueData*>(page)) == PAD_ERROR_INVALID_ARG);
    Require(errno == 71);
    Require(scePadParseUniqueDataLeonardo(12345, localData, static_cast<PadLeonardoUniqueData*>(page)) == PAD_ERROR_INVALID_ARG);
    Require(errno == 71);
    for (std::uint8_t& byte : leonardoUniqueData) byte = 0xaa;
    Require(scePadParseUniqueDataLeonardo(12345, static_cast<const PadLeonardoData*>(page), localUniqueData) == PAD_ERROR_INVALID_HANDLE);
    Require(errno == 71);
    for (std::uint8_t byte : leonardoUniqueData) Require(byte == 0);
    const auto* const lastByteData = reinterpret_cast<const PadLeonardoData*>(static_cast<const char*>(page) + pageSize - 1);
    for (std::uint8_t& byte : leonardoUniqueData) byte = 0xaa;
    Require(scePadParseUniqueDataLeonardo(12345, lastByteData, localUniqueData) == PAD_ERROR_INVALID_HANDLE);
    Require(errno == 71);
    for (std::uint8_t byte : leonardoUniqueData) Require(byte == 0);
    for (std::uint8_t& byte : leonardoUniqueData) byte = 0xaa;
    Require(scePadParseUniqueDataLeonardo(12345, localData, localUniqueData) == PAD_ERROR_INVALID_HANDLE);
    Require(errno == 71);
    for (std::uint8_t byte : leonardoUniqueData) Require(byte == 0);
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
    Require(scePadGetMotionSensorPosition(12345, reinterpret_cast<PadMotionSensorPosition*>(0x10)) == PAD_ERROR_INVALID_ARG);
    Require(scePadGetSphereRadius(12345, reinterpret_cast<float*>(0x10)) == PAD_ERROR_INVALID_ARG);
    Require(scePadGetBluetoothAddress(12345, reinterpret_cast<std::uint64_t*>(0x10)) == PAD_ERROR_INVALID_ARG);
    Require(scePadClose_nid_postfix(special) == PAD_OK);
#ifdef _WIN32
    Require(VirtualFree(page, 0, MEM_RELEASE) != 0);
#else
    Require(munmap(page, static_cast<std::size_t>(size)) == 0);
#endif
}
