#include "prx/libc/include/general/VabiMacros.hpp"
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <string>

extern "C" {
int APS5_VABI tcgetattr_nid_postfix(int, void*);
int APS5_VABI tcsetattr_nid_postfix(int, int, const void*);
int APS5_VABI open_nid_postfix(const char*, int, int);
int APS5_VABI pipe_nid_postfix(int*);
int APS5_VABI socket_nid_postfix(int, int, int);
int APS5_VABI close_nid_postfix(int);
int* APS5_VABI __error_nid_postfix();
}

static void Check(bool value, int line) {
    if (!value) {
        std::fprintf(stderr, "terminal check failed at line %d, errno %d\n", line, *__error_nid_postfix());
        std::abort();
    }
}
#define Require(value) Check((value), __LINE__)

constexpr int Now = 0;
constexpr int Drain = 1;
constexpr int Flush = 2;
constexpr int Soft = 0x10;
constexpr int NotTerminal = 25;
constexpr int BadDescriptor = 9;
constexpr int InvalidArgument = 22;
constexpr int BadAddress = 14;
constexpr int AddressNotAvailable = 49;

static bool Fails(int result, int error) { return result == -1 && *__error_nid_postfix() == error; }

static void RequireNotTerminal(int descriptor) {
    unsigned char attributes[44];
    std::memset(attributes, 0xaa, sizeof(attributes));
    unsigned char untouched[44];
    std::memset(untouched, 0xaa, sizeof(untouched));
    *__error_nid_postfix() = 0;
    Require(Fails(tcgetattr_nid_postfix(descriptor, attributes), NotTerminal));
    Require(std::memcmp(attributes, untouched, sizeof(attributes)) == 0);
    for (const int action : {Now, Drain, Flush, Now | Soft, Flush | Soft}) {
        *__error_nid_postfix() = 0;
        Require(Fails(tcsetattr_nid_postfix(descriptor, action, attributes), NotTerminal));
    }
}

int main() {
    const auto name = "anyps5-terminal-test-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count());
    const int file = open_nid_postfix(name.c_str(), 0x202, 0644);
    Require(file >= 0);
    RequireNotTerminal(file);
    RequireNotTerminal(0);
    RequireNotTerminal(1);
    RequireNotTerminal(2);

    int ends[2] = {-1, -1};
    Require(pipe_nid_postfix(ends) == 0);
    RequireNotTerminal(ends[0]);
    RequireNotTerminal(ends[1]);

    const int socket = socket_nid_postfix(2, 2, 0);
    Require(socket >= 0);
    unsigned char untouched[44];
    std::memset(untouched, 0xaa, sizeof(untouched));
    unsigned char probe[44];
    std::memset(probe, 0xaa, sizeof(probe));
    Require(Fails(tcgetattr_nid_postfix(socket, probe), AddressNotAvailable));
    Require(std::memcmp(probe, untouched, sizeof(probe)) == 0);
    Require(Fails(tcsetattr_nid_postfix(socket, Flush, probe), AddressNotAvailable));

    unsigned char attributes[44]{};
    Require(Fails(tcsetattr_nid_postfix(file, 3, attributes), InvalidArgument));
    Require(Fails(tcsetattr_nid_postfix(file, -1, attributes), InvalidArgument));
    Require(Fails(tcsetattr_nid_postfix(file, Soft | 3, attributes), InvalidArgument));
    Require(Fails(tcsetattr_nid_postfix(-1, 3, attributes), InvalidArgument));
    Require(Fails(tcsetattr_nid_postfix(file, Now, nullptr), BadAddress));
    Require(Fails(tcsetattr_nid_postfix(-1, Now, nullptr), BadAddress));
    Require(Fails(tcgetattr_nid_postfix(-1, attributes), BadDescriptor));
    Require(Fails(tcsetattr_nid_postfix(-1, Now, attributes), BadDescriptor));

    Require(close_nid_postfix(socket) == 0);
    Require(Fails(tcgetattr_nid_postfix(socket, attributes), BadDescriptor));
    Require(Fails(tcsetattr_nid_postfix(socket, Now, attributes), BadDescriptor));
    Require(close_nid_postfix(ends[0]) == 0 && close_nid_postfix(ends[1]) == 0);
    Require(close_nid_postfix(file) == 0);
    Require(Fails(tcgetattr_nid_postfix(file, attributes), BadDescriptor));
    Require(Fails(tcsetattr_nid_postfix(file, Flush, attributes), BadDescriptor));
    std::filesystem::remove(name);
    return 0;
}
