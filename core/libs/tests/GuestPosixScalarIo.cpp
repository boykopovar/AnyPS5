#include "SceTypes.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include "prx/libkernel/File/include/FileFlags.hpp"
#include <cerrno>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <exception>
#include <filesystem>
#include <fstream>
#include <limits>
#include <stdexcept>
#include <string>
#ifdef _WIN32
#include <io.h>
#else
#include <fcntl.h>
#include <unistd.h>
#endif

extern "C" {
int APS5_VABI open_nid_postfix(const char*, int, int);
int APS5_VABI close_nid_postfix(int);
int APS5_VABI pipe_nid_postfix(int*);
int* APS5_VABI __error_nid_postfix();
std::int64_t APS5_VABI read_nid_postfix(int, void*, std::uint64_t);
std::int64_t APS5_VABI write_nid_postfix(int, const char*, std::int64_t);
std::int64_t APS5_VABI _read_nid_postfix(int, void*, std::size_t);
std::int64_t APS5_VABI _write_nid_postfix(int, const void*, std::size_t);
std::int64_t APS5_VABI lseek_nid_postfix(int, std::int64_t, int);
std::int64_t APS5_VABI pread_nid_postfix(int, void*, std::size_t, std::int64_t);
std::int64_t APS5_VABI pwrite_nid_disambig1_nid_postfix(int, const void*, std::size_t, std::int64_t);
std::int64_t APS5_VABI sceKernelRead(int, void*, std::size_t);
std::int64_t APS5_VABI sceKernelWrite(int, const void*, std::size_t);
std::int64_t APS5_VABI sceKernelLseek(int, std::int64_t, int);
int APS5_VABI sceKernelStat(const char*, FileStat*);
int APS5_VABI sceKernelUnlink(const char*);
int APS5_VABI sceKernelTruncate_nid_postfix(const char*, std::int64_t);
int APS5_VABI sceKernelUtimes_nid_postfix(const char*, const void*);
int APS5_VABI sceKernelMkdir(const char*, std::uint16_t);
int APS5_VABI sceKernelRmdir(const char*);
int APS5_VABI sceKernelRename(const char*, const char*);
}

static void Check(bool value, int line) {
    if (!value) {
        std::fprintf(stderr, "POSIX scalar I/O check failed at line %d (guest errno %d)\n", line, *__error_nid_postfix());
        std::abort();
    }
}

#define Require(value) Check((value), __LINE__)

#ifndef _WIN32
static constexpr int GuestEagain = 35;
static constexpr int GuestEisdir = 21;
#endif

template <typename Operation>
static void CheckPosixFailure(Operation operation, int expectedError, int line) {
    std::int64_t result = 0;
    try {
        result = operation();
    } catch (const std::exception& error) {
        std::fprintf(stderr, "POSIX syscall threw instead of returning -1: %s\n", error.what());
        Check(false, line);
        return;
    }
    Check(result == -1 && *__error_nid_postfix() == expectedError, line);
}

#define RequirePosixFailure(operation, error) CheckPosixFailure((operation), (error), __LINE__)

