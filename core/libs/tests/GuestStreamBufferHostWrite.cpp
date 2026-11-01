#include "prx/libkernel/include/Api.hpp"
#include "prx/libc/include/Api.hpp"
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include "prx/libc/include/GuestArena.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include <algorithm>
#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <random>
#include <string>

static void Require(bool value, const char* message) {
    if (!value) {
        std::fprintf(stderr, "Stream buffer host write: %s\n", message);
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
    constexpr std::size_t bufferBytes = page + 0x1000;
    std::string contents(bufferBytes * 2, '\0');
    for (std::size_t i = 0; i < contents.size(); ++i) contents[i] = static_cast<char>('a' + i % 26);
    const auto path = std::filesystem::temp_directory_path() / ("anyps5-stream-buffer-" + std::to_string(std::random_device{}()));
    std::ofstream(path, std::ios::binary).write(contents.data(), static_cast<std::streamsize>(contents.size()));

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

    auto* stream = fopen_nid_postfix(path.string().c_str(), "rb");
    Require(stream != nullptr, "open the file");
    Require(setvbuf_nid_postfix(stream, buffer, 0, bufferBytes) == 0, "give the stream a buffer in guest memory");
    {
        const GuestArena::HostWrite destination(buffer, bufferBytes);
        Require(destination.Open(), "fill the stream buffer");
        std::memset(buffer, '!', bufferBytes);
    }
    collect();
    Require(collect() == 0, "stream buffer initially clean");
    Require(Protection(buffer) == PAGE_READONLY && Protection(buffer + bufferBytes - 1) == PAGE_READONLY,
        "write tracking arms the stream buffer pages");

    Require(fgetc_nid_postfix(stream) == contents[0], "fgetc refills a watched stream buffer");
    std::array<char, 64> chunk{};
    Require(fread_nid_postfix(chunk.data(), 1, chunk.size(), stream) == chunk.size() &&
        std::memcmp(chunk.data(), contents.data() + 1, chunk.size()) == 0, "small fread from the stream buffer");
    collect();
    Require(Protection(buffer) == PAGE_READONLY, "stream buffer rearmed");
    std::string rest(contents.size() - 1 - chunk.size(), '\0');
    for (char& value : rest) {
        const int next = fgetc_nid_postfix(stream);
        Require(next != EOF, "fgetc refills the rearmed stream buffer");
        value = static_cast<char>(next);
    }
    Require(rest == contents.substr(1 + chunk.size()), "stream contents through the watched buffer");
    Require(fgetc_nid_postfix(stream) == EOF && ferror_nid_postfix(stream) == 0, "end of file without an error");
    Require(fclose_nid_postfix(stream) == 0, "close the stream");
    Require(std::all_of(buffer, buffer + bufferBytes, [](char value) { return value == '!'; }), "guest stream buffer left untouched");
    std::filesystem::remove(path);
    return 0;
}
