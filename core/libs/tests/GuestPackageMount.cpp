#include "SceTypes.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iterator>
#include <set>
#include <string>
#include <vector>

extern "C" {
int APS5_VABI sceKernelOpen(const char*, int, unsigned short);
int APS5_VABI sceKernelClose(int);
std::int64_t APS5_VABI sceKernelRead(int, void*, std::size_t);
std::int64_t APS5_VABI sceKernelWrite(int, const void*, std::size_t);
std::int64_t APS5_VABI sceKernelLseek(int, std::int64_t, int);
std::int64_t APS5_VABI sceKernelPread(int, void*, std::size_t, std::int64_t);
int APS5_VABI sceKernelStat(const char*, FileStat*);
int APS5_VABI sceKernelFstat(int, FileStat*);
int APS5_VABI sceKernelGetdents(int, char*, int);
int APS5_VABI sceKernelUnlink(const char*);
int APS5_VABI sceKernelMkdir(const char*, std::uint16_t);
int APS5_VABI sceKernelRename(const char*, const char*);
int APS5_VABI sceKernelCheckReachability(const char*);
int APS5_VABI open_nid_postfix(const char*, int, int);
int APS5_VABI close_nid_postfix(int);
std::int64_t APS5_VABI pread_nid_postfix(int, void*, std::size_t, std::int64_t);
int* APS5_VABI __error_nid_postfix();
}

