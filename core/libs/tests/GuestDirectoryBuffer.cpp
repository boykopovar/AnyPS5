#include "prx/libkernel/include/Api.hpp"
#include "prx/libkernel/File/include/File.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include "prx/libkernel/File/include/FileFlags.hpp"
#include "prx/libkernel/KernelErrors.hpp"
#include <algorithm>
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

static void Check(bool value, int line) {
    if (!value) {
        std::fprintf(stderr, "Directory buffer check failed at line %d\n", line);
        std::abort();
    }
}
#define Require(value) Check((value), __LINE__)

static void CheckName(const std::u8string& utf8Name) {
    const auto root = std::filesystem::temp_directory_path() /
        ("anyps5-directory-buffer-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    Require(std::filesystem::create_directory(root));
    const std::string name(utf8Name.begin(), utf8Name.end());
    { std::ofstream file(root / std::filesystem::path(utf8Name)); Require(static_cast<bool>(file)); }
    const int directory = sceKernelOpen(root.string().c_str(), SCE_KERNEL_O_RDONLY | SCE_KERNEL_O_DIRECTORY, 0);
    Require(directory >= 0);
    std::array<char, 512> buffer;
    buffer.fill('x');
    std::int64_t base = -1;
    Require(sceKernelGetdirentries(directory, buffer.data(), 8, &base) == SCE_KERNEL_ERROR_EINVAL);
    Require(std::all_of(buffer.begin(), buffer.end(), [](char byte) { return byte == 'x'; }));
    Require(sceKernelGetdents(directory, buffer.data(), 8) == SCE_KERNEL_ERROR_EINVAL);
    Require(sceKernelGetdents(directory, nullptr, 256) == SCE_KERNEL_ERROR_EFAULT);
    Require(sceKernelGetdents(directory, buffer.data(), 0) == SCE_KERNEL_ERROR_EINVAL);
    Require(sceKernelGetdents(directory, buffer.data(), -1) == SCE_KERNEL_ERROR_EINVAL);
    Require(sceKernelGetdents(directory, buffer.data(), 20) == 12);
    Require(std::strcmp(buffer.data() + 8, ".") == 0);
    Require(sceKernelGetdents(directory, buffer.data(), 12) == 12);
    Require(std::strcmp(buffer.data() + 8, "..") == 0);
    buffer.fill('x');
    Require(sceKernelGetdents(directory, buffer.data(), 12) == SCE_KERNEL_ERROR_EINVAL);
    Require(std::all_of(buffer.begin(), buffer.end(), [](char byte) { return byte == 'x'; }));
    const int recordSize = static_cast<int>((8 + name.size() + 1 + 3) & ~std::size_t{3});
    Require(sceKernelGetdirentries(directory, buffer.data(), recordSize, &base) == recordSize);
    Require(std::strcmp(buffer.data() + 8, name.c_str()) == 0);
    Require(static_cast<unsigned char>(buffer[7]) == name.size());
    std::uint16_t storedSize = 0;
    std::memcpy(&storedSize, buffer.data() + 4, sizeof(storedSize));
    Require(storedSize == recordSize);
    const auto rootUtf8 = root.u8string();
    const std::string enumeratedPath = std::string(rootUtf8.begin(), rootUtf8.end()) + '/' + (buffer.data() + 8);
    const int file = sceKernelOpen(enumeratedPath.c_str(), SCE_KERNEL_O_RDONLY, 0);
    Require(file >= 0 && sceKernelClose(file) == 0);
    Require(sceKernelGetdents(directory, buffer.data(), 8) == 0);
    Require(sceKernelGetdents(directory, buffer.data(), 256) == 0);
    Require(sceKernelClose(directory) == 0);
    Require(sceKernelGetdents(-1, buffer.data(), 256) == SCE_KERNEL_ERROR_EBADF);
    std::filesystem::remove_all(root);
}

static void CheckOversizedName() {
    const auto root = std::filesystem::temp_directory_path() /
        ("anyps5-directory-name-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    Require(std::filesystem::create_directory(root));
    std::u8string name;
    for (int i = 0; i < 86; ++i) name += u8"\u30bb";
    { std::ofstream file(root / std::filesystem::path(name)); Require(static_cast<bool>(file)); }
    const int directory = sceKernelOpen(root.string().c_str(), SCE_KERNEL_O_RDONLY | SCE_KERNEL_O_DIRECTORY, 0);
    Require(directory >= 0);
    std::array<char, 512> buffer;
    bool threw = false;
    try {
        sceKernelGetdents(directory, buffer.data(), static_cast<int>(buffer.size()));

    } catch (const std::runtime_error&) {
        threw = true;
    }
    Require(threw);
    Require(sceKernelClose(directory) == 0);
    std::filesystem::remove_all(root);
}

static void CheckReadOnlyDestination() {
    constexpr std::size_t page = 0x4000;
    const auto root = std::filesystem::temp_directory_path() /
        ("anyps5-directory-readonly-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    Require(std::filesystem::create_directory(root));
    { std::ofstream file(root / "entry.bin"); Require(static_cast<bool>(file)); }
    int directory = sceKernelOpen(root.string().c_str(), SCE_KERNEL_O_RDONLY | SCE_KERNEL_O_DIRECTORY, 0);
    Require(directory >= 0);
    std::int64_t physical = 0;
    Require(sceKernelAllocateDirectMemory(0, 0x7fffffffffll, page, 0, 0, &physical) == 0);
    void* mapping = nullptr;
    Require(sceKernelMapDirectMemory(&mapping, page, 3, 0, physical, 0) == 0 && mapping != nullptr);
    std::int64_t base = -1;
    Require(sceKernelGetdirentries(directory, static_cast<char*>(mapping), 4096, &base) > 0);
    Require(sceKernelClose(directory) == 0);
    directory = sceKernelOpen(root.string().c_str(), SCE_KERNEL_O_RDONLY | SCE_KERNEL_O_DIRECTORY, 0);
    Require(directory >= 0);
    Require(sceKernelMprotect(mapping, page, 1) == 0);
    Require(sceKernelGetdirentries(directory, static_cast<char*>(mapping), 4096, &base) == SCE_KERNEL_ERROR_EFAULT);
    Require(sceKernelGetdirentries(12345, static_cast<char*>(mapping), 4096, &base) == SCE_KERNEL_ERROR_EBADF);
    std::array<char, 512> buffer{};
    Require(sceKernelGetdirentries(directory, buffer.data(), static_cast<int>(buffer.size()),
        static_cast<std::int64_t*>(mapping)) == SCE_KERNEL_ERROR_EFAULT);
    Require(sceKernelMprotect(mapping, page, 3) == 0);
    Require(sceKernelGetdirentries(directory, buffer.data(), static_cast<int>(buffer.size()), &base) > 0);
    Require(sceKernelMunmap(reinterpret_cast<std::uint64_t>(mapping), page) == 0);
    Require(sceKernelReleaseDirectMemory(physical, page) == 0);
    Require(sceKernelClose(directory) == 0);
    std::filesystem::remove_all(root);
}

int main() {
    CheckName(u8"a-long-directory-entry-name.bin");
    CheckName(u8"caf\u00e9-music.bin");
    CheckName(u8"\u30bb\u30fc\u30d6-music.bin");
    CheckName(u8"\U0001f3ae-music.bin");
    std::u8string longest;
    for (int i = 0; i < 85; ++i) longest += u8"\u30bb";
    CheckName(longest);
    CheckOversizedName();
    CheckReadOnlyDestination();
}
