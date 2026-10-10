#include "prx/libc/include/General.hpp"

#include <Testing/Test.hpp>

#include <chrono>
#include <filesystem>
#include <fstream>
#include <string>
#include <system_error>
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

namespace {

using Testing::Case;
using Testing::Require;
using Testing::RequireEqual;

constexpr int enoent = 2;
constexpr int eacces = 13;
constexpr int efault = 14;
constexpr int einval = 22;

class AccessFixture {
public:
    AccessFixture()
        : name("anyps5-access-test-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count())),
          directory(std::filesystem::current_path() / name),
          file(directory / "sample.txt") {
        Require(std::filesystem::create_directory(directory), "create the test directory");
        std::ofstream(file) << "sample";
        Require(chdir_nid_postfix(name.c_str()) == 0, "enter the test directory");
    }

    ~AccessFixture() {
        chdir_nid_postfix("/");
#ifdef _WIN32
        SetFileAttributesW(file.c_str(), FILE_ATTRIBUTE_NORMAL);
#endif
        std::error_code ignored;
        std::filesystem::remove_all(directory, ignored);
    }

    AccessFixture(const AccessFixture&) = delete;
    AccessFixture& operator=(const AccessFixture&) = delete;

    const std::string name;
    const std::filesystem::path directory;
    const std::filesystem::path file;
};

void RequireFailure(int result, int expectedError, const char* message) {
    RequireEqual(result, -1, message);
    RequireEqual(*__error_nid_postfix(), expectedError, message);
}

const Case existingFile{"Access_ExistingFile_SucceedsAndKeepsErrno", [] {
    const AccessFixture fixture;
    *__error_nid_postfix() = 34;
    RequireEqual(access_nid_postfix("sample.txt", 0), 0, "existence check");
    RequireEqual(*__error_nid_postfix(), 34, "errno untouched on success");
}};

const Case readWriteFile{"Access_ReadWriteModeOnWritableFile_Succeeds", [] {
    const AccessFixture fixture;
    RequireEqual(access_nid_postfix("sample.txt", 6), 0, "read and write");
}};

const Case directoryAllModes{"Access_AllModesOnDirectory_Succeeds", [] {
    const AccessFixture fixture;
    RequireEqual(access_nid_postfix(".", 7), 0, "directory rwx");
}};

const Case absoluteGuestPath{"Access_AbsoluteGuestPath_ResolvesFromGuestRoot", [] {
    const AccessFixture fixture;
    RequireEqual(access_nid_postfix(("/" + fixture.name + "/sample.txt").c_str(), 4), 0, "absolute path");
}};

const Case pathAlias{"Access_PathAlias_ResolvesToAliasedDirectory", [] {
    const AccessFixture fixture;
    AddPathAlias_nid_no_patch("access-alias", fixture.directory.string().c_str());
    const int result = access_nid_postfix("/access-alias/sample.txt", 0);
    RemovePathAlias_nid_no_patch("access-alias");
    RequireEqual(result, 0, "aliased path");
}};

const Case missingFile{"Access_MissingFile_FailsWithEnoent", [] {
    const AccessFixture fixture;
    RequireFailure(access_nid_postfix("missing", 0), enoent, "missing file");
}};

const Case emptyPath{"Access_EmptyPath_FailsWithEnoent", [] {
    const AccessFixture fixture;
    RequireFailure(access_nid_postfix("", 0), enoent, "empty path");
}};

const Case nullPath{"Access_NullPath_FailsWithEfault", [] {
    const AccessFixture fixture;
    RequireFailure(access_nid_postfix(nullptr, 0), efault, "null path");
}};

const Case invalidMode{"Access_UnknownModeBits_FailsWithEinval", [] {
    const AccessFixture fixture;
    RequireFailure(access_nid_postfix("sample.txt", 8), einval, "mode 8");
    RequireFailure(access_nid_postfix("sample.txt", -1), einval, "mode -1");
}};

#ifdef _WIN32
const Case readOnlyFile{"Access_WriteOnReadOnlyFile_FailsWithEacces", [] {
    const AccessFixture fixture;
    Require(SetFileAttributesW(fixture.file.c_str(), FILE_ATTRIBUTE_READONLY), "mark file read-only");
    RequireEqual(access_nid_postfix("sample.txt", 4), 0, "read still allowed");
    RequireFailure(access_nid_postfix("sample.txt", 2), eacces, "write denied");
}};

const Case executeDenied{"Access_ExecuteDeniedByAcl_FailsWithEaccesButStaysReadable", [] {
    const AccessFixture fixture;
    PSECURITY_DESCRIPTOR original = nullptr;
    PACL originalAcl = nullptr;
    Require(GetNamedSecurityInfoW(const_cast<wchar_t*>(fixture.file.c_str()), SE_FILE_OBJECT,
        DACL_SECURITY_INFORMATION, nullptr, nullptr, &originalAcl, nullptr, &original) == ERROR_SUCCESS, "read the original ACL");
    char sid[SECURITY_MAX_SID_SIZE];
    DWORD sidBytes = sizeof(sid);
    Require(CreateWellKnownSid(WinWorldSid, nullptr, sid, &sidBytes), "create the world SID");
    EXPLICIT_ACCESSW denial{};
    denial.grfAccessPermissions = FILE_EXECUTE;
    denial.grfAccessMode = DENY_ACCESS;
    denial.Trustee.TrusteeForm = TRUSTEE_IS_SID;
    denial.Trustee.ptstrName = reinterpret_cast<wchar_t*>(sid);
    PACL deniedAcl = nullptr;
    Require(SetEntriesInAclW(1, &denial, originalAcl, &deniedAcl) == ERROR_SUCCESS, "build the denying ACL");
    Require(SetNamedSecurityInfoW(const_cast<wchar_t*>(fixture.file.c_str()), SE_FILE_OBJECT,
        DACL_SECURITY_INFORMATION, nullptr, nullptr, deniedAcl, nullptr) == ERROR_SUCCESS, "apply the denying ACL");
    const int denied = access_nid_postfix("sample.txt", 1);
    const int deniedError = *__error_nid_postfix();
    const int readable = access_nid_postfix("sample.txt", 4);
    const bool restored = SetNamedSecurityInfoW(const_cast<wchar_t*>(fixture.file.c_str()), SE_FILE_OBJECT,
        DACL_SECURITY_INFORMATION, nullptr, nullptr, originalAcl, nullptr) == ERROR_SUCCESS;
    LocalFree(deniedAcl);
    LocalFree(original);
    Require(restored, "restore the original ACL");
    RequireEqual(denied, -1, "execute denied");
    RequireEqual(deniedError, eacces, "execute denied errno");
    RequireEqual(readable, 0, "read still allowed");
}};
#else
const Case executeBit{"Access_ExecuteFollowsOwnerExecuteBit", [] {
    const AccessFixture fixture;
    std::filesystem::permissions(fixture.file, std::filesystem::perms::owner_read | std::filesystem::perms::owner_write);
    RequireFailure(access_nid_postfix("sample.txt", 1), eacces, "execute without the bit");
    std::filesystem::permissions(fixture.file, std::filesystem::perms::owner_exec, std::filesystem::perm_options::add);
    RequireEqual(access_nid_postfix("sample.txt", 1), 0, "execute with the bit");
}};
#endif

} // namespace
