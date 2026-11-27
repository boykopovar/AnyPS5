#include "prx/libkernel/include/Api.hpp"
#include "prx/libc/include/Api.hpp"
#include "prx/libc/include/ApplicationHeap.hpp"
#include "prx/libScePosixForWebKit/include/Api.hpp"
#include "prx/libc/include/General.hpp"
#include <vector>
#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <map>
#include <random>
#include <source_location>
#include <string>
#include <utility>

#ifdef _WIN32
#include <aclapi.h>
#include <windows.h>
#else
#include <unistd.h>
#endif

namespace {

constexpr int FtwF = 0;
constexpr int FtwD = 1;
constexpr int FtwDnr = 2;
constexpr int FtwNs = 4;
constexpr int GuestEinval = 22;

std::map<std::string, int> g_types;
std::vector<std::string> g_order;
std::vector<std::pair<std::string, int>> g_visits;
int g_stopAt = 0;
int g_stopReturn = 0;
int g_calls = 0;

void Require(bool condition, std::source_location location = std::source_location::current()) {
    if (!condition) {
        std::fprintf(stderr, "ftw check failed at line %u (guest errno %d)\n", location.line(), *__error_nid_postfix());
        std::abort();
    }
}

int APS5_VABI Recorder(const char* path, const void*, int type) {
    g_types[path] = type;
    g_order.push_back(path);
    g_visits.emplace_back(path, type);
    ++g_calls;
    if (g_stopAt != 0 && g_calls == g_stopAt) return g_stopReturn;
    return 0;
}

#ifdef _WIN32
PACL DenyListing(const std::wstring& directory, PSECURITY_DESCRIPTOR& owner) {
    HANDLE token = nullptr;
    Require(::OpenProcessToken(::GetCurrentProcess(), TOKEN_QUERY, &token) != FALSE);
    DWORD size = 0;
    ::GetTokenInformation(token, TokenUser, nullptr, 0, &size);
    std::vector<unsigned char> user(size);
    const BOOL queried = ::GetTokenInformation(token, TokenUser, user.data(), size, &size);
    ::CloseHandle(token);
    Require(queried != FALSE);
    PACL original = nullptr;
    Require(::GetNamedSecurityInfoW(directory.c_str(), SE_FILE_OBJECT, DACL_SECURITY_INFORMATION,
        nullptr, nullptr, &original, nullptr, &owner) == ERROR_SUCCESS);
    _EXPLICIT_ACCESS_W denied{};
    denied.grfAccessPermissions = FILE_LIST_DIRECTORY;
    denied.grfAccessMode = DENY_ACCESS;
    denied.grfInheritance = NO_INHERITANCE;
    denied.Trustee.TrusteeForm = TRUSTEE_IS_SID;
    denied.Trustee.TrusteeType = TRUSTEE_IS_USER;
    denied.Trustee.ptstrName = static_cast<LPWSTR>(reinterpret_cast<TOKEN_USER*>(user.data())->User.Sid);
    PACL blocked = nullptr;
    Require(::SetEntriesInAclW(1, &denied, original, &blocked) == ERROR_SUCCESS);
    const DWORD applied = ::SetNamedSecurityInfoW(const_cast<LPWSTR>(directory.c_str()), SE_FILE_OBJECT,
        DACL_SECURITY_INFORMATION, nullptr, nullptr, blocked, nullptr);
    ::LocalFree(blocked);
    Require(applied == ERROR_SUCCESS);
    return original;
}
#endif

}

