#include "prx/libkernel/include/Api.hpp"
#include "prx/libkernel/File/include/File.hpp"
#include "prx/libc/include/Api.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include "prx/libkernel/File/include/FileFlags.hpp"
#include <barrier>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <set>
#include <string>
#include <thread>
#include <vector>
#ifdef _WIN32
#include <windows.h>
#endif

static void Check(bool value, int line) {
    if (!value) {
        std::fprintf(stderr, "Removed directory entry check failed at line %d\n", line);
        std::abort();
    }
}
#define Require(value) Check((value), __LINE__)

static std::set<std::string> ReadAll(int directory, std::vector<char>& buffer) {
    std::set<std::string> names;
    int read = 0;
    while ((read = sceKernelGetdents(directory, buffer.data(), static_cast<int>(buffer.size()))) > 0) {
        for (int offset = 0; offset < read;) {
            std::uint16_t recordLength = 0;
            std::memcpy(&recordLength, buffer.data() + offset + 4, sizeof(recordLength));
            const auto nameLength = static_cast<std::uint8_t>(buffer[offset + 7]);
            const std::string name(buffer.data() + offset + 8);
            Require(name.size() == nameLength && recordLength == ((8 + nameLength + 1 + 3) & ~3));
            Require(offset + recordLength <= read);
            Require(names.insert(name).second);
            offset += recordLength;
        }
    }
    Require(read == 0);
    return names;
}

static int OpenDirectory(const std::filesystem::path& root) {
    const int directory = sceKernelOpen(root.string().c_str(), SCE_KERNEL_O_RDONLY | SCE_KERNEL_O_DIRECTORY, 0);
    Require(directory >= 0);
    return directory;
}

static std::string Name(const char* prefix, unsigned round, unsigned index) {
    return std::string(prefix) + std::to_string(round) + "-" + std::to_string(index);
}

int main() {
    constexpr unsigned Rounds = 12;
    constexpr unsigned Kept = 8;
    constexpr unsigned Removed = 256;
    const auto root = std::filesystem::path("anyps5-directory-removed-entries-test-" +
        std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    Require(std::filesystem::create_directory(root));
    std::vector<char> buffer(1 << 16);
#ifdef _WIN32
    const auto pending = root / "pending.txt";
    std::ofstream(pending) << "pending";
    const auto handle = CreateFileW(pending.c_str(), DELETE, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
        nullptr, OPEN_EXISTING, 0, nullptr);
    Require(handle != INVALID_HANDLE_VALUE);
    FILE_DISPOSITION_INFO disposition{TRUE};
    Require(SetFileInformationByHandle(handle, FileDispositionInfo, &disposition, sizeof(disposition)) != 0);
    FileStat status{};
    Require(sceKernelStat(pending.string().c_str(), &status) == static_cast<int>(0x80020002u));
    Require(lstat_nid_postfix(pending.string().c_str(), &status) == -1 && *__error_nid_postfix() == 2);
    const int pendingDirectory = OpenDirectory(root);
    Require(ReadAll(pendingDirectory, buffer) == std::set<std::string>({".", ".."}));
    Require(sceKernelClose(pendingDirectory) == 0);
    Require(CloseHandle(handle) != 0 && !std::filesystem::exists(pending));
#endif
    unsigned partialListings = 0;
    for (unsigned round = 0; round < Rounds; ++round) {
        for (unsigned index = 0; index < Kept; ++index) std::ofstream(root / Name("kept-", round, index)) << "kept";
        for (unsigned index = 0; index < Removed; ++index) std::ofstream(root / Name("removed-", round, index)) << "removed";
        const int directory = OpenDirectory(root);
        std::barrier start(2);
        std::thread remover([&] {
            start.arrive_and_wait();
            for (unsigned index = Removed; index-- > 0;) Require(std::filesystem::remove(root / Name("removed-", round, index)));
        });
        start.arrive_and_wait();
        const auto names = ReadAll(directory, buffer);
        remover.join();
        Require(sceKernelClose(directory) == 0);
        Require(names.contains(".") && names.contains(".."));
        for (unsigned index = 0; index < Kept; ++index) Require(names.contains(Name("kept-", round, index)));
        unsigned listedRemoved = 0;
        for (unsigned index = 0; index < Removed; ++index) listedRemoved += names.contains(Name("removed-", round, index)) ? 1u : 0u;
        Require(names.size() == 2 + Kept * (round + 1) + listedRemoved);
        if (listedRemoved != 0 && listedRemoved != Removed) ++partialListings;
    }
    std::printf("rounds=%u partial listings=%u\n", Rounds, partialListings);
    std::filesystem::remove_all(root);
}
