#include "prx/libc/include/general/VabiMacros.hpp"
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <stdexcept>
#include <thread>

extern "C" {
int APS5_VABI sceCoredumpRegisterCoredumpHandler(uint64_t handler, size_t stackSize, uint64_t context);
int APS5_VABI sceCoredumpUnregisterCoredumpHandler(void);
int APS5_VABI sceCoredumpAttachMemoryRegion(uint32_t userValue, const void* mem, size_t size);
int APS5_VABI sceCoredumpAttachUserMemoryFile(uint32_t userValue, const void* mem, size_t size);
int APS5_VABI sceCoredumpGetStopInfoCpu(void* info, size_t size);
int APS5_VABI sceCoredumpGetThreadContextInfo(void* threadContextInfo, size_t threadContextInfoSize);
}

namespace {
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
    alignas(uint64_t) std::array<unsigned char, 64> region{};
    for (size_t index = 0; index < region.size(); ++index) region[index] = static_cast<unsigned char>(index * 7 + 3);
    const auto original = region;
    alignas(uint64_t) std::array<unsigned char, 160> context{};
    for (size_t index = 0; index < context.size(); ++index) context[index] = static_cast<unsigned char>(index * 5 + 1);
    const auto originalContext = context;

    Require(sceCoredumpAttachMemoryRegion(1, region.data(), region.size()) == NotInCoredumpHandler);
    Require(sceCoredumpAttachUserMemoryFile(UserFileTruncateIfExceedLimit | 2, region.data(), region.size()) == NotInCoredumpHandler);
    Require(sceCoredumpGetStopInfoCpu(region.data() + 16, 16) == NotInCoredumpHandler);
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

    Require(Throws([&] { sceCoredumpAttachMemoryRegion(1, nullptr, region.size()); }, "sceCoredumpAttachMemoryRegion"));
    Require(Throws([&] { sceCoredumpAttachMemoryRegion(1, region.data(), 0); }, "sceCoredumpAttachMemoryRegion"));
    Require(Throws([&] { sceCoredumpAttachMemoryRegion(1, region.data(), SIZE_MAX); }, "sceCoredumpAttachMemoryRegion"));
    Require(Throws([&] { sceCoredumpAttachUserMemoryFile(1, nullptr, region.size()); }, "sceCoredumpAttachUserMemoryFile"));
    Require(Throws([&] { sceCoredumpAttachUserMemoryFile(1, region.data(), 0); }, "sceCoredumpAttachUserMemoryFile"));
    Require(Throws([&] { sceCoredumpAttachUserMemoryFile(1, region.data(), SIZE_MAX); }, "sceCoredumpAttachUserMemoryFile"));
    Require(Throws([&] { sceCoredumpGetStopInfoCpu(nullptr, 16); }, "sceCoredumpGetStopInfoCpu"));
    Require(Throws([&] { sceCoredumpGetStopInfoCpu(region.data() + 1, 16); }, "sceCoredumpGetStopInfoCpu"));
    Require(Throws([&] { sceCoredumpGetStopInfoCpu(reinterpret_cast<void*>(UINTPTR_MAX - 7u), 16); }, "sceCoredumpGetStopInfoCpu"));
    for (const size_t size : {size_t{0}, size_t{1}, size_t{15}, size_t{17}, SIZE_MAX})
        Require(Throws([&] { sceCoredumpGetStopInfoCpu(region.data() + 16, size); }, "sceCoredumpGetStopInfoCpu"));
    Require(Throws([&] { sceCoredumpGetThreadContextInfo(nullptr, 144); }, "sceCoredumpGetThreadContextInfo"));
    Require(Throws([&] { sceCoredumpGetThreadContextInfo(context.data() + 1, 144); }, "sceCoredumpGetThreadContextInfo"));
    Require(Throws([&] { sceCoredumpGetThreadContextInfo(reinterpret_cast<void*>(UINTPTR_MAX - 7u), 144); }, "sceCoredumpGetThreadContextInfo"));
    for (const size_t size : {size_t{0}, size_t{1}, size_t{143}, size_t{145}, SIZE_MAX})
        Require(Throws([&] { sceCoredumpGetThreadContextInfo(context.data(), size); }, "sceCoredumpGetThreadContextInfo"));

    Require(sceCoredumpUnregisterCoredumpHandler() == 0);
    Require(sceCoredumpAttachMemoryRegion(1, region.data(), region.size()) == NotInCoredumpHandler);
    Require(sceCoredumpAttachUserMemoryFile(1, region.data(), region.size()) == NotInCoredumpHandler);
    Require(sceCoredumpGetStopInfoCpu(region.data() + 16, 16) == NotInCoredumpHandler);
    Require(sceCoredumpGetThreadContextInfo(context.data() + 8, 144) == NotInCoredumpHandler);
    Require(region == original && context == originalContext);
    Require(!handlerRan);
    return 0;
}
