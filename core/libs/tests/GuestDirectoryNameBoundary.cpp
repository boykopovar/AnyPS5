#include "prx/libkernel/include/Api.hpp"
#include "prx/libkernel/File/include/File.hpp"
#include "prx/libkernel/File/include/FileFlags.hpp"
#include "prx/libkernel/KernelErrors.hpp"
#include "prx/libc/include/GuestErrno.hpp"
#include <algorithm>
#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <set>
#include <stdexcept>
#include <string>
#include <system_error>
#include <sys/stat.h>

#define Require(value) do { if (!(value)) throw std::runtime_error("Directory name boundary check failed at line " + std::to_string(__LINE__)); } while (false)

struct DirectoryNameBoundaryFixture {
    std::string root = "anyps5-directory-name-boundary-XXXXXX";
    int directory = -1;

    DirectoryNameBoundaryFixture() {
        Require(::mkdtemp(root.data()) != nullptr);
    }

    ~DirectoryNameBoundaryFixture() {
        if (directory >= 0) sceKernelClose(directory);
        std::error_code error;
        std::filesystem::remove_all(root, error);
    }
};

static void CheckDirectoryNameBoundary(int directory, const std::string& longName, bool posix) {
    Require(sceKernelLseek(directory, 0, SEEK_SET) == 0);
    const auto read = [&](char* buffer, int size, std::int64_t* base) {
        if (!posix) return sceKernelGetdirentries(directory, buffer, size, base);
        *__error_nid_postfix() = 1234;
        const int result = getdirentries_nid_postfix(directory, buffer, size, base);
        Require(*__error_nid_postfix() == (result < 0 ? 22 : 1234));
        return result;
    };
    const int invalid = posix ? -1 : SCE_KERNEL_ERROR_EINVAL;
    std::set<std::string> names;
    const auto collect = [&](const char* buffer, int size) {
        for (int offset = 0; offset < size;) {
            Require(size - offset >= 8);
            std::uint16_t recordLength = 0;
            std::memcpy(&recordLength, buffer + offset + 4, sizeof(recordLength));
            const auto nameLength = static_cast<std::uint8_t>(buffer[offset + 7]);
            Require(recordLength >= 12 && recordLength <= size - offset);
            Require(recordLength == ((8 + nameLength + 1 + 3) & ~3));
            Require(buffer[offset + 8 + nameLength] == '\0');
            const std::string name(buffer + offset + 8, nameLength);
            Require(name == "." || name == ".." || name == longName);
            Require(names.insert(name).second);
            offset += recordLength;
        }
    };

    bool positioned = false;
    std::int64_t base = -1;
    for (int attempt = 0; attempt < 3; ++attempt) {
        std::array<char, 12> small{};
        const int result = read(small.data(), static_cast<int>(small.size()), &base);
        if (result == invalid) {
            positioned = true;
            break;
        }
        Require(result == 12);
        collect(small.data(), result);
    }
    Require(positioned);
    const auto startingCookie = sceKernelLseek(directory, 0, SEEK_CUR);
    Require(startingCookie >= 0);
    std::array<char, 280> buffer;
    buffer.fill('x');
    Require(read(buffer.data(), 263, &base) == invalid);
    Require(sceKernelLseek(directory, 0, SEEK_CUR) == startingCookie);
    Require(std::all_of(buffer.begin(), buffer.end(), [](char byte) { return byte == 'x'; }));

    Require(read(buffer.data(), 264, &base) == 264);
    Require(base == startingCookie);
    std::uint16_t recordLength = 0;
    std::memcpy(&recordLength, buffer.data() + 4, sizeof(recordLength));
    Require(recordLength == 264);
    Require(static_cast<std::uint8_t>(buffer[7]) == 255);
    Require(std::memcmp(buffer.data() + 8, longName.data(), 255) == 0);
    Require(buffer[263] == '\0');
    Require(std::all_of(buffer.begin() + 264, buffer.end(), [](char byte) { return byte == 'x'; }));
    collect(buffer.data(), 264);

    Require(sceKernelLseek(directory, base, SEEK_SET) == startingCookie);
    std::array<char, 280> replay;
    replay.fill('x');
    Require(read(replay.data(), 264, &base) == 264);
    Require(base == startingCookie);
    Require(std::memcmp(buffer.data(), replay.data(), 264) == 0);
    Require(std::all_of(replay.begin() + 264, replay.end(), [](char byte) { return byte == 'x'; }));

    bool finished = false;
    for (int attempt = 0; attempt < 3; ++attempt) {
        const int result = read(buffer.data(), 264, &base);
        if (result == 0) {
            finished = true;
            break;
        }
        Require(result > 0 && result <= 264);
        collect(buffer.data(), result);
    }
    Require(finished);
    const std::set<std::string> expected{".", "..", longName};
    Require(names == expected);
}

int main() {
    try {
        DirectoryNameBoundaryFixture fixture;
        const std::string longName(255, 'n');
        {
            std::ofstream file(std::filesystem::path(fixture.root) / longName);
            Require(static_cast<bool>(file));
        }
        fixture.directory = sceKernelOpen(fixture.root.c_str(), SCE_KERNEL_O_RDONLY | SCE_KERNEL_O_DIRECTORY, 0);
        Require(fixture.directory >= 0);
        struct stat opened{};
        struct stat native{};
        Require(::fstat(fixture.directory, &opened) == 0);
        Require(::stat(fixture.root.c_str(), &native) == 0);
        Require(S_ISDIR(opened.st_mode));
        Require(opened.st_dev == native.st_dev && opened.st_ino == native.st_ino);
        CheckDirectoryNameBoundary(fixture.directory, longName, false);
        CheckDirectoryNameBoundary(fixture.directory, longName, true);
        Require(sceKernelClose(fixture.directory) == 0);
        fixture.directory = -1;
        Require(std::filesystem::remove_all(fixture.root) == 2);
    } catch (const std::exception& error) {
        std::fprintf(stderr, "%s\n", error.what());
        return 1;
    }
}
