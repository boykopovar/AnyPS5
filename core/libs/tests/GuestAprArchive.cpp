#include "AppArchiveFixture.hpp"
#include "prx/libc/include/Api.hpp"
#include "prx/libc/include/AppArchive.hpp"
#include "prx/libkernel/include/Api.hpp"
#include "prx/libkernel/Apr/include/AprCommandBuffer.hpp"

#include <algorithm>
#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <map>
#include <string>
#include <system_error>
#include <vector>

namespace {

constexpr std::size_t BigSize = 200000;

void Require(bool condition, const char* message) {
    if (condition) return;
    std::fprintf(stderr, "%s\n", message);
    std::exit(1);
}

int ReadStatus(std::uint32_t fileId, void* destination, std::uint64_t bytes, std::uint64_t offset) {
    const Apr::ReadFileCommand command{{Apr::Opcode::ReadFile, sizeof(Apr::ReadFileCommand)}, fileId, 0,
        reinterpret_cast<std::uint64_t>(destination), bytes, offset};
    alignas(8) std::array<std::uint8_t, sizeof(command)> memory{};
    std::memcpy(memory.data(), &command, sizeof(command));
    Apr::CommandBufferObject buffer{Apr::BufferType::Generic, 0, sizeof(memory), 1, sizeof(memory), memory.data()};
    std::uint32_t result = 0xFFFFFFFFu;
    std::uint32_t id = 0;
    Require(sceKernelAprSubmitCommandBufferAndGetResult(&buffer, 0, &result, &id) == 0 && id != 0, "APR read submission failed");
    return sceKernelAprWaitCommandBuffer(id);
}

}

int main() {
    const auto root = AppArchiveFixture::EnterFreshRoot("anyps5-apr-archive");
    const auto expected = AppArchiveFixture::Pattern(BigSize);
    AppArchiveFixture::WriteArchive(root / "app0.zar", {{"Cache/All.bin", expected}, {"Cache/Small.txt", {'a', 'b', 'c'}}});
    { std::ofstream onDisk(root / "app0" / "Disk.txt"); onDisk << "disk!"; }
    Require(AppArchivePath_nid_no_patch() == root / "app0.zar", "the archive beside app0 was not opened");

    const char* paths[] = {"/app0/cache/all.bin", "/app0/Cache/Small.txt", "/app0/Disk.txt"};
    std::array<std::uint32_t, 3> ids{};
    std::array<std::uint64_t, 3> sizes{};
    std::uint32_t failed = 0xFFFFFFFFu;
    Require(sceKernelAprResolveFilepathsToIdsAndFileSizes(paths, 3, ids.data(), sizes.data(), &failed) == 0, "APR did not resolve archived and disk files together");
    Require(sizes == std::array<std::uint64_t, 3>{BigSize, 3, 5}, "APR reported wrong sizes for archived or disk files");

    std::vector<char> read(BigSize, 0);
    Require(ReadStatus(ids[0], read.data(), read.size(), 0) == 0 && read == expected, "APR did not read a whole archived file");
    std::array<char, 16> slice{};
    Require(ReadStatus(ids[0], slice.data(), slice.size(), 65530) == 0 && std::equal(slice.begin(), slice.end(), expected.begin() + 65530), "APR read wrong bytes across an archive block");
    std::array<char, 8> tail{};
    tail.fill('x');
    Require(ReadStatus(ids[1], tail.data(), tail.size(), 1) == 0 && std::string(tail.data(), 2) == "bc" && std::all_of(tail.begin() + 2, tail.end(), [](char c) { return c == 0; }), "APR did not zero the part of a read past an archived file's end");
    std::array<char, 5> disk{};
    Require(ReadStatus(ids[2], disk.data(), disk.size(), 0) == 0 && std::string(disk.data(), disk.size()) == "disk!", "APR stopped reading a file on disk");

    FileStat stat{};
    Require(sceKernelAprGetFileStat(ids[0], &stat) == 0 && (stat.st_mode & 0777) == 0444 && stat.st_size == static_cast<std::int64_t>(BigSize), "APR stat of an archived file did not report its read-only mode and size");

    const char* directory = "/app0/Cache";
    std::uint32_t directoryId = 0;
    failed = 0xFFFFFFFFu;
    Require(sceKernelAprResolveFilepathsToIds(&directory, 1, &directoryId, &failed) == -1 && failed == 0, "APR resolved an archived directory as a file");

    std::error_code error;
    std::filesystem::current_path(std::filesystem::temp_directory_path(), error);
    std::filesystem::remove_all(root, error);
    return 0;
}
