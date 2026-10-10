#include "prx/libc/include/General.hpp"
#include <chrono>
#include <cstdlib>
#include <fstream>
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <aclapi.h>
#endif

extern "C" {
int APS5_VABI access_nid_postfix(const char*, int);
int APS5_VABI chdir_nid_postfix(const char*);
int* APS5_VABI __error_nid_postfix();
}

static void Require(bool value) { if (!value) std::abort(); }

int main() {
    const auto name = "anyps5-access-test-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count());
    const auto directory = std::filesystem::current_path() / name;
    Require(std::filesystem::create_directory(directory));
    const auto file = directory / "sample.txt";
    { std::ofstream output(file); output << "sample"; }
    Require(chdir_nid_postfix(name.c_str()) == 0);
    *__error_nid_postfix() = 34;
    Require(access_nid_postfix("sample.txt", 0) == 0 && *__error_nid_postfix() == 34);
    Require(access_nid_postfix("sample.txt", 6) == 0);
    Require(access_nid_postfix(".", 7) == 0);
    Require(access_nid_postfix(("/" + name + "/sample.txt").c_str(), 4) == 0);
    AddPathAlias_nid_no_patch("access-alias", directory.string().c_str());
    Require(access_nid_postfix("/access-alias/sample.txt", 0) == 0);
    RemovePathAlias_nid_no_patch("access-alias");
    Require(access_nid_postfix("missing", 0) == -1 && *__error_nid_postfix() == 2);
    Require(access_nid_postfix("", 0) == -1 && *__error_nid_postfix() == 2);
    Require(access_nid_postfix(nullptr, 0) == -1 && *__error_nid_postfix() == 14);
    Require(access_nid_postfix("sample.txt", 8) == -1 && *__error_nid_postfix() == 22);
    Require(access_nid_postfix("sample.txt", -1) == -1 && *__error_nid_postfix() == 22);
#ifdef _WIN32
    Require(SetFileAttributesW(file.c_str(), FILE_ATTRIBUTE_READONLY));
    Require(access_nid_postfix("sample.txt", 4) == 0);
    Require(access_nid_postfix("sample.txt", 2) == -1 && *__error_nid_postfix() == 13);
    Require(SetFileAttributesW(file.c_str(), FILE_ATTRIBUTE_NORMAL));
    PSECURITY_DESCRIPTOR original = nullptr;
    PACL originalAcl = nullptr;
    Require(GetNamedSecurityInfoW(const_cast<wchar_t*>(file.c_str()), SE_FILE_OBJECT,
        DACL_SECURITY_INFORMATION, nullptr, nullptr, &originalAcl, nullptr, &original) == ERROR_SUCCESS);
    char sid[SECURITY_MAX_SID_SIZE];
    DWORD sidBytes = sizeof(sid);
    Require(CreateWellKnownSid(WinWorldSid, nullptr, sid, &sidBytes));
    EXPLICIT_ACCESSW denial{};
    denial.grfAccessPermissions = FILE_EXECUTE;
    denial.grfAccessMode = DENY_ACCESS;
    denial.Trustee.TrusteeForm = TRUSTEE_IS_SID;
    denial.Trustee.ptstrName = reinterpret_cast<wchar_t*>(sid);
    PACL deniedAcl = nullptr;
    Require(SetEntriesInAclW(1, &denial, originalAcl, &deniedAcl) == ERROR_SUCCESS);
    Require(SetNamedSecurityInfoW(const_cast<wchar_t*>(file.c_str()), SE_FILE_OBJECT,
        DACL_SECURITY_INFORMATION, nullptr, nullptr, deniedAcl, nullptr) == ERROR_SUCCESS);
    const int denied = access_nid_postfix("sample.txt", 1);
    const int deniedError = *__error_nid_postfix();
    const int readable = access_nid_postfix("sample.txt", 4);
    Require(SetNamedSecurityInfoW(const_cast<wchar_t*>(file.c_str()), SE_FILE_OBJECT,
        DACL_SECURITY_INFORMATION, nullptr, nullptr, originalAcl, nullptr) == ERROR_SUCCESS);
    LocalFree(deniedAcl);
    LocalFree(original);
    Require(denied == -1 && deniedError == 13 && readable == 0);
#else
    std::filesystem::permissions(file, std::filesystem::perms::owner_read | std::filesystem::perms::owner_write);
    Require(access_nid_postfix("sample.txt", 1) == -1 && *__error_nid_postfix() == 13);
    std::filesystem::permissions(file, std::filesystem::perms::owner_exec, std::filesystem::perm_options::add);
    Require(access_nid_postfix("sample.txt", 1) == 0);
#endif
    Require(chdir_nid_postfix("/") == 0);
    std::filesystem::remove(file);
    std::filesystem::remove(directory);
}
