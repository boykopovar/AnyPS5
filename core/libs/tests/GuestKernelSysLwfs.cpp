#include "prx/libc/include/Api.hpp"
#include "prx/libc/include/General.hpp"
#include "prx/libkernel/include/Api.hpp"
#include "prx/libkernel_sys/include/Api.hpp"
#include <chrono>
#include <cstdint>
#include <filesystem>
#include <cstdio>
#include <cstdlib>
#include <source_location>
#include <string>

namespace {

constexpr std::int64_t SceEbadf = static_cast<std::int32_t>(0x80020009u);
constexpr std::int64_t SceEnotty = static_cast<std::int32_t>(0x80020019u);
constexpr std::int64_t SceEaddrnotavail = static_cast<std::int32_t>(0x80020031u);

void Require(bool condition, std::source_location location = std::source_location::current()) {
    if (!condition) {
        std::fprintf(stderr, "lwfs check failed at line %u\n", location.line());
        std::abort();
    }
}

void ExpectAll(int fd, std::int64_t sceError, int guestErrno, std::source_location location = std::source_location::current()) {
    static const char data[16] = "lwfs";
    *__error_nid_postfix() = 0;
    Require(sceKernelLwfsWrite(fd, data, sizeof(data)) == sceError, location);
    Require(*__error_nid_postfix() == guestErrno, location);
    *__error_nid_postfix() = 0;
    Require(sceKernelLwfsWrite(fd, nullptr, 0) == sceError, location);
    *__error_nid_postfix() = 0;
    Require(sceKernelLwfsLseek(fd, 0, 0) == sceError, location);
    Require(*__error_nid_postfix() == guestErrno, location);
    *__error_nid_postfix() = 0;
    Require(sceKernelLwfsSetAttribute(fd, 0) == sceError, location);
    Require(*__error_nid_postfix() == guestErrno, location);
    *__error_nid_postfix() = 0;
    Require(sceKernelLwfsAllocateBlock(fd, 0) == sceError, location);
    Require(*__error_nid_postfix() == guestErrno, location);
}

void ExpectCompression(int fd, std::int64_t sceError, int guestErrno, std::source_location location = std::source_location::current()) {
    for (const int attribute : {0, 1}) {
        *__error_nid_postfix() = 0;
        Require(sceKernelSetCompressionAttribute(fd, attribute) == sceError, location);
        Require(*__error_nid_postfix() == guestErrno, location);
    }
    for (const int attribute : {-1, 2, 3, 255}) {
        *__error_nid_postfix() = 0;
        Require(sceKernelSetCompressionAttribute(fd, attribute) == static_cast<std::int32_t>(0x80020016u), location);
        Require(*__error_nid_postfix() == 0, location);
    }
}

} // namespace

int main() {
    const auto unique = std::chrono::steady_clock::now().time_since_epoch().count();
    const auto root = std::filesystem::temp_directory_path() / ("anyps5-lwfs-" + std::to_string(unique));
    Require(std::filesystem::create_directory(root));
    const auto host = root.string();
    AddPathAlias_nid_no_patch("/lwfs-test", host.c_str());
    const std::string path = "/lwfs-test/file";
    const int file = open_nid_postfix(path.c_str(), 0x602, 0600);
    Require(file >= 0);
    ExpectAll(file, SceEnotty, 25);
    ExpectCompression(file, SceEnotty, 25);
    for (const void* data : {static_cast<const void*>(nullptr), reinterpret_cast<const void*>(0x10)}) {
        *__error_nid_postfix() = 0;
        Require(sceKernelLwfsWrite(file, data, 16) == static_cast<std::int32_t>(0x8002000eu));
        Require(*__error_nid_postfix() == 14);
    }
    Require(close_nid_postfix(file) == 0);
    Require(unlink_nid_postfix(path.c_str()) == 0);
    std::filesystem::remove_all(root);

    ExpectAll(-1, SceEbadf, 9);
    ExpectAll(100000, SceEbadf, 9);

    int pipeFds[2]{-1, -1};
    Require(pipe_nid_postfix(pipeFds) == 0);
    ExpectAll(pipeFds[0], SceEnotty, 25);
    ExpectAll(pipeFds[1], SceEnotty, 25);
    Require(close_nid_postfix(pipeFds[0]) == 0);
    Require(close_nid_postfix(pipeFds[1]) == 0);
    ExpectAll(pipeFds[0], SceEbadf, 9);

    const int sock = socket_nid_postfix(2, 1, 0);
    Require(sock >= 0);
    ExpectAll(sock, SceEaddrnotavail, 49);
    Require(close_nid_postfix(sock) == 0);
    ExpectAll(sock, SceEbadf, 9);

    const int queue = kqueue_nid_postfix();
    Require(queue >= 0);
    ExpectAll(queue, SceEnotty, 25);
    Require(close_nid_postfix(queue) == 0);
    ExpectAll(queue, SceEbadf, 9);

    ExpectCompression(-1, SceEbadf, 9);
    ExpectCompression(100000, SceEbadf, 9);
    int compressionPipe[2]{-1, -1};
    Require(pipe_nid_postfix(compressionPipe) == 0);
    ExpectCompression(compressionPipe[0], SceEnotty, 25);
    Require(close_nid_postfix(compressionPipe[0]) == 0);
    Require(close_nid_postfix(compressionPipe[1]) == 0);
    ExpectCompression(compressionPipe[0], SceEbadf, 9);
    const int compressionSocket = socket_nid_postfix(2, 1, 0);
    Require(compressionSocket >= 0);
    ExpectCompression(compressionSocket, SceEaddrnotavail, 49);
    Require(close_nid_postfix(compressionSocket) == 0);
    const int compressionQueue = kqueue_nid_postfix();
    Require(compressionQueue >= 0);
    ExpectCompression(compressionQueue, SceEnotty, 25);
    Require(close_nid_postfix(compressionQueue) == 0);
    return 0;
}
