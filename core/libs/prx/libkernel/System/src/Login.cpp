#include "prx/libkernel/include/Api.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include <array>
#include <cerrno>
#include <cstring>
#include <mutex>
#include <optional>
#include <stdexcept>
#include <string>
#include <system_error>
#ifdef _WIN32
#include <windows.h>
#include <lmcons.h>
#else
#include <pwd.h>
#include <unistd.h>
#endif

namespace {
std::optional<std::string> g_loginForTest;

int ReadLogin(char* output, int size) {
    if (g_loginForTest) {
        if (g_loginForTest->empty()) throw std::runtime_error("getlogin_r: host effective user has no account name");
        const std::size_t length = g_loginForTest->size() + 1;
        if (length > static_cast<std::size_t>(size)) return 34;
        std::memcpy(output, g_loginForTest->c_str(), length);
        return 0;
    }
#ifdef _WIN32
    std::array<wchar_t, UNLEN + 1> name{};
    DWORD count = static_cast<DWORD>(name.size());
    if (!GetUserNameW(name.data(), &count))
        throw std::system_error(GetLastError(), std::system_category(), "getlogin_r: GetUserNameW");
    std::array<char, UNLEN * 4 + 1> utf8{};
    const int length = WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, name.data(),
        static_cast<int>(count), utf8.data(), static_cast<int>(utf8.size()), nullptr, nullptr);
    if (length == 0)
        throw std::system_error(GetLastError(), std::system_category(), "getlogin_r: UTF-8 conversion");
    if (length > size) return 34;
    std::memcpy(output, utf8.data(), static_cast<std::size_t>(length));
    return 0;
#else
    std::array<char, 16384> storage{};
    passwd entry{};
    passwd* account = nullptr;
    const int error = ::getpwuid_r(::geteuid(), &entry, storage.data(), storage.size(), &account);
    if (error != 0)
        throw std::system_error(error, std::generic_category(), "getlogin_r: getpwuid_r");
    if (account == nullptr || account->pw_name == nullptr || account->pw_name[0] == '\0')
        throw std::runtime_error("getlogin_r: host effective user has no account name");
    const std::size_t length = std::strlen(account->pw_name) + 1;
    if (length > static_cast<std::size_t>(size)) return 34;
    std::memcpy(output, account->pw_name, length);
    return 0;
#endif
}
}

extern "C" void LoginNameForTest_nid_no_patch(const char* name) {
    if (name == nullptr) g_loginForTest.reset();
    else g_loginForTest = name;
}

namespace {
constexpr std::size_t MaxLoginName = 33;
std::mutex sessionMutex;
std::optional<std::string> sessionLogin;
}

extern "C" int SetLogin_nid_no_patch(const char* name) {
    if (name == nullptr) return 14;
    const std::size_t length = ::strnlen(name, MaxLoginName);
    if (length == MaxLoginName) return 22;
    std::lock_guard lock(sessionMutex);
    sessionLogin.emplace(name, length);
    return 0;
}

extern "C" int APS5_VABI getlogin_r_nid_postfix(char* output, int size) {
    if (size <= 0) return 34;
    if (output == nullptr) return 14;
    {
        std::lock_guard lock(sessionMutex);
        if (sessionLogin) {
            if (sessionLogin->size() + 1 > static_cast<std::size_t>(size)) return 34;
            std::memcpy(output, sessionLogin->c_str(), sessionLogin->size() + 1);
            return 0;
        }
    }
    const int saved = errno;
    const int result = ReadLogin(output, size);
    errno = saved;
    return result;
}
