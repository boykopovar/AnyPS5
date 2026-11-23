#include "prx/libc/include/general/VabiMacros.hpp"
#include "prx/libSceNKWebKit/libSceNKWebKit.hpp"
#include "prx/libc/include/ApplicationHeap.hpp"
#include "prx/libc/include/General.hpp"
#include "prx/libc/include/GuestErrno.hpp"

#include <array>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <string>
#include <system_error>

#ifdef _WIN32
#include <windows.h>
#else
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>
#endif

namespace {

using NKWebKit::WtfAbi::String;
using NKWebKit::WtfAbi::StringImpl;
using NKWebKit::WtfAbi::StringView;

void Check(bool condition, int line) {
    if (!condition) {
        std::fprintf(stderr, "legacy SQLiteFileSystem check failed at line %d\n", line);
        std::fflush(stderr);
        std::_Exit(1);
    }
}
#define Require(value) Check((value), __LINE__)

constexpr auto appendName = &_ZN7WebCore16SQLiteFileSystem28appendDatabaseFileNameToPathERKN3WTF6StringES4_;
constexpr auto deleteEmptyDirectory = &_ZN7WebCore16SQLiteFileSystem28deleteEmptyDatabaseDirectoryERKN3WTF6StringE;
constexpr auto modificationTime = &_ZN7WebCore16SQLiteFileSystem24databaseModificationTimeERKN3WTF6StringE;

String Make(const std::string& text) {
    return _ZN3WTF6String8fromUTF8EPKhm(reinterpret_cast<const unsigned char*>(text.data()), text.size());
}

std::u16string Units(const String& string) {
    Require(string.impl != nullptr);
    const StringView view = NKWebKit::WtfAbi::ViewOf(string.impl);
    std::u16string text;
    for (unsigned index = 0; index < view.length; ++index) text.push_back(NKWebKit::WtfAbi::CodeUnitAt(view, index));
    return text;
}

bool Is8Bit(const String& string) {
    return (string.impl->hashAndFlags & 4) != 0;
}

void RequireAppended(const String& path, const String& name, const std::u16string& expected, int line) {
    const bool narrow = (!path.impl || Is8Bit(path)) && (!name.impl || Is8Bit(name));
    const unsigned pathBefore = path.impl ? path.impl->refCount : 0;
    const unsigned nameBefore = name.impl ? name.impl->refCount : 0;
    {
        const String result = appendName(path, name);
        Check(result.impl && Units(result) == expected && Is8Bit(result) == narrow, line);
        Check(result.impl != path.impl && result.impl != name.impl && result.impl->refCount == 2, line);
    }
    if (path.impl) Check(path.impl->refCount == pathBefore, line);
    if (name.impl) Check(name.impl->refCount == nameBefore, line);
}

#define Appended(path, name, expected) RequireAppended((path), (name), (expected), __LINE__)

void TestAppending() {
    const String null;
    Require(Is8Bit(Make("/db")) && !Is8Bit(Make("/\xe4\xb8\xad")));
    Appended(Make("/db"), Make("a.sqlite"), u"/db/a.sqlite");
    Appended(Make("/db/"), Make("a.sqlite"), u"/db/a.sqlite");
    Appended(Make("/db//"), Make("/a"), u"/db///a");
    Appended(Make("db"), Make("a"), u"db/a");
    Appended(Make(""), Make("a"), u"/a");
    Appended(null, Make("a"), u"/a");
    Appended(Make("/db"), Make(""), u"/db/");
    Appended(Make("/db"), null, u"/db/");
    Appended(Make("/db/"), Make(""), u"/db/");
    Appended(Make("/"), null, u"/");
    Appended(Make(""), Make(""), u"/");
    Appended(null, null, u"/");
    Appended(Make("/caf\xc3\xa9"), Make("\xc3\xa9t\xc3\xa9"), u"/café/été");
    Appended(Make("/\xe4\xb8\xad"), Make("a"), u"/中/a");
    Appended(Make("/db"), Make("\xe4\xb8\xad"), u"/db/中");
    Appended(Make("/\xe4\xb8\xad/"), Make("\xe6\x96\x87"), u"/中/文");
    Appended(Make("\xe4\xb8\xad"), null, u"中/");
    Appended(null, Make("\xe4\xb8\xad"), u"/中");
    const String path = Make("/shared/");
    Appended(path, path, u"/shared//shared/");
}

std::string NativeText(const std::filesystem::path& path) {
    return std::string(reinterpret_cast<const char*>(path.u8string().c_str()));
}

std::filesystem::path TempDirectory(const std::filesystem::path& base, const char* name) {
#ifdef _WIN32
    std::error_code error;
    const auto root = base / (std::string(name) + "-" + std::to_string(GetCurrentProcessId()));
    std::filesystem::create_directory(root, error);
    Require(!error);
    return root;
#else
    std::string pattern = NativeText(base / (std::string(name) + "-XXXXXX"));
    Require(mkdtemp(pattern.data()) != nullptr);
    return std::filesystem::path(pattern);
#endif
}

void WriteFile(const std::filesystem::path& path, const std::string& contents) {
    std::ofstream output(path, std::ios::binary | std::ios::trunc);
    Require(output.good());
    output.write(contents.data(), static_cast<std::streamsize>(contents.size()));
    output.close();
    Require(output.good());
}

void SetModificationTime(const std::filesystem::path& path, std::int64_t seconds, int milliseconds) {
#ifdef _WIN32
    const HANDLE handle = CreateFileW(path.c_str(), FILE_WRITE_ATTRIBUTES, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, nullptr, OPEN_EXISTING,
                                      FILE_FLAG_BACKUP_SEMANTICS, nullptr);
    Require(handle != INVALID_HANDLE_VALUE);
    const std::uint64_t ticks = (static_cast<std::uint64_t>(seconds) + 11644473600ULL) * 10000000ULL + static_cast<std::uint64_t>(milliseconds) * 10000ULL;
    FILETIME time{static_cast<DWORD>(ticks), static_cast<DWORD>(ticks >> 32)};
    Require(SetFileTime(handle, nullptr, nullptr, &time) != 0);
    CloseHandle(handle);
#else
    const timespec times[2] = {{0, UTIME_OMIT}, {static_cast<time_t>(seconds), milliseconds * 1000000L}};
    Require(utimensat(AT_FDCWD, path.c_str(), times, 0) == 0);
#endif
}

void* APS5_VABI TestHeapAllocate(std::size_t size) { return std::malloc(size == 0 ? 1 : size); }
void APS5_VABI TestHeapFree(void* pointer) { std::free(pointer); }
void* APS5_VABI TestHeapCallocate(std::size_t count, std::size_t size) {
    return std::calloc(count == 0 ? 1 : count, size == 0 ? 1 : size);
}
void* APS5_VABI TestHeapReallocate(void* pointer, std::size_t size) { return std::realloc(pointer, size == 0 ? 1 : size); }
void* APS5_VABI UnexpectedAlign(std::size_t, std::size_t) { std::abort(); }
void* APS5_VABI UnexpectedRealign(void*, std::size_t, std::size_t) { std::abort(); }
int APS5_VABI UnexpectedPosixAlign(void**, std::size_t, std::size_t) { std::abort(); }

void TestFiles() {
    std::error_code error;
    const auto tempRoot = std::filesystem::temp_directory_path(error);
    Require(!error);
    const auto root = TempDirectory(tempRoot, "nkwebkit-legacy-sqlite");
    const auto rootText = root.u8string();
    AddPathAlias_nid_no_patch("/nkwebkit-legacy-sqlite", reinterpret_cast<const char*>(rootText.c_str()));
    const auto guest = [](const std::string& name) { return Make("/nkwebkit-legacy-sqlite/" + name); };

    WriteFile(root / "old.db", "sqlite");
    SetModificationTime(root / "old.db", 1000000000, 0);
    const auto old = modificationTime(guest("old.db"));
    Require(old.hasValue && old.value == 1000000000.0);
    WriteFile(root / "fraction.db", "sqlite");
    SetModificationTime(root / "fraction.db", 1600000000, 750);
    const auto fraction = modificationTime(guest("fraction.db"));
    Require(fraction.hasValue && fraction.value == 1600000000.0);
    WriteFile(root / "before-epoch.db", "sqlite");
    SetModificationTime(root / "before-epoch.db", -1, 750);
    const auto quarterBefore = modificationTime(guest("before-epoch.db"));
    Require(quarterBefore.hasValue && quarterBefore.value == -1.0);
    SetModificationTime(root / "before-epoch.db", -2, 500);
    const auto secondAndHalfBefore = modificationTime(guest("before-epoch.db"));
    Require(secondAndHalfBefore.hasValue && secondAndHalfBefore.value == -2.0);
    SetModificationTime(root / "before-epoch.db", -1, 0);
    const auto secondBefore = modificationTime(guest("before-epoch.db"));
    Require(secondBefore.hasValue && secondBefore.value == -1.0);
    SetModificationTime(root / "before-epoch.db", 0, 0);
    const auto epoch = modificationTime(guest("before-epoch.db"));
    Require(epoch.hasValue && epoch.value == 0.0);
    std::filesystem::create_directory(root / "directory.db");
    SetModificationTime(root / "directory.db", 1200000000, 0);
    const auto directory = modificationTime(guest("directory.db"));
    Require(directory.hasValue && directory.value == 1200000000.0);
    WriteFile(root / std::filesystem::path(u8"中.db"), "wide");
    SetModificationTime(root / std::filesystem::path(u8"中.db"), 1300000000, 0);
    const auto wide = modificationTime(guest("\xe4\xb8\xad.db"));
    Require(wide.hasValue && wide.value == 1300000000.0);
    Require(!modificationTime(guest("missing.db")).hasValue);
    *__error_nid_postfix() = 91;
    Require(!modificationTime(Make("")).hasValue && *__error_nid_postfix() == 91);
    Require(!modificationTime(String()).hasValue && *__error_nid_postfix() == 91);
    Require(!modificationTime(Make(std::string(1, char{}) + "old.db")).hasValue && *__error_nid_postfix() == 91);
    Require(!modificationTime(guest("missing.db")).hasValue && *__error_nid_postfix() == 2);
    *__error_nid_postfix() = 91;
    Require(modificationTime(guest("old.db")).hasValue && *__error_nid_postfix() == 91);

    std::filesystem::create_directory(root / "empty");
    Require(deleteEmptyDirectory(guest("empty")));
    Require(!std::filesystem::exists(root / "empty"));
    Require(!deleteEmptyDirectory(guest("empty")));
    std::filesystem::create_directory(root / "full");
    WriteFile(root / "full" / "kept.db", "sqlite");
    Require(!deleteEmptyDirectory(guest("full")));
    Require(std::filesystem::exists(root / "full" / "kept.db"));
    Require(!deleteEmptyDirectory(guest("old.db")));
    Require(std::filesystem::exists(root / "old.db"));
    Require(!deleteEmptyDirectory(Make("")));
    Require(!deleteEmptyDirectory(String()));
    std::filesystem::create_directory(root / std::filesystem::path(u8"中"));
    Require(deleteEmptyDirectory(guest("\xe4\xb8\xad")));
    Require(!std::filesystem::exists(root / std::filesystem::path(u8"中")));

    std::filesystem::create_directory(root / "twin-a");
    std::filesystem::create_directory(root / "twin-b");
    Require(_ZN3WTF14FileSystemImpl20deleteEmptyDirectoryERKNS_6StringE(guest("twin-a")) == deleteEmptyDirectory(guest("twin-b")));

    std::filesystem::remove_all(root, error);
}

}  // namespace

int main() {
    std::array<void*, 10> heapApi{
        reinterpret_cast<void*>(TestHeapAllocate),
        reinterpret_cast<void*>(TestHeapFree),
        reinterpret_cast<void*>(TestHeapCallocate),
        reinterpret_cast<void*>(TestHeapReallocate),
        reinterpret_cast<void*>(UnexpectedAlign),
        reinterpret_cast<void*>(UnexpectedRealign),
        reinterpret_cast<void*>(UnexpectedPosixAlign),
    };
    ApplicationHeapRegister_nid_no_patch(heapApi.data());
    TestAppending();
    TestFiles();
    std::puts("libSceNKWebKit legacy SQLiteFileSystem tests passed");
    return 0;
}
