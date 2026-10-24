#include "prx/libc/include/general/VabiMacros.hpp"
#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>

extern "C" {
int APS5_VABI sceKernelIsCEX(void);
int APS5_VABI sceKernelIsDevKit(void);
int APS5_VABI get_authinfo_nid_postfix(int pid, void* info);
int APS5_VABI sceKernelGetOpenPsId(void* id);
int APS5_VABI sceKernelGetOpenPsIdForSystem(void* id);
int APS5_VABI getpid_nid_postfix(void);
int* APS5_VABI __error_nid_postfix(void);
}

namespace {

constexpr int GuestEperm = 1;
constexpr int GuestEsrch = 3;

void Require(bool condition, const char* message) {
    if (!condition) {
        std::fprintf(stderr, "KERNEL SYS CONSOLE IDENTITY: %s\n", message);
        std::abort();
    }
}

void CheckConsoleKind() {
    Require(sceKernelIsCEX() == 1, "the console must be reported as a retail unit");
    Require(sceKernelIsDevKit() == 0, "the console must not be reported as a development kit");
}

void CheckAuthInfo() {
    std::array<std::uint64_t, 18> info;
    *__error_nid_postfix() = 0;
    Require(get_authinfo_nid_postfix(0, nullptr) == -1 && *__error_nid_postfix() == GuestEperm, "a null result must fail with EPERM");
    info.fill(0x5a5a5a5a5a5a5a5a);
    *__error_nid_postfix() = 0;
    Require(get_authinfo_nid_postfix(getpid_nid_postfix() + 1, info.data()) == -1 && *__error_nid_postfix() == GuestEsrch,
            "another process must fail with ESRCH");
    Require(info[0] == 0x5a5a5a5a5a5a5a5a, "a refused call must not write the result");
    for (const int pid : {0, getpid_nid_postfix()}) {
        info.fill(0x5a5a5a5a5a5a5a5a);
        Require(get_authinfo_nid_postfix(pid, info.data()) == 0, "the guest process must be described");
        Require(info[0] == 0 && info[1] == 0x2000000000000000, "the authority ID or first capability word is wrong");
        for (std::size_t i = 2; i < 17; ++i) Require(info[i] == 0, "the rest of the result must be cleared");
        Require(info[17] == 0x5a5a5a5a5a5a5a5a, "get_authinfo wrote past its 0x88-byte result");
    }
}

void CheckOpenPsId() {
    std::array<std::uint8_t, 16> plain{};
    std::array<std::uint8_t, 24> system;
    system.fill(0xa5);
    Require(sceKernelGetOpenPsId(plain.data()) == 0, "the OpenPSID must be readable");
    Require(sceKernelGetOpenPsIdForSystem(system.data()) == 0, "the system OpenPSID must be readable");
    Require(std::memcmp(plain.data(), system.data(), plain.size()) == 0, "both OpenPSID queries must agree");
    for (std::size_t i = plain.size(); i < system.size(); ++i) Require(system[i] == 0xa5, "the system OpenPSID wrote past 16 bytes");
}

}

int main() {
    CheckConsoleKind();
    CheckAuthInfo();
    CheckOpenPsId();
    return 0;
}
