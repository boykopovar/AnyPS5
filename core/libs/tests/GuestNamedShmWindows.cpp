#include "prx/libc/include/Api.hpp"
#include "prx/libkernel/include/Api.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include "prx/libkernel/File/include/SharedMemory.hpp"
#include "nid/Sha1.hpp"
#include <windows.h>
#include <aclapi.h>
#include <io.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <cwchar>
#include <filesystem>
#include <stdexcept>
#include <string>
#include <string_view>
#include <system_error>
#include <thread>
#include <utility>
#include <vector>
#include <barrier>
#include <algorithm>


namespace {
constexpr int ReadOnly = 0;
constexpr int ReadWrite = 2;
constexpr int Create = 0x200;
constexpr int Exclusive = 0x800;
constexpr std::int64_t RaceSize = 4096;

void Require(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(std::string(message) + " (guest errno=" + std::to_string(*__error_nid_postfix()) + ")");
}

void RequireError(int descriptor, int expected, const char* message) {
    Require(descriptor == -1 && *__error_nid_postfix() == expected, message);
}

void RequireNoAdministratorsAccess(HANDLE handle) {
    PACL dacl = nullptr;
    PSECURITY_DESCRIPTOR descriptor = nullptr;
    const DWORD result = ::GetSecurityInfo(handle, SE_FILE_OBJECT, DACL_SECURITY_INFORMATION,
        nullptr, nullptr, &dacl, nullptr, &descriptor);
    if (result != ERROR_SUCCESS || dacl == nullptr) {
        if (descriptor != nullptr) ::LocalFree(descriptor);
        Require(false, "reading named shared-memory object permissions failed");
    }
    SID_IDENTIFIER_AUTHORITY authority = SECURITY_NT_AUTHORITY;
    PSID administrators = nullptr;
    if (::AllocateAndInitializeSid(&authority, 2, SECURITY_BUILTIN_DOMAIN_RID,
        DOMAIN_ALIAS_RID_ADMINS, 0, 0, 0, 0, 0, 0, &administrators) == FALSE) {
        ::LocalFree(descriptor);
        Require(false, "building the Administrators SID failed");
    }
    ACL_SIZE_INFORMATION information{};
    const BOOL gotInformation = ::GetAclInformation(dacl, &information, sizeof(information), AclSizeInformation);
    bool grantsAdministrators = false;
    if (gotInformation) {
        for (DWORD index = 0; index < information.AceCount; ++index) {
            void* rawAce = nullptr;
            if (!::GetAce(dacl, index, &rawAce)) continue;
            const auto* header = static_cast<const ACE_HEADER*>(rawAce);
            if (header->AceType != ACCESS_ALLOWED_ACE_TYPE) continue;
            const auto* ace = static_cast<const ACCESS_ALLOWED_ACE*>(rawAce);
            if (::EqualSid(const_cast<DWORD*>(&ace->SidStart), administrators) != FALSE) {
                grantsAdministrators = true;
                break;
            }
        }
    }
    ::FreeSid(administrators);
    ::LocalFree(descriptor);
    Require(gotInformation && !grantsAdministrators,
        "named shared-memory object grants access to the Administrators group");
}

void RequireNoUnlinkedFiles(const std::string& name) {
    const auto digest = Nid::Sha1(std::vector<std::uint8_t>(name.begin(), name.end()));
    std::string prefix;
    for (const auto byte : digest) {
        prefix += "0123456789abcdef"[byte >> 4];
        prefix += "0123456789abcdef"[byte & 15];
    }
    const auto directory = std::filesystem::temp_directory_path() / L"AnyPS5" / L"shm";
    if (!std::filesystem::exists(directory)) return;
    for (const auto& entry : std::filesystem::directory_iterator(directory)) {
        const auto filename = entry.path().filename().string();
        Require(!filename.starts_with(prefix + "-unlinked-"),
                "closing the last descriptor did not reap its unlinked host object");
    }
}

void VerifyUntrackedDescriptor(int descriptor) {
    Require(File::IsSharedMemoryDescriptor_nid_no_patch(descriptor), "named descriptor was not tracked");
    std::vector<wchar_t> path(32768);
    const DWORD length = ::GetFinalPathNameByHandleW(reinterpret_cast<HANDLE>(::_get_osfhandle(descriptor)),
        path.data(), static_cast<DWORD>(path.size()), FILE_NAME_NORMALIZED | VOLUME_NAME_DOS);
    Require(length > 0 && length < path.size(), "locating the tracked file failed");
    const HANDLE handle = ::CreateFileW(path.data(), GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
        nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    Require(handle != INVALID_HANDLE_VALUE, "opening an untracked handle to the named file failed");
    const int untracked = ::_open_osfhandle(reinterpret_cast<std::intptr_t>(handle), _O_RDONLY | _O_BINARY);
    if (untracked < 0) ::CloseHandle(handle);
    Require(untracked >= 0, "creating an untracked descriptor failed");
    const bool classified = File::IsSharedMemoryDescriptor_nid_no_patch(untracked);
    Require(::_close(untracked) == 0, "closing the untracked descriptor failed");
    Require(!classified, "an untracked descriptor entered the named shared-memory path");
    Require(!File::IsSharedMemoryDescriptor_nid_no_patch(untracked), "a closed untracked descriptor was classified as shared memory");
}

void VerifyAnonymousDescriptorClassification() {
    const int descriptor = shm_open_nid_postfix(reinterpret_cast<const char*>(1), ReadWrite, 0600);
    Require(descriptor >= 0 && File::IsSharedMemoryDescriptor_nid_no_patch(descriptor),
            "anonymous shared memory was not recognized");
    *__error_nid_postfix() = 0;
    Require(fchflags_nid_postfix(descriptor, 0) == -1 && *__error_nid_postfix() == 22,
            "fchflags did not reject an anonymous shared-memory descriptor");
    const int duplicate = dup_nid_postfix(descriptor);
    Require(duplicate >= 0 && File::IsSharedMemoryDescriptor_nid_no_patch(duplicate),
            "duplicating an anonymous shared-memory descriptor lost its classification");
    Require(close_nid_postfix(descriptor) == 0, "closing the original anonymous descriptor failed");
    Require(!File::IsSharedMemoryDescriptor_nid_no_patch(descriptor),
            "the closed anonymous descriptor remained classified as shared memory");
    const int reusedOriginal = ::_open("NUL", _O_RDONLY | _O_BINARY);
    Require(reusedOriginal == descriptor && !File::IsSharedMemoryDescriptor_nid_no_patch(reusedOriginal),
            "reusing the original descriptor retained its shared-memory classification");
    Require(File::IsSharedMemoryDescriptor_nid_no_patch(duplicate),
            "closing the original anonymous descriptor invalidated its duplicate");
    Require(close_nid_postfix(duplicate) == 0, "closing the anonymous descriptor duplicate failed");
    Require(!File::IsSharedMemoryDescriptor_nid_no_patch(duplicate),
            "the closed anonymous descriptor duplicate remained classified as shared memory");
    const int reusedDuplicate = ::_open("NUL", _O_RDONLY | _O_BINARY);
    Require(reusedDuplicate == duplicate && !File::IsSharedMemoryDescriptor_nid_no_patch(reusedDuplicate),
            "reusing the duplicate descriptor retained its shared-memory classification");
    Require(close_nid_postfix(reusedOriginal) == 0, "closing the reused original descriptor failed");
    Require(close_nid_postfix(reusedDuplicate) == 0, "closing the reused duplicate descriptor failed");
}

void VerifyConcurrentDescriptorReuse(const std::string& name) {
    std::barrier start(3);
    std::vector<int> okay(2, 1);
    std::vector<std::thread> workers;
    for (int index = 0; index < 2; ++index) {
        workers.emplace_back([&, index] {
            start.arrive_and_wait();
            for (int iteration = 0; iteration < 64; ++iteration) {
                const int descriptor = shm_open_nid_postfix(name.c_str(), ReadWrite, 0);
                if (descriptor < 0) { okay[index] = 0; break; }
                if (!File::IsSharedMemoryDescriptor_nid_no_patch(descriptor) ||
                    pwrite_nid_disambig1_nid_postfix(descriptor, "x", 1, 64) != 0) okay[index] = 0;
                if (close_nid_postfix(descriptor) != 0) okay[index] = 0;
            }
        });
    }
    start.arrive_and_wait();
    for (auto& worker : workers) worker.join();
    Require(okay[0] && okay[1], "concurrent close/open lost named-descriptor tracking or bounded writes");
}

bool RunResizeWriteRace(const std::string& name) {
    const int descriptor = shm_open_nid_postfix(name.c_str(), ReadWrite, 0);
    if (descriptor < 0) return false;
    bool okay = true;
    for (int iteration = 0; iteration < 300; ++iteration) {
        const auto size = (iteration & 1) ? 0 : RaceSize;
        if (ftruncate_nid_postfix(descriptor, size) != 0) { okay = false; break; }
        if (pwrite_nid_disambig1_nid_postfix(descriptor, "x", 1, RaceSize / 2) < 0) { okay = false; break; }
        struct _stat64 status{};
        if (::_fstat64(descriptor, &status) != 0 || (status.st_size != 0 && status.st_size != RaceSize)) {
            okay = false;
            break;
        }
    }
    close_nid_postfix(descriptor);
    return okay;
}

void VerifyCrossProcessResizeWrite(const std::string& name, int descriptor) {
    wchar_t executable[32768]{};
    const DWORD length = ::GetModuleFileNameW(nullptr, executable, static_cast<DWORD>(std::size(executable)));
    Require(length > 0 && length < std::size(executable), "locating the Windows shared-memory test executable failed");
    std::vector<PROCESS_INFORMATION> workers;
    for (int index = 0; index < 2; ++index) {
        std::wstring command = L"\"" + std::wstring(executable, length) + L"\" --race " + std::wstring(name.begin(), name.end());
        STARTUPINFOW startup{};
        startup.cb = sizeof(startup);
        PROCESS_INFORMATION process{};
        Require(::CreateProcessW(nullptr, command.data(), nullptr, nullptr, FALSE, CREATE_NO_WINDOW,
                                 nullptr, nullptr, &startup, &process) != FALSE,
                "starting a cross-process shared-memory race worker failed");
        workers.push_back(process);
    }
    const bool localSucceeded = RunResizeWriteRace(name);
    bool childrenSucceeded = true;
    for (const auto& process : workers) {
        childrenSucceeded = childrenSucceeded && ::WaitForSingleObject(process.hProcess, 10000) == WAIT_OBJECT_0;
        DWORD exitCode = 1;
        childrenSucceeded = childrenSucceeded && ::GetExitCodeProcess(process.hProcess, &exitCode) && exitCode == 0;
        ::CloseHandle(process.hThread);
        ::CloseHandle(process.hProcess);
    }
    Require(localSucceeded && childrenSucceeded,
            "cross-process truncate/write race produced an invalid shared-object size");
    struct _stat64 status{};
    Require(::_fstat64(descriptor, &status) == 0 && (status.st_size == 0 || status.st_size == RaceSize),
            "cross-process truncate/write race left an invalid final size");
    Require(shm_unlink_nid_postfix(name.c_str()) == 0, "cross-process race cleanup failed");
}

void VerifyResizeReadPosition(const std::string& name) {
    constexpr std::size_t payloadSize = 2u << 20;
    const int descriptor = shm_open_nid_postfix(name.c_str(), ReadWrite | Create | Exclusive, 0600);
    Require(descriptor >= 0 && ftruncate_nid_postfix(descriptor, payloadSize) == 0,
            "creating the resize/read object failed");
    const int resizer = dup_nid_postfix(descriptor);
    Require(resizer >= 0, "duplicating the resize/read descriptor failed");
    std::vector<char> payload(payloadSize, 'r');
    Require(pwrite_nid_disambig1_nid_postfix(descriptor, payload.data(), payload.size(), 0) == payloadSize &&
            lseek_nid_postfix(descriptor, 137, 0) == 137, "preparing the resize/read object failed");
    std::barrier start(2);
    bool resized = true;
    std::thread resizing([&] {
        start.arrive_and_wait();
        for (int iteration = 0; iteration < 256; ++iteration)
            if (ftruncate_nid_postfix(resizer, payloadSize + ((iteration & 1) ? 4096 : 0)) != 0) resized = false;
    });
    start.arrive_and_wait();
    std::vector<char> output(payloadSize);
    bool read = true;
    for (int iteration = 0; iteration < 32; ++iteration)
        if (pread_nid_postfix(descriptor, output.data(), output.size(), 0) != payloadSize || output != payload) read = false;
    resizing.join();
    Require(resized && read && lseek_nid_postfix(descriptor, 0, 1) == 137,
            "concurrent resize/read changed the shared file position or contents");
    Require(shm_unlink_nid_postfix(name.c_str()) == 0 && close_nid_postfix(descriptor) == 0 && close_nid_postfix(resizer) == 0,
            "resize/read cleanup failed");
    RequireNoUnlinkedFiles(name);
}

void Run() {
    VerifyAnonymousDescriptorClassification();
    const std::string base = "/anyps5-windows-named-" + std::to_string(::GetCurrentProcessId()) + "-" + std::to_string(::GetTickCount64());
    const std::string name = base + "-lifecycle";
    Require(shm_unlink_nid_postfix(name.c_str()) == -1 && *__error_nid_postfix() == 2,
            "new test name unexpectedly existed");

    *__error_nid_postfix() = 73;
    const int original = shm_open_nid_postfix(name.c_str(), ReadWrite | Create | Exclusive, 0600);
    Require(original >= 0 && *__error_nid_postfix() == 73, "exclusive creation failed or changed guest errno");
    VerifyUntrackedDescriptor(original);
    Require(ftruncate_nid_postfix(original, 64) == 0, "sizing the named object failed");
    VerifyConcurrentDescriptorReuse(name);
    const char originalBytes[] = "original";
    Require(pwrite_nid_disambig1_nid_postfix(original, originalBytes, sizeof(originalBytes), 0) == sizeof(originalBytes),
            "write to named object failed");

    *__error_nid_postfix() = 0;
    RequireError(shm_open_nid_postfix(name.c_str(), ReadWrite | Create | Exclusive, 0600), 17,
                 "exclusive creation replaced an existing named object");
    const int reader = shm_open_nid_postfix(name.c_str(), ReadOnly, 0);
    char contents[sizeof(originalBytes)]{};
    Require(reader >= 0 && pread_nid_postfix(reader, contents, sizeof(contents), 0) == sizeof(contents) &&
            std::memcmp(contents, originalBytes, sizeof(contents)) == 0, "reopen did not share the original contents");
    *__error_nid_postfix() = 0;
    Require(pwrite_nid_disambig1_nid_postfix(reader, "x", 1, 0) == -1 && *__error_nid_postfix() == 9,
            "read-only descriptor accepted a write");
    *__error_nid_postfix() = 0;
    Require(pwrite_nid_disambig1_nid_postfix(reader, "x", 1, 64) == -1 && *__error_nid_postfix() == 9,
            "read-only descriptor accepted a write at end of file");
    *__error_nid_postfix() = 0;
    Require(pwrite_nid_disambig1_nid_postfix(reader, "", 0, 64) == -1 && *__error_nid_postfix() == 9,
            "read-only descriptor accepted a zero-length write");

    Require(shm_unlink_nid_postfix(name.c_str()) == 0, "unlink of named object failed");
    *__error_nid_postfix() = 0;
    RequireError(shm_open_nid_postfix(name.c_str(), ReadOnly, 0), 2, "unlinked name remained openable");
    std::memset(contents, 0, sizeof(contents));
    Require(pread_nid_postfix(original, contents, sizeof(contents), 0) == sizeof(contents) &&
            std::memcmp(contents, originalBytes, sizeof(contents)) == 0, "open descriptor stopped working after unlink");
    const char updatedByte = 'X';
    Require(pwrite_nid_disambig1_nid_postfix(original, &updatedByte, 1, 0) == 1,
            "open descriptor stopped accepting writes after unlink");

    const int replacement = shm_open_nid_postfix(name.c_str(), ReadWrite | Create | Exclusive, 0600);
    Require(replacement >= 0, "name could not be recreated while old descriptors remained open");
    Require(ftruncate_nid_postfix(replacement, 64) == 0, "sizing the replacement object failed");
    Require(fchmod_nid_postfix(original, 0400) == 0,
            "fchmod failed on the old open object after unlink and name recreation");
    Require(fchmod_nid_postfix(original, 0600) == 0,
            "restoring mode on the old open object after name recreation failed");
    std::memset(contents, 0x7f, sizeof(contents));
    Require(pread_nid_postfix(replacement, contents, sizeof(contents), 0) == sizeof(contents) &&
            std::memcmp(contents, "\0\0\0\0\0\0\0\0", sizeof(contents)) == 0,
            "recreated name still referred to the unlinked object");
    Require(pwrite_nid_disambig1_nid_postfix(replacement, "y", 1, 0) == 1,
            "fchmod on the old descriptor changed the replacement object's permissions");
    const char replacementBytes[] = "replacement";
    Require(pwrite_nid_disambig1_nid_postfix(replacement, replacementBytes, sizeof(replacementBytes), 0) == sizeof(replacementBytes),
            "write to replacement object failed");
    std::memset(contents, 0, sizeof(contents));
    Require(pread_nid_postfix(original, contents, sizeof(contents), 0) == sizeof(contents) &&
            contents[0] == updatedByte && std::memcmp(contents + 1, originalBytes + 1, sizeof(originalBytes) - 2) == 0,
            "replacement write changed the old open object");
    Require(shm_unlink_nid_postfix(name.c_str()) == 0, "replacement unlink failed");
    Require(close_nid_postfix(reader) == 0 && close_nid_postfix(original) == 0 && close_nid_postfix(replacement) == 0,
            "closing lifecycle descriptors failed");
    RequireNoUnlinkedFiles(name);

    const std::string duplicateName = base + "-duplicate";
    const int duplicateOriginal = shm_open_nid_postfix(duplicateName.c_str(), ReadWrite | Create | Exclusive, 0600);
    Require(duplicateOriginal >= 0 && ftruncate_nid_postfix(duplicateOriginal, 64) == 0,
            "creating the duplicate test object failed");
    const int duplicate = dup_nid_postfix(duplicateOriginal);
    Require(duplicate >= 0 && pwrite_nid_disambig1_nid_postfix(duplicate, "d", 1, 0) == 1,
            "dup lost shared-memory write access");
    const int duplicateReader = shm_open_nid_postfix(duplicateName.c_str(), ReadOnly, 0);
    Require(duplicateReader >= 0 && shm_unlink_nid_postfix(duplicateName.c_str()) == 0,
            "unlinking the duplicate test object failed");
    Require(close_nid_postfix(duplicateOriginal) == 0, "closing the duplicated original failed");
    Require(dup2_nid_postfix(-1, duplicate) == -1,
            "dup2 accepted an invalid source");
    Require(pwrite_nid_disambig1_nid_postfix(duplicate, "e", 1, 0) == 1,
            "failed dup2 destroyed its shared-memory target");
    Require(dup2_nid_postfix(duplicateReader, duplicate) == duplicate,
            "dup2 could not replace a shared-memory target");
    Require(pwrite_nid_disambig1_nid_postfix(duplicate, "f", 1, 0) == -1 && *__error_nid_postfix() == 9,
            "dup2 lost shared-memory read-only access");
    Require(close_nid_postfix(duplicateReader) == 0, "closing the duplicate reader failed");
    const std::string replacementName = base + "-duplicate-replacement";
    const int duplicateReplacement = shm_open_nid_postfix(replacementName.c_str(), ReadWrite | Create | Exclusive, 0600);
    Require(duplicateReplacement >= 0 && dup2_nid_postfix(duplicateReplacement, duplicate) == duplicate,
            "dup2 could not replace the last unlinked descriptor");
    RequireNoUnlinkedFiles(duplicateName);
    Require(shm_unlink_nid_postfix(replacementName.c_str()) == 0 &&
            close_nid_postfix(duplicateReplacement) == 0 && close_nid_postfix(duplicate) == 0,
            "cleaning up dup2 replacement descriptors failed");
    RequireNoUnlinkedFiles(replacementName);
    VerifyResizeReadPosition(base + "-resize-read");

    const std::string concurrentName = base + "-exclusive-race";
    constexpr int contenderCount = 12;
    std::vector<int> descriptors(contenderCount, -2);
    std::vector<int> errors(contenderCount, 0);
    std::vector<std::thread> contenders;
    contenders.reserve(contenderCount);
    for (int index = 0; index < contenderCount; ++index) {
        contenders.emplace_back([&, index] {
            descriptors[index] = shm_open_nid_postfix(concurrentName.c_str(), ReadWrite | Create | Exclusive, 0600);
            errors[index] = *__error_nid_postfix();
        });
    }
    for (auto& contender : contenders) contender.join();
    int created = 0;
    for (int index = 0; index < contenderCount; ++index) {
        if (descriptors[index] >= 0) {
            ++created;
            Require(close_nid_postfix(descriptors[index]) == 0, "closing exclusive-race descriptor failed");
        } else {
            Require(errors[index] == 17, "exclusive-race loser did not receive EEXIST");
        }
    }
    Require(created == 1, "exclusive create race produced more or fewer than one winner");
    Require(shm_unlink_nid_postfix(concurrentName.c_str()) == 0, "exclusive-race cleanup failed");

    const std::string raceName = base + "-cross-process-race";
    const int raceDescriptor = shm_open_nid_postfix(raceName.c_str(), ReadWrite | Create | Exclusive, 0600);
    Require(raceDescriptor >= 0 && ftruncate_nid_postfix(raceDescriptor, RaceSize) == 0,
            "cross-process race object creation failed");
    VerifyCrossProcessResizeWrite(raceName, raceDescriptor);
    Require(close_nid_postfix(raceDescriptor) == 0, "closing cross-process race descriptor failed");

    const std::string permissionsName = base + "-permissions";
    const int restricted = shm_open_nid_postfix(permissionsName.c_str(), ReadWrite | Create | Exclusive, 0400);
    Require(restricted >= 0, "read-only mode creation failed");
    RequireNoAdministratorsAccess(reinterpret_cast<HANDLE>(::_get_osfhandle(restricted)));
    Require(close_nid_postfix(restricted) == 0, "closing read-only mode object failed");
    *__error_nid_postfix() = 0;
    RequireError(shm_open_nid_postfix(permissionsName.c_str(), ReadWrite, 0), 13,
                 "mode 0400 allowed a later read-write open");
    const int restrictedReader = shm_open_nid_postfix(permissionsName.c_str(), ReadOnly, 0);
    Require(restrictedReader >= 0, "mode 0400 rejected a read-only open");
    Require(fchmod_nid_postfix(restrictedReader, 0600) == 0,
            "fchmod could not restore write permission on a named object");
    const int restoredWriter = shm_open_nid_postfix(permissionsName.c_str(), ReadWrite, 0);
    Require(restoredWriter >= 0, "mode 0600 after fchmod rejected read-write open");
    Require(fchmod_nid_postfix(restoredWriter, 0400) == 0,
            "fchmod could not remove write permission on a named object");
    *__error_nid_postfix() = 0;
    Require(shm_unlink_nid_postfix(permissionsName.c_str()) == -1 && *__error_nid_postfix() == 13,
            "unlink of a read-only named object did not return EACCES");
    Require(fchmod_nid_postfix(restoredWriter, 0600) == 0,
            "fchmod could not restore permission before unlink");
    Require(shm_unlink_nid_postfix(permissionsName.c_str()) == 0,
            "owner could not unlink a writable named object");
    Require(close_nid_postfix(restoredWriter) == 0, "closing restored writer failed");
    Require(close_nid_postfix(restrictedReader) == 0, "closing restricted reader failed");

    const std::string noPermissionsName = base + "-mode-zero";
    const int noPermissions = shm_open_nid_postfix(noPermissionsName.c_str(), ReadWrite | Create | Exclusive, 0000);
    Require(noPermissions >= 0 && fchmod_nid_postfix(noPermissions, 0600) == 0,
            "fchmod could not restore access to a mode 0000 named object");
    Require(shm_unlink_nid_postfix(noPermissionsName.c_str()) == 0 && close_nid_postfix(noPermissions) == 0,
            "mode 0000 object cleanup failed");

    *__error_nid_postfix() = 0;
    RequireError(shm_open_nid_postfix(nullptr, ReadWrite, 0), 14, "null name did not return EFAULT");
    *__error_nid_postfix() = 0;
    RequireError(shm_open_nid_postfix("relative", ReadWrite, 0), 22, "relative name did not return EINVAL");
    std::puts("Windows named shared-memory lifecycle checks passed");
}

bool EnvironmentEntryHasKey(const std::wstring& entry, const wchar_t* key) {
    const auto separator = entry.find(L'=');
    return separator != std::wstring::npos && separator == std::wcslen(key) &&
        ::CompareStringOrdinal(entry.data(), static_cast<int>(separator), key, static_cast<int>(separator), TRUE) == CSTR_EQUAL;
}

std::vector<wchar_t> EnvironmentWithTempPath(const std::wstring& path) {
    std::vector<std::wstring> entries;
    LPWCH environment = ::GetEnvironmentStringsW();
    if (environment == nullptr) throw std::runtime_error("reading the Windows environment failed");
    try {
        for (const wchar_t* current = environment; *current != L'\0'; current += std::wcslen(current) + 1) {
            std::wstring entry(current);
            if (!EnvironmentEntryHasKey(entry, L"TMP") && !EnvironmentEntryHasKey(entry, L"TEMP"))
                entries.push_back(std::move(entry));
        }
    } catch (...) {
        ::FreeEnvironmentStringsW(environment);
        throw;
    }
    ::FreeEnvironmentStringsW(environment);
    entries.push_back(L"TMP=" + path);
    entries.push_back(L"TEMP=" + path);
    std::sort(entries.begin(), entries.end(), [](const auto& left, const auto& right) {
        return ::CompareStringOrdinal(left.c_str(), -1, right.c_str(), -1, TRUE) == CSTR_LESS_THAN;
    });
    std::vector<wchar_t> block;
    for (const auto& entry : entries) {
        block.insert(block.end(), entry.begin(), entry.end());
        block.push_back(L'\0');
    }
    block.push_back(L'\0');
    return block;
}

bool SameDirectory(HANDLE left, HANDLE right) {
    BY_HANDLE_FILE_INFORMATION leftInfo{}, rightInfo{};
    return ::GetFileInformationByHandle(left, &leftInfo) && ::GetFileInformationByHandle(right, &rightInfo) &&
        leftInfo.dwVolumeSerialNumber == rightInfo.dwVolumeSerialNumber &&
        leftInfo.nFileIndexHigh == rightInfo.nFileIndexHigh && leftInfo.nFileIndexLow == rightInfo.nFileIndexLow;
}

std::filesystem::path ExtendedPath(const std::filesystem::path& path) {
    wchar_t fullPath[32768]{};
    const DWORD length = ::GetFullPathNameW(path.c_str(), static_cast<DWORD>(std::size(fullPath)), fullPath, nullptr);
    if (length == 0 || length >= std::size(fullPath))
        throw std::runtime_error("getting the full private temporary path failed");
    std::wstring full(fullPath, length);
    if (full.starts_with(L"\\\\?\\")) return full;
    if (full.starts_with(L"\\\\")) return L"\\\\?\\UNC\\" + full.substr(2);
    return L"\\\\?\\" + full;
}

std::filesystem::path DosPath(const std::filesystem::path& path) {
    const auto& native = path.native();
    if (native.starts_with(L"\\\\?\\UNC\\")) return L"\\\\" + native.substr(8);
    if (native.starts_with(L"\\\\?\\")) return native.substr(4);
    return path;
}

void RemovePrivateDirectory(const std::filesystem::path& directory) {
    WIN32_FIND_DATAW entry{};
    const HANDLE search = ::FindFirstFileW((directory / L"*").c_str(), &entry);
    if (search == INVALID_HANDLE_VALUE) {
        if (::GetLastError() != ERROR_FILE_NOT_FOUND)
            throw std::runtime_error("enumerating the private temporary directory for cleanup failed");
    } else {
        try {
            do {
                if (std::wcscmp(entry.cFileName, L".") == 0 || std::wcscmp(entry.cFileName, L"..") == 0) continue;
                if (entry.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT)
                    throw std::runtime_error("the private temporary directory contains a reparse point");
                const auto child = directory / entry.cFileName;
                if (entry.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) RemovePrivateDirectory(child);
                else if (!::DeleteFileW(child.c_str()))
                    throw std::runtime_error("removing a private temporary file failed");
            } while (::FindNextFileW(search, &entry));
            if (::GetLastError() != ERROR_NO_MORE_FILES)
                throw std::runtime_error("enumerating the private temporary directory for cleanup failed");
        } catch (...) {
            ::FindClose(search);
            throw;
        }
        ::FindClose(search);
    }
    if (!::RemoveDirectoryW(directory.c_str()))
        throw std::runtime_error("removing the private temporary directory failed");
}

int RunWithShortTempPath(bool longPath) {
    wchar_t tempBuffer[32768]{};
    const DWORD tempLength = ::GetTempPathW(static_cast<DWORD>(std::size(tempBuffer)), tempBuffer);
    if (tempLength == 0 || tempLength >= std::size(tempBuffer))
        throw std::runtime_error("locating the Windows temporary directory failed");
    const std::filesystem::path parent(std::wstring(tempBuffer, tempLength));
    std::filesystem::path baseRoot;
    bool created = false;
    for (unsigned attempt = 0; attempt < 16; ++attempt) {
        baseRoot = parent / (L"AnyPS5-Temp-Regression-" + std::to_wstring(::GetCurrentProcessId()) + L"-" +
            std::to_wstring(::GetTickCount64()) + L"-" + std::to_wstring(attempt));
        if (::CreateDirectoryW(baseRoot.c_str(), nullptr)) {
            created = true;
            break;
        }
        if (::GetLastError() != ERROR_ALREADY_EXISTS) throw std::runtime_error("creating a private temporary directory failed");
    }
    if (!created) throw std::runtime_error("creating a unique private temporary directory failed");
    const auto cleanupRoot = ExtendedPath(baseRoot);
    std::filesystem::path longRoot = cleanupRoot;
    const auto cleanup = [&] {
        RemovePrivateDirectory(cleanupRoot);
    };

    try {
        std::vector<wchar_t> shortBuffer(32768);
        DWORD shortLength = ::GetShortPathNameW(longRoot.c_str(), shortBuffer.data(), static_cast<DWORD>(shortBuffer.size()));
        if (shortLength == 0)
            throw std::runtime_error("GetShortPathNameW failed (Win32 error " + std::to_string(::GetLastError()) + ")");
        if (shortLength >= shortBuffer.size())
            throw std::runtime_error("GetShortPathNameW required a buffer of " + std::to_string(shortLength) + " characters");
        std::filesystem::path shortRoot(std::wstring(shortBuffer.data(), shortLength));
        if (longPath) {
            bool aliasAvailable = true;
            bool longAliasReady = false;
            for (unsigned level = 0; level < 80; ++level) {
                longRoot /= L"LongSharedMemoryTempDirectoryRegressionSegment";
                if (!::CreateDirectoryW(longRoot.c_str(), nullptr))
                    throw std::runtime_error("creating the nested long temporary directory failed (Win32 error " + std::to_string(::GetLastError()) + ")");
                shortLength = ::GetShortPathNameW(longRoot.c_str(), shortBuffer.data(), static_cast<DWORD>(shortBuffer.size()));
                if (shortLength == 0)
                    throw std::runtime_error("GetShortPathNameW failed for nested path (Win32 error " + std::to_string(::GetLastError()) + ")");
                if (shortLength >= shortBuffer.size())
                    throw std::runtime_error("GetShortPathNameW required a buffer of " + std::to_string(shortLength) + " characters");
                shortRoot = std::filesystem::path(std::wstring(shortBuffer.data(), shortLength));
                if (::CompareStringOrdinal(longRoot.filename().c_str(), -1, shortRoot.filename().c_str(), -1, TRUE) == CSTR_EQUAL) {
                    aliasAvailable = false;
                    break;
                }
                if (DosPath(longRoot).native().size() > MAX_PATH + 64 && DosPath(shortRoot).native().size() < MAX_PATH - 64) {
                    longAliasReady = true;
                    break;
                }
            }
            if (!aliasAvailable) {
                std::puts("long-short-temp regression skipped: this volume did not create an 8.3 alias for the nested directory");
                cleanup();
                return 77;
            }
            if (!longAliasReady)
                throw std::runtime_error("nested private path did not exceed MAX_PATH with a usable short alias");
        }
        if (::CompareStringOrdinal(longRoot.filename().c_str(), -1, shortRoot.filename().c_str(), -1, TRUE) == CSTR_EQUAL) {
            std::puts("short-temp regression skipped: this volume did not create an 8.3 alias for the private directory");
            cleanup();
            return 77;
        }
        const HANDLE longHandle = ::CreateFileW(longRoot.c_str(), FILE_READ_ATTRIBUTES,
            FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, nullptr, OPEN_EXISTING,
            FILE_FLAG_BACKUP_SEMANTICS, nullptr);
        if (longHandle == INVALID_HANDLE_VALUE) throw std::runtime_error("opening the long temporary path failed");
        const HANDLE shortHandle = ::CreateFileW(shortRoot.c_str(), FILE_READ_ATTRIBUTES,
            FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, nullptr, OPEN_EXISTING,
            FILE_FLAG_BACKUP_SEMANTICS, nullptr);
        if (shortHandle == INVALID_HANDLE_VALUE) {
            ::CloseHandle(longHandle);
            throw std::runtime_error("opening the 8.3 temporary alias failed");
        }
        const bool isSameDirectory = SameDirectory(longHandle, shortHandle);
        ::CloseHandle(shortHandle);
        ::CloseHandle(longHandle);
        if (!isSameDirectory) throw std::runtime_error("the short temporary path did not identify the created directory");
        std::wprintf(L"%ls regression verified 8.3 directory alias (%llu/%llu characters):\n%ls\n%ls\n",
            longPath ? L"long-short-temp" : L"short-temp",
            static_cast<unsigned long long>(longRoot.native().size()),
            static_cast<unsigned long long>(shortRoot.native().size()), longRoot.c_str(), shortRoot.c_str());
        std::fflush(stdout);

        wchar_t executable[32768]{};
        const DWORD executableLength = ::GetModuleFileNameW(nullptr, executable, static_cast<DWORD>(std::size(executable)));
        if (executableLength == 0 || executableLength >= std::size(executable))
            throw std::runtime_error("locating the Windows shared-memory test executable failed");
        std::wstring command = L"\"" + std::wstring(executable, executableLength) + L"\" --short-temp-child";
        auto environment = EnvironmentWithTempPath(DosPath(shortRoot).native());
        STARTUPINFOW startup{};
        startup.cb = sizeof(startup);
        PROCESS_INFORMATION process{};
        if (!::CreateProcessW(nullptr, command.data(), nullptr, nullptr, FALSE,
                CREATE_NO_WINDOW | CREATE_UNICODE_ENVIRONMENT, environment.data(), nullptr, &startup, &process))
            throw std::runtime_error("starting the short temporary path child failed");
        const DWORD wait = ::WaitForSingleObject(process.hProcess, 16000);
        DWORD exitCode = 1;
        const bool childSucceeded = wait == WAIT_OBJECT_0 && ::GetExitCodeProcess(process.hProcess, &exitCode) && exitCode == 0;
        if (wait != WAIT_OBJECT_0) {
            ::TerminateProcess(process.hProcess, 1);
            ::WaitForSingleObject(process.hProcess, 1000);
        }
        if (!childSucceeded)
            std::fprintf(stderr, "%s child failed: wait=%lu exit=%lu\n", longPath ? "long-short-temp" : "short-temp",
                static_cast<unsigned long>(wait), static_cast<unsigned long>(exitCode));
        ::CloseHandle(process.hThread);
        ::CloseHandle(process.hProcess);
        cleanup();
        return childSucceeded ? 0 : 1;
    } catch (...) {
        cleanup();
        throw;
    }
}
}

int main(int argumentCount, char** arguments) {
    try {
        if (argumentCount == 3 && std::string_view(arguments[1]) == "--race")
            return RunResizeWriteRace(arguments[2]) ? 0 : 1;
        if (argumentCount == 2 && std::string_view(arguments[1]) == "--short-temp")
            return RunWithShortTempPath(false);
        if (argumentCount == 2 && std::string_view(arguments[1]) == "--long-short-temp")
            return RunWithShortTempPath(true);
        if (argumentCount == 2 && std::string_view(arguments[1]) == "--short-temp-child") {
            Run();
            return 0;
        }
        Run();
        return 0;
    } catch (const std::exception& error) {
        std::fprintf(stderr, "%s\n", error.what());
        return 1;
    }
}
