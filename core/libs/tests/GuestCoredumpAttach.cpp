#include "prx/libkernel/include/Api.hpp"
#include "ProcessParametersFixture.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <stdexcept>
#include <thread>

namespace {
alignas(16) std::array<std::byte, 0x40> process{};
constexpr int InvalidArgument = static_cast<int>(0x81180000u);
constexpr int NotInCoredumpHandler = static_cast<int>(0x81180003u);
constexpr uint32_t UserFileTruncateIfExceedLimit = 0x80000000u;
bool handlerRan = false;
int handlerContext = 0;

int APS5_VABI Handler(void*) {
    handlerRan = true;
    return 0;
}

void Require(bool value) { if (!value) std::abort(); }

template <typename TAttach>
bool Throws(TAttach attach, const char* function) {
    try { attach(); }
    catch (const std::runtime_error& error) { return std::strstr(error.what(), function) != nullptr; }
    return false;
}
}

int main() {
    const std::uint64_t processSize = process.size();
    const std::uint32_t processMagic = 0x4942524f;
    const std::uint32_t sdkVersion = 0x06500000;
    std::memcpy(process.data(), &processSize, sizeof(processSize));
    std::memcpy(process.data() + 8, &processMagic, sizeof(processMagic));
    std::memcpy(process.data() + 16, &sdkVersion, sizeof(sdkVersion));
    Require(ProcessParametersFixture::Install(process.data(), process.size()));
    alignas(uint64_t) std::array<unsigned char, 64> region{};
    for (size_t index = 0; index < region.size(); ++index) region[index] = static_cast<unsigned char>(index * 7 + 3);
    const auto original = region;
    alignas(uint64_t) std::array<unsigned char, 160> context{};
    for (size_t index = 0; index < context.size(); ++index) context[index] = static_cast<unsigned char>(index * 5 + 1);
    const auto originalContext = context;

    Require(sceCoredumpAttachMemoryRegion(1, region.data(), region.size()) == NotInCoredumpHandler);
    Require(sceCoredumpAttachUserMemoryFile(UserFileTruncateIfExceedLimit | 2, region.data(), region.size()) == NotInCoredumpHandler);
    Require(sceCoredumpGetStopInfoCpu(region.data() + 16, 16) == NotInCoredumpHandler);
    Require(sceCoredumpGetStopInfoCpu(region.data() + 16, 8) == NotInCoredumpHandler);
    Require(sceCoredumpGetThreadContextInfo(context.data() + 8, 144) == NotInCoredumpHandler);
    Require(region == original && context == originalContext);

    Require(sceCoredumpRegisterCoredumpHandler(reinterpret_cast<uint64_t>(&Handler), 16 * 1024, reinterpret_cast<uint64_t>(&handlerContext)) == 0);
    Require(sceCoredumpAttachMemoryRegion(0, region.data(), 1) == NotInCoredumpHandler);
    Require(sceCoredumpAttachUserMemoryFile(0xFFFFFFFFu, region.data(), region.size()) == NotInCoredumpHandler);
    Require(sceCoredumpGetStopInfoCpu(region.data() + 16, 16) == NotInCoredumpHandler);
    int fromThread = 0;
    std::thread([&] { fromThread = sceCoredumpAttachMemoryRegion(3, region.data(), region.size()); }).join();
    Require(fromThread == NotInCoredumpHandler);
    std::thread([&] { fromThread = sceCoredumpGetStopInfoCpu(region.data() + 16, 16); }).join();
    Require(fromThread == NotInCoredumpHandler);
    Require(sceCoredumpGetThreadContextInfo(context.data(), 144) == NotInCoredumpHandler);
    std::thread([&] { fromThread = sceCoredumpGetThreadContextInfo(context.data() + 8, 144); }).join();
    Require(fromThread == NotInCoredumpHandler);
    Require(region == original && context == originalContext);
    Require(!handlerRan);

    Require(sceCoredumpAttachMemoryRegion(1, nullptr, region.size()) == NotInCoredumpHandler);
    Require(sceCoredumpAttachMemoryRegion(1, region.data(), 0) == NotInCoredumpHandler);
    Require(sceCoredumpAttachMemoryRegion(1, region.data(), -1) == 0);
    Require(sceCoredumpAttachUserMemoryFile(1, nullptr, region.size()) == NotInCoredumpHandler);
    Require(sceCoredumpAttachUserMemoryFile(1, region.data(), 0) == NotInCoredumpHandler);
    Require(sceCoredumpAttachUserMemoryFile(1, region.data(), -1) == 0);
    Require(sceCoredumpGetStopInfoCpu(nullptr, 16) == InvalidArgument);
    Require(sceCoredumpGetStopInfoCpu(region.data() + 1, 16) == NotInCoredumpHandler);
    Require(sceCoredumpGetStopInfoCpu(reinterpret_cast<void*>(UINTPTR_MAX - 7u), 16) == NotInCoredumpHandler);
    for (const size_t size : {size_t{0}, size_t{1}, size_t{15}, size_t{17}, SIZE_MAX})
        Require(sceCoredumpGetStopInfoCpu(region.data() + 16, size) == InvalidArgument);
    Require(sceCoredumpGetThreadContextInfo(nullptr, 144) == InvalidArgument);
    Require(sceCoredumpGetThreadContextInfo(context.data() + 1, 144) == NotInCoredumpHandler);
    Require(sceCoredumpGetThreadContextInfo(reinterpret_cast<void*>(UINTPTR_MAX - 7u), 144) == NotInCoredumpHandler);
    for (const size_t size : {size_t{0}, size_t{1}, size_t{143}, size_t{145}, SIZE_MAX})
        Require(sceCoredumpGetThreadContextInfo(context.data(), size) == InvalidArgument);

    for (const auto length : {INT32_MIN, -1, 0, 1, INT32_MAX}) {
        const int expected = length < 0 ? 0 : NotInCoredumpHandler;
        Require(sceCoredumpAttachMemoryRegion(0, nullptr, length) == expected);
        Require(sceCoredumpAttachUserMemoryFile(0, nullptr, length) == expected);
        Require(sceCoredumpAttachMemoryRegion(0xffffffffu, region.data(), length) == expected);
        Require(sceCoredumpAttachUserMemoryFile(0xffffffffu, region.data(), length) == expected);
    }
    Require(sceCoredumpUnregisterCoredumpHandler() == 0);
    Require(sceCoredumpAttachMemoryRegion(1, region.data(), region.size()) == NotInCoredumpHandler);
    Require(sceCoredumpAttachUserMemoryFile(1, region.data(), region.size()) == NotInCoredumpHandler);
    Require(sceCoredumpGetStopInfoCpu(region.data() + 16, 16) == NotInCoredumpHandler);
    Require(sceCoredumpGetThreadContextInfo(context.data() + 8, 144) == NotInCoredumpHandler);
    Require(region == original && context == originalContext);
    Require(!handlerRan);

    Require(sceCoredumpGetStopInfoGpu_Agc(region.data() + 8, 8) == NotInCoredumpHandler);
    Require(sceCoredumpGetStopInfoGpu_Agc(region.data() + 8, 3) == NotInCoredumpHandler);
    Require(sceCoredumpGetStopInfoGpu_Agc(nullptr, 8) == InvalidArgument);
    Require(sceCoredumpWriteUserString(1, "text", 4) == 0);
    Require(sceCoredumpWriteUserString(0, "", 0) == 0);
    Require(sceCoredumpWriteUserString(1, nullptr, 4) == InvalidArgument);
    Require(sceCoredumpWriteUserString(15, "x", 256) == 0 && sceCoredumpWriteUserString(0, "x", 0) == 0);
    Require(sceCoredumpWriteUserString(16, "x", 1) == InvalidArgument && sceCoredumpWriteUserString(0x80000000u, "x", 1) == InvalidArgument);
    Require(sceCoredumpWriteUserString(1, "x", 257) == InvalidArgument && sceCoredumpWriteUserString(1, "x", -1) == InvalidArgument);
    Require(region == original);
    Require(sceCoredumpConfigDumpMode(1) == 0);
    for (const int mode : {0, 2, -1}) Require(sceCoredumpConfigDumpMode(mode) == InvalidArgument);
    return 0;
}
