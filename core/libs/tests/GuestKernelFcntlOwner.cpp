#include "prx/libc/include/general/VabiMacros.hpp"
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <source_location>
#include <string>

extern "C" {
int* APS5_VABI __error_nid_postfix();
int APS5_VABI getpid_nid_postfix(void);
int APS5_VABI pipe_nid_postfix(int* descriptors);
int APS5_VABI dup_nid_postfix(int descriptor);
int APS5_VABI open_nid_postfix(const char* path, int flags, int mode);
int APS5_VABI close_nid_postfix(int descriptor);
int APS5_VABI fcntl_nid_postfix(int descriptor, int command, ...);
int APS5_VABI ioctl_nid_postfix(int descriptor, std::uint64_t request, void* argument);
}

namespace {

constexpr int Esrch = 3;
constexpr int Ebadf = 9;
constexpr int Efault = 14;
constexpr int Enotty = 25;
constexpr int GetOwner = 5;
constexpr int SetOwner = 6;
constexpr std::uint64_t SetOwnerRequest = 0x8004667c;
constexpr std::uint64_t GetOwnerRequest = 0x4004667b;
constexpr std::uint64_t NonblockingRequest = 0x8004667e;
constexpr int ReadOnly = 0;
constexpr int Directory = 0x20000;

void Require(bool condition, std::source_location location = std::source_location::current()) {
    if (condition) return;
    std::fprintf(stderr, "check failed at line %u\n", static_cast<unsigned>(location.line()));
    std::exit(1);
}

int Error() {
    return *__error_nid_postfix();
}

int IoctlOwner(int descriptor) {
    int owner = 12345;
    Require(ioctl_nid_postfix(descriptor, GetOwnerRequest, &owner) == 0);
    return owner;
}

void Pipes(int process) {
    int ends[2] = {-1, -1};
    Require(pipe_nid_postfix(ends) == 0);
    Require(fcntl_nid_postfix(ends[0], GetOwner) == 0);
    Require(fcntl_nid_postfix(ends[0], SetOwner, process) == 0 && fcntl_nid_postfix(ends[0], GetOwner) == process);
    const int duplicate = dup_nid_postfix(ends[0]);
    Require(duplicate >= 0 && fcntl_nid_postfix(duplicate, GetOwner) == process);
    Require(fcntl_nid_postfix(ends[1], GetOwner) == 0);
    Require(fcntl_nid_postfix(duplicate, SetOwner, -process) == 0 && fcntl_nid_postfix(ends[0], GetOwner) == -process);
    Require(fcntl_nid_postfix(ends[0], SetOwner, process + 1) == -1 && Error() == Esrch);
    Require(fcntl_nid_postfix(ends[0], SetOwner, -(process + 1)) == -1 && Error() == Esrch);
    Require(fcntl_nid_postfix(ends[0], GetOwner) == -process);
    Require(fcntl_nid_postfix(ends[0], SetOwner, 0) == 0 && fcntl_nid_postfix(duplicate, GetOwner) == 0);

    int owner = process;
    Require(ioctl_nid_postfix(ends[1], SetOwnerRequest, &owner) == 0 && IoctlOwner(ends[1]) == process);
    Require(fcntl_nid_postfix(ends[1], GetOwner) == process);
    owner = process + 1;
    Require(ioctl_nid_postfix(ends[1], SetOwnerRequest, &owner) == -1 && Error() == Esrch && IoctlOwner(ends[1]) == process);
    Require(ioctl_nid_postfix(ends[1], GetOwnerRequest, nullptr) == -1 && Error() == Efault);
    void* const unreadable = reinterpret_cast<void*>(std::uintptr_t{0x10});
    Require(ioctl_nid_postfix(ends[1], SetOwnerRequest, nullptr) == -1 && Error() == Efault && IoctlOwner(ends[1]) == process);
    Require(ioctl_nid_postfix(ends[1], SetOwnerRequest, unreadable) == -1 && Error() == Efault && IoctlOwner(ends[1]) == process);
    Require(ioctl_nid_postfix(ends[1], NonblockingRequest, unreadable) == -1 && Error() == Efault);
    Require(close_nid_postfix(duplicate) == 0 && close_nid_postfix(ends[0]) == 0 && close_nid_postfix(ends[1]) == 0);
}

void Vnodes(int process, const std::filesystem::path& root) {
    const auto path = (root / "file.txt").string();
    std::ofstream(path).put('x');
    const int file = open_nid_postfix(path.c_str(), ReadOnly, 0);
    const int directory = open_nid_postfix(root.string().c_str(), ReadOnly | Directory, 0);
    Require(file >= 0 && directory >= 0);
    for (const int descriptor : {file, directory}) {
        Require(fcntl_nid_postfix(descriptor, GetOwner) == -1 && Error() == Enotty);
        Require(fcntl_nid_postfix(descriptor, SetOwner, process) == -1 && Error() == Enotty);
        int owner = process;
        Require(ioctl_nid_postfix(descriptor, SetOwnerRequest, &owner) == -1 && Error() == Enotty);
        Require(ioctl_nid_postfix(descriptor, GetOwnerRequest, &owner) == -1 && Error() == Enotty);
    }
    Require(close_nid_postfix(directory) == 0 && close_nid_postfix(file) == 0);
    Require(fcntl_nid_postfix(file, GetOwner) == -1 && Error() == Ebadf);
    Require(fcntl_nid_postfix(file, SetOwner, process) == -1 && Error() == Ebadf);
}

}

int main() {
    const int process = getpid_nid_postfix();
    Require(process > 1);
    const auto root = std::filesystem::path("anyps5-fcntl-owner-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    Require(std::filesystem::create_directory(root));
    Pipes(process);
    Vnodes(process, root);
    std::filesystem::remove_all(root);
    std::puts("guest_kernel_fcntl_owner passed");
    return 0;
}
