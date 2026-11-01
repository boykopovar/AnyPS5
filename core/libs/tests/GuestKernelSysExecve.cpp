#include "prx/libkernel_sys/include/AdditionalExports.hpp"
#include "prx/libc/include/General.hpp"
#include "prx/libc/include/Api.hpp"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

namespace {

constexpr int GuestE2big = 7;
constexpr int GuestEacces = 13;
constexpr int GuestEnoent = 2;
constexpr int GuestEnoexec = 8;
constexpr int GuestEfault = 14;
constexpr int GuestEnotdir = 20;
constexpr int GuestEnametoolong = 63;

void Require(bool condition, const char* message) {
    if (!condition) {
        std::fprintf(stderr, "kernel sys execve: %s\n", message);
        std::abort();
    }
}

void RequireFailure(int result, int error, const char* message) {
    Require(result == -1 && *__error_nid_postfix() == error, message);
}

std::string RepeatedComponents(std::size_t length) {
    std::string path = "a";
    while (path.size() + 2 <= length) path += "/a";
    return path;
}

void WriteFile(const char* name, const char* content, std::streamsize bytes, std::filesystem::perms mode) {
    std::ofstream file(name, std::ios::binary);
    file.write(content, bytes);
    file.close();
#ifndef _WIN32
    std::filesystem::permissions(name, mode);
#else
    (void)mode;
#endif
}

} // namespace

int main() {
    char first[] = "first";
    char second[] = "second";
    char* const argv[] = {first, second, nullptr};
    char* const emptyArgv[] = {nullptr};
    char* const environment[] = {second, nullptr};
    char* const junkArgument[] = {reinterpret_cast<char*>(1), nullptr};
    RequireFailure(_execve_nid_postfix("", nullptr, nullptr), GuestEfault, "execve with a null argv did not fail with EFAULT");
    static char unterminated[1101];
    std::memset(unterminated, 'x', sizeof(unterminated));
    unterminated[sizeof(unterminated) - 1] = '\0';
    RequireFailure(_execve_nid_postfix(unterminated, nullptr, nullptr), GuestEfault, "execve did not check a null argv before the pathname");
    RequireFailure(_execve_nid_postfix("kernel_execve_missing.bin", reinterpret_cast<char* const*>(1), nullptr), GuestEfault, "execve with an unreadable argv array did not fail with EFAULT");
    RequireFailure(_execve_nid_postfix("kernel_execve_missing.bin", junkArgument, nullptr), GuestEfault, "execve with an unreadable argument did not fail with EFAULT");
    RequireFailure(_execve_nid_postfix("kernel_execve_missing.bin", emptyArgv, reinterpret_cast<char* const*>(1)), GuestEfault, "execve with an unreadable envp array did not fail with EFAULT");
    const std::string severalComponents = RepeatedComponents(1023);
    Require(severalComponents.size() == 1023, "the 1023 byte pathname test built the wrong length");
    RequireFailure(_execve_nid_postfix(severalComponents.c_str(), emptyArgv, nullptr), GuestEnoent, "execve with a 1023 byte pathname did not reach the lookup");
    const std::string tooLongPath = "a" + std::string(1024, 'x');
    Require(tooLongPath.size() == 1025, "the 1024 byte unterminated pathname test built the wrong length");
    RequireFailure(_execve_nid_postfix(tooLongPath.c_str(), emptyArgv, nullptr), GuestEnametoolong, "execve with an unterminated 1024 byte pathname did not fail with ENAMETOOLONG");
    RequireFailure(_execve_nid_postfix("", emptyArgv, nullptr), GuestEnoent, "execve with an empty pathname did not fail with ENOENT");
    RequireFailure(_execve_nid_postfix("kernel_execve_missing.bin", argv, environment), GuestEnoent, "execve of a missing file did not fail with ENOENT");
    constexpr char prefixBody[] = "prefix";
    WriteFile("kernel_execve_prefix.bin", prefixBody, sizeof(prefixBody) - 1, std::filesystem::perms::owner_all);
    RequireFailure(_execve_nid_postfix("kernel_execve_prefix.bin/inside", argv, nullptr), GuestEnotdir, "execve through a file prefix did not fail with ENOTDIR");
    std::filesystem::create_directory("kernel_execve_dir");
    RequireFailure(_execve_nid_postfix("kernel_execve_dir", argv, nullptr), GuestEacces, "execve of a directory did not fail with EACCES");
    WriteFile("kernel_execve_zero.bin", "", 0, std::filesystem::perms::owner_all | std::filesystem::perms::group_all);
    RequireFailure(_execve_nid_postfix("kernel_execve_zero.bin", argv, nullptr), GuestEnoexec, "execve of an empty file did not fail with ENOEXEC");
    WriteFile("kernel_execve_short.bin", "EL", 2, std::filesystem::perms::owner_all | std::filesystem::perms::group_exec | std::filesystem::perms::others_exec);
    RequireFailure(_execve_nid_postfix("kernel_execve_short.bin", argv, nullptr), GuestEnoexec, "execve of a two byte file did not fail with ENOEXEC");
    WriteFile("kernel_execve_plain.bin", "just data", 9, std::filesystem::perms::owner_all | std::filesystem::perms::group_exec | std::filesystem::perms::others_exec);
    RequireFailure(_execve_nid_postfix("kernel_execve_plain.bin", argv, nullptr), GuestEnoexec, "execve of a plain file did not fail with ENOEXEC");
#ifndef _WIN32
    WriteFile("kernel_execve_unexec.bin", "just data", 9, std::filesystem::perms::owner_read | std::filesystem::perms::owner_write | std::filesystem::perms::group_read | std::filesystem::perms::others_read);
    RequireFailure(_execve_nid_postfix("kernel_execve_unexec.bin", argv, nullptr), GuestEacces, "execve without execute bits did not fail with EACCES");
#endif
    const std::string oversized(2048, 'a');
    std::vector<std::string> storage;
    storage.reserve(128);
    std::vector<char*> oversizedArgv;
    oversizedArgv.reserve(129);
    for (int index = 0; index < 128; ++index) {
        storage.push_back(oversized);
        oversizedArgv.push_back(storage.back().data());
    }
    oversizedArgv.push_back(nullptr);
    RequireFailure(_execve_nid_postfix("kernel_execve_missing.bin", oversizedArgv.data(), nullptr), GuestE2big, "execve with arguments beyond ARG_MAX did not fail with E2BIG");
    std::filesystem::remove("kernel_execve_prefix.bin");
    std::filesystem::remove("kernel_execve_zero.bin");
    std::filesystem::remove("kernel_execve_short.bin");
    std::filesystem::remove("kernel_execve_plain.bin");
    std::filesystem::remove("kernel_execve_unexec.bin");
    std::filesystem::remove("kernel_execve_dir");
    std::puts("guest_kernel_sys_execve passed");
    return 0;
}
