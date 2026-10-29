#include "prx/libkernel_sys/libkernel_sys.hpp"
#include "prx/libc/libc.hpp"
#include <array>
#include <bit>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>

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
    Require(sceKernelIsGenuineCEX() == 1, "the console must be reported as a genuine retail unit");
    Require(sceKernelIsGenuineDevKit() == 0, "the console must not be reported as a genuine development kit");
    Require(sceKernelIsGenuineTestKit() == 0, "the console must not be reported as a genuine test kit");
    Require(sceKernelIsTestKit() == 0, "the console must not be reported as a test kit");
    Require(sceKernelIsProspero() == 1, "the architecture must be reported as Prospero");
    Require(sceKernelIsKratos() == 0, "the architecture must not be reported as Kratos");
}

void CheckAuthInfo() {
    struct {
        KernelAuthInfo value;
        std::uint64_t guard;
    } info;
    *__error_nid_postfix() = 0;
    Require(get_authinfo_nid_postfix(0, nullptr) == -1 && *__error_nid_postfix() == GuestEperm, "a null result must fail with EPERM");
    std::memset(&info, 0x5a, sizeof(info));
    *__error_nid_postfix() = 0;
    Require(get_authinfo_nid_postfix(getpid_nid_postfix() + 1, &info.value) == -1 && *__error_nid_postfix() == GuestEsrch,
            "another process must fail with ESRCH");
    for (const auto byte : std::bit_cast<std::array<std::uint8_t, sizeof(info)>>(info))
        Require(byte == 0x5a, "a refused call must not write the result");
    for (const int pid : {0, getpid_nid_postfix()}) {
        std::memset(&info, 0x5a, sizeof(info));
        Require(get_authinfo_nid_postfix(pid, &info.value) == 0, "the guest process must be described");
        Require(info.value.programAuthorityId == 0 && info.value.capabilities[0] == 0x2000000000000000, "the authority ID or first capability word is wrong");
        const auto bytes = std::bit_cast<std::array<std::uint8_t, sizeof(KernelAuthInfo)>>(info.value);
        for (std::size_t i = 16; i < bytes.size(); ++i) Require(bytes[i] == 0, "the rest of the result must be cleared");
        Require(info.guard == 0x5a5a5a5a5a5a5a5a, "get_authinfo wrote past its 0x88-byte result");
    }
}

void CheckOpenPsId() {
    std::array<std::uint8_t, 16> plain{};
    std::array<std::uint8_t, 24> system;
    system.fill(0xa5);
    *__error_nid_postfix() = GuestEsrch;
    Require(sceKernelGetOpenPsIdForSystem(nullptr) == 22, "a null system OpenPSID must return positive EINVAL");
    Require(*__error_nid_postfix() == GuestEsrch, "a null system OpenPSID must leave errno unchanged");
    for (auto byte : system) Require(byte == 0xa5, "a null system OpenPSID must not write output");
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