namespace {

constexpr int ReadOnly = 0;
constexpr int WriteOnly = 1;
constexpr int Create = 0x200;
constexpr int Directory = 0x20000;
constexpr int Erofs = static_cast<int>(0x8002001eu);
constexpr int Eexist = static_cast<int>(0x80020011u);
constexpr int Enoent = static_cast<int>(0x80020002u);
constexpr int Ebadf = static_cast<int>(0x80020009u);

void Check(const bool value, const int line) {
    if (!value) {
        std::fprintf(stderr, "Package mount check failed at line %d\n", line);
        std::exit(1);
    }
}
#define Require(value) Check((value), __LINE__)

std::vector<std::uint8_t> Expected(const std::string& relative) {
    std::ifstream stream("expected/" + relative, std::ios::binary);
    Require(static_cast<bool>(stream));
    return std::vector<std::uint8_t>(std::istreambuf_iterator<char>(stream), std::istreambuf_iterator<char>());
}

std::vector<std::uint8_t> ReadWhole(const char* path, const std::size_t chunk) {
    const int fd = sceKernelOpen(path, ReadOnly, 0);
    Require(fd >= 0x08000000 && fd <= 0x0fffffff);
    std::vector<std::uint8_t> bytes;
    std::vector<std::uint8_t> buffer(chunk);
    for (;;) {
        const auto read = sceKernelRead(fd, buffer.data(), buffer.size());
        Require(read >= 0);
        if (read == 0) break;
        bytes.insert(bytes.end(), buffer.begin(), buffer.begin() + read);
    }
    Require(sceKernelClose(fd) == 0);
    return bytes;
}

void Files() {
    for (const char* relative : {"eboot.bin", "data/random.bin", "data/sparse.bin", "data/zero.bin", "sce_sys/keystone", "sce_sys/param.json", "sce_sys/trophy2/npbind.dat"}) {
        const auto expected = Expected(relative);
        const std::string guest = std::string("/app0/") + relative;
        FileStat status{};
        Require(sceKernelStat(guest.c_str(), &status) == 0);
        Require((status.st_mode & 0xf000) == 0x8000 && status.st_size == static_cast<std::int64_t>(expected.size()));
        Require(ReadWhole(guest.c_str(), 7777) == expected);
        Require(ReadWhole(guest.c_str(), 0x50000) == expected);
        Require(sceKernelCheckReachability(guest.c_str()) == 0);
    }

    const auto random = Expected("data/random.bin");
    const int fd = sceKernelOpen("/app0/data/random.bin", ReadOnly, 0);
    std::vector<std::uint8_t> buffer(0x30000);
    for (const std::int64_t offset : {std::int64_t{0}, std::int64_t{0x3ffff}, std::int64_t{0x40000}, std::int64_t{0xf0000}, static_cast<std::int64_t>(random.size()) - 5}) {
        const auto read = sceKernelPread(fd, buffer.data(), buffer.size(), offset);
        const auto expected = std::min<std::int64_t>(static_cast<std::int64_t>(buffer.size()), static_cast<std::int64_t>(random.size()) - offset);
        Require(read == expected && std::memcmp(buffer.data(), random.data() + offset, static_cast<std::size_t>(read)) == 0);
        Require(pread_nid_postfix(fd, buffer.data(), 16, offset) == std::min<std::int64_t>(16, expected));
    }
    Require(sceKernelLseek(fd, -10, 2) == static_cast<std::int64_t>(random.size()) - 10);
    Require(sceKernelRead(fd, buffer.data(), 100) == 10 && std::memcmp(buffer.data(), random.data() + random.size() - 10, 10) == 0);
    Require(sceKernelRead(fd, buffer.data(), 100) == 0);
    Require(sceKernelLseek(fd, 0x12345, 0) == 0x12345);
    Require(sceKernelRead(fd, buffer.data(), 4) == 4 && std::memcmp(buffer.data(), random.data() + 0x12345, 4) == 0);
    FileStat status{};
    Require(sceKernelFstat(fd, &status) == 0 && status.st_size == static_cast<std::int64_t>(random.size()));
    Require(sceKernelWrite(fd, buffer.data(), 1) == Ebadf);
    Require(sceKernelClose(fd) == 0);
    Require(sceKernelClose(fd) == Ebadf);

    Require(ReadWhole("/app0/DATA/Random.BIN", 0x10000) == random);
    Require(ReadWhole("/app0/data/../data/random.bin", 0x10000) == random);
}

void Directories() {
    FileStat status{};
    Require(sceKernelStat("/app0", &status) == 0 && (status.st_mode & 0xf000) == 0x4000);
    Require(sceKernelStat("/app0/data", &status) == 0 && (status.st_mode & 0xf000) == 0x4000);
    const int fd = sceKernelOpen("/app0/data", ReadOnly | Directory, 0);
    Require(fd >= 0x08000000);
    std::set<std::string> names;
    char buffer[64];
    for (;;) {
        const int read = sceKernelGetdents(fd, buffer, sizeof(buffer));
        Require(read >= 0);
        if (read == 0) break;
        for (int offset = 0; offset < read;) {
            std::uint16_t length;
            std::memcpy(&length, buffer + offset + 4, sizeof(length));
            names.emplace(buffer + offset + 8, static_cast<std::size_t>(static_cast<std::uint8_t>(buffer[offset + 7])));
            offset += length;
        }
    }
    Require((names == std::set<std::string>{".", "..", "random.bin", "sparse.bin", "zero.bin"}));
    Require(sceKernelClose(fd) == 0);
    Require(sceKernelOpen("/app0/eboot.bin", ReadOnly | Directory, 0) == static_cast<int>(0x80020014u));
}

void ReadOnlyMount() {
    Require(sceKernelOpen("/app0/eboot.bin", WriteOnly, 0) == Erofs);
    Require(sceKernelOpen("/app0/eboot.bin", ReadOnly | Create, 0666) == Erofs);
    Require(sceKernelUnlink("/app0/eboot.bin") == Erofs);
    Require(sceKernelRename("/app0/eboot.bin", "/app0/other.bin") == Erofs);
    Require(sceKernelMkdir("/app0/data", 0777) == Eexist);
    Require(open_nid_postfix("/app0/data/zero.bin", WriteOnly, 0) == -1 && *__error_nid_postfix() == 30);
    const int fd = open_nid_postfix("/app0/data/zero.bin", ReadOnly, 0);
    Require(fd >= 0x08000000 && close_nid_postfix(fd) == 0);
}

void HostFallback() {
    FileStat status{};
    Require(sceKernelStat("/app0/sce_module/provider.prx.guest.prx", &status) == 0 && status.st_size == 5);
    Require(sceKernelStat("/app0/missing.bin", &status) == Enoent);
    Require(sceKernelCheckReachability("/app0/missing.bin") == Enoent);
    const int fd = sceKernelOpen("/app0/sce_module/provider.prx.guest.prx", ReadOnly, 0);
    Require(fd >= 0 && fd < 0x08000000);
    Require(sceKernelClose(fd) == 0);
}

}

int main() {
    Files();
    Directories();
    ReadOnlyMount();
    HostFallback();
    std::puts("Package mount tests passed");
    return 0;
}
