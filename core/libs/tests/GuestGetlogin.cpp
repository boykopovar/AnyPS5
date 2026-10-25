#include "prx/libkernel/include/Api.hpp"
#include "prx/libc/include/Api.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include <array>
#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <string>
#include <thread>
#include <vector>
#ifdef _WIN32
#include <windows.h>
#include <lmcons.h>
#else
#include <pwd.h>
#include <unistd.h>
#endif

static void Check(bool condition, int line) {
    if (!condition) {
        std::fprintf(stderr, "getlogin_r check failed at line %d\n", line);
        std::abort();
    }
}
#define Require(value) Check((value), __LINE__)

static std::string HostLogin() {
#ifdef _WIN32
    wchar_t name[UNLEN + 1];
    DWORD count = UNLEN + 1;
    Require(GetUserNameW(name, &count) != 0);
    std::array<char, UNLEN * 4 + 1> output{};
    Require(WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, name, -1,
        output.data(), static_cast<int>(output.size()), nullptr, nullptr) > 0);
    return output.data();
#else
    const passwd* account = ::getpwuid(::geteuid());
    Require(account != nullptr && account->pw_name != nullptr && account->pw_name[0] != '\0');
    return account->pw_name;
#endif
}

static void RequireLogin(const std::string& expected) {
    std::vector<char> output(expected.size() + 3, '\x5A');
    *__error_nid_postfix() = 12345;
    Require(getlogin_r_nid_postfix(output.data() + 1, static_cast<int>(expected.size() + 1)) == 0);
    Require(*__error_nid_postfix() == 12345);
    Require(output.front() == '\x5A' && output.back() == '\x5A');
    Require(std::memcmp(output.data() + 1, expected.c_str(), expected.size() + 1) == 0);
    output.assign(output.size(), '\x5A');
    const auto saved = output;
    Require(getlogin_r_nid_postfix(output.data() + 1, static_cast<int>(expected.size())) == 34);
    Require(*__error_nid_postfix() == 12345 && output == saved);
}

int main() {
    std::array<char, 4096> output;
    output.fill('\x5A');
    const auto untouched = output;
    for (int size : {0, -1, std::numeric_limits<int>::min()}) {
        *__error_nid_postfix() = 12345;
        Require(getlogin_r_nid_postfix(output.data(), size) == 34);
        Require(*__error_nid_postfix() == 12345 && output == untouched);
    }
    Require(getlogin_r_nid_postfix(nullptr, 1) == 14);
    Require(*__error_nid_postfix() == 12345);
    const std::string expected = HostLogin();
    *__error_nid_postfix() = 12345;
    Require(getlogin_r_nid_postfix(output.data(), static_cast<int>(output.size())) == 0);
    Require(*__error_nid_postfix() == 12345);
    Require(std::strcmp(output.data(), expected.c_str()) == 0);
    RequireLogin(expected);
    std::array<std::thread, 4> readers;
    for (auto& reader : readers) reader = std::thread([&] {
        for (int i = 0; i != 20; ++i) RequireLogin(expected);
    });
    for (auto& reader : readers) reader.join();
}
