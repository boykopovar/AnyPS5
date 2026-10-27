#include "prx/libkernel/include/Api.hpp"
#include "prx/libc/include/Api.hpp"
#include "SceTypes.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include "prx/libc/include/General.hpp"
#include "prx/libc/include/FileStream.hpp"
#include "prx/libc/include/RenameDot.hpp"
#include "prx/libkernel/File/include/File.hpp"
#include "prx/libkernel/File/include/NativeStat.hpp"
#include "prx/libkernel/KernelErrors.hpp"
#include <algorithm>
#include <array>
#include <chrono>
#include <cerrno>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <cstdio>
#include <cstring>
#include <functional>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>
#ifdef _WIN32
#include <io.h>
#include <windows.h>
#include <winioctl.h>
#else
#include <unistd.h>
#endif
#ifdef __linux__
#include <sys/inotify.h>
#endif

#ifdef _WIN32
static bool g_invalidParameter = false;
static void RecordInvalidParameter(const wchar_t*, const wchar_t*, const wchar_t*, unsigned, std::uintptr_t) { g_invalidParameter = true; }
#endif
static void Check(bool value, int line) {
    if (!value) {
#ifdef _WIN32
        const auto nativeError = ::GetLastError();
#else
        const auto nativeError = errno;
#endif
        const auto guestError = *__error_nid_postfix();
#ifdef _WIN32
        std::fprintf(stderr, "Filesystem check failed at line %d (guest errno %d, Win32 error %lu)\n",
            line, guestError, static_cast<unsigned long>(nativeError));
#else
        std::fprintf(stderr, "Filesystem check failed at line %d (guest errno %d, host errno %d)\n",
            line, guestError, nativeError);
#endif
        std::abort();
    }
}
#define Require(value) Check((value), __LINE__)

static bool ChflagsThrows(const std::filesystem::path& path, std::uint64_t flags, const char* names) {
    try {
        chflags_nid_postfix(path.string().c_str(), flags);

    } catch (const std::runtime_error& error) {
        return std::strstr(error.what(), names) != nullptr;
    }
    return false;
}

static bool FchflagsThrows(int descriptor, std::uint64_t flags, const char* names) {
    try {
        fchflags_nid_postfix(descriptor, flags);

    } catch (const std::runtime_error& error) {
        return std::strstr(error.what(), names) != nullptr;
    }
    return false;
}

static std::string CaptureStderr(const std::function<void()>& action) {
    std::FILE* capture = std::tmpfile();
    Require(capture != nullptr);
    std::fflush(stderr);
#ifdef _WIN32
    const int stream = ::_fileno(stderr);
    const int saved = ::_dup(stream);
    Require(saved >= 0 && ::_dup2(::_fileno(capture), stream) == 0);
#else
    const int stream = ::fileno(stderr);
    const int saved = ::dup(stream);
    Require(saved >= 0 && ::dup2(::fileno(capture), stream) >= 0);
#endif
    action();
    std::fflush(stderr);
#ifdef _WIN32
    Require(::_dup2(saved, stream) == 0);
    ::_close(saved);
#else
    Require(::dup2(saved, stream) >= 0);
    ::close(saved);
#endif
    Require(std::fseek(capture, 0, SEEK_SET) == 0);
    std::string output;
    char buffer[512];
    for (;;) {
        const auto bytes = std::fread(buffer, 1, sizeof(buffer), capture);
        output.append(buffer, bytes);
        if (bytes != sizeof(buffer)) break;
    }
    std::fclose(capture);
    return output;
}

