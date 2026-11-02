#include "prx/libkernel/include/Api.hpp"
#include "prx/libSceSaveData/include/Api.hpp"
#include "SceTypes.hpp"
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include "prx/libc/include/GuestArena.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include <array>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <string>

static void Require(bool value, const char* message) {
    if (!value) {
        std::fprintf(stderr, "Save-data memory host write: %s\n", message);
        std::exit(1);
    }
}

static DWORD Protection(const void* address) {
    MEMORY_BASIC_INFORMATION information{};
    Require(VirtualQuery(address, &information, sizeof(information)) != 0, "query host protection");
    return information.Protect;
}

int main() {
    constexpr std::size_t page = 0x4000;
    constexpr std::size_t offset = 3;
    std::string contents(page + 0x1000 + offset, '\0');
    for (std::size_t i = 0; i < contents.size(); ++i) contents[i] = static_cast<char>('a' + i % 26);
    const auto previousDirectory = std::filesystem::current_path();
    const auto root = std::filesystem::temp_directory_path() /
        ("anyps5-save-host-write-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    Require(std::filesystem::create_directory(root), "create the save root");
    std::filesystem::current_path(root);
    const auto path = std::filesystem::path("_sd_mem/u7532/slot0.bin");
    std::filesystem::create_directories(path.parent_path());
    std::ofstream(path, std::ios::binary).write(contents.data(), static_cast<std::streamsize>(contents.size()));
    Require(sceSaveDataInitialize3(nullptr) == 0, "initialize save data");

    std::int64_t physical = 0;
    Require(sceKernelAllocateDirectMemory(0, 0x7fffffffffll, page * 3, 0, 0, &physical) == 0, "allocate direct memory");
    void* mapping = nullptr;
    Require(sceKernelMapDirectMemory(&mapping, page * 3, 3, 0, physical, 0) == 0, "map read/write memory");
    void* twin = nullptr;
    Require(sceKernelMapDirectMemory(&twin, page * 3, 3, 0, physical, 0) == 0, "map read/write memory a second time");
    auto* buffer = static_cast<char*>(mapping) + page - 7;
    const std::size_t bytes = contents.size() - offset;
    const auto collect = [&] {
        std::array<void*, 16> pages{};
        std::size_t count = pages.size();
        Require(GuestArena::GuestArenaCollectWrites_nid_postfix(reinterpret_cast<std::uintptr_t>(mapping), page * 3,
            pages.data(), &count, true), "collect tracked writes");
        return count;
    };
    {
        const GuestArena::HostWrite destination(buffer - 1, bytes + 2);
        Require(destination.Open(), "reset the memory buffer");
        std::memset(buffer - 1, '!', bytes + 2);
    }
    collect();
    Require(collect() == 0, "memory buffer initially clean");
    Require(Protection(buffer) == PAGE_READONLY && Protection(buffer + bytes - 1) == PAGE_READONLY,
        "write tracking arms the memory buffer pages");

    SaveDataMemoryData data{};
    data.buf = buffer;
    data.buf_size = bytes;
    data.offset = offset;
    SaveDataMemoryGet2 get{};
    get.user_id = 7532;
    get.data = &data;
    Require(sceSaveDataGetSaveDataMemory2(&get) == 0, "read save memory into watched memory");
    Require(buffer[-1] == '!' && buffer[bytes] == '!' && std::memcmp(buffer, contents.data() + offset, bytes) == 0,
        "save memory bytes in watched memory");
    Require(collect() != 0, "save memory read reported as a tracked write");

    Require(sceKernelMprotect(mapping, page * 3, 1) == 0, "make the memory buffer read-only");
    bool refused = false;
    try {
        sceSaveDataGetSaveDataMemory2(&get);
    } catch (const std::runtime_error&) {
        refused = true;
    }
    Require(refused, "save memory read into read-only memory is refused");

    Require(sceSaveDataTerminate() == 0, "terminate save data");
    std::filesystem::current_path(previousDirectory);
    std::filesystem::remove_all(root);
    return 0;
}