int main() {
    ApplicationHeapInitialize_nid_no_patch(ApplicationEmptyProcessParameters_nid_no_patch());
    std::random_device random;
    const auto root = std::filesystem::temp_directory_path() /
        ("anyps5-ftw-test-" + std::to_string(random()) + "-" + std::to_string(random()));
    Require(std::filesystem::create_directory(root));
    std::ofstream(root / "a.txt") << "x";
    Require(std::filesystem::create_directory(root / "d"));
    std::ofstream(root / "d" / "b.txt") << "x";
    Require(std::filesystem::create_directory(root / "d" / "e"));
    std::ofstream(root / "d" / "e" / "c.txt") << "x";
    Require(std::filesystem::create_directory(root / "f"));
    AddPathAlias_nid_no_patch("/anyps5-ftw-test", root.string().c_str());

    g_types.clear();
    g_calls = 0;
    Require(ftw_nid_postfix("/anyps5-ftw-test", Recorder, 4) == 0);
    const std::map<std::string, int> expected = {
        {"/anyps5-ftw-test", FtwD},
        {"/anyps5-ftw-test/a.txt", FtwF},
        {"/anyps5-ftw-test/d", FtwD},
        {"/anyps5-ftw-test/d/b.txt", FtwF},
        {"/anyps5-ftw-test/d/e", FtwD},
        {"/anyps5-ftw-test/d/e/c.txt", FtwF},
        {"/anyps5-ftw-test/f", FtwD},
    };
    Require(g_types == expected);

    g_types.clear();
    g_calls = 0;
    g_stopAt = 2;
    g_stopReturn = 9;
    Require(ftw_nid_postfix("/anyps5-ftw-test", Recorder, 4) == 9);
    Require(g_calls == 2);
    g_stopAt = 0;
    g_stopReturn = 0;

    g_types.clear();
    g_order.clear();
    g_calls = 0;
    Require(ftw_nid_postfix("/anyps5-ftw-test", Recorder, 4) == 0);
    auto position = [](const std::string& path) {
        return std::find(g_order.begin(), g_order.end(), path) - g_order.begin();
    };
    Require(!g_order.empty() && g_order.front() == "/anyps5-ftw-test");
    Require(position("/anyps5-ftw-test/d") < position("/anyps5-ftw-test/d/b.txt"));
    Require(position("/anyps5-ftw-test/d/e") < position("/anyps5-ftw-test/d/e/c.txt"));

    const auto stopIndex = position("/anyps5-ftw-test/d/b.txt") + 1;
    g_order.clear();
    g_calls = 0;
    g_stopAt = static_cast<int>(stopIndex);
    g_stopReturn = 9;
    Require(ftw_nid_postfix("/anyps5-ftw-test", Recorder, 4) == 9);
    Require(static_cast<long>(g_order.size()) == stopIndex && g_order.back() == "/anyps5-ftw-test/d/b.txt");
    g_stopAt = 0;
    g_stopReturn = 0;

    g_types.clear();
    g_order.clear();
    Require(ftw_nid_postfix("/anyps5-ftw-test/", Recorder, 4) == 0);
    Require(std::find(g_order.begin(), g_order.end(), "/anyps5-ftw-test/a.txt") != g_order.end());
    for (const std::string& path : g_order) Require(path.find("//") == std::string::npos);

    g_types.clear();
    Require(ftw_nid_postfix("/anyps5-ftw-test/missing", Recorder, 4) == 0);
    Require(g_types.size() == 1 && g_types.begin()->second == FtwNs);

    *__error_nid_postfix() = 0;
    Require(ftw_nid_postfix("/anyps5-ftw-test", Recorder, 0) == -1);
    Require(*__error_nid_postfix() == GuestEinval);

#ifndef _WIN32
    std::filesystem::create_symlink(root / "a.txt", root / "link");
    std::filesystem::create_symlink(root / "missing.txt", root / "dangling");
    g_types.clear();
    Require(ftw_nid_postfix("/anyps5-ftw-test", Recorder, 4) == 0);
    Require(g_types.at("/anyps5-ftw-test/link") == FtwF);
    Require(g_types.at("/anyps5-ftw-test/dangling") == FtwNs);
    std::filesystem::create_symlink(root, root / "d" / "loop");
    g_types.clear();
    *__error_nid_postfix() = 0;
    Require(ftw_nid_postfix("/anyps5-ftw-test", Recorder, 4) == -1);
    Require(*__error_nid_postfix() == 62);
    std::filesystem::remove(root / "d" / "loop");
#endif

    const auto locked = root / "locked";
    Require(std::filesystem::create_directory(locked));
    std::ofstream(locked / "hidden.txt") << "x";
#ifdef _WIN32
    PSECURITY_DESCRIPTOR owner = nullptr;
    const PACL original = DenyListing(locked.wstring(), owner);
    const bool unreadable = true;
#else
    std::filesystem::permissions(locked, std::filesystem::perms::none);
    std::error_code listing;
    std::filesystem::directory_iterator probe(locked, listing);
    const bool unreadable = static_cast<bool>(listing);
#endif
    if (unreadable) {
        const std::pair<std::string, int> entered{"/anyps5-ftw-test/locked", FtwD};
        const std::pair<std::string, int> refused{"/anyps5-ftw-test/locked", FtwDnr};
        g_visits.clear();
        g_calls = 0;
        Require(ftw_nid_postfix("/anyps5-ftw-test", Recorder, 4) == 0);
        const auto visit = std::find(g_visits.begin(), g_visits.end(), entered);
        Require(visit != g_visits.end() && std::next(visit) != g_visits.end() && *std::next(visit) == refused);
        Require(std::count(g_visits.begin(), g_visits.end(), refused) == 1);
        Require(std::none_of(g_visits.begin(), g_visits.end(), [](const auto& seen) { return seen.first.starts_with("/anyps5-ftw-test/locked/"); }));

        g_stopAt = static_cast<int>(visit - g_visits.begin()) + 1;
        g_stopReturn = 7;
        g_visits.clear();
        g_calls = 0;
        Require(ftw_nid_postfix("/anyps5-ftw-test", Recorder, 4) == 7);
        Require(!g_visits.empty() && g_visits.back() == entered);
        g_stopAt = 0;
        g_stopReturn = 0;
    }
#ifdef _WIN32
    Require(::SetNamedSecurityInfoW(const_cast<LPWSTR>(locked.wstring().c_str()), SE_FILE_OBJECT,
        DACL_SECURITY_INFORMATION, nullptr, nullptr, original, nullptr) == ERROR_SUCCESS);
    ::LocalFree(owner);
#else
    std::filesystem::permissions(locked, std::filesystem::perms::owner_all);
#endif

    RemovePathAlias_nid_no_patch("/anyps5-ftw-test");
    Require(std::filesystem::remove_all(root) > 0);
    return 0;
}