static void TestParentTraversal() {
    const auto root = std::filesystem::path("anyps5-parent-test-" +
        std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    Require(std::filesystem::create_directories(root / "Existing"));
    const auto present = (root / "present.txt").generic_string();
    { std::ofstream file(present); file << "preserved"; }
    const auto directory = (root / "victim").generic_string();
    Require(std::filesystem::create_directory(directory));
    const auto checkInvalid = [&](const std::string& prefix, int error) {
        const auto path = prefix + "/present.txt";
        const auto missingLeaf = prefix + "/new.txt";
        const int kernelError = static_cast<int>(0x80020000u | static_cast<unsigned>(error));
        FileStat status{};
        Require(stat_nid_postfix(path.c_str(), &status) == -1 && *__error_nid_postfix() == error);
        Require(lstat_nid_postfix(path.c_str(), &status) == -1 && *__error_nid_postfix() == error);
        Require(sceKernelStat(path.c_str(), &status) == kernelError);
        Require(open_nid_postfix(path.c_str(), 0, 0) == -1 && *__error_nid_postfix() == error);
        Require(sceKernelOpen(path.c_str(), 0x601, 0600) == kernelError);
        Require(sceKernelOpen(missingLeaf.c_str(), 0x601, 0600) == kernelError);
        Require(access_nid_postfix(path.c_str(), 0) == -1 && *__error_nid_postfix() == error);
        Require(sceKernelCheckReachability(path.c_str()) == kernelError);
        Require(chmod_nid_postfix(path.c_str(), 0400) == -1 && *__error_nid_postfix() == error);
        Require(sceKernelChmod_nid_postfix(path.c_str(), 0400) == kernelError);
        Require(sceKernelTruncate_nid_postfix(path.c_str(), 0) == kernelError);
        Require(sceKernelUtimes_nid_postfix(path.c_str(), nullptr) == kernelError);
        Require(unlink_nid_postfix(path.c_str()) == -1 && *__error_nid_postfix() == error);
        Require(sceKernelUnlink(path.c_str()) == kernelError);
        Require(remove_nid_postfix(path.c_str()) == -1 && *__error_nid_postfix() == error);
        Require(rename_nid_postfix(path.c_str(), missingLeaf.c_str()) == -1 && *__error_nid_postfix() == error);
        Require(rename_nid_postfix(present.c_str(), missingLeaf.c_str()) == -1 && *__error_nid_postfix() == error);
        Require(sceKernelRename(path.c_str(), missingLeaf.c_str()) == kernelError);
        Require(sceKernelRename(present.c_str(), missingLeaf.c_str()) == kernelError);
        const auto victim = prefix + "/victim";
        Require(rmdir_nid_postfix(victim.c_str()) == -1 && *__error_nid_postfix() == error);
        Require(sceKernelRmdir(victim.c_str()) == kernelError);
        Require(remove_nid_postfix(victim.c_str()) == -1 && *__error_nid_postfix() == error);
        Require(mkdir_nid_postfix(missingLeaf.c_str(), 0700) == -1 && *__error_nid_postfix() == error);
        Require(sceKernelMkdir(missingLeaf.c_str(), 0700) == kernelError);
        Require(chdir_nid_postfix(prefix.c_str()) == -1 && *__error_nid_postfix() == error);
        Require(opendir_nid_postfix(prefix.c_str()) == nullptr && *__error_nid_postfix() == error);
        Require(fopen_nid_postfix(path.c_str(), "wb") == nullptr && *__error_nid_postfix() == error);
        auto* stream = fopen_nid_postfix(present.c_str(), "rb");
        Require(stream != nullptr);
        const int descriptor = stream->GuestState().descriptor;
        Require(freopen_nid_postfix(path.c_str(), "wb", stream) == nullptr && *__error_nid_postfix() == error);
        Require(!DescriptorIsOpen_nid_no_patch(descriptor));
        std::ifstream file(present);
        std::string contents;
        std::getline(file, contents);
        Require(contents == "preserved" && std::filesystem::is_directory(directory));
        Require(!std::filesystem::exists(root / "new.txt"));
    };
    checkInvalid(root.generic_string() + "/missing/..", 2);
    checkInvalid(root.generic_string() + "/present.txt/..", 20);
    checkInvalid(root.generic_string() + "/present.txt/child/../..", 20);
    checkInvalid("/" + root.generic_string() + "/missing/..", 2);
    checkInvalid(root.generic_string() + "/missing/../../" + root.generic_string(), 2);
    AddPathAlias_nid_no_patch("parent-test-mount", std::filesystem::absolute(root).string().c_str());
    checkInvalid("/PARENT-TEST-MOUNT/missing/..", 2);
    checkInvalid("/parent-test-mount/present.txt/..", 20);
    FileStat status{};
    Require(sceKernelStat("/parent-test-mount/existing/../present.txt", &status) == 0 && status.st_size == 9);
    for (const char* name : {"/parent-test-mount/present.txt/.", "/parent-test-mount/present.txt/./"}) {
        Require(stat_nid_postfix(name, &status) == -1 && *__error_nid_postfix() == 20);
        Require(sceKernelStat(name, &status) == static_cast<int>(0x80020014u));
        Require(access_nid_postfix(name, 0) == -1 && *__error_nid_postfix() == 20);
        Require(open_nid_postfix(name, 0, 0) == -1 && *__error_nid_postfix() == 20);
        Require(mkdir_nid_postfix(name, 0755) == -1 && *__error_nid_postfix() == 20);
    }
    Require(stat_nid_postfix("/parent-test-mount/missing/.", &status) == -1 && *__error_nid_postfix() == 2);
    Require(stat_nid_postfix("/parent-test-mount/existing/.", &status) == 0 && (status.st_mode & 0170000) == 0040000);
    Require(access_nid_postfix("/parent-test-mount/existing/./", 0) == 0);
    Require(TrimTrailingSeparators_nid_no_patch("/") == "/");
    Require(TrimTrailingSeparators_nid_no_patch("/mount/dir//") == "/mount/dir");
    Require(TrimTrailingSeparators_nid_no_patch("dir/") == "dir");
#ifdef _WIN32
    Require(TrimTrailingSeparators_nid_no_patch("C:\\") == "C:\\");
    Require(TrimTrailingSeparators_nid_no_patch("C:/") == "C:/");
    Require(TrimTrailingSeparators_nid_no_patch("C:\\dir\\") == "C:\\dir");
    Require(TrimTrailingSeparators_nid_no_patch("C:\\\\") == "C:\\");
    Require(TrimTrailingSeparators_nid_no_patch("C:\\folder\\\\") == "C:\\folder");
#endif
    std::filesystem::create_directory(root / "dot-victim");
    for (const char* name : {"/parent-test-mount/dot-victim/.", "/parent-test-mount/dot-victim/..", "/parent-test-mount/dot-victim/./", "/parent-test-mount/dot-victim/../"}) {
        Require(unlink_nid_postfix(name) == -1 && *__error_nid_postfix() == 22);
        Require(rmdir_nid_postfix(name) == -1 && *__error_nid_postfix() == 22);
        Require(sceKernelUnlink(name) == static_cast<int>(0x80020016u));
        Require(sceKernelRmdir(name) == static_cast<int>(0x80020016u));
    }
    Require(unlink_nid_postfix("/parent-test-mount/present.txt/.") == -1 && *__error_nid_postfix() == 20);
    Require(rename_nid_postfix("/parent-test-mount/present.txt/.", "/parent-test-mount/dot-renamed") == -1 && *__error_nid_postfix() == 20);
    Require(rename_nid_postfix("/parent-test-mount/present.txt/..", "/parent-test-mount/dot-renamed") == -1 && *__error_nid_postfix() == 20);
    Require(rmdir_nid_postfix("/parent-test-mount/missing/..") == -1 && *__error_nid_postfix() == 2);
    Require(std::filesystem::is_directory(root / "dot-victim") && std::filesystem::is_regular_file(root / "present.txt"));
    std::filesystem::remove(root / "dot-victim");
    for (const char* name : {"slash-unlink", "slash-remove", "slash-rename", "slash-open", "slash-kernel"}) std::ofstream(root / name) << "kept";
    Require(unlink_nid_postfix("/parent-test-mount/slash-unlink/") == -1 && *__error_nid_postfix() == 20);
    Require(remove_nid_postfix("/parent-test-mount/slash-remove/") == -1 && *__error_nid_postfix() == 20);
    Require(rename_nid_postfix("/parent-test-mount/slash-rename/", "/parent-test-mount/slash-renamed") == -1 && *__error_nid_postfix() == 20);
    Require(open_nid_postfix("/parent-test-mount/slash-open/", 0, 0) == -1 && *__error_nid_postfix() == 20);
    Require(sceKernelUnlink("/parent-test-mount/slash-kernel/") == static_cast<int>(0x80020014u));
    Require(rename_nid_postfix("/parent-test-mount/slash-remove", "/parent-test-mount/slash-open/") == -1 && *__error_nid_postfix() == 20);
    Require(stat_nid_postfix("/parent-test-mount/slash-open/", &status) == -1 && *__error_nid_postfix() == 20);
    Require(access_nid_postfix("/parent-test-mount/slash-open/", 0) == -1 && *__error_nid_postfix() == 20);
    Require(chmod_nid_postfix("/parent-test-mount/slash-open/", 0644) == -1 && *__error_nid_postfix() == 20);
    Require(mkdir_nid_postfix("/parent-test-mount/slash-open/", 0755) == -1 && *__error_nid_postfix() == 20);
    Require(rmdir_nid_postfix("/parent-test-mount/slash-open/") == -1 && *__error_nid_postfix() == 20);
    Require(sceKernelOpen("/parent-test-mount/slash-open/", 0x601, 0600) == static_cast<int>(0x80020014u));
    Require(unlink_nid_postfix((root.generic_string() + "/slash-kernel/").c_str()) == -1 && *__error_nid_postfix() == 20);
    for (const char* name : {"slash-unlink", "slash-remove", "slash-rename", "slash-open", "slash-kernel"}) Require(std::filesystem::is_regular_file(root / name));
    Require(!std::filesystem::exists(root / "slash-renamed"));
    Require(sceKernelStat("/parent-test-mount/Existing/", &status) == 0 && (status.st_mode & 0170000) == 0040000);
    const int listed = sceKernelOpen("/parent-test-mount/Existing/", 0x20000, 0);
    Require(listed >= 0 && sceKernelClose(listed) == 0);
    Require(mkdir_nid_postfix("/parent-test-mount/slash-directory/", 0755) == 0 && std::filesystem::is_directory(root / "slash-directory"));
    Require(rmdir_nid_postfix("/parent-test-mount/slash-directory/") == 0 && !std::filesystem::exists(root / "slash-directory"));
    RemovePathAlias_nid_no_patch("parent-test-mount");
    std::error_code linkError;
    std::filesystem::create_directory_symlink("Existing", root / "directory-link", linkError);
    if (!linkError) {
        Require(sceKernelStat((root.generic_string() + "/directory-link/../present.txt").c_str(), &status) == 0 && status.st_size == 9);
        std::filesystem::create_symlink("present.txt", root / "file-link");
        std::filesystem::create_directory_symlink("missing", root / "dangling-link");
        checkInvalid(root.generic_string() + "/file-link/..", 20);
        checkInvalid(root.generic_string() + "/dangling-link/..", 2);
    }
    Require(chdir_nid_postfix(root.generic_string().c_str()) == 0);
    Require(sceKernelStat("missing/../present.txt", &status) == static_cast<int>(0x80020002u));
    Require(sceKernelStat("present.txt/..", &status) == static_cast<int>(0x80020014u));
    Require(sceKernelStat("EXISTING/../present.txt", &status) == 0 && status.st_size == 9);
    const int created = sceKernelOpen("Existing/../new.txt", 0x601, 0600);
    Require(created >= 0 && sceKernelClose(created) == 0);
    Require(chdir_nid_postfix("/") == 0);
    std::filesystem::remove_all(root);
}

static void TestRelinkedModuleNames() {
    const auto root = std::filesystem::path("anyps5-relinked-names-" +
        std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    Require(std::filesystem::create_directories(root));
    const auto original = (root / "Plugin.prx").generic_string();
    const auto relinked = original + ".guest.prx";
    FileStat status{};
    Require(sceKernelStat(original.c_str(), &status) == static_cast<int>(0x80020002u));
    Require(lstat_nid_postfix(original.c_str(), &status) == -1 && *__error_nid_postfix() == 2);
    Require(sceKernelCheckReachability(original.c_str()) == static_cast<int>(0x80020002u));
    Require(sceKernelOpen(original.c_str(), 0, 0) == static_cast<int>(0x80020002u));
    const auto missing = (root / "missing-content.pak").string();
    int openResult = 0;
    const auto openLog = CaptureStderr([&] { openResult = sceKernelOpen(missing.c_str(), 0, 0); });
    Require(openResult == static_cast<int>(0x80020002u));
    Require(openLog.find("path=" + missing) != std::string::npos);
    Require(openLog.find("resolvedPath=") != std::string::npos && openLog.find("missing-content.pak") != std::string::npos);
    Require(openLog.find("result=0x80020002") != std::string::npos);
    { std::ofstream file(relinked); file << "relinked"; }
    Require(sceKernelStat(original.c_str(), &status) == 0 && status.st_size == 8);
    Require(stat_nid_postfix(original.c_str(), &status) == 0 && status.st_size == 8);
    Require(lstat_nid_postfix(original.c_str(), &status) == 0 && status.st_size == 8);
    Require(sceKernelCheckReachability(original.c_str()) == 0);
    const int opened = sceKernelOpen(original.c_str(), 0, 0);
    Require(opened >= 0 && sceKernelClose(opened) == 0);
    Require(sceKernelStat((root / "Other.prx").generic_string().c_str(), &status) == static_cast<int>(0x80020002u));
    Require(!std::filesystem::exists(original));
    { std::ofstream file(original); file << "original-bytes"; }
    Require(sceKernelStat(original.c_str(), &status) == 0 && status.st_size == 14);
    Require(std::filesystem::remove(original));
    const int created = sceKernelOpen(original.c_str(), 0x601, 0600);
    Require(created >= 0 && sceKernelClose(created) == 0);
    Require(std::filesystem::file_size(original) == 0 && std::filesystem::file_size(relinked) == 8);
    std::filesystem::remove_all(root);
}

static void TestRelinkedModuleNameCase() {
    const auto root = std::filesystem::path("anyps5-relinked-case-" +
        std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    Require(std::filesystem::create_directories(root));
    { std::ofstream file(root / "librenoircore.ps5.prx.guest.prx"); file << "relinked"; }
    const auto requested = (root / "libRenoirCore.PS5.prx").generic_string();
    FileStat status{};
    Require(sceKernelStat(requested.c_str(), &status) == 0 && status.st_size == 8);
    Require(stat_nid_postfix(requested.c_str(), &status) == 0 && status.st_size == 8);
    Require(sceKernelCheckReachability(requested.c_str()) == 0);
    const int opened = sceKernelOpen(requested.c_str(), 0, 0);
    Require(opened >= 0 && sceKernelClose(opened) == 0);
    Require(sceKernelStat((root / "libRenoirCore.PS5.prx2").generic_string().c_str(), &status) == static_cast<int>(0x80020002u));
    std::filesystem::remove_all(root);
}

#ifdef _WIN32
static void TestCaseSensitiveHostDirectory() {
    const auto root = std::filesystem::temp_directory_path() / ("anyps5-sensitive-" +
        std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    Require(std::filesystem::create_directory(root));
    const auto handle = CreateFileW(root.c_str(), FILE_READ_ATTRIBUTES | FILE_WRITE_ATTRIBUTES,
        FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, nullptr, OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS, nullptr);
    Require(handle != INVALID_HANDLE_VALUE);
    struct { ULONG flags; } information{FILE_CS_FLAG_CASE_SENSITIVE_DIR};
    constexpr auto caseSensitiveInfo = static_cast<FILE_INFO_BY_HANDLE_CLASS>(23);
    const bool enabled = SetFileInformationByHandle(handle, caseSensitiveInfo, &information, sizeof(information));
    const auto error = GetLastError();
    CloseHandle(handle);
    if (!enabled && (error == ERROR_NOT_SUPPORTED || error == ERROR_INVALID_PARAMETER || error == ERROR_ACCESS_DENIED)) {
        std::printf("case-sensitive host directory unavailable: %lu\n", error);
        std::filesystem::remove(root);
        return;
    }
    Require(enabled);
    { std::ofstream file(root / "File.txt"); file << "payload"; }
    Require(std::filesystem::create_directory(root / "Sub"));
    Require(std::filesystem::create_directory(root / "Both"));
    Require(std::filesystem::create_directory(root / "both"));
    { std::ofstream file(root / "libCase.prx.guest.prx"); file << "module"; }
    const auto host = root.string();
    AddPathAlias_nid_no_patch("/sensitive-host", host.c_str());
    Require(ResolvePath_nid_no_patch("/sensitive-host/file.txt") == root / "File.txt");
    Require(ResolvePath_nid_no_patch("/sensitive-host/sub") == root / "Sub");
    Require(ResolvePathOrRelinkedModule_nid_no_patch("/sensitive-host/libcase.prx") == root / "libCase.prx.guest.prx");
    bool ambiguous = false;
    try { ResolvePath_nid_no_patch("/sensitive-host/BOTH/file.txt"); }
    catch (const std::runtime_error& failure) { ambiguous = std::strstr(failure.what(), "Ambiguous case-insensitive guest path") != nullptr; }
    Require(ambiguous);
    RemovePathAlias_nid_no_patch("/sensitive-host");
    std::filesystem::remove_all(root);
}

static std::filesystem::path ExtendedPath(const std::filesystem::path& path) {
    const auto& native = path.native();
    if (native.starts_with(L"\\\\?\\") || native.starts_with(L"\\\\.\\")) return path;
    return native.starts_with(L"\\\\") ? std::filesystem::path(L"\\\\?\\UNC\\" + native.substr(2))
                                            : std::filesystem::path(L"\\\\?\\" + native);
}

static void TestLongHostPathStat() {
    auto root = std::filesystem::absolute(std::filesystem::temp_directory_path()).lexically_normal();
    if (root.filename().empty()) root = root.parent_path();
    root /= L"anyps5-stat-long-" + std::to_wstring(std::chrono::steady_clock::now().time_since_epoch().count());
    auto deep = root;
    while (deep.native().size() < MAX_PATH + 24) deep /= std::wstring(40, L'd');
    Require(::CreateDirectoryW(ExtendedPath(root).c_str(), nullptr) != 0);
    auto directory = root;
    for (const auto& component : std::filesystem::path(deep).lexically_relative(root)) {
        directory /= component;
        Require(::CreateDirectoryW(ExtendedPath(directory).c_str(), nullptr) != 0);
    }
    const auto file = deep / L"payload.bin";
    const auto handle = ::CreateFileW(ExtendedPath(file).c_str(), GENERIC_WRITE, 0, nullptr, CREATE_NEW, FILE_ATTRIBUTE_NORMAL, nullptr);
    Require(handle != INVALID_HANDLE_VALUE);
    DWORD written = 0;
    const bool wrote = ::WriteFile(handle, "metadata", 8, &written, nullptr) != 0 && written == 8;
    ::CloseHandle(handle);
    Require(wrote);
    const auto host = deep.string();
    AddPathAlias_nid_no_patch("/long-stat-host", host.c_str());
    FileStat status{};
    Require(ResolvePath_nid_no_patch("/long-stat-host/payload.bin").native().size() > MAX_PATH);
    Require(sceKernelStat("/long-stat-host/payload.bin", &status) == 0 && status.st_size == 8);
    Require(sceKernelStat("/long-stat-host/missing.bin", &status) == static_cast<int>(0x80020002u));
    Require(sceKernelStat("/long-stat-host/", &status) == 0 && (status.st_mode & 0170000) == 0040000);
    RemovePathAlias_nid_no_patch("/long-stat-host");
    const auto extendedHost = ExtendedPath(deep).string();
    AddPathAlias_nid_no_patch("/long-stat-extended", extendedHost.c_str());
    Require(ResolvePath_nid_no_patch("/long-stat-extended/payload.bin").native().starts_with(L"\\\\?\\"));
    Require(sceKernelStat("/long-stat-extended/payload.bin", &status) == 0 && status.st_size == 8);
    Require(sceKernelStat("/long-stat-extended/missing.bin", &status) == static_cast<int>(0x80020002u));
    RemovePathAlias_nid_no_patch("/long-stat-extended");
    ::DeleteFileW(ExtendedPath(file).c_str());
    for (auto path = deep; ; path = path.parent_path()) {
        ::RemoveDirectoryW(ExtendedPath(path).c_str());
        if (path == root) break;
    }
}
#endif

#ifdef __linux__
static int DirectoryOpens(int watch) {
    alignas(inotify_event) char buffer[4096];
    int opens = 0;
    for (ssize_t size; (size = read(watch, buffer, sizeof buffer)) > 0;) {
        for (char* event = buffer; event < buffer + size; event += sizeof(inotify_event) + reinterpret_cast<inotify_event*>(event)->len)
            if (reinterpret_cast<inotify_event*>(event)->mask & IN_OPEN) ++opens;
    }
    return opens;
}

static void TestMissingPathListsDirectoryOnce() {
    const auto root = std::filesystem::path("anyps5-missing-listing-" +
        std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    Require(std::filesystem::create_directories(root));
    { std::ofstream file(root / "present.txt"); file << "present"; }
    const int watch = inotify_init1(IN_NONBLOCK);
    Require(watch >= 0 && inotify_add_watch(watch, root.c_str(), IN_OPEN | IN_CLOSE_NOWRITE) >= 0);
    const auto missing = (root / "Missing.txt").generic_string();
    FileStat status{};
    Require(sceKernelStat(missing.c_str(), &status) == static_cast<int>(0x80020002u) && DirectoryOpens(watch) == 1);
    Require(sceKernelCheckReachability(missing.c_str()) == static_cast<int>(0x80020002u) && DirectoryOpens(watch) == 1);
    Require(sceKernelOpen(missing.c_str(), 0, 0) == static_cast<int>(0x80020002u) && DirectoryOpens(watch) == 1);
    close(watch);
    { std::ofstream file(root / "plugin.prx.guest.prx"); file << "lower"; }
    { std::ofstream file(root / "Plugin.prx.guest.prx"); file << "upper"; }
    bool ambiguous = false;
    try {
        sceKernelStat((root / "PLUGIN.prx").generic_string().c_str(), &status);

    } catch (const std::runtime_error&) {
        ambiguous = true;
    }
    Require(ambiguous);
    std::filesystem::remove_all(root);
}
#endif

int main() {
    TestParentTraversal();
    TestRelinkedModuleNames();
    TestRelinkedModuleNameCase();
#ifdef _WIN32
    TestCaseSensitiveHostDirectory();
    TestLongHostPathStat();
#endif
#ifdef __linux__
    TestMissingPathListsDirectoryOnce();
#endif
    Require(chmod_nid_postfix(nullptr, 0600) == -1 && *__error_nid_postfix() == 14);
    Require(sceKernelChmod_nid_postfix(nullptr, 0600) == static_cast<int>(0x8002000eu));
    Require(chmod_nid_postfix("", 0600) == -1 && *__error_nid_postfix() == 2);
    Require(chmod_nid_postfix("anyps5_missing_chmod_path", 0600) == -1 && *__error_nid_postfix() == 2);
    Require(sceKernelChmod_nid_postfix("anyps5_missing_chmod_path", 0600) == static_cast<int>(0x80020002u));
    Require(sceKernelStat("anyps5_missing_chmod_path", nullptr) == static_cast<int>(0x80020002u));
    Require(sceKernelStat(".", nullptr) == static_cast<int>(0x8002000eu));
    Require(sceKernelDebugOutText(-1, "text") == static_cast<int>(0x80020016u));
    std::uint64_t throttling[4] = {1, 2, 3, 4};
    Require(sceKernelWriteThrottlingStatus(throttling) == 0);
    Require(throttling[0] == 0xffffffffu && throttling[1] == 0 && throttling[2] == 0 && throttling[3] == 0);
    Require(sceKernelWriteThrottlingStatus(nullptr) == static_cast<int>(0x8002000eu));
    Require(sceKernelDebugOutText(0, nullptr) == static_cast<int>(0x8002000eu));
    auto* captured = std::tmpfile();
    Require(captured != nullptr);
#ifdef _WIN32
    const int saved = ::_dup(::_fileno(stderr));
    Require(saved >= 0 && ::_dup2(::_fileno(captured), ::_fileno(stderr)) == 0);
#else
    const int saved = ::dup(::fileno(stderr));
    Require(saved >= 0 && ::dup2(::fileno(captured), ::fileno(stderr)) == ::fileno(stderr));
#endif
    const int debugResult = sceKernelDebugOutText(3, "%s%d literal\n");
#ifdef _WIN32
    Require(::_dup2(saved, ::_fileno(stderr)) == 0 && ::_close(saved) == 0);
#else
    Require(::dup2(saved, ::fileno(stderr)) == ::fileno(stderr) && ::close(saved) == 0);
#endif
    Require(debugResult == 0);
    std::rewind(captured);
    char debugText[64]{};
    Require(std::fread(debugText, 1, sizeof(debugText), captured) == 23);
    Require(std::strcmp(debugText, "[debug:3] %s%d literal\n") == 0);
    Require(std::fclose(captured) == 0);
    Require(pipe_nid_postfix(nullptr) == -1 && *__error_nid_postfix() == 14);
    int descriptors[2] = {-1, -1};
    Require(pipe_nid_postfix(descriptors) == 0 && descriptors[0] >= 0 && descriptors[1] >= 0);
    const char payload[] = {'A', '\0', '\r', '\n', '\x1a', 'Z'};
    Require(write_nid_postfix(descriptors[1], payload, sizeof(payload)) == sizeof(payload));
    Require(close_nid_postfix(descriptors[1]) == 0);
    char received[sizeof(payload)]{};
    Require(read_nid_postfix(descriptors[0], received, sizeof(received)) == sizeof(received));
    Require(std::memcmp(received, payload, sizeof(payload)) == 0);
    Require(read_nid_postfix(descriptors[0], received, sizeof(received)) == 0);
    {
        int unseekable[2] = {-1, -1};
        Require(pipe_nid_postfix(unseekable) == 0 && write_nid_postfix(unseekable[1], "pipe", 4) == 4);
        for (const int end : unseekable) {
            for (int whence = 0; whence < 3; ++whence) {
                Require(lseek_nid_postfix(end, 0, whence) == -1 && *__error_nid_postfix() == 29);
                Require(sceKernelLseek(end, 1, whence) == static_cast<int>(0x8002001du));
                Require(lseek_nid_postfix(end, -1, whence) == -1 && *__error_nid_postfix() == 29);
                Require(sceKernelLseek(end, -100, whence) == static_cast<int>(0x8002001du));
            }
        }
        char piped[8]{};
        Require(read_nid_postfix(unseekable[0], piped, sizeof(piped)) == 4 && std::memcmp(piped, "pipe", 4) == 0);
        Require(close_nid_postfix(unseekable[0]) == 0 && close_nid_postfix(unseekable[1]) == 0);
        Require(lseek_nid_postfix(unseekable[0], 0, 1) == -1 && *__error_nid_postfix() == 9);
    }
    {
        std::filesystem::remove("creat_probe.bin");
        int created = creat_nid_postfix("creat_probe.bin", 0644);
        Require(created >= 0 && write_nid_postfix(created, "abc", 3) == 3);
        Require(close_nid_postfix(created) == 0 && std::filesystem::file_size("creat_probe.bin") == 3);
        created = creat_nid_postfix("creat_probe.bin", 0644);
        Require(created >= 0 && close_nid_postfix(created) == 0 && std::filesystem::file_size("creat_probe.bin") == 0);
        Require(std::filesystem::remove("creat_probe.bin"));
    }
    {
        int ends[2] = {-1, -1};
        Require(pipe_nid_postfix(ends) == 0 && write_nid_postfix(ends[1], "abc", 3) == 3);
        for (const int end : ends) {
            Require(sceKernelFtruncate(end, 0) == static_cast<int>(0x80020016u) && sceKernelFtruncate(end, 8) == static_cast<int>(0x80020016u));
        }
        char kept[4]{};
        Require(read_nid_postfix(ends[0], kept, sizeof(kept)) == 3 && std::memcmp(kept, "abc", 3) == 0);
        Require(close_nid_postfix(ends[0]) == 0 && close_nid_postfix(ends[1]) == 0);
        for (const char* name : {".", "/dev/urandom"}) {
            const int opened = open_nid_postfix(name, 0, 0);
            Require(opened >= 0 && sceKernelFtruncate(opened, 0) == static_cast<int>(0x80020016u) && close_nid_postfix(opened) == 0);
        }
    }
    Require(close_nid_postfix(descriptors[0]) == 0);
    auto* hostCapture = std::tmpfile();
    Require(hostCapture != nullptr && pipe_nid_postfix(descriptors) == 0);
#ifdef _WIN32
    const int hostError = ::_dup(2);
    Require(hostError >= 0 && ::_dup2(::_fileno(hostCapture), 2) == 0);
#else
    const int hostError = ::dup(2);
    Require(hostError >= 0 && ::dup2(::fileno(hostCapture), 2) == 2);
#endif
    Require(dup2_nid_postfix(descriptors[1], 2) == 2);
    Require(sceKernelDebugOutText(1, "kept\n") == 0);
    Require(write_nid_postfix(2, "P", 1) == 1);
#ifdef _WIN32
    Require(::_dup2(hostError, 2) == 0 && ::_close(hostError) == 0);
#else
    Require(::dup2(hostError, 2) == 2 && ::close(hostError) == 0);
#endif
    Require(close_nid_postfix(descriptors[1]) == 0);
    Require(read_nid_postfix(descriptors[0], received, sizeof(received)) == 1 && received[0] == 'P');
    Require(close_nid_postfix(descriptors[0]) == 0);
    std::rewind(hostCapture);
    Require(std::fread(debugText, 1, sizeof(debugText), hostCapture) == 15 && std::memcmp(debugText, "[debug:1] kept\n", 15) == 0);
    Require(std::fclose(hostCapture) == 0);
#ifdef _WIN32
    for (const bool reopen : {false, true}) {
        auto* hostCapture = std::tmpfile();
        const int hostError = ::_dup(2);
        Require(hostCapture != nullptr && hostError >= 0);
        int redirect[2] = {-1, -1};
        Require(reopen || pipe_nid_postfix(redirect) == 0);
        const bool captured = ::_dup2(::_fileno(hostCapture), 2) == 0 && SetStdHandle(STD_ERROR_HANDLE, reinterpret_cast<HANDLE>(::_get_osfhandle(2)));
        const bool redirected = reopen ? freopen_nid_postfix("stderr_probe.txt", "w", &_Stderr_nid_postfix) == &_Stderr_nid_postfix : dup2_nid_postfix(redirect[1], 2) == 2;
        const HANDLE kept = GetStdHandle(STD_ERROR_HANDLE);
        DWORD count = 0;
        const bool separate = kept != reinterpret_cast<HANDLE>(::_get_osfhandle(2)) && WriteFile(kept, "kept", 4, &count, nullptr) && count == 4;
        const bool written = write_nid_postfix(2, "G", 1) == 1;
        const bool restored = ::_dup2(hostError, 2) == 0 && ::_close(hostError) == 0 && SetStdHandle(STD_ERROR_HANDLE, reinterpret_cast<HANDLE>(::_get_osfhandle(2)));
        Require(captured && redirected && separate && written && restored && CloseHandle(kept));
        std::rewind(hostCapture);
        char keptText[8]{};
        Require(std::fread(keptText, 1, sizeof(keptText), hostCapture) == 4 && std::memcmp(keptText, "kept", 4) == 0);
        Require(std::fclose(hostCapture) == 0);
        char guestText = 0;
        if (reopen) {
            Require(std::filesystem::file_size("stderr_probe.txt") == 1 && std::filesystem::remove("stderr_probe.txt"));
        } else {
            Require(close_nid_postfix(redirect[1]) == 0 && read_nid_postfix(redirect[0], &guestText, 1) == 1 && guestText == 'G');
            Require(close_nid_postfix(redirect[0]) == 0);
        }
    }
#endif
    const auto root = std::filesystem::path("anyps5-filesystem-test-" +
        std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    Require(mkdir_nid_postfix(root.string().c_str(), 0700) == 0);
    Require(mkdir_nid_postfix(root.string().c_str(), 0700) == -1 && *__error_nid_postfix() == 17);
    for (const char* openMode : {"w", "wb", "r+", "a"}) {
        Require(fopen_nid_postfix(root.string().c_str(), openMode) == nullptr && *__error_nid_postfix() == 21);
    }
    {
        const auto listed = root / "listed.txt";
        std::ofstream(listed, std::ios::binary) << "listed";
        Require(fopen_nid_postfix(listed.string().c_str(), "wx") == nullptr && *__error_nid_postfix() == 17);
        for (const char* invalidMode : {"z", "", "q"}) {
            *__error_nid_postfix() = 0;
            Require(fopen_nid_postfix(listed.string().c_str(), invalidMode) == nullptr && *__error_nid_postfix() == 22);
        }
        auto* readWrite = fopen_nid_postfix(listed.string().c_str(), "rw");
        char word[8] = {};
        Require(readWrite != nullptr && fread_nid_postfix(word, 1, 6, readWrite) == 6 && std::string(word) == "listed");
        Require(fclose_nid_postfix(readWrite) == 0);
        for (const char* openMode : {"r", "rb", "re"}) {
            auto* directoryStream = fopen_nid_postfix(root.string().c_str(), openMode);
            Require(directoryStream != nullptr);
            const int directoryDescriptor = fileno_nid_postfix(directoryStream);
            FileStat directoryStatus{};
            Require(directoryDescriptor >= 0 && sceKernelFstat(directoryDescriptor, &directoryStatus) == 0 && (directoryStatus.st_mode & 0170000) == 0040000);
            Require(feof_nid_postfix(directoryStream) == 0 && ferror_nid_postfix(directoryStream) == 0 && fflush_nid_postfix(directoryStream) == 0);
            char unread[8] = {};
            int scanned = 0;
            *__error_nid_postfix() = 0;
            Require(fread_nid_postfix(unread, 1, sizeof(unread), directoryStream) == 0 && *__error_nid_postfix() == 21);
            Require(ferror_nid_postfix(directoryStream) != 0 && feof_nid_postfix(directoryStream) == 0);
            clearerr_nid_postfix(directoryStream);

            Require(ferror_nid_postfix(directoryStream) == 0);
            *__error_nid_postfix() = 0;
            Require(fgetc_nid_postfix(directoryStream) == -1 && *__error_nid_postfix() == 21 && ferror_nid_postfix(directoryStream) != 0);
            clearerr_nid_postfix(directoryStream);

            *__error_nid_postfix() = 0;
            Require(fgets_nid_postfix(unread, sizeof(unread), directoryStream) == nullptr && *__error_nid_postfix() == 21 && ferror_nid_postfix(directoryStream) != 0);
            clearerr_nid_postfix(directoryStream);

            *__error_nid_postfix() = 0;
            Require(fscanf_nid_postfix(directoryStream, "%d", &scanned) == -1 && *__error_nid_postfix() == 21 && ferror_nid_postfix(directoryStream) != 0);
            clearerr_nid_postfix(directoryStream);

            Require(fseek_nid_postfix(directoryStream, 0, 0) == 0 && ftell_nid_postfix(directoryStream) == 0);
            *__error_nid_postfix() = 0;
            Require(fputc_nid_postfix('x', directoryStream) == -1 && *__error_nid_postfix() == 9 && ferror_nid_postfix(directoryStream) != 0);
            Require(fclose_nid_postfix(directoryStream) == 0);
#ifdef _WIN32
            const int reused = open_nid_postfix(listed.string().c_str(), 0, 0);
            char text[6]{};
            Require(reused == directoryDescriptor && read_nid_postfix(reused, text, sizeof(text)) == 6 && std::memcmp(text, "listed", 6) == 0);
            Require(close_nid_postfix(reused) == 0);
#endif
        }
        Require(std::filesystem::remove(listed));
    }
    Require(mkdir_nid_postfix((root / "missing" / "child").string().c_str(), 0700) == -1 && *__error_nid_postfix() == 2);
    Require(mkdir_nid_postfix(nullptr, 0700) == -1 && *__error_nid_postfix() == 14);
    const auto file = root / "file.txt";
    { std::ofstream stream(file); stream << "retained until removal"; }
    Require(chflags_nid_postfix(nullptr, 0) == -1 && *__error_nid_postfix() == 14);
    Require(chflags_nid_postfix("", 0) == -1 && *__error_nid_postfix() == 2);
    Require(chflags_nid_postfix((root / "missing").string().c_str(), 0) == -1 && *__error_nid_postfix() == 2);
    Require(chflags_nid_postfix((root / "missing").string().c_str(), ~0ULL) == -1 && *__error_nid_postfix() == 2);
    Require(chflags_nid_postfix((file / "child").string().c_str(), 0) == -1 && *__error_nid_postfix() == 20);
    Require(chflags_nid_postfix((file.string() + "/").c_str(), 0) == -1 && *__error_nid_postfix() == 20);
    Require(chflags_nid_postfix(file.string().c_str(), 0) == 0);
    Require(chflags_nid_postfix(root.string().c_str(), 0) == 0);
    Require(chflags_nid_postfix(file.string().c_str(), 0x20) == -1 && *__error_nid_postfix() == 45);
    Require(chflags_nid_postfix(file.string().c_str(), 1ULL << 32) == -1 && *__error_nid_postfix() == 45);
    Require(chflags_nid_postfix(file.string().c_str(), ~0ULL) == -1 && *__error_nid_postfix() == 45);
    Require(chflags_nid_postfix(file.string().c_str(), 0x200000) == -1 && *__error_nid_postfix() == 1);
    Require(ChflagsThrows(file, 0x2, "UF_IMMUTABLE"));
    Require(ChflagsThrows(file, 0x1000 | 0x40000, "UF_READONLY|SF_APPEND"));
    const auto chflagsDangling = root / "dangling.link";
    const auto chflagsPointing = root / "file.link";
#ifdef _WIN32
    const bool chflagsLinked = CreateSymbolicLinkW(chflagsDangling.wstring().c_str(), L"missing.bin", SYMBOLIC_LINK_FLAG_ALLOW_UNPRIVILEGED_CREATE) != 0 &&
        CreateSymbolicLinkW(chflagsPointing.wstring().c_str(), L"file.txt", SYMBOLIC_LINK_FLAG_ALLOW_UNPRIVILEGED_CREATE) != 0;
#else
    std::error_code chflagsLinkError;
    std::filesystem::create_symlink("missing.bin", chflagsDangling, chflagsLinkError);
    if (!chflagsLinkError) std::filesystem::create_symlink("file.txt", chflagsPointing, chflagsLinkError);
    const bool chflagsLinked = !chflagsLinkError;
#endif
    if (chflagsLinked) {
        Require(chflags_nid_postfix(chflagsDangling.string().c_str(), 0) == -1 && *__error_nid_postfix() == 2);
        Require(chflags_nid_postfix(chflagsPointing.string().c_str(), 0) == 0);
        Require(chflags_nid_postfix((chflagsPointing.string() + "/").c_str(), 0) == -1 && *__error_nid_postfix() == 20);
        Require(chflags_nid_postfix(chflagsPointing.string().c_str(), 0x20) == -1 && *__error_nid_postfix() == 45);
        Require(unlink_nid_postfix(chflagsDangling.string().c_str()) == 0 && unlink_nid_postfix(chflagsPointing.string().c_str()) == 0);
    } else {
        unlink_nid_postfix(chflagsDangling.string().c_str());
        std::fprintf(stderr, "symbolic links unavailable: the chflags link cases not tested\n");
    }
    const int flagged = open_nid_postfix(file.string().c_str(), 0, 0);
    Require(flagged >= 0 && fchflags_nid_postfix(flagged, 0) == 0);
    Require(fchflags_nid_postfix(flagged, 0x20) == -1 && *__error_nid_postfix() == 45);
    Require(fchflags_nid_postfix(flagged, 1ULL << 32) == -1 && *__error_nid_postfix() == 45);
    Require(fchflags_nid_postfix(flagged, ~0ULL) == -1 && *__error_nid_postfix() == 45);
    Require(fchflags_nid_postfix(flagged, 0x200000) == -1 && *__error_nid_postfix() == 1);
    Require(FchflagsThrows(flagged, 0x2, "UF_IMMUTABLE"));
    Require(FchflagsThrows(flagged, 0x1000 | 0x40000, "UF_READONLY|SF_APPEND"));
    Require(close_nid_postfix(flagged) == 0);
    Require(fchflags_nid_postfix(flagged, 0) == -1 && *__error_nid_postfix() == 9);
    Require(fchflags_nid_postfix(flagged, ~0ULL) == -1 && *__error_nid_postfix() == 9);
    Require(fchflags_nid_postfix(-1, 0) == -1 && *__error_nid_postfix() == 9);
    const int flaggedKqueue = kqueue_nid_postfix();
    Require(flaggedKqueue >= 0);
    Require(fchflags_nid_postfix(flaggedKqueue, 0) == -1 && *__error_nid_postfix() == 22);
    Require(close_nid_postfix(flaggedKqueue) == 0);
    const int flaggedRoot = sceKernelOpen(root.string().c_str(), 0x20000, 0);
    Require(flaggedRoot >= 0 && fchflags_nid_postfix(flaggedRoot, 0) == 0 && sceKernelClose(flaggedRoot) == 0);
    const auto exclusiveLink = root / "exclusive.link";
    const auto exclusiveTarget = root / "exclusive-target.txt";
#ifdef _WIN32
    const bool exclusiveLinked = CreateSymbolicLinkW(exclusiveLink.wstring().c_str(), L"exclusive-target.txt",
        SYMBOLIC_LINK_FLAG_ALLOW_UNPRIVILEGED_CREATE) != 0;
#else
    std::error_code exclusiveLinkError;
    std::filesystem::create_symlink("exclusive-target.txt", exclusiveLink, exclusiveLinkError);
    const bool exclusiveLinked = !exclusiveLinkError;
#endif
    if (exclusiveLinked) {
        Require(open_nid_postfix(exclusiveLink.string().c_str(), 0xa01, 0644) == -1 && *__error_nid_postfix() == 17);
        Require(sceKernelOpen(exclusiveLink.string().c_str(), 0xa01, 0644) == static_cast<int>(0x80020011u));
        Require(!std::filesystem::exists(exclusiveTarget));
        const int exclusiveCreated = open_nid_postfix(exclusiveLink.string().c_str(), 0x201, 0644);
        Require(exclusiveCreated >= 0 && close_nid_postfix(exclusiveCreated) == 0 && std::filesystem::is_regular_file(exclusiveTarget));
        Require(open_nid_postfix(exclusiveLink.string().c_str(), 0xa01, 0644) == -1 && *__error_nid_postfix() == 17);
        Require(unlink_nid_postfix(exclusiveLink.string().c_str()) == 0 && unlink_nid_postfix(exclusiveTarget.string().c_str()) == 0);
    } else {
        std::fprintf(stderr, "symbolic links unavailable: the O_EXCL link cases not tested\n");
    }
    int flaggedPipe[2];
    Require(pipe_nid_postfix(flaggedPipe) == 0);
    Require(fchflags_nid_postfix(flaggedPipe[0], 0) == -1 && *__error_nid_postfix() == 22);
    Require(fchflags_nid_postfix(flaggedPipe[1], ~0ULL) == -1 && *__error_nid_postfix() == 22);
    Require(close_nid_postfix(flaggedPipe[0]) == 0 && close_nid_postfix(flaggedPipe[1]) == 0);
    const int flaggedSocket = socket_nid_postfix(2, 2, 0);
    Require(flaggedSocket >= 0 && fchflags_nid_postfix(flaggedSocket, 0) == -1 && *__error_nid_postfix() == 22);
    Require(close_nid_postfix(flaggedSocket) == 0);
    const auto held = root / "held.txt";
    const auto heldName = held.string();
    { std::ofstream stream(held); stream << "held"; }
    const int heldDescriptor = sceKernelOpen(heldName.c_str(), 2, 0);
    Require(heldDescriptor >= 0 && sceKernelUnlink(heldName.c_str()) == 0 && !std::filesystem::exists(held));
    char heldBytes[4]{};
    Require(read_nid_postfix(heldDescriptor, heldBytes, sizeof(heldBytes)) == sizeof(heldBytes) && std::memcmp(heldBytes, "held", 4) == 0);
    const int recreated = sceKernelOpen(heldName.c_str(), 0x0a01, 0644);
    Require(recreated >= 0 && sceKernelClose(heldDescriptor) == 0 && std::filesystem::file_size(held) == 0);
    const auto moved = root / "moved.txt";
    Require(rename_nid_postfix(heldName.c_str(), moved.string().c_str()) == 0 && !std::filesystem::exists(held));
    Require(write_nid_postfix(recreated, "moved", 5) == 5 && sceKernelClose(recreated) == 0 && std::filesystem::file_size(moved) == 5);
    const int removing = open_nid_postfix(moved.string().c_str(), 0, 0);
    Require(removing >= 0 && remove_nid_postfix(moved.string().c_str()) == 0 && !std::filesystem::exists(moved));
    Require(close_nid_postfix(removing) == 0);
    Require(remove_nid_postfix(root.string().c_str()) == -1);
    Require(*__error_nid_postfix() == 66);
    Require(std::filesystem::is_regular_file(file));
    Require(remove_nid_postfix((file / "invalid").string().c_str()) == -1);
    Require(remove_nid_postfix("") == -1 && *__error_nid_postfix() == 2);
    Require(remove_nid_postfix(nullptr) == -1 && *__error_nid_postfix() == 14);
    const auto readOnly = root / "read-only.txt";
    { std::ofstream stream(readOnly); stream << "removable"; }
    Require(sceKernelChmod_nid_postfix(readOnly.string().c_str(), 0400) == 0);
    Require(remove_nid_postfix(readOnly.string().c_str()) == 0 && !std::filesystem::exists(readOnly));
    const auto renamed = root / "renamed.txt";
    { std::ofstream stream(renamed); stream << "old contents"; }
    Require(rename_nid_postfix(file.string().c_str(), renamed.string().c_str()) == 0);
    Require(!std::filesystem::exists(file));
    { std::ifstream stream(renamed); std::string contents; std::getline(stream, contents);
      Require(contents == "retained until removal"); }
    Require(rename_nid_postfix(renamed.string().c_str(), renamed.string().c_str()) == 0);
    Require(rename_nid_postfix(file.string().c_str(), renamed.string().c_str()) == -1);
    Require(*__error_nid_postfix() == 2);
    Require(rename_nid_postfix(renamed.string().c_str(), file.string().c_str()) == 0);
    {
        const auto area = root / "libc_rename";
        const auto sourceFile = area / "source.txt";
        const auto targetFile = area / "target.txt";
        const auto sourceDirectory = area / "source_directory";
        const auto emptyDirectory = area / "empty_directory";
        const auto fullDirectory = area / "full_directory";
        Require(std::filesystem::create_directories(area));
        { std::ofstream stream(sourceFile); stream << "source"; }
        { std::ofstream stream(targetFile); stream << "target"; }
        Require(std::filesystem::create_directories(sourceDirectory / "child"));
        Require(std::filesystem::create_directories(emptyDirectory));
        Require(std::filesystem::create_directories(fullDirectory));
        { std::ofstream stream(fullDirectory / "entry"); stream << "entry"; }
        const auto name = [](const std::filesystem::path& path) { return path.string(); };
        const auto missing = name(area / "missing");
        const auto absentParent = name(area / "absent" / "target");
        const auto fileParent = name(sourceFile / "target");
        Require(rename_nid_postfix(missing.c_str(), name(targetFile).c_str()) == -1 && *__error_nid_postfix() == 2);
        Require(rename_nid_postfix(name(sourceFile).c_str(), absentParent.c_str()) == -1 && *__error_nid_postfix() == 2);
        Require(rename_nid_postfix(name(sourceFile).c_str(), fileParent.c_str()) == -1 && *__error_nid_postfix() == 20);
        Require(rename_nid_postfix(name(sourceFile).c_str(), name(fullDirectory).c_str()) == -1 && *__error_nid_postfix() == 21);
        Require(rename_nid_postfix(name(sourceDirectory).c_str(), name(targetFile).c_str()) == -1 && *__error_nid_postfix() == 20);
        Require(rename_nid_postfix(name(sourceDirectory).c_str(), name(fullDirectory).c_str()) == -1 && *__error_nid_postfix() == 66);
        const auto childDestination = name(sourceDirectory / "child" / "moved");
        Require(rename_nid_postfix(name(sourceDirectory).c_str(), childDestination.c_str()) == -1 && *__error_nid_postfix() == 22);
        const auto childDirectory = name(sourceDirectory / "child");
        Require(rename_nid_postfix(name(sourceDirectory).c_str(), childDirectory.c_str()) == -1 && *__error_nid_postfix() == 22);
        Require(std::filesystem::is_regular_file(sourceFile) && std::filesystem::is_regular_file(fullDirectory / "entry"));
        Require(rename_nid_postfix(name(sourceFile).c_str(), name(targetFile).c_str()) == 0);
        { std::ifstream stream(targetFile); std::string contents; std::getline(stream, contents); Require(contents == "source"); }
        Require(!std::filesystem::exists(sourceFile));
        Require(rename_nid_postfix(name(targetFile).c_str(), name(targetFile).c_str()) == 0);
        Require(rename_nid_postfix(name(sourceDirectory).c_str(), name(emptyDirectory).c_str()) == 0);
        Require(!std::filesystem::exists(sourceDirectory) && std::filesystem::is_directory(emptyDirectory / "child"));
        const auto kernelSource = area / "kernel_source";
        const auto kernelTarget = area / "kernel_target";
        Require(std::filesystem::create_directories(kernelSource / "child"));
        Require(std::filesystem::create_directory(kernelTarget));
        { std::ofstream stream(kernelSource / "marker"); stream << "kernel directory contents"; }
        const int directoryResult = sceKernelRename(name(kernelSource).c_str(), name(kernelTarget).c_str());
        if (directoryResult != 0)
            std::fprintf(stderr, "kernel empty-directory replacement result=0x%08x, expected=0\n", static_cast<unsigned>(directoryResult));
        Require(directoryResult == 0);
        Require(!std::filesystem::exists(kernelSource) && std::filesystem::is_directory(kernelTarget / "child"));
        { std::ifstream stream(kernelTarget / "marker"); std::string contents; std::getline(stream, contents);
          Require(contents == "kernel directory contents"); }
        const int parentResult = sceKernelRename(name(targetFile).c_str(), absentParent.c_str());
        Require(parentResult == static_cast<int>(0x80020002u));
        Require(std::filesystem::is_regular_file(targetFile) && !std::filesystem::exists(area / "absent"));
        const int prefixResult = sceKernelRename(name(targetFile).c_str(), name(targetFile / "target").c_str());
        Require(prefixResult == static_cast<int>(0x80020014u));
        Require(std::filesystem::is_regular_file(targetFile));
        const int fileDirectoryResult = sceKernelRename(name(targetFile).c_str(), name(fullDirectory).c_str());
        Require(fileDirectoryResult == static_cast<int>(0x80020015u));
        Require(std::filesystem::is_regular_file(targetFile) && std::filesystem::is_regular_file(fullDirectory / "entry"));
        const int directoryFileResult = sceKernelRename(name(kernelTarget).c_str(), name(targetFile).c_str());
        Require(directoryFileResult == static_cast<int>(0x80020014u));
        Require(std::filesystem::is_regular_file(kernelTarget / "marker") && std::filesystem::is_regular_file(targetFile));
        const int nonemptyResult = sceKernelRename(name(kernelTarget).c_str(), name(fullDirectory).c_str());
        Require(nonemptyResult == static_cast<int>(0x80020042u));
        Require(std::filesystem::is_regular_file(kernelTarget / "marker") && std::filesystem::is_regular_file(fullDirectory / "entry"));
        const auto descendant = kernelTarget / "child" / "moved";
        const int descendantResult = sceKernelRename(name(kernelTarget).c_str(), name(descendant).c_str());
        Require(descendantResult == static_cast<int>(0x80020016u));
        Require(std::filesystem::is_directory(kernelTarget / "child") && !std::filesystem::exists(descendant));
        { std::ifstream stream(targetFile); std::string contents; std::getline(stream, contents); Require(contents == "source"); }
        { std::ifstream stream(fullDirectory / "entry"); std::string contents; std::getline(stream, contents); Require(contents == "entry"); }
        { std::ifstream stream(kernelTarget / "marker"); std::string contents; std::getline(stream, contents);
          Require(contents == "kernel directory contents"); }
#ifdef _WIN32
        for (const bool kernel : {true, false}) {
            const auto lockedSource = area / (kernel ? "locked_kernel_source" : "locked_libc_source");
            const auto preservedTarget = area / (kernel ? "preserved_kernel_target" : "preserved_libc_target");
            Require(std::filesystem::create_directory(lockedSource));
            Require(std::filesystem::create_directory(preservedTarget));
            const auto marker = lockedSource / "marker";
            { std::ofstream stream(marker); stream << "preserved source contents"; }
            const HANDLE held = CreateFileW(marker.c_str(), GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE,
                nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
            Require(held != INVALID_HANDLE_VALUE);
            const int result = kernel ? sceKernelRename(name(lockedSource).c_str(), name(preservedTarget).c_str()) :
                rename_nid_postfix(name(lockedSource).c_str(), name(preservedTarget).c_str());

            Require(CloseHandle(held) != 0);
            Require(result != 0 && std::filesystem::is_regular_file(marker));
            if (!std::filesystem::is_directory(preservedTarget))
                std::fprintf(stderr, "failed %s directory rename removed its destination\n", kernel ? "kernel" : "libc");
            Require(std::filesystem::is_directory(preservedTarget) && std::filesystem::is_empty(preservedTarget));
            { std::ifstream stream(marker); std::string contents; std::getline(stream, contents);
              Require(contents == "preserved source contents"); }
        }
#endif
        for (const bool kernel : {false, true}) {
            const auto incoming = name(area / "incoming.txt");
            const auto occupied = name(area / "occupied.txt");
            const auto replace = [&] { return kernel ? sceKernelRename(incoming.c_str(), occupied.c_str()) : rename_nid_postfix(incoming.c_str(), occupied.c_str()); };
            const auto contents = [&] { std::ifstream stream(occupied); std::string text; std::getline(stream, text); return text; };
            { std::ofstream stream(incoming); stream << "incoming"; }
            { std::ofstream stream(occupied); stream << "occupied"; }
            const int occupiedDescriptor = sceKernelOpen(occupied.c_str(), 2, 0);
            Require(occupiedDescriptor >= 0 && replace() == 0 && !std::filesystem::exists(incoming) && contents() == "incoming");
            char occupiedBytes[8]{};
            Require(read_nid_postfix(occupiedDescriptor, occupiedBytes, sizeof(occupiedBytes)) == sizeof(occupiedBytes) && std::memcmp(occupiedBytes, "occupied", 8) == 0);
            Require(sceKernelClose(occupiedDescriptor) == 0 && contents() == "incoming");
            { std::ofstream stream(incoming); stream << "replaced"; }
            Require(sceKernelChmod_nid_postfix(occupied.c_str(), 0400) == 0);
            Require(replace() == 0 && !std::filesystem::exists(incoming) && contents() == "replaced");
            Require(remove_nid_postfix(occupied.c_str()) == 0);
        }
#ifndef _WIN32
        const auto dangling = area / "dangling";
        std::filesystem::create_symlink(area / "nonexistent", dangling);
        Require(std::filesystem::is_symlink(dangling));
        const auto replacement = area / "replacement";
        { std::ofstream stream(replacement); stream << "replacement"; }
        Require(rename_nid_postfix(name(replacement).c_str(), name(dangling).c_str()) == 0);
        Require(!std::filesystem::is_symlink(dangling) && std::filesystem::is_regular_file(dangling));
#endif
        std::error_code cleanupError;
        for (int attempt = 0; attempt < 10; ++attempt) {
            std::filesystem::remove_all(area, cleanupError);
            if (!cleanupError) break;
            std::this_thread::sleep_for(std::chrono::milliseconds(20));
        }
        Require(!cleanupError);
    }
    Require(remove_nid_postfix(file.string().c_str()) == 0);
    Require(!std::filesystem::exists(file));
    Require(remove_nid_postfix(file.string().c_str()) == -1 && *__error_nid_postfix() == 2);
    const auto sized = root / "sized.txt";
    { std::ofstream stream(sized); stream << "0123456789abcdef"; }
    Require(sceKernelChmod_nid_postfix(sized.string().c_str(), 0600) == 0);
    Require(sceKernelTruncate_nid_postfix(sized.string().c_str(), 6) == 0);
    Require(std::filesystem::file_size(sized) == 6);
    { std::ifstream stream(sized); std::string contents; std::getline(stream, contents);
      Require(contents == "012345"); }
    Require(sceKernelTruncate_nid_postfix((sized / "missing").string().c_str(), 6) == static_cast<int>(0x80020014u));
    Require(sceKernelUtimes_nid_postfix(sized.string().c_str(), nullptr) == 0);
    std::FILE* native = std::fopen(sized.string().c_str(), "r+b");
    Require(native != nullptr);
#ifdef _WIN32
    const int descriptor = _fileno(native);
#else
    const int descriptor = ::fileno(native);
#endif
    Require(descriptor >= 0 && sceKernelFsync(descriptor) == 0);
    Require(fdatasync_nid_postfix(descriptor) == 0);
    Require(sceKernelFdatasync(descriptor) == 0);
    const auto ownerWrite = [&] {
        return (std::filesystem::status(sized).permissions() & std::filesystem::perms::owner_write) != std::filesystem::perms::none;
    };
    Require(sceKernelFchmod(descriptor, 0400) == 0 && !ownerWrite());
    Require(fchmod_nid_postfix(descriptor, 0600) == 0 && ownerWrite());
    const auto sizedLink = sized.parent_path() / "sized.link";
    const auto missingLink = sized.parent_path() / "missing.link";
    const auto loopLink = sized.parent_path() / "loop.link";
#ifdef _WIN32
    const DWORD linkFlags = SYMBOLIC_LINK_FLAG_ALLOW_UNPRIVILEGED_CREATE;
    const bool chmodLinked = CreateSymbolicLinkW(sizedLink.wstring().c_str(), sized.filename().wstring().c_str(), linkFlags) &&
        CreateSymbolicLinkW(missingLink.wstring().c_str(), L"missing.bin", linkFlags) &&
        CreateSymbolicLinkW(loopLink.wstring().c_str(), L"loop.link", linkFlags);
#else
    std::error_code chmodLinkError;
    std::filesystem::create_symlink(sized.filename(), sizedLink, chmodLinkError);
    if (!chmodLinkError) std::filesystem::create_symlink("missing.bin", missingLink, chmodLinkError);
    if (!chmodLinkError) std::filesystem::create_symlink("loop.link", loopLink, chmodLinkError);
    const bool chmodLinked = !chmodLinkError;
#endif
    if (chmodLinked) {
        Require(chmod_nid_postfix(sizedLink.string().c_str(), 0400) == 0 && !ownerWrite());
#ifdef _WIN32
        Require((GetFileAttributesW(sizedLink.wstring().c_str()) & FILE_ATTRIBUTE_READONLY) == 0);
#endif
        Require(chmod_nid_postfix(sizedLink.string().c_str(), 0600) == 0 && ownerWrite());
        Require(chmod_nid_postfix(missingLink.string().c_str(), 0600) == -1 && *__error_nid_postfix() == 2);
#ifdef _WIN32
        Require(chmod_nid_postfix(loopLink.string().c_str(), 0600) == -1 && *__error_nid_postfix() == 62);
#endif
    } else {
        std::fprintf(stderr, "symbolic links unavailable: the chmod link cases not tested\n");
    }
    unlink_nid_postfix(sizedLink.string().c_str());
    unlink_nid_postfix(missingLink.string().c_str());
    unlink_nid_postfix(loopLink.string().c_str());
    Require(sceKernelFtruncate(descriptor, 3) == 0);
    FileStat times{};
    const KernelTimeval past[2]{{1000000000, 0}, {1000000000, 500000}};
    Require(sceKernelFutimes(descriptor, past) == 0);
    Require(stat_nid_postfix(sized.string().c_str(), &times) == 0 && times.st_mtim.tv_sec == 1000000000);
    Require(futimes_nid_postfix(descriptor, nullptr) == 0);
    Require(stat_nid_postfix(sized.string().c_str(), &times) == 0 && times.st_mtim.tv_sec > 1000000000);
    const KernelTimeval overflow[2]{{0, 0}, {0, 1000000}};
    Require(sceKernelFutimes(descriptor, overflow) == static_cast<int>(0x80020016u));
    Require(futimes_nid_postfix(descriptor, overflow) == -1 && *__error_nid_postfix() == 22);
    const KernelTimeval negative[2]{{0, -1}, {0, 0}};
    Require(futimes_nid_postfix(descriptor, negative) == -1 && *__error_nid_postfix() == 22);
    Require(std::fclose(native) == 0);
    Require(std::filesystem::file_size(sized) == 3);
    Require(sceKernelFchmod(descriptor, 0600) == static_cast<int>(0x80020009u));
    Require(fchmod_nid_postfix(descriptor, 0600) == -1 && *__error_nid_postfix() == 9);
    Require(sceKernelFutimes(descriptor, nullptr) == static_cast<int>(0x80020009u));
    Require(futimes_nid_postfix(descriptor, nullptr) == -1 && *__error_nid_postfix() == 9);
    Require(fdatasync_nid_postfix(descriptor) == -1 && *__error_nid_postfix() == 9);
    Require(sceKernelFdatasync(descriptor) == static_cast<int>(0x80020009u));
    Require(fsync_nid_postfix(descriptor) == -1 && *__error_nid_postfix() == 9);
    Require(sceKernelFsync(descriptor) == static_cast<int>(0x80020009u));
    const int socket = socket_nid_postfix(2, 2, 0);
    Require(socket >= 0);
    Require(sceKernelFchmod(socket, 0600) == static_cast<int>(0x80020016u));
    Require(fchmod_nid_postfix(socket, 0600) == -1 && *__error_nid_postfix() == 22);
    Require(futimes_nid_postfix(socket, nullptr) == -1 && *__error_nid_postfix() == 22);
    Require(fdatasync_nid_postfix(socket) == -1 && *__error_nid_postfix() == 22);
    Require(sceKernelFdatasync(socket) == static_cast<int>(0x80020016u));
    Require(fsync_nid_postfix(socket) == -1 && *__error_nid_postfix() == 22);
    Require(sceKernelFsync(socket) == static_cast<int>(0x80020016u));
    int syncPipe[2] = {-1, -1};
    Require(pipe_nid_postfix(syncPipe) == 0);
    for (const int end : syncPipe) {
        Require(fsync_nid_postfix(end) == -1 && *__error_nid_postfix() == 22);
        Require(fdatasync_nid_postfix(end) == -1 && *__error_nid_postfix() == 22);
        Require(sceKernelFsync(end) == static_cast<int>(0x80020016u) && sceKernelFdatasync(end) == static_cast<int>(0x80020016u));
        Require(fchmod_nid_postfix(end, 0600) == -1 && *__error_nid_postfix() == 22);
        Require(sceKernelFchmod(end, 0600) == static_cast<int>(0x80020016u));
    }
    Require(close_nid_postfix(syncPipe[0]) == 0 && close_nid_postfix(syncPipe[1]) == 0);
    Require(close_nid_postfix(socket) == 0);
    Require(fchmod_nid_postfix(socket, 0600) == -1 && *__error_nid_postfix() == 9);
    Require(futimes_nid_postfix(socket, nullptr) == -1 && *__error_nid_postfix() == 9);
    Require(fdatasync_nid_postfix(socket) == -1 && *__error_nid_postfix() == 9);
    Require(sceKernelFdatasync(socket) == static_cast<int>(0x80020009u));
    Require(fsync_nid_postfix(socket) == -1 && *__error_nid_postfix() == 9);
    Require(sceKernelFsync(socket) == static_cast<int>(0x80020009u));
    Require(sceKernelFdatasync(0x7fffffff) == static_cast<int>(0x80020009u));
    Require(sceKernelFsync(0x7fffffff) == static_cast<int>(0x80020009u));
#ifdef _WIN32
    const auto previousParameterHandler = _set_invalid_parameter_handler(RecordInvalidParameter);
#endif
    FileStat closedStatus{};
    Require(sceKernelFstat(descriptor, &closedStatus) == static_cast<int>(0x80020009u));
    Require(sceKernelFstat(socket, &closedStatus) == static_cast<int>(0x80020009u));
    Require(sceKernelFstat(0x7fffffff, &closedStatus) == static_cast<int>(0x80020009u));
#ifdef _WIN32
    _set_invalid_parameter_handler(previousParameterHandler);
    Require(!g_invalidParameter);
#endif
    Require(remove_nid_postfix(sized.string().c_str()) == 0);
    const auto present = root / "present.txt";
    const auto presentName = present.string();
    const auto missing = root / "missing.txt";
    const auto missingName = missing.string();
    const auto rootName = root.string();
    { std::ofstream stream(present); stream << "posix"; }
    FileStat status{};
    Require(sceKernelStat("", &status) == static_cast<int>(0x80020002u));
    Require(sceKernelCheckReachability("") == static_cast<int>(0x80020002u));
    Require(sceKernelRename("", presentName.c_str()) == static_cast<int>(0x80020002u));
    Require(sceKernelRename(presentName.c_str(), "") == static_cast<int>(0x80020002u));
    Require(sceKernelRename(missingName.c_str(), nullptr) == static_cast<int>(0x80020002u));
    Require(sceKernelRename("", nullptr) == static_cast<int>(0x80020002u));
    Require(sceKernelRename(".", nullptr) == static_cast<int>(0x80020016u));
    Require(sceKernelRename(missingName.c_str(), ".") == static_cast<int>(0x80020002u));
    Require(sceKernelRename(presentName.c_str(), nullptr) == static_cast<int>(0x8002000Eu));
    Require(sceKernelRename(nullptr, presentName.c_str()) == static_cast<int>(0x8002000Eu));
    Require(rename_nid_postfix(missingName.c_str(), nullptr) == -1 && *__error_nid_postfix() == 2);
    Require(rename_nid_postfix(presentName.c_str(), nullptr) == -1 && *__error_nid_postfix() == 14);
    Require(sceKernelRmdir("") == static_cast<int>(0x80020002u));
    Require(sceKernelMkdir("", 0700) == static_cast<int>(0x80020002u));
    const auto cwdPermissions = std::filesystem::status(".").permissions();
    Require(sceKernelChmod_nid_postfix("", static_cast<unsigned short>(cwdPermissions) & 07777) == static_cast<int>(0x80020002u));
    Require(sceKernelTruncate_nid_postfix("", 0) == static_cast<int>(0x80020002u));
    Require(sceKernelTruncate_nid_postfix(nullptr, -1) == static_cast<int>(0x80020016u));
    Require(sceKernelTruncate_nid_postfix("", -1) == static_cast<int>(0x80020016u));
    Require(truncate_nid_postfix(nullptr, -1) == -1 && *__error_nid_postfix() == 22);
    Require(truncate_nid_postfix("", -1) == -1 && *__error_nid_postfix() == 22);
    Require(sceKernelTruncate_nid_postfix(nullptr, 0) == static_cast<int>(0x8002000Eu));
    const auto cwdWriteTime = std::filesystem::last_write_time(".");
    const int emptyUtimes = sceKernelUtimes_nid_postfix("", nullptr);
    std::error_code restoreTimeError;
    std::filesystem::last_write_time(".", cwdWriteTime, restoreTimeError);
    Require(emptyUtimes == static_cast<int>(0x80020002u));
    Require(stat_nid_postfix(presentName.c_str(), &status) == 0 && status.st_size == 5);
    Require(stat_nid_postfix(missingName.c_str(), &status) == -1 && *__error_nid_postfix() == 2);
    Require(sceKernelStat(missingName.c_str(), &status) == static_cast<int>(0x80020002u));
    Require(stat_nid_postfix("", &status) == -1 && *__error_nid_postfix() == 2);
    Require(stat_nid_postfix((presentName + "/").c_str(), &status) == -1 && *__error_nid_postfix() == 20);
    Require(sceKernelStat((presentName + "/").c_str(), &status) == static_cast<int>(0x80020014u));
    Require(sceKernelStat("\xff\xfe", &status) == static_cast<int>(0x80020002u));
    Require(stat_nid_postfix("\xff\xfe", &status) == -1 && *__error_nid_postfix() == 2);
    FileStat dirStatus{};
    Require(stat_nid_postfix((rootName + "/").c_str(), &dirStatus) == 0 && (dirStatus.st_mode & 0170000) == 0040000);
    const auto viaParent = rootName + "/../" + root.filename().string() + "/present.txt";
    Require(stat_nid_postfix(viaParent.c_str(), &status) == 0 && status.st_size == 5);
    Require(stat_nid_postfix(nullptr, &status) == -1 && *__error_nid_postfix() == 14);
    Require(stat_nid_postfix(presentName.c_str(), nullptr) == -1 && *__error_nid_postfix() == 14);
    FileStat linkStatus{};
    Require(lstat_nid_postfix(presentName.c_str(), &linkStatus) == 0 && linkStatus.st_size == 5);
    Require((linkStatus.st_mode & 0170000) == 0100000 && linkStatus.st_ino == status.st_ino);
    Require(lstat_nid_postfix(rootName.c_str(), &linkStatus) == 0 && (linkStatus.st_mode & 0170000) == 0040000);
    Require(lstat_nid_postfix(missingName.c_str(), &linkStatus) == -1 && *__error_nid_postfix() == 2);
    Require(lstat_nid_postfix("", &linkStatus) == -1 && *__error_nid_postfix() == 2);
    Require(lstat_nid_postfix(nullptr, &linkStatus) == -1 && *__error_nid_postfix() == 14);
    Require(lstat_nid_postfix(presentName.c_str(), nullptr) == -1 && *__error_nid_postfix() == 14);
#ifndef _WIN32
    const auto link = root / "link";
    std::filesystem::create_symlink("present.txt", link);
    Require(lstat_nid_postfix(link.string().c_str(), &linkStatus) == 0);
    Require((linkStatus.st_mode & 0170000) == 0120000 && linkStatus.st_size == 11);
    Require(stat_nid_postfix(link.string().c_str(), &status) == 0 && (status.st_mode & 0170000) == 0100000 && status.st_size == 5);
    const auto dangling = root / "dangling";
    std::filesystem::create_symlink("missing.txt", dangling);
    const auto statLoop = root / "stat-loop";
    std::filesystem::create_symlink("stat-loop", statLoop);
    errno = 0;
    Require(!File::TryFillFileStat(statLoop, &status) && errno == ELOOP);
    Require(sceKernelStat(statLoop.generic_string().c_str(), &status) == SCE_KERNEL_ERROR_ELOOP);
    Require(lstat_nid_postfix(dangling.string().c_str(), &linkStatus) == 0 && (linkStatus.st_mode & 0170000) == 0120000);
    constexpr int noFollow = 0x100;
    Require(sceKernelOpen(link.string().c_str(), noFollow, 0) == static_cast<int>(0x8002001Fu));
    Require(open_nid_postfix(link.string().c_str(), noFollow, 0) == -1 && *__error_nid_postfix() == 31);
    Require(open_nid_postfix(dangling.string().c_str(), noFollow | 0x201, 0600) == -1 && *__error_nid_postfix() == 31);
    Require(!std::filesystem::exists(root / "missing.txt"));
    Require(open_nid_postfix(dangling.string().c_str(), noFollow | 0xA01, 0600) == -1 && *__error_nid_postfix() == 17);
    const int followed = open_nid_postfix(link.string().c_str(), 0, 0);
    Require(followed >= 0 && close_nid_postfix(followed) == 0);
    const int plain = open_nid_postfix(presentName.c_str(), noFollow, 0);
    Require(plain >= 0 && close_nid_postfix(plain) == 0);
    Require(stat_nid_postfix(dangling.string().c_str(), &status) == -1 && *__error_nid_postfix() == 2);
    Require(lstat_nid_postfix((root / "present.txt" / "child").string().c_str(), &linkStatus) == -1 && *__error_nid_postfix() == 20);
    const auto viaMissing = rootName + "/missing/../present.txt";
    Require(stat_nid_postfix(viaMissing.c_str(), &status) == -1 && *__error_nid_postfix() == 2);
    Require(open_nid_postfix(viaMissing.c_str(), 0, 0) == -1 && *__error_nid_postfix() == 2);
    Require(unlink_nid_postfix(viaMissing.c_str()) == -1 && *__error_nid_postfix() == 2 && std::filesystem::exists(present));
    Require(stat_nid_postfix((presentName + "/..").c_str(), &status) == -1 && *__error_nid_postfix() == 20);
    Require(sceKernelStat((presentName + "/../present.txt").c_str(), &status) == static_cast<int>(0x80020014u));
    Require(unlink_nid_postfix(link.string().c_str()) == 0 && unlink_nid_postfix(dangling.string().c_str()) == 0 &&
        unlink_nid_postfix(statLoop.string().c_str()) == 0);
#else
    const auto fileLink = root / "file-link";
    if (CreateSymbolicLinkW(fileLink.c_str(), L"present.txt", SYMBOLIC_LINK_FLAG_ALLOW_UNPRIVILEGED_CREATE) == 0) {
        Require(GetLastError() == ERROR_PRIVILEGE_NOT_HELD);
        std::fprintf(stderr, "Windows lstat link checks skipped: creating symbolic links needs Developer Mode or administrator rights\n");
    } else {
        const auto targetDirectory = root / "target-directory";
        Require(std::filesystem::create_directory(targetDirectory));
        const auto directoryLink = root / "directory-link";
        const auto danglingLink = root / "dangling-link";
        const auto absoluteLink = root / "absolute-link";
        Require(CreateSymbolicLinkW(directoryLink.c_str(), L"target-directory",
            SYMBOLIC_LINK_FLAG_DIRECTORY | SYMBOLIC_LINK_FLAG_ALLOW_UNPRIVILEGED_CREATE) != 0);
        Require(CreateSymbolicLinkW(danglingLink.c_str(), L"missing.txt", SYMBOLIC_LINK_FLAG_ALLOW_UNPRIVILEGED_CREATE) != 0);
        Require(CreateSymbolicLinkW(absoluteLink.c_str(), std::filesystem::absolute(present).c_str(),
            SYMBOLIC_LINK_FLAG_ALLOW_UNPRIVILEGED_CREATE) != 0);
        const auto statLoop = root / "stat-loop";
        Require(CreateSymbolicLinkW(statLoop.c_str(), L"stat-loop", SYMBOLIC_LINK_FLAG_ALLOW_UNPRIVILEGED_CREATE) != 0);
        Require(sceKernelStat(statLoop.generic_string().c_str(), &status) == static_cast<int>(0x8002003eu));
        Require(sceKernelOpen(statLoop.generic_string().c_str(), 0, 0) == static_cast<int>(0x8002003eu));
        Require(open_nid_postfix(statLoop.string().c_str(), 0, 0) == -1 && *__error_nid_postfix() == 62);
        Require(fopen_nid_postfix(statLoop.string().c_str(), "r") == nullptr && *__error_nid_postfix() == 62);
        const auto checkRelativeLink = [](const std::filesystem::path& link, std::int64_t targetLength) {
            FileStat own{};
            return lstat_nid_postfix(link.string().c_str(), &own) == 0 && (own.st_mode & 0170000) == 0120000 &&
                own.st_size == targetLength && own.st_nlink == 1;
        };
        Require(checkRelativeLink(fileLink, 11));
        Require(checkRelativeLink(directoryLink, 16));
        Require(checkRelativeLink(danglingLink, 11));
        const auto belowStatLoop = statLoop.string() + "\\inside";
        Require(sceKernelUnlink(belowStatLoop.c_str()) == static_cast<int>(0x8002003eu));
        Require(unlink_nid_postfix(belowStatLoop.c_str()) == -1 && *__error_nid_postfix() == 62);
        Require(sceKernelMkdir(belowStatLoop.c_str(), 0700) == static_cast<int>(0x8002003eu));
        Require(mkdir_nid_postfix(belowStatLoop.c_str(), 0700) == -1 && *__error_nid_postfix() == 62);
        Require(sceKernelRmdir(belowStatLoop.c_str()) == static_cast<int>(0x8002003eu));
        Require(rmdir_nid_postfix(belowStatLoop.c_str()) == -1 && *__error_nid_postfix() == 62);
        Require(sceKernelRmdir(statLoop.string().c_str()) == static_cast<int>(0x80020014u));
        Require(sceKernelTruncate_nid_postfix(statLoop.string().c_str(), 0) == static_cast<int>(0x8002003eu));
        Require(truncate_nid_postfix(statLoop.string().c_str(), 0) == -1 && *__error_nid_postfix() == 62);
        FileStat followed{};
        Require(lstat_nid_postfix(fileLink.string().c_str(), &linkStatus) == 0);
        Require(stat_nid_postfix(fileLink.string().c_str(), &followed) == 0 && (followed.st_mode & 0170000) == 0100000);
        Require(followed.st_ino == status.st_ino && linkStatus.st_ino != status.st_ino && linkStatus.st_dev == status.st_dev);
        Require(stat_nid_postfix(directoryLink.string().c_str(), &followed) == 0 && (followed.st_mode & 0170000) == 0040000);
        const auto renameThroughLoop = statLoop.string() + "\\inside";
        const auto renameTarget = (root / "rename-target").string();
        Require(sceKernelRename(renameThroughLoop.c_str(), renameTarget.c_str()) == static_cast<int>(0x8002003eu));
        Require(rename_nid_postfix(renameThroughLoop.c_str(), renameTarget.c_str()) == -1 && *__error_nid_postfix() == 62);
        Require(sceKernelRename(present.string().c_str(), renameThroughLoop.c_str()) == static_cast<int>(0x8002003eu));
        Require(rename_nid_postfix(present.string().c_str(), renameThroughLoop.c_str()) == -1 && *__error_nid_postfix() == 62);
        const auto missingSource = (root / "missing-source").string();
        const auto underFile = present.string() + "\\child";
        Require(sceKernelRename(missingSource.c_str(), renameThroughLoop.c_str()) == static_cast<int>(0x80020002u));
        Require(rename_nid_postfix(missingSource.c_str(), renameThroughLoop.c_str()) == -1 && *__error_nid_postfix() == 2);
        Require(sceKernelRename(underFile.c_str(), renameThroughLoop.c_str()) == static_cast<int>(0x80020014u));
        Require(rename_nid_postfix(underFile.c_str(), renameThroughLoop.c_str()) == -1 && *__error_nid_postfix() == 20);
        Require(stat_nid_postfix(fileLink.string().c_str(), &status) == 0 && status.st_size == 5);
        FileStat absoluteStatus{};
        Require(lstat_nid_postfix(absoluteLink.string().c_str(), &absoluteStatus) == 0);
        Require((absoluteStatus.st_mode & 0170000) == 0120000 && absoluteStatus.st_size > 0 && absoluteStatus.st_nlink == 1);
        Require(stat_nid_postfix(absoluteLink.string().c_str(), &followed) == 0 && followed.st_ino == status.st_ino);
        Require(sceKernelOpen((statLoop.generic_string() + "/inside").c_str(), 0, 0) == static_cast<int>(0x8002003eu));
        Require(open_nid_postfix((statLoop.string() + "\\inside").c_str(), 0, 0) == -1 && *__error_nid_postfix() == 62);
        Require(std::filesystem::remove(fileLink) && std::filesystem::remove(directoryLink) && DeleteFileW(danglingLink.c_str()) != 0 &&
            DeleteFileW(statLoop.c_str()) != 0 &&
            std::filesystem::remove(absoluteLink) && std::filesystem::remove(targetDirectory));
    }
    const auto junctionTarget = root / "junction-target";
    const auto junction = root / "junction";
    Require(std::filesystem::create_directory(junctionTarget));
    const auto junctionCommand = L"cmd /c mklink /J \"" + junction.wstring() + L"\" \"" + junctionTarget.wstring() + L"\"";
    Require(_wsystem(junctionCommand.c_str()) == 0);
    FileStat junctionStatus{};
    Require(lstat_nid_postfix(junction.string().c_str(), &junctionStatus) == 0);
    Require((junctionStatus.st_mode & 0170000) == 0120000 && junctionStatus.st_size > 0);
    FileStat junctionFollowed{};
    Require(stat_nid_postfix(junction.string().c_str(), &junctionFollowed) == 0 && (junctionFollowed.st_mode & 0170000) == 0040000);
    constexpr int noFollow = 0x100;
    Require(sceKernelOpen(junction.string().c_str(), noFollow, 0) == static_cast<int>(0x8002001Fu));
    Require(open_nid_postfix(junction.string().c_str(), noFollow, 0) == -1 && *__error_nid_postfix() == 31);
    const int followedJunction = sceKernelOpen(junction.string().c_str(), 0, 0);
    Require(followedJunction >= 0 && sceKernelClose(followedJunction) == 0);
    Require(std::filesystem::remove(junction) && std::filesystem::remove(junctionTarget));
    const auto tagged = root / "tagged.bin";
    { std::ofstream stream(tagged, std::ios::binary); stream << std::string(1u << 18, 'a'); }
    const HANDLE taggedHandle = CreateFileW(tagged.c_str(), GENERIC_WRITE, 0, nullptr, OPEN_EXISTING,
        FILE_FLAG_OPEN_REPARSE_POINT | FILE_FLAG_BACKUP_SEMANTICS, nullptr);
    Require(taggedHandle != INVALID_HANDLE_VALUE);
    std::array<std::byte, REPARSE_GUID_DATA_BUFFER_HEADER_SIZE + 4> reparseData{};
    REPARSE_GUID_DATA_BUFFER reparseHeader{};
    reparseHeader.ReparseTag = 0x00000ABC;
    reparseHeader.ReparseDataLength = 4;
    reparseHeader.ReparseGuid = {0x6a1e3d0b, 0x1f6c, 0x4c1e, {0x9b, 0x52, 0x3d, 0x8e, 0x41, 0x07, 0x5c, 0x2a}};
    std::memcpy(reparseData.data(), &reparseHeader, REPARSE_GUID_DATA_BUFFER_HEADER_SIZE);
    DWORD returned = 0;
    const bool taggedSet = DeviceIoControl(taggedHandle, FSCTL_SET_REPARSE_POINT, reparseData.data(),
        static_cast<DWORD>(reparseData.size()), nullptr, 0, &returned, nullptr) != 0;
    if (!taggedSet) Require(GetLastError() == ERROR_PRIVILEGE_NOT_HELD || GetLastError() == ERROR_ACCESS_DENIED);
    Require(CloseHandle(taggedHandle) != 0);
    if (!taggedSet) {
        Require(std::filesystem::remove(tagged));
        std::fprintf(stderr, "Windows reparse point checks skipped: setting a reparse point on a file needs rights this user lacks\n");
    } else {
        Require((GetFileAttributesW(tagged.c_str()) & FILE_ATTRIBUTE_REPARSE_POINT) != 0);
        FileStat taggedStatus{};
        FileStat taggedFollowed{};
        const int taggedResult = lstat_nid_postfix(tagged.string().c_str(), &taggedStatus);
        const int taggedError = *__error_nid_postfix();
        Require(stat_nid_postfix(tagged.string().c_str(), &taggedFollowed) == taggedResult);
        Require(taggedResult == 0 ? (taggedStatus.st_ino == taggedFollowed.st_ino && taggedStatus.st_mode == taggedFollowed.st_mode)
                                  : *__error_nid_postfix() == taggedError);
        const HANDLE taggedCleanup = CreateFileW(tagged.c_str(), GENERIC_WRITE, 0, nullptr, OPEN_EXISTING,
            FILE_FLAG_OPEN_REPARSE_POINT | FILE_FLAG_BACKUP_SEMANTICS, nullptr);
        Require(taggedCleanup != INVALID_HANDLE_VALUE);
        reparseHeader.ReparseDataLength = 0;
        Require(DeviceIoControl(taggedCleanup, FSCTL_DELETE_REPARSE_POINT, &reparseHeader, REPARSE_GUID_DATA_BUFFER_HEADER_SIZE,
            nullptr, 0, &returned, nullptr) != 0);
        Require(CloseHandle(taggedCleanup) != 0 && std::filesystem::remove(tagged));
    }

    const auto fullTarget = root / "junction-full-target";
    const auto fullJunction = root / "junction-full";
    Require(std::filesystem::create_directory(fullTarget));
    FILE* const inside = std::fopen((fullTarget / "inside.txt").string().c_str(), "wb");
    Require(inside != nullptr);
    std::fclose(inside);
    const auto fullCommand = L"cmd /c mklink /J \"" + fullJunction.wstring() + L"\" \"" + fullTarget.wstring() + L"\"";
    Require(_wsystem(fullCommand.c_str()) == 0);
    Require(rmdir_nid_postfix(fullJunction.string().c_str()) == -1 && *__error_nid_postfix() == 20);
    Require(sceKernelRmdir(fullJunction.string().c_str()) == static_cast<int>(0x80020014u));
    Require(std::filesystem::is_regular_file(fullTarget / "inside.txt"));
    Require(sceKernelUnlink(fullJunction.string().c_str()) == 0);
    Require(!std::filesystem::exists(std::filesystem::symlink_status(fullJunction)));
    Require(std::filesystem::is_regular_file(fullTarget / "inside.txt"));
    Require(std::filesystem::remove(fullTarget / "inside.txt") && std::filesystem::remove(fullTarget));
#endif
    FileStat identity{};
    FileStat byDescriptor{};
    const int identityDescriptor = open_nid_postfix(presentName.c_str(), 0, 0);
    Require(identityDescriptor >= 0 && stat_nid_postfix(presentName.c_str(), &identity) == 0);
    Require(sceKernelFstat(identityDescriptor, &byDescriptor) == 0 && close_nid_postfix(identityDescriptor) == 0);
    Require(identity.st_ino != 0 && byDescriptor.st_dev == identity.st_dev && byDescriptor.st_ino == identity.st_ino);
    const auto sibling = root / "sibling.txt";
    { std::ofstream stream(sibling); stream << "other"; }
    FileStat siblingStatus{};
    Require(stat_nid_postfix(sibling.string().c_str(), &siblingStatus) == 0);
    Require(siblingStatus.st_dev == identity.st_dev && siblingStatus.st_ino != identity.st_ino);
    Require(unlink_nid_postfix(sibling.string().c_str()) == 0);
    const auto hardLink = root / "hardlink.txt";
    std::error_code linkError;
    std::filesystem::create_hard_link(present, hardLink, linkError);
    if (!linkError) {
        FileStat linked{};
        Require(stat_nid_postfix(hardLink.string().c_str(), &linked) == 0);
        Require(linked.st_dev == identity.st_dev && linked.st_ino == identity.st_ino && linked.st_nlink == 2);
        Require(unlink_nid_postfix(hardLink.string().c_str()) == 0);
    }
    const int opened = open_nid_postfix(presentName.c_str(), 0, 0);
    Require(opened >= 0 && close_nid_postfix(opened) == 0);
    const int reopened = _open_nid_postfix(presentName.c_str(), 0);
    Require(reopened >= 0 && close_nid_postfix(reopened) == 0);
    const int locked = open_nid_postfix(presentName.c_str(), 0, 0);
    const int other = open_nid_postfix(presentName.c_str(), 0, 0);
    Require(locked >= 0 && other >= 0);
    Require(flock_nid_postfix(locked, 8) == 0);
    Require(flock_nid_postfix(locked, 2 | 4) == 0);
    Require(flock_nid_postfix(locked, 2 | 4) == 0);
    Require(flock_nid_postfix(locked, 1 | 4) == 0);
    Require(flock_nid_postfix(other, 1 | 4) == 0);
    Require(flock_nid_postfix(other, 8) == 0);
    Require(flock_nid_postfix(locked, 1 | 4) == 0);
    Require(flock_nid_postfix(locked, 2 | 4) == 0);
    Require(flock_nid_postfix(locked, 8) == 0);
    Require(flock_nid_postfix(locked, 8) == 0);
    Require(flock_nid_postfix(other, 2 | 4) == 0);
    Require(flock_nid_postfix(other, 8) == 0);
    Require(flock_nid_postfix(locked, 2 | 4) == 0);
    Require(lseek_nid_postfix(-1, 0, 7) == -1 && *__error_nid_postfix() == 9);
    Require(lseek_nid_postfix(0x7fffffff, 0, -1) == -1 && *__error_nid_postfix() == 9);
    Require(lseek_nid_postfix(locked, 0, 7) == -1 && *__error_nid_postfix() == 22);
    {
        char transfer[4]{};
        int transferPipe[2] = {-1, -1};
        Require(pipe_nid_postfix(transferPipe) == 0);
        Require(read_nid_postfix(-1, nullptr, 1) == -1 && *__error_nid_postfix() == 9);
        Require(write_nid_postfix(-1, nullptr, 1) == -1 && *__error_nid_postfix() == 9);
        Require(sceKernelRead(-1, nullptr, 1) == static_cast<int>(0x80020009u));
        Require(sceKernelWrite(-1, nullptr, 1) == static_cast<int>(0x80020009u));
        Require(read_nid_postfix(locked, nullptr, 1) == -1 && *__error_nid_postfix() == 14);
        Require(pread_nid_postfix(-1, transfer, 1, -1) == -1 && *__error_nid_postfix() == 9);
        Require(pread_nid_postfix(-1, nullptr, 1, 0) == -1 && *__error_nid_postfix() == 9);
        Require(sceKernelPread(-1, transfer, 1, -1) == static_cast<int>(0x80020009u));
        Require(pread_nid_postfix(transferPipe[0], transfer, 1, -1) == -1 && *__error_nid_postfix() == 29);
        Require(pread_nid_postfix(locked, transfer, 1, -1) == -1 && *__error_nid_postfix() == 22);
        Require(pread_nid_postfix(locked, nullptr, 1, -1) == -1 && *__error_nid_postfix() == 22);
        Require(pread_nid_postfix(locked, nullptr, 1, 0) == -1 && *__error_nid_postfix() == 14);
        Require(close_nid_postfix(transferPipe[0]) == 0 && close_nid_postfix(transferPipe[1]) == 0);
    }
    Require(lseek_nid_postfix(locked, 4, 0) == 4);
    Require(lseek_nid_postfix(locked, INT64_MAX, 1) == -1 && *__error_nid_postfix() == 84);
    Require(lseek_nid_postfix(locked, 0, 1) == 4);
    Require(lseek_nid_postfix(locked, -1, 0) == -1 && *__error_nid_postfix() == 22);
    const std::int64_t lockedSize = lseek_nid_postfix(locked, 0, 2);
    Require(lockedSize >= 0);
    Require(lseek_nid_postfix(locked, INT64_MAX - lockedSize + 1, 2) == -1 && *__error_nid_postfix() == 84);
    Require(lseek_nid_postfix(locked, 0, 1) == lockedSize);
    Require(flock_nid_postfix(-1, 2 | 4) == -1 && *__error_nid_postfix() == 9);
    Require(flock_nid_postfix(0x7fffffff, 1) == -1 && *__error_nid_postfix() == 9);
    int flockPipe[2] = {-1, -1};
    Require(pipe_nid_postfix(flockPipe) == 0);
    Require(flock_nid_postfix(flockPipe[0], 2 | 4) == -1 && *__error_nid_postfix() == 45);
    Require(flock_nid_postfix(flockPipe[1], 8) == -1 && *__error_nid_postfix() == 45);
    Require(close_nid_postfix(flockPipe[0]) == 0 && close_nid_postfix(flockPipe[1]) == 0);
    Require(close_nid_postfix(locked) == 0);
    const int recycled = open_nid_postfix(presentName.c_str(), 0, 0);
    Require(recycled >= 0 && flock_nid_postfix(recycled, 8) == 0);
    Require(flock_nid_postfix(recycled, 2 | 4) == 0 && sceKernelClose(recycled) == 0);
    const int kernelRecycled = sceKernelOpen(presentName.c_str(), 0, 0);
    Require(kernelRecycled >= 0 && flock_nid_postfix(kernelRecycled, 8) == 0);
    Require(flock_nid_postfix(other, 0) == -1 && *__error_nid_postfix() == 9);
    Require(flock_nid_postfix(other, 4 | 0x10) == -1 && *__error_nid_postfix() == 9);
    Require(flock_nid_postfix(kernelRecycled, 2 | 4 | 0x10) == 0);
    Require(flock_nid_postfix(kernelRecycled, 8 | 2) == 0);
    Require(flock_nid_postfix(other, 2 | 4) == 0 && flock_nid_postfix(other, 8) == 0);
    Require(sceKernelClose(kernelRecycled) == 0 && close_nid_postfix(other) == 0);
    const int holder = open_nid_postfix(presentName.c_str(), 0, 0);
    const int contender = open_nid_postfix(presentName.c_str(), 0, 0);
    Require(holder >= 0 && contender >= 0 && flock_nid_postfix(holder, 2 | 4) == 0);
    Require(flock_nid_postfix(contender, 2 | 4) == -1 && *__error_nid_postfix() == 35);
    Require(flock_nid_postfix(contender, 1 | 4) == -1 && *__error_nid_postfix() == 35);
    Require(flock_nid_postfix(holder, 1 | 4) == 0 && flock_nid_postfix(contender, 1 | 4) == 0);
    Require(flock_nid_postfix(contender, 2 | 4) == -1 && *__error_nid_postfix() == 35);
    Require(flock_nid_postfix(holder, 8) == 0 && flock_nid_postfix(contender, 2 | 4) == 0);
    Require(flock_nid_postfix(contender, 8) == 0);
    Require(flock_nid_postfix(holder, 1 | 2 | 4 | 0x10) == 0);
    Require(flock_nid_postfix(contender, 1 | 4) == -1 && *__error_nid_postfix() == 35);
    Require(flock_nid_postfix(holder, 8) == 0);
    Require(close_nid_postfix(contender) == 0 && close_nid_postfix(holder) == 0);
    Require(open_nid_postfix(missingName.c_str(), 0, 0) == -1 && *__error_nid_postfix() == 2);
    Require(_open_nid_postfix(missingName.c_str(), 0) == -1 && *__error_nid_postfix() == 2);
    Require(sceKernelOpen(missingName.c_str(), 0, 0) == static_cast<int>(0x80020002u));
    Require(open_nid_postfix(presentName.c_str(), 0x0a02, 0644) == -1 && *__error_nid_postfix() == 17);
    Require(_open_nid_postfix(presentName.c_str(), 0x0a02, 0644) == -1 && *__error_nid_postfix() == 17);
    Require(open_nid_postfix(rootName.c_str(), 0x0a02, 0644) == -1 && *__error_nid_postfix() == 17);
    Require(open_nid_postfix(rootName.c_str(), 0x0a00, 0644) == -1 && *__error_nid_postfix() == 17);
    Require(open_nid_postfix(rootName.c_str(), 0x0200, 0644) == -1 && *__error_nid_postfix() == 21);
    Require(open_nid_postfix(rootName.c_str(), 0x0002, 0) == -1 && *__error_nid_postfix() == 21);
    Require(open_nid_postfix(rootName.c_str(), 0x0001, 0) == -1 && *__error_nid_postfix() == 21);
    const int syncedDirectory = open_nid_postfix(rootName.c_str(), 0, 0);
    Require(syncedDirectory >= 0 && sceKernelFsync(syncedDirectory) == 0 && fdatasync_nid_postfix(syncedDirectory) == 0);
    Require(sceKernelFdatasync(syncedDirectory) == 0 && close_nid_postfix(syncedDirectory) == 0);
    const int syncedReader = open_nid_postfix(presentName.c_str(), 0, 0);
    Require(syncedReader >= 0 && sceKernelFsync(syncedReader) == 0 && fdatasync_nid_postfix(syncedReader) == 0);
    Require(sceKernelFdatasync(syncedReader) == 0 && close_nid_postfix(syncedReader) == 0);
    Require(sceKernelChmod_nid_postfix(presentName.c_str(), 0400) == 0);
    const int syncedLocked = open_nid_postfix(presentName.c_str(), 0, 0);
    Require(syncedLocked >= 0 && sceKernelFsync(syncedLocked) == 0 && close_nid_postfix(syncedLocked) == 0);
    Require(sceKernelChmod_nid_postfix(presentName.c_str(), 0600) == 0);
    const int directoryDescriptor = open_nid_postfix(rootName.c_str(), 0, 0);
    Require(directoryDescriptor >= 0);
    char directoryBytes[8]{};
    KernelIovec directoryVector[2] = {{directoryBytes, 3}, {directoryBytes + 3, 5}};
    Require(read_nid_postfix(directoryDescriptor, directoryBytes, sizeof(directoryBytes)) == -1 && *__error_nid_postfix() == 21);
    Require(sceKernelRead(directoryDescriptor, directoryBytes, sizeof(directoryBytes)) == static_cast<int>(0x80020015u));
    Require(sceKernelReadv(directoryDescriptor, directoryVector, 2) == static_cast<int>(0x80020015u));
    Require(sceKernelPread(directoryDescriptor, directoryBytes, sizeof(directoryBytes), 0) == static_cast<int>(0x80020015u));
    Require(sceKernelPreadv(directoryDescriptor, directoryVector, 2, 0) == static_cast<int>(0x80020015u));
    Require(sceKernelPwrite(directoryDescriptor, "x", 1, 0) == static_cast<int>(0x80020009u));
    Require(sceKernelWrite(directoryDescriptor, "x", 1) == static_cast<int>(0x80020009u));
    Require(write_nid_postfix(directoryDescriptor, "x", 1) == -1 && *__error_nid_postfix() == 9);
    Require(pwrite_nid_disambig1_nid_postfix(directoryDescriptor, "x", 1, 0) == -1 && *__error_nid_postfix() == 9);
    Require(sceKernelWritev(directoryDescriptor, directoryVector, 2) == static_cast<int>(0x80020009u));
    Require(sceKernelPwritev(directoryDescriptor, directoryVector, 2, 0) == static_cast<int>(0x80020009u));
    std::vector<char> listing(4096);
    Require(sceKernelGetdents(directoryDescriptor, listing.data(), static_cast<int>(listing.size())) > 0);
    const int plainFile = open_nid_postfix(presentName.c_str(), 0, 0);
    Require(plainFile >= 0);
    Require(sceKernelGetdents(plainFile, listing.data(), static_cast<int>(listing.size())) == static_cast<int>(0x80020016u));
    Require(getdents_nid_postfix(plainFile, listing.data(), static_cast<int>(listing.size())) == -1 && *__error_nid_postfix() == 22);
    int64_t plainBase = 0;
    Require(getdirentries_nid_postfix(plainFile, listing.data(), static_cast<int>(listing.size()), &plainBase) == -1 && *__error_nid_postfix() == 22);
    Require(close_nid_postfix(plainFile) == 0);
    Require(close_nid_postfix(directoryDescriptor) == 0);
    const int flaggedDirectory = sceKernelOpen(rootName.c_str(), 0x20000, 0);
    Require(flaggedDirectory >= 0 && sceKernelClose(flaggedDirectory) == 0);
    std::vector<char> entries(4096);
    for (const auto& directoryName : {std::string("/"), std::string("."), rootName + "/", rootName + "/.."}) {
        const int listed = sceKernelOpen(directoryName.c_str(), 0x20000, 0);
        Require(listed >= 0);
        int read = sceKernelGetdents(listed, entries.data(), static_cast<int>(entries.size()));
        Require(read > 0);
        while (read > 0) read = sceKernelGetdents(listed, entries.data(), static_cast<int>(entries.size()));
        Require(read == 0 && sceKernelClose(listed) == 0);
    }
    Require(open_nid_postfix(presentName.c_str(), 0x20000, 0) == -1 && *__error_nid_postfix() == 20);
    Require(sceKernelOpen(presentName.c_str(), 0x20000, 0) == static_cast<int>(0x80020014u));
    Require(sceKernelOpen(presentName.c_str(), 0x20402, 0) == static_cast<int>(0x80020014u));
    Require(std::filesystem::file_size(present) == 5);
    Require(sceKernelOpen(missingName.c_str(), 0x20000, 0) == static_cast<int>(0x80020002u));
    const auto invalidFlagsName = (root / "invalid-flags.txt").string();
    Require(sceKernelOpen(presentName.c_str(), 3, 0) == static_cast<int>(0x80020016u));
    Require(sceKernelOpen(missingName.c_str(), 0x203, 0600) == static_cast<int>(0x80020016u));
    Require(open_nid_postfix(presentName.c_str(), 3, 0) == -1 && *__error_nid_postfix() == 22);
    Require(_open_nid_postfix(presentName.c_str(), 0x403) == -1 && *__error_nid_postfix() == 22);
    Require(sceKernelOpen(invalidFlagsName.c_str(), 0x20a02, 0600) == static_cast<int>(0x80020016u));
    Require(open_nid_postfix(invalidFlagsName.c_str(), 0x20a00, 0600) == -1 && *__error_nid_postfix() == 22);
    Require(!std::filesystem::exists(invalidFlagsName));
    Require(open_nid_postfix(nullptr, 3, 0) == -1 && *__error_nid_postfix() == 22);
    Require(open_nid_postfix("", 0x20a00, 0600) == -1 && *__error_nid_postfix() == 22);
    Require(_open_nid_postfix(nullptr, 0x403) == -1 && *__error_nid_postfix() == 22);
    Require(_open_nid_postfix("", 3) == -1 && *__error_nid_postfix() == 22);
    Require(std::filesystem::file_size(present) == 5);
    Require(open_nid_postfix("", 0, 0) == -1 && *__error_nid_postfix() == 2);
    Require(_open_nid_postfix("", 0) == -1 && *__error_nid_postfix() == 2);
    Require(sceKernelOpen("", 0, 0) == static_cast<int>(0x80020002u));
    Require(open_nid_postfix(nullptr, 0, 0) == -1 && *__error_nid_postfix() == 14);
    Require(_open_nid_postfix(nullptr, 0) == -1 && *__error_nid_postfix() == 14);
    const auto created = root / "created.txt";
    const int createdDescriptor = creat_nid_postfix(created.string().c_str(), 0644);
    Require(createdDescriptor >= 0);
    Require(write_nid_postfix(createdDescriptor, "created", 7) == 7 && close_nid_postfix(createdDescriptor) == 0);
    Require(std::filesystem::file_size(created) == 7);
    const int truncatedDescriptor = creat_nid_postfix(created.string().c_str(), 0600);
    Require(truncatedDescriptor >= 0 && close_nid_postfix(truncatedDescriptor) == 0);
    Require(std::filesystem::file_size(created) == 0);
    Require(unlink_nid_postfix(created.string().c_str()) == 0);
    Require(creat_nid_postfix((root / "absent" / "child.txt").string().c_str(), 0644) == -1 &&
        *__error_nid_postfix() == 2);
    Require(creat_nid_postfix(rootName.c_str(), 0644) == -1 && *__error_nid_postfix() == 21);
    Require(creat_nid_postfix("", 0644) == -1 && *__error_nid_postfix() == 2);
    Require(creat_nid_postfix(nullptr, 0644) == -1 && *__error_nid_postfix() == 14);
    Require(rmdir_nid_postfix(rootName.c_str()) == -1 && *__error_nid_postfix() == 66);
    Require(sceKernelRmdir(rootName.c_str()) == static_cast<int>(0x80020042u));
    Require(rmdir_nid_postfix(presentName.c_str()) == -1 && *__error_nid_postfix() == 20);
    Require(rmdir_nid_postfix(missingName.c_str()) == -1 && *__error_nid_postfix() == 2);
    const auto lockedFile = [&](const char* name) {
        const auto locked = root / name;
        { std::ofstream stream(locked); stream << "removable"; }
        Require(sceKernelChmod_nid_postfix(locked.string().c_str(), 0400) == 0);
        return locked;
    };
    const auto lockedPosix = lockedFile("locked-posix");
    Require(unlink_nid_postfix(lockedPosix.string().c_str()) == 0 && !std::filesystem::exists(lockedPosix));
    const auto lockedKernel = lockedFile("locked-kernel");
    Require(sceKernelUnlink(lockedKernel.string().c_str()) == 0 && !std::filesystem::exists(lockedKernel));
    const auto lockedDirectory = [&](const char* name) {
        const auto locked = root / name;
        Require(mkdir_nid_postfix(locked.string().c_str(), 0700) == 0);
        Require(sceKernelChmod_nid_postfix(locked.string().c_str(), 0555) == 0);
        return locked;
    };
    const auto lockedDirectoryPosix = lockedDirectory("locked-directory-posix");
    Require(rmdir_nid_postfix(lockedDirectoryPosix.string().c_str()) == 0 && !std::filesystem::exists(lockedDirectoryPosix));
    const auto lockedDirectoryKernel = lockedDirectory("locked-directory-kernel");
    Require(sceKernelRmdir(lockedDirectoryKernel.string().c_str()) == 0 && !std::filesystem::exists(lockedDirectoryKernel));
    const auto underFile = presentName + "/child";
    const auto deepUnderFile = presentName + "/child/leaf";
    Require(mkdir_nid_postfix(underFile.c_str(), 0700) == -1 && *__error_nid_postfix() == 20);
    Require(mkdir_nid_postfix(deepUnderFile.c_str(), 0700) == -1 && *__error_nid_postfix() == 20);
    Require(sceKernelMkdir(underFile.c_str(), 0700) == static_cast<int>(0x80020014u));
    Require(sceKernelMkdir(deepUnderFile.c_str(), 0700) == static_cast<int>(0x80020014u));
    Require(rmdir_nid_postfix(underFile.c_str()) == -1 && *__error_nid_postfix() == 20);
    Require(sceKernelRmdir(deepUnderFile.c_str()) == static_cast<int>(0x80020014u));
    Require(rmdir_nid_postfix((presentName + "/").c_str()) == -1 && *__error_nid_postfix() == 20);
    Require(rmdir_nid_postfix((missingName + "/").c_str()) == -1 && *__error_nid_postfix() == 2);
    Require(sceKernelRmdir((presentName + "/").c_str()) == static_cast<int>(0x80020014u));
    Require(sceKernelRmdir((missingName + "/").c_str()) == static_cast<int>(0x80020002u));
    const auto missingChild = missingName + "/child";
    for (const auto* blocked : {&underFile, &deepUnderFile, &missingChild}) {
        const int expected = blocked == &missingChild ? 2 : 20;
        FileStat blockedStatus{};
        Require(open_nid_postfix(blocked->c_str(), 0, 0) == -1 && *__error_nid_postfix() == expected);
        Require(open_nid_postfix(blocked->c_str(), 0x202, 0644) == -1 && *__error_nid_postfix() == expected);
        Require(stat_nid_postfix(blocked->c_str(), &blockedStatus) == -1 && *__error_nid_postfix() == expected);
        Require(lstat_nid_postfix(blocked->c_str(), &blockedStatus) == -1 && *__error_nid_postfix() == expected);
        Require(access_nid_postfix(blocked->c_str(), 0) == -1 && *__error_nid_postfix() == expected);
        Require(unlink_nid_postfix(blocked->c_str()) == -1 && *__error_nid_postfix() == expected);
        Require(chmod_nid_postfix(blocked->c_str(), 0644) == -1 && *__error_nid_postfix() == expected);
        Require(sceKernelTruncate_nid_postfix(blocked->c_str(), 0) == static_cast<int>(0x80020000u | static_cast<unsigned>(expected)));
        Require(sceKernelUtimes_nid_postfix(blocked->c_str(), nullptr) == static_cast<int>(0x80020000u | static_cast<unsigned>(expected)));
        Require(rename_nid_postfix(blocked->c_str(), (root / "renamed-target").string().c_str()) == -1 && *__error_nid_postfix() == expected);
        if (blocked != &missingChild) Require(rename_nid_postfix(presentName.c_str(), blocked->c_str()) == -1 && *__error_nid_postfix() == expected);
        Require(remove_nid_postfix(blocked->c_str()) == -1 && *__error_nid_postfix() == expected);
        Require(opendir_nid_postfix(blocked->c_str()) == nullptr && *__error_nid_postfix() == expected);
        Require(chdir_nid_postfix(blocked->c_str()) == -1 && *__error_nid_postfix() == expected);
        for (const char* openMode : {"r", "w"}) {
            Require(fopen_nid_postfix(blocked->c_str(), openMode) == nullptr && *__error_nid_postfix() == expected);
        }
    }
#ifndef _WIN32
    const auto linkedDirectory = root / "linked-directory";
    const auto directoryLink = root / "directory-link";
    const auto danglingDirectoryLink = root / "dangling-directory-link";
    Require(std::filesystem::create_directory(linkedDirectory));
    std::filesystem::create_directory_symlink("linked-directory", directoryLink);
    std::filesystem::create_directory_symlink("missing-directory", danglingDirectoryLink);
    Require(rmdir_nid_postfix(directoryLink.string().c_str()) == -1 && *__error_nid_postfix() == 20);
    Require(sceKernelRmdir(directoryLink.string().c_str()) == static_cast<int>(0x80020014u));
    Require(rmdir_nid_postfix(danglingDirectoryLink.string().c_str()) == -1 && *__error_nid_postfix() == 20);
    Require(std::filesystem::is_symlink(directoryLink) && std::filesystem::is_directory(linkedDirectory));
    Require(unlink_nid_postfix(directoryLink.string().c_str()) == 0 && unlink_nid_postfix(danglingDirectoryLink.string().c_str()) == 0);
    Require(rmdir_nid_postfix(linkedDirectory.string().c_str()) == 0 && !std::filesystem::exists(linkedDirectory));
#endif
    const auto dotDirectory = root / "dot-directory";
    Require(std::filesystem::create_directory(dotDirectory));
    const auto dotDirectoryName = dotDirectory.string() + "/.";
    const auto dotDirectoryTrailingName = dotDirectoryName + "/";
    Require(rmdir_nid_postfix(dotDirectoryName.c_str()) == -1 && *__error_nid_postfix() == 22);
    Require(sceKernelRmdir(dotDirectoryName.c_str()) == static_cast<int>(0x80020016u));
    Require(remove_nid_postfix(dotDirectoryName.c_str()) == -1 && *__error_nid_postfix() == 22);
    Require(rmdir_nid_postfix(dotDirectoryTrailingName.c_str()) == -1 && *__error_nid_postfix() == 22);
    Require(sceKernelRmdir(dotDirectoryTrailingName.c_str()) == static_cast<int>(0x80020016u));
    Require(remove_nid_postfix(dotDirectoryTrailingName.c_str()) == -1 && *__error_nid_postfix() == 22);
    Require(std::filesystem::is_directory(dotDirectory));
    const auto dotFileName = presentName + "/.";
    Require(rmdir_nid_postfix(dotFileName.c_str()) == -1 && *__error_nid_postfix() == 20);
    Require(sceKernelRmdir(dotFileName.c_str()) == static_cast<int>(0x80020014u));
    Require(remove_nid_postfix(dotFileName.c_str()) == -1 && *__error_nid_postfix() == 20);
    const auto dotMissingName = missingName + "/.";
    Require(rmdir_nid_postfix(dotMissingName.c_str()) == -1 && *__error_nid_postfix() == 2);
    Require(sceKernelRmdir(dotMissingName.c_str()) == static_cast<int>(0x80020002u));
    Require(remove_nid_postfix(dotMissingName.c_str()) == -1 && *__error_nid_postfix() == 2);
    const auto missingParentLeaf = rootName + "/missing/leaf";
    Require(sceKernelMkdir(missingParentLeaf.c_str(), 0700) == static_cast<int>(0x80020002u));
#ifndef _WIN32
    const auto longLeaf = rootName + "/" + std::string(300, 'n');
    Require(sceKernelMkdir(longLeaf.c_str(), 0700) == static_cast<int>(0x8002003fu));
    Require(mkdir_nid_postfix(longLeaf.c_str(), 0700) == -1 && *__error_nid_postfix() == 63);
    FileStat unreachable{};
    Require(sceKernelOpen(longLeaf.c_str(), 0, 0) == static_cast<int>(0x8002003fu));
    Require(sceKernelOpen(longLeaf.c_str(), 0x201, 0600) == static_cast<int>(0x8002003fu));
    Require(open_nid_postfix(longLeaf.c_str(), 0, 0) == -1 && *__error_nid_postfix() == 63);
    Require(sceKernelUnlink(longLeaf.c_str()) == static_cast<int>(0x8002003fu));
    Require(unlink_nid_postfix(longLeaf.c_str()) == -1 && *__error_nid_postfix() == 63);
    Require(sceKernelStat(longLeaf.c_str(), &unreachable) == static_cast<int>(0x8002003fu));
    Require(stat_nid_postfix(longLeaf.c_str(), &unreachable) == -1 && *__error_nid_postfix() == 63);
    const auto loop = root / "loop";
    std::filesystem::create_symlink("loop", loop);
    const auto loopName = loop.string();
    const auto underLoop = loopName + "/leaf";
    Require(sceKernelOpen(loopName.c_str(), 0, 0) == static_cast<int>(0x8002003eu));
    Require(open_nid_postfix(loopName.c_str(), 0, 0) == -1 && *__error_nid_postfix() == 62);
    Require(sceKernelStat(loopName.c_str(), &unreachable) == static_cast<int>(0x8002003eu));
    Require(stat_nid_postfix(loopName.c_str(), &unreachable) == -1 && *__error_nid_postfix() == 62);
    Require(sceKernelUnlink(underLoop.c_str()) == static_cast<int>(0x8002003eu));
    Require(unlink_nid_postfix(underLoop.c_str()) == -1 && *__error_nid_postfix() == 62);
    Require(unlink_nid_postfix(loopName.c_str()) == 0 && !std::filesystem::is_symlink(loop));
#endif
    const auto invalidNameUnderFile = presentName + "/a:b/x";
    Require(mkdir_nid_postfix(invalidNameUnderFile.c_str(), 0700) == -1 && *__error_nid_postfix() == 20);
    Require(sceKernelMkdir(invalidNameUnderFile.c_str(), 0700) == static_cast<int>(0x80020014u));
    const auto trailingPosix = root / "trailing_posix";
    const auto trailingKernel = root / "trailing_kernel";
    const auto trailingPosixName = trailingPosix.string() + "/";
    const auto trailingKernelName = trailingKernel.string() + "/";
    Require(mkdir_nid_postfix(trailingPosixName.c_str(), 0700) == 0 && std::filesystem::is_directory(trailingPosix));
    Require(sceKernelMkdir(trailingKernelName.c_str(), 0700) == 0 && std::filesystem::is_directory(trailingKernel));
    Require(rmdir_nid_postfix(trailingPosix.string().c_str()) == 0 && sceKernelRmdir(trailingKernel.string().c_str()) == 0);
    Require(!std::filesystem::exists(trailingPosix) && !std::filesystem::exists(trailingKernel));
    Require(std::filesystem::is_regular_file(present));
    Require(rmdir_nid_postfix("") == -1 && *__error_nid_postfix() == 2);
    Require(rmdir_nid_postfix(nullptr) == -1 && *__error_nid_postfix() == 14);
    Require(unlink_nid_postfix(missingName.c_str()) == -1 && *__error_nid_postfix() == 2);
    Require(sceKernelUnlink(missingName.c_str()) == static_cast<int>(0x80020002u));
    Require(sceKernelUnlink("") == static_cast<int>(0x80020002u));
    Require(unlink_nid_postfix("") == -1 && *__error_nid_postfix() == 2);
    Require(unlink_nid_postfix(nullptr) == -1 && *__error_nid_postfix() == 14);
    Require(unlink_nid_postfix(rootName.c_str()) == -1 && *__error_nid_postfix() == 1 && std::filesystem::is_directory(root));
    Require(sceKernelUnlink(rootName.c_str()) == static_cast<int>(0x80020001u));
    const int closable = sceKernelOpen(presentName.c_str(), 0, 0);
    Require(closable >= 0 && sceKernelClose(closable) == 0);
    Require(sceKernelClose(closable) == static_cast<int>(0x80020009u));
    Require(sceKernelClose(-1) == static_cast<int>(0x80020009u));
    for (const char* device : {"/dev/urandom", "/dev/random"}) {
        const int random = open_nid_postfix(device, 0, 0);
        Require(random >= 0);
        unsigned char first[64]{};
        unsigned char second[64]{};
        Require(read_nid_postfix(random, first, sizeof(first)) == sizeof(first));
        Require(read_nid_postfix(random, second, 3) == 3);
        Require(std::memcmp(first, second, sizeof(first)) != 0 && second[3] == 0);
        Require(close_nid_postfix(random) == 0);
        auto* stream = fopen_nid_postfix(device, "rb");
        Require(stream != nullptr);
        Require(setvbuf_nid_postfix(stream, nullptr, 2, 0) == 0);
        std::memset(first, 0, sizeof(first));
        std::memset(second, 0, sizeof(second));
        Require(fread_nid_postfix(first, 1, sizeof(first), stream) == sizeof(first));
        Require(fread_nid_postfix(second, 8, 8, stream) == 8);
        Require(std::memcmp(first, second, sizeof(first)) != 0);
        Require(feof_nid_postfix(stream) == 0 && ferror_nid_postfix(stream) == 0);
        flockfile_nid_postfix(stream);

        Require(fread_nid_postfix(first, 1, 4, stream) == 4);
        funlockfile_nid_postfix(stream);

        Require(ftrylockfile_nid_postfix(stream) == 0);
        funlockfile_nid_postfix(stream);

        Require(fflush_nid_postfix(stream) == 0);
        bool unsupported = false;
        try {
            fgetc_nid_postfix(stream);

        } catch (const std::runtime_error&) {
            unsupported = true;
        }
        Require(unsupported);
        Require(fclose_nid_postfix(stream) == 0);
        for (const char* mode : {"wb", "ab", "r+b"}) {
            bool rejected = false;
            try {
                fopen_nid_postfix(device, mode);

            } catch (const std::runtime_error&) {
                rejected = true;
            }
            Require(rejected);
        }
    }
    Require(unlink_nid_postfix(presentName.c_str()) == 0 && !std::filesystem::exists(present));
    const auto empty = root / "empty";
    Require(std::filesystem::create_directory(empty));
    Require(rmdir_nid_postfix(empty.string().c_str()) == 0 && !std::filesystem::exists(empty));
    Require(rmdir_nid_postfix(dotDirectory.string().c_str()) == 0 && !std::filesystem::exists(dotDirectory));
    Require(remove_nid_postfix(root.string().c_str()) == 0);
    Require(!std::filesystem::exists(root));
}
