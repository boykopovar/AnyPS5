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

constexpr int GuestEnoent = 2;
constexpr int GuestEacces = 13;
constexpr int GuestEnoexec = 8;
constexpr int GuestE2big = 7;
constexpr int GuestEfault = 14;

void Require(bool condition, const char* message) {
    if (!condition) {
        std::fprintf(stderr, "kernel sys execvpe: %s\n", message);
        std::abort();
    }
}

void RequireFailure(int result, int error, const char* message) {
    Require(result == -1 && *__error_nid_postfix() == error, message);
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
    constexpr auto fullExecute = std::filesystem::perms::owner_all | std::filesystem::perms::group_exec | std::filesystem::perms::others_exec;
    constexpr auto plainRead = std::filesystem::perms::owner_read | std::filesystem::perms::owner_write | std::filesystem::perms::group_read | std::filesystem::perms::others_read;
    constexpr char plainBody[] = "plain data";
    RequireFailure(_execvpe_nid_postfix("", argv, nullptr), GuestEnoent, "execvpe with an empty name did not fail with ENOENT");
    RequireFailure(_execvpe_nid_postfix(nullptr, argv, nullptr), GuestEfault, "execvpe with a null name did not fail with EFAULT");
    RequireFailure(_execvpe_nid_postfix("kernel_execvpe_absent.bin", nullptr, nullptr), GuestEnoent, "execvpe with a null argv did not fail with ENOENT");
    const std::string longName(1024 * 1024, 'x');
    RequireFailure(_execvpe_nid_postfix(longName.c_str(), argv, nullptr), GuestEnoent, "an overlong name did not end the bounded search");
    RequireFailure(_execvpe_nid_postfix(("/" + longName).c_str(), argv, nullptr), GuestEnoent, "an overlong slash-qualified name did not end the bounded search");
    const std::string oversized(2048, 'a');
    std::vector<std::string> storage;
    std::vector<char*> oversizedArgv;
    for (int index = 0; index < 128; ++index) {
        storage.push_back(oversized);
        oversizedArgv.push_back(storage.back().data());
    }
    oversizedArgv.push_back(nullptr);
    RequireFailure(_execvpe_nid_postfix("kernel_execvpe_absent.bin", oversizedArgv.data(), nullptr), GuestE2big, "execvpe with arguments beyond ARG_MAX did not fail with E2BIG");
    WriteFile("kernel_execvpe_data.bin", plainBody, sizeof(plainBody) - 1, static_cast<std::filesystem::perms>(fullExecute));
    RequireFailure(_execvpe_nid_postfix("./kernel_execvpe_data.bin", argv, nullptr), GuestEnoent, "execvpe did not return ENOENT for a missing shell");
    std::filesystem::remove("kernel_execvpe_data.bin");
    std::filesystem::create_directory("bin");
    WriteFile("bin/sh", "", 0, static_cast<std::filesystem::perms>(fullExecute));
    RequireFailure(_execvpe_nid_postfix("./bin/sh", argv, nullptr), GuestEnoexec, "execvpe did not reach the empty shell through its search and fallback");
    WriteFile("bin/plain.bin", "plain", 5, static_cast<std::filesystem::perms>(fullExecute));
    RequireFailure(_execvpe_nid_postfix("./bin/plain.bin", argv, nullptr), GuestEnoexec, "execvpe did not reach the empty shell through its search and fallback");
    std::filesystem::create_directory("sbin");
    RequireFailure(_execvpe_nid_postfix("kernel_execvpe_absent.bin", argv, nullptr), GuestEnoent, "execvpe with the default search path and a missing file did not fail with ENOENT");
#ifndef _WIN32
    WriteFile("bin/kernel_execvpe_refused.bin", "plain", 5, static_cast<std::filesystem::perms>(plainRead));
    RequireFailure(_execvpe_nid_postfix("kernel_execvpe_refused.bin", argv, nullptr), GuestEacces, "execvpe without execute bits did not report EACCES after the search");
    std::filesystem::remove("bin/kernel_execvpe_refused.bin");
#endif
    std::filesystem::create_directory("usr");
    std::filesystem::create_directory("usr/bin");
    WriteFile("usr/bin/kernel_execvpe_data.bin", plainBody, sizeof(plainBody) - 1, static_cast<std::filesystem::perms>(fullExecute));
    RequireFailure(_execvpe_nid_postfix("kernel_execvpe_data.bin", argv, nullptr), GuestEnoexec, "execvpe did not reach the empty shell through its search and fallback");
    std::filesystem::create_directory("sandbox");
    WriteFile("sandbox/kernel_execvpe_tool.bin", plainBody, sizeof(plainBody) - 1, static_cast<std::filesystem::perms>(fullExecute));
    setenv_nid_postfix("PATH", "/sandbox", 1);
    RequireFailure(_execvpe_nid_postfix("kernel_execvpe_tool.bin", argv, nullptr), GuestEnoexec, "execvpe did not reach the empty shell through its search and fallback");
    unsetenv_nid_postfix("PATH");
    RequireFailure(_execvpe_nid_postfix("kernel_execvpe_tool.bin", argv, nullptr), GuestEnoent, "execvpe with a cleared PATH did not fail with ENOENT");
    WriteFile("kernel_execvpe_tool.bin", plainBody, sizeof(plainBody) - 1, static_cast<std::filesystem::perms>(fullExecute));
    setenv_nid_postfix("PATH", "", 1);
    RequireFailure(_execvpe_nid_postfix("kernel_execvpe_tool.bin", argv, nullptr), GuestEnoexec, "execvpe did not reach the empty shell through its search and fallback");
    unsetenv_nid_postfix("PATH");
    std::filesystem::remove("kernel_execvpe_tool.bin");
    setenv_nid_postfix("PATH", "/sandbox", 1);
    WriteFile("sandbox/kernel_execvpe_plain.bin", "plain", 5, static_cast<std::filesystem::perms>(fullExecute));
    RequireFailure(_execvpe_nid_postfix("kernel_execvpe_plain.bin", argv, nullptr), GuestEnoexec, "execvpe did not reach the empty shell through its search and fallback");
    std::filesystem::remove("bin/sh");
    RequireFailure(_execvpe_nid_postfix("./bin/plain.bin", argv, nullptr), GuestEnoent, "execvpe of a slash-named plain file without the shell did not fail with ENOENT");
    std::filesystem::remove("bin/plain.bin");
    std::filesystem::create_directory("scripts");
    WriteFile("scripts/kernel_execvpe_plain.bin", "plain", 5, static_cast<std::filesystem::perms>(fullExecute));
    setenv_nid_postfix("PATH", "/scripts", 1);
    RequireFailure(_execvpe_nid_postfix("kernel_execvpe_plain.bin", argv, nullptr), GuestEnoent, "execvpe of an unrecognised file without the shell did not fail with ENOENT");
    setenv_nid_postfix("PATH", "/absent:/scripts", 1);
    RequireFailure(_execvpe_nid_postfix("kernel_execvpe_plain.bin", argv, nullptr), GuestEnoent, "execvpe over a missing first element did not fail with ENOENT");
    unsetenv_nid_postfix("PATH");
    std::filesystem::remove("sandbox/kernel_execvpe_tool.bin");
    std::filesystem::remove("sandbox/kernel_execvpe_plain.bin");
    std::filesystem::remove("usr/bin/kernel_execvpe_data.bin");
    std::filesystem::remove("scripts/kernel_execvpe_plain.bin");
    std::filesystem::remove("bin");
    std::filesystem::remove("sbin");
    std::filesystem::remove("sandbox");
    std::filesystem::remove("scripts");
    std::filesystem::remove("usr/bin");
    std::filesystem::remove("usr");
    std::puts("guest_kernel_sys_execvpe passed");
    return 0;
}
