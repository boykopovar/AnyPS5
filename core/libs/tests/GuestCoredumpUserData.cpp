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
constexpr int64_t InvalidArgument = static_cast<int32_t>(0x81180000u);
constexpr int64_t NotInCoredumpHandler = static_cast<int32_t>(0x81180003u);
bool handlerRan = false;
int handlerContext = 0;

int APS5_VABI Handler(void*) {
    handlerRan = true;
    return 0;
}

void Require(bool value) { if (!value) std::abort(); }

template <typename TCall>
bool Throws(TCall call, const char* function) {
    try { call(); }
    catch (const std::runtime_error& error) { return std::strstr(error.what(), function) != nullptr; }
    return false;
}

void RequireWritesRefused(const std::array<unsigned char, 64>& region) {
    Require(sceCoredumpWriteUserData(region.data(), region.size()) == NotInCoredumpHandler);
    Require(sceCoredumpWriteUserData(region.data(), 1) == NotInCoredumpHandler);
    int64_t fromThread = 0;
    std::thread([&] { fromThread = sceCoredumpWriteUserData(region.data(), region.size()); }).join();
    Require(fromThread == NotInCoredumpHandler);
}

void RequireAttachesRefused(const std::array<unsigned char, 64>& region) {
    Require(sceCoredumpAttachUserFile(1, "/app0/eboot.bin") == NotInCoredumpHandler);
    Require(sceCoredumpAttachUserFile(0xFFFFFFFFu, "") == NotInCoredumpHandler);
    Require(sceCoredumpAttachMemoryRegionAsUserFile(999, region.data(), region.size(), "region.txt") == NotInCoredumpHandler);
    Require(sceCoredumpAttachMemoryRegionAsUserFile(0, region.data(), 1, "") == NotInCoredumpHandler);
    int fromThread = 0;
    std::thread([&] { fromThread = sceCoredumpAttachMemoryRegionAsUserFile(2, region.data(), region.size(), "thread.txt"); }).join();
    Require(fromThread == NotInCoredumpHandler);
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
    std::array<unsigned char, 64> region{};
    for (size_t index = 0; index < region.size(); ++index) region[index] = static_cast<unsigned char>(index * 5 + 1);
    const auto original = region;
    const char text[] = "coredump text\n";
    Require(sceCoredumpUnregisterCoredumpHandler() == static_cast<int>(0x81180001u));
    Require(sceCoredumpRegisterCoredumpHandler(0, 0x4000, 0) == InvalidArgument);
    for (const std::size_t stackSize : {std::size_t{0}, std::size_t{0x3fff}, std::size_t{0x20000000}, SIZE_MAX}) {
        Require(sceCoredumpRegisterCoredumpHandler(reinterpret_cast<std::uint64_t>(&Handler), stackSize, 0) == InvalidArgument);
    }
    Require(sceCoredumpAttachUserFile(0x40000000u, "") == NotInCoredumpHandler);

    Require(NotInCoredumpHandler < 0);
    Require(sceCoredumpSetUserDataType(0) == 0);
    RequireWritesRefused(region);
    RequireAttachesRefused(region);
    sceCoredumpDebugTextOut(text, static_cast<int>(sizeof(text) - 1));
    sceCoredumpDebugTextOut(text, 8);
    sceCoredumpDebugTextOut(text, 0);
    sceCoredumpDebugTextOut(nullptr, 0);

    Require(sceCoredumpRegisterCoredumpHandler(reinterpret_cast<uint64_t>(&Handler), 16 * 1024, reinterpret_cast<uint64_t>(&handlerContext)) == 0);
    Require(sceCoredumpRegisterCoredumpHandler(0, 0, 0) == static_cast<int>(0x81180002u));
    Require(sceCoredumpAttachUserFile(0x40000000u, "") == InvalidArgument);
    Require(sceCoredumpAttachMemoryRegionAsUserFile(0x40000000u, nullptr, -1, "") == InvalidArgument);
    RequireWritesRefused(region);
    RequireAttachesRefused(region);
    sceCoredumpDebugTextOut(text, static_cast<int>(sizeof(text) - 1));

    Require(!handlerRan);

    Require(sceCoredumpWriteUserData(nullptr, region.size()) == InvalidArgument);
    Require(sceCoredumpWriteUserData(nullptr, 0) == InvalidArgument);
    Require(sceCoredumpWriteUserData(region.data(), 0) == NotInCoredumpHandler);
    Require(sceCoredumpWriteUserData(region.data(), INT32_MAX) == NotInCoredumpHandler);
    Require(sceCoredumpWriteUserData(region.data(), std::size_t{INT32_MAX} + 1) == InvalidArgument);
    Require(sceCoredumpWriteUserData(region.data(), SIZE_MAX) == InvalidArgument);
    Require(Throws([&] { sceCoredumpDebugTextOut(nullptr, 4); }, "sceCoredumpDebugTextOut"));
    Require(Throws([&] { sceCoredumpDebugTextOut(text, -1); }, "sceCoredumpDebugTextOut"));
    Require(sceCoredumpAttachUserFile(1, nullptr) == InvalidArgument);
    Require(sceCoredumpAttachMemoryRegionAsUserFile(1, nullptr, region.size(), "a.txt") == NotInCoredumpHandler);
    Require(sceCoredumpAttachMemoryRegionAsUserFile(1, region.data(), 0, "a.txt") == NotInCoredumpHandler);
    Require(sceCoredumpAttachMemoryRegionAsUserFile(1, region.data(), -1, "a.txt") == 0);
    Require(sceCoredumpAttachMemoryRegionAsUserFile(1, region.data(), region.size(), nullptr) == InvalidArgument);

    Require(sceCoredumpUnregisterCoredumpHandler() == 0);
    RequireWritesRefused(region);
    RequireAttachesRefused(region);
    Require(region == original);
    Require(sceCoredumpUnregisterCoredumpHandler() == static_cast<int>(0x81180001u));
    Require(sceCoredumpAttachUserFile(0x40000000u, "") == InvalidArgument);
    for (const std::uint32_t version : {0x064fffffu, 0x06500000u, 0x12000000u}) {
        std::memcpy(process.data() + 16, &version, sizeof(version));
        Require(sceCoredumpRegisterCoredumpHandler(reinterpret_cast<std::uint64_t>(&Handler), 0x4000, 0) == 0);
        for (const std::uint32_t flags : {0u, 0xffffu, 0x10000u, 0x40000000u, 0x40000001u, 0x80000000u, 0xc0000000u, UINT32_MAX}) {
            const bool invalid = version >= 0x06500000 && (flags & 0xc0000000u) == 0x40000000u;
            Require(sceCoredumpAttachUserFile(flags, "") == (invalid ? InvalidArgument : NotInCoredumpHandler));
            Require(sceCoredumpAttachMemoryRegionAsUserFile(flags, nullptr, 0, "") == (invalid ? InvalidArgument : NotInCoredumpHandler));
            Require(sceCoredumpAttachMemoryRegionAsUserFile(flags, nullptr, -1, "") == (invalid ? InvalidArgument : 0));
            Require(sceCoredumpAttachMemoryRegionAsUserFile(flags, nullptr, -1, nullptr) == InvalidArgument);
        }
        Require(sceCoredumpUnregisterCoredumpHandler() == 0);
    }
    Require(!handlerRan);
    return 0;
}
