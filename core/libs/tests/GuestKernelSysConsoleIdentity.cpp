#include "prx/libkernel_sys/libkernel_sys.hpp"
#include "prx/libc/libc.hpp"
#include <array>
#include <bit>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <random>
#include <string>
#include <string_view>

namespace {

constexpr int GuestEperm = 1;
constexpr int GuestEsrch = 3;
constexpr int GuestEfault = 14;

void Require(bool condition, const char* message) {
    if (!condition) {
        std::fprintf(stderr, "KERNEL SYS CONSOLE IDENTITY: %s\n", message);
        std::abort();
    }
}

void CheckConsoleKind(bool ps4) {
    Require(sceKernelIsCEX() == 1, "the console must be reported as a retail unit");
    Require(sceKernelIsDevKit() == 0, "the console must not be reported as a development kit");
    Require(sceKernelIsGenuineCEX() == 1, "the console must be reported as a genuine retail unit");
    Require(sceKernelIsGenuineDevKit() == 0, "the console must not be reported as a genuine development kit");
    Require(sceKernelIsGenuineTestKit() == 0, "the console must not be reported as a genuine test kit");
    Require(sceKernelIsTestKit() == 0, "the console must not be reported as a test kit");
    Require(sceKernelIsProspero() == 1, "the architecture must be reported as Prospero");
    Require(sceKernelIsKratos() == 0, "the architecture must not be reported as Kratos");
    if (ps4) {
        Require(sceKernelIsPs4Process() == 1, "a CUSA title must be reported as a PS4 process");
    } else {
        Require(sceKernelIsPs4Process() == 0, "the default process must not be reported as a PS4 process");
    }
}

void CheckAuthInfo() {
    struct {
        KernelAuthInfo value;
        std::uint64_t guard;
    } info;
    *__error_nid_postfix() = 0;
    Require(get_authinfo_nid_postfix(0, nullptr) == -1 && *__error_nid_postfix() == GuestEperm, "a null result must fail with EPERM");
    Require(get_authinfo_nid_postfix(0, reinterpret_cast<KernelAuthInfo*>(0x10)) == -1 && *__error_nid_postfix() == GuestEfault,
            "an unwritable store must fail with EFAULT");
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

int main(int argc, char** argv) {
    if (argc == 2 && std::string_view(argv[1]) == "ps4-child") {
        CheckConsoleKind(true);
        return 0;
    }
    CheckConsoleKind(false);
    CheckAuthInfo();
    CheckOpenPsId();

    const auto directory = std::filesystem::temp_directory_path() / ("ps4-identity-test-" + std::to_string(std::random_device{}()));
    std::filesystem::create_directories(directory / "app0" / "sce_sys");
    struct Entry { std::uint16_t key, format; std::uint32_t length, maximum, offset; };
    const std::uint32_t header[5] = {0x46535000, 0x0101, 52, 72, 2};
    const Entry entries[2] = {{0, 0x0204, 8, 8, 0}, {6, 0x0204, 10, 12, 8}};
    {
        std::ofstream file(directory / "app0/sce_sys/param.sfo", std::ios::binary);
        file.write(reinterpret_cast<const char*>(header), sizeof(header));
        file.write(reinterpret_cast<const char*>(entries), sizeof(entries));
        file << std::string("TITLE\0TITLE_ID\0\0\0\0\0\0", 20) << std::string("Example\0", 8) << "CUSA00000" << std::string("\0\0\0", 3);
    }
#ifdef _WIN32
    const std::string command = "\"cd /d \"" + directory.string() + "\" && \"" + argv[0] + "\" ps4-child\"";
#else
    const std::string command = "cd \"" + directory.string() + "\" && \"" + argv[0] + "\" ps4-child";
#endif
    const int status = std::system(command.c_str());
    std::filesystem::remove_all(directory);
    Require(status == 0, "the PS4 child process must succeed");
    return 0;
}