int main() {
    const auto root = std::filesystem::path("anyps5-posix-scalar-io-" +
        std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    Require(std::filesystem::create_directory(root));
    const auto file = root / "scalar.txt";
    { std::ofstream stream(file, std::ios::binary); stream << "posix"; }
    const auto fileName = file.string();
    const int descriptor = open_nid_postfix(fileName.c_str(), SCE_KERNEL_O_RDWR, 0);
    Require(descriptor >= 0);
    int* guestErrno = __error_nid_postfix();
    char buffer[3]{};
    *guestErrno = EACCES;
    Require(lseek_nid_postfix(descriptor, 0, SEEK_SET) == 0);
    Require(*guestErrno == EACCES);
    *guestErrno = EACCES;
    Require(read_nid_postfix(descriptor, buffer, 1) == 1 && buffer[0] == 'p');
    Require(*guestErrno == EACCES);
    *guestErrno = EACCES;
    Require(_read_nid_postfix(descriptor, buffer + 1, 1) == 1 && buffer[1] == 'o');
    Require(*guestErrno == EACCES);
    constexpr std::int64_t sparseOffset = (std::int64_t{1} << 32) + 17;
    *guestErrno = EACCES;
    Require(lseek_nid_postfix(descriptor, sparseOffset, SEEK_SET) == sparseOffset);
    Require(*guestErrno == EACCES);
    *guestErrno = EACCES;
    Require(lseek_nid_postfix(descriptor, 0, SEEK_SET) == 0);
    Require(*guestErrno == EACCES);
    const char payload[] = {'P', 'S'};
    *guestErrno = EACCES;
    Require(write_nid_postfix(descriptor, payload, 1) == 1);
    Require(*guestErrno == EACCES);
    *guestErrno = EACCES;
    Require(_write_nid_postfix(descriptor, payload + 1, 1) == 1);
    Require(*guestErrno == EACCES);
    *guestErrno = EACCES;
    Require(lseek_nid_postfix(descriptor, 0, SEEK_SET) == 0);
    Require(*guestErrno == EACCES);
    Require(_read_nid_postfix(descriptor, buffer, 2) == 2 && buffer[0] == 'P' && buffer[1] == 'S');
    RequirePosixFailure([&] { return read_nid_postfix(-1, buffer, 1); }, EBADF);
    RequirePosixFailure([&] { return _read_nid_postfix(-1, buffer, 1); }, EBADF);
    RequirePosixFailure([&] { return write_nid_postfix(-1, payload, 1); }, EBADF);
    RequirePosixFailure([&] { return _write_nid_postfix(-1, payload, 1); }, EBADF);
    RequirePosixFailure([&] { return lseek_nid_postfix(-1, 0, SEEK_SET); }, EBADF);
    RequirePosixFailure([&] { return lseek_nid_postfix(descriptor, 0, -1); }, EINVAL);
    RequirePosixFailure([&] { return lseek_nid_postfix(descriptor, 0, 3); }, EINVAL);
    RequirePosixFailure([&] { return read_nid_postfix(descriptor, nullptr, 1); }, EFAULT);
    RequirePosixFailure([&] { return _read_nid_postfix(descriptor, nullptr, 1); }, EFAULT);
    RequirePosixFailure([&] { return write_nid_postfix(descriptor, nullptr, 1); }, EFAULT);
    RequirePosixFailure([&] { return _write_nid_postfix(descriptor, nullptr, 1); }, EFAULT);
    Require(read_nid_postfix(descriptor, nullptr, 0) == 0);
    Require(_read_nid_postfix(descriptor, nullptr, 0) == 0);
    Require(write_nid_postfix(descriptor, nullptr, 0) == 0);
    Require(_write_nid_postfix(descriptor, nullptr, 0) == 0);
    RequirePosixFailure([&] { return read_nid_postfix(-1, nullptr, 0); }, EBADF);
    RequirePosixFailure([&] { return _read_nid_postfix(-1, nullptr, 0); }, EBADF);
    RequirePosixFailure([&] { return write_nid_postfix(-1, nullptr, 0); }, EBADF);
    RequirePosixFailure([&] { return _write_nid_postfix(-1, nullptr, 0); }, EBADF);
    constexpr auto oversized = static_cast<std::size_t>(std::numeric_limits<std::int64_t>::max()) + 1;
    RequirePosixFailure([&] { return read_nid_postfix(descriptor, buffer, oversized); }, EINVAL);
    RequirePosixFailure([&] { return _read_nid_postfix(descriptor, buffer, oversized); }, EINVAL);
    RequirePosixFailure([&] { return write_nid_postfix(descriptor, payload, -1); }, EINVAL);
    RequirePosixFailure([&] { return write_nid_postfix(descriptor, payload, std::numeric_limits<std::int64_t>::min()); }, EINVAL);
    RequirePosixFailure([&] { return _write_nid_postfix(descriptor, payload, oversized); }, EINVAL);
    RequirePosixFailure([&] { return pread_nid_postfix(descriptor, buffer, oversized, 0); }, EINVAL);
    RequirePosixFailure([&] { return pwrite_nid_disambig1_nid_postfix(descriptor, payload, oversized, 0); }, EINVAL);
    RequirePosixFailure([&] { return read_nid_postfix(-1, buffer, oversized); }, EINVAL);
    RequirePosixFailure([&] { return write_nid_postfix(-1, payload, -1); }, EINVAL);
#ifdef _WIN32
    errno = EACCES;
    bool limitDiagnosticPreserved = false;
    try {
        write_nid_postfix(descriptor, payload, static_cast<std::int64_t>(std::numeric_limits<unsigned int>::max()) + 1);
    } catch (const std::runtime_error& error) {
        limitDiagnosticPreserved = std::string(error.what()).find("nbytes exceeds platform limit") != std::string::npos;
    }
    Require(limitDiagnosticPreserved && errno == EACCES);
#endif
    char direct[1]{};
    Require(sceKernelRead(-1, direct, 1) == static_cast<std::int32_t>(0x80020009u));
    Require(sceKernelWrite(-1, direct, 1) == static_cast<std::int32_t>(0x80020009u));
    Require(sceKernelLseek(-1, 0, 0) == static_cast<std::int32_t>(0x80020009u));
    Require(sceKernelRead(descriptor, nullptr, 1) == static_cast<std::int32_t>(0x8002000eu));
    Require(sceKernelWrite(descriptor, nullptr, 1) == static_cast<std::int32_t>(0x8002000eu));
    Require(sceKernelRead(descriptor, nullptr, 0) == 0);
    Require(sceKernelWrite(descriptor, nullptr, 0) == 0);
    Require(sceKernelRead(-1, nullptr, 0) == static_cast<std::int32_t>(0x80020009u));
    Require(sceKernelWrite(-1, nullptr, 0) == static_cast<std::int32_t>(0x80020009u));
    FileStat nullStat{};
    Require(sceKernelStat(nullptr, &nullStat) == static_cast<std::int32_t>(0x8002000eu));
    Require(sceKernelStat(nullptr, nullptr) == static_cast<std::int32_t>(0x8002000eu));
    Require(sceKernelUnlink(nullptr) == static_cast<std::int32_t>(0x8002000eu));
    Require(sceKernelTruncate_nid_postfix(nullptr, 0) == static_cast<std::int32_t>(0x8002000eu));
    Require(sceKernelUtimes_nid_postfix(nullptr, nullptr) == static_cast<std::int32_t>(0x8002000eu));
    Require(sceKernelMkdir(nullptr, 0777) == static_cast<std::int32_t>(0x8002000eu));
    Require(sceKernelRmdir(nullptr) == static_cast<std::int32_t>(0x8002000eu));
    Require(sceKernelRename(nullptr, "dest") == static_cast<std::int32_t>(0x8002000eu));
    Require(sceKernelRename("src", nullptr) == static_cast<std::int32_t>(0x80020002u));
    Require(sceKernelRename(nullptr, nullptr) == static_cast<std::int32_t>(0x8002000eu));
    Require(close_nid_postfix(descriptor) == 0);
    const int readOnly = open_nid_postfix(fileName.c_str(), SCE_KERNEL_O_RDONLY, 0);
    Require(readOnly >= 0);
    RequirePosixFailure([&] { return write_nid_postfix(readOnly, payload, 1); }, EBADF);
    RequirePosixFailure([&] { return _write_nid_postfix(readOnly, payload, 1); }, EBADF);
    Require(close_nid_postfix(readOnly) == 0);
    const int writeOnly = open_nid_postfix(fileName.c_str(), SCE_KERNEL_O_WRONLY, 0);
    Require(writeOnly >= 0);
    RequirePosixFailure([&] { return read_nid_postfix(writeOnly, buffer, 1); }, EBADF);
    RequirePosixFailure([&] { return _read_nid_postfix(writeOnly, buffer, 1); }, EBADF);
    Require(close_nid_postfix(writeOnly) == 0);
    int descriptors[2] = {-1, -1};
    Require(pipe_nid_postfix(descriptors) == 0);
    const char pipePayload[] = {'p', 'i', 'p', 'e'};
    Require(write_nid_postfix(descriptors[1], pipePayload, sizeof(pipePayload)) == sizeof(pipePayload));
    Require(close_nid_postfix(descriptors[1]) == 0);
    char pipeBuffer[sizeof(pipePayload)]{};
    Require(_read_nid_postfix(descriptors[0], pipeBuffer, sizeof(pipeBuffer)) == sizeof(pipeBuffer));
    Require(std::memcmp(pipeBuffer, pipePayload, sizeof(pipePayload)) == 0);
    Require(close_nid_postfix(descriptors[0]) == 0);
    int capacityDescriptors[2] = {-1, -1};
    Require(pipe_nid_postfix(capacityDescriptors) == 0);
    std::string largePayload(8192, 'x');
    Require(write_nid_postfix(capacityDescriptors[1], largePayload.data(), largePayload.size()) == static_cast<std::int64_t>(largePayload.size()));
    Require(close_nid_postfix(capacityDescriptors[1]) == 0);
    std::string largeBuffer(8192, '\0');
    Require(_read_nid_postfix(capacityDescriptors[0], largeBuffer.data(), largeBuffer.size()) == static_cast<std::int64_t>(largeBuffer.size()));
    Require(largeBuffer == largePayload);
    Require(close_nid_postfix(capacityDescriptors[0]) == 0);
#ifndef _WIN32
    int nonBlockingPipe[2] = {-1, -1};
    Require(pipe_nid_postfix(nonBlockingPipe) == 0);
    const int pipeFlags = fcntl(nonBlockingPipe[0], F_GETFL);
    Require(pipeFlags >= 0 && fcntl(nonBlockingPipe[0], F_SETFL, pipeFlags | O_NONBLOCK) == 0);
    RequirePosixFailure([&] { return read_nid_postfix(nonBlockingPipe[0], buffer, 1); }, GuestEagain);
    RequirePosixFailure([&] { return _read_nid_postfix(nonBlockingPipe[0], buffer, 1); }, GuestEagain);
    Require(sceKernelRead(nonBlockingPipe[0], direct, 1) == static_cast<std::int32_t>(0x80020000u | GuestEagain));
    Require(close_nid_postfix(nonBlockingPipe[0]) == 0);
    Require(close_nid_postfix(nonBlockingPipe[1]) == 0);
#endif
#ifndef _WIN32
    const std::string rootName = root.string();
    const int directory = open_nid_postfix(rootName.c_str(), SCE_KERNEL_O_RDONLY, 0);
    Require(directory >= 0);
    RequirePosixFailure([&] { return read_nid_postfix(directory, buffer, 1); }, GuestEisdir);
    RequirePosixFailure([&] { return _read_nid_postfix(directory, buffer, 1); }, GuestEisdir);
    Require(close_nid_postfix(directory) == 0);
#endif
    Require(std::filesystem::remove_all(root) == 2);
    return 0;
}
