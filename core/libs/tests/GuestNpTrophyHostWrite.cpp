#include "prx/libkernel/include/Api.hpp"
#include "prx/libSceNpTrophy/include/Api.hpp"
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include "prx/libc/include/GuestArena.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <random>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

void Require(bool condition, const char* message) {
    if (!condition) {
        std::fprintf(stderr, "NpTrophy host write: %s\n", message);
        std::exit(1);
    }
}

DWORD Protection(const void* address) {
    MEMORY_BASIC_INFORMATION information{};
    Require(VirtualQuery(address, &information, sizeof(information)) != 0, "query host protection");
    return information.Protect;
}

void PutBigEndian(std::vector<std::uint8_t>& bytes, std::size_t at, std::uint64_t value, int width) {
    for (int i = 0; i < width; ++i) bytes[at + i] = static_cast<std::uint8_t>(value >> (8 * (width - 1 - i)));
}

void WritePackage(const std::filesystem::path& path, const std::vector<std::pair<std::string, std::string>>& files) {
    const std::size_t header = 0x60, entry = 0x40;
    std::vector<std::uint8_t> bytes(header + entry * files.size());
    PutBigEndian(bytes, 0, 0xdca24d00u, 4);
    PutBigEndian(bytes, 0x10, files.size(), 4);
    PutBigEndian(bytes, 0x14, entry, 4);
    for (std::size_t i = 0; i < files.size(); ++i) {
        const auto at = header + entry * i;
        std::memcpy(bytes.data() + at, files[i].first.data(), files[i].first.size());
        PutBigEndian(bytes, at + 32, bytes.size(), 8);
        PutBigEndian(bytes, at + 40, files[i].second.size(), 8);
        bytes.insert(bytes.end(), files[i].second.begin(), files[i].second.end());
    }
    std::filesystem::create_directories(path.parent_path());
    std::ofstream(path, std::ios::binary).write(reinterpret_cast<const char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
}

}

int main() {
    constexpr std::size_t page = 0x4000;
    std::string icon(page + 0x1000, '\0');
    for (std::size_t i = 0; i < icon.size(); ++i) icon[i] = static_cast<char>('a' + i % 26);
    const auto root = std::filesystem::temp_directory_path() / ("anyps5-np-trophy-host-write-" + std::to_string(std::random_device{}()));
    std::filesystem::remove_all(root);
    WritePackage(root / "app0/sce_sys/trophy/trophy00.trp", {{"TROPCONF.ESFM", "conf"}, {"ICON0.PNG", icon}, {"TROP000.PNG", "first"}});
    std::filesystem::current_path(root);

    int context = 0, handle = 0;
    Require(sceNpTrophyCreateContext(&context, 1, 0, 0) == 0, "create the context");
    Require(sceNpTrophyCreateHandle(&handle) == 0, "create the handle");
    Require(sceNpTrophyRegisterContext(context, handle, 0) == 0, "register the context");

    std::int64_t physical = 0;
    Require(sceKernelAllocateDirectMemory(0, 0x7fffffffffll, page * 3, 0, 0, &physical) == 0, "allocate direct memory");
    void* mapping = nullptr;
    Require(sceKernelMapDirectMemory(&mapping, page * 3, 3, 0, physical, 0) == 0, "map read/write memory");
    void* twin = nullptr;
    Require(sceKernelMapDirectMemory(&twin, page * 3, 3, 0, physical, 0) == 0, "map read/write memory a second time");
    auto* buffer = static_cast<char*>(mapping) + page - 7;
    const auto collect = [&] {
        std::array<void*, 16> pages{};
        std::size_t count = pages.size();
        Require(GuestArena::GuestArenaCollectWrites_nid_postfix(reinterpret_cast<std::uintptr_t>(mapping), page * 3,
            pages.data(), &count, true), "collect tracked writes");
        return count;
    };
    {
        const GuestArena::HostWrite destination(buffer - 1, icon.size() + 2);
        Require(destination.Open(), "reset the icon buffer");
        std::memset(buffer - 1, '!', icon.size() + 2);
    }
    collect();
    Require(collect() == 0, "icon buffer initially clean");
    Require(Protection(buffer) == PAGE_READONLY && Protection(buffer + icon.size() - 1) == PAGE_READONLY,
        "write tracking arms the icon pages");

    std::uint64_t size = icon.size();
    Require(sceNpTrophyGetGameIcon(context, handle, buffer, &size) == 0 && size == icon.size(), "game icon into watched memory");
    Require(buffer[-1] == '!' && buffer[icon.size()] == '!' && std::memcmp(buffer, icon.data(), icon.size()) == 0,
        "game icon bytes in watched memory");
    Require(collect() != 0, "game icon reported as a tracked write");

    Require(sceKernelMprotect(mapping, page * 3, 1) == 0, "make the icon buffer read-only");
    bool refused = false;
    try {
        size = icon.size();
        sceNpTrophyGetGameIcon(context, handle, buffer, &size);
    } catch (const std::runtime_error&) {
        refused = true;
    }
    Require(refused, "game icon into read-only memory is refused");

    Require(sceNpTrophyDestroyHandle(handle) == 0, "destroy the handle");
    Require(sceNpTrophyDestroyContext(context) == 0, "destroy the context");
    std::filesystem::current_path(std::filesystem::temp_directory_path());
    std::filesystem::remove_all(root);
    return 0;
}
