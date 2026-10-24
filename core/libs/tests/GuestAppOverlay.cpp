#include "AppArchiveFixture.hpp"
#include "SceTypes.hpp"
#include "prx/libc/include/AppArchive.hpp"
#include "prx/libc/include/General.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include "prx/libkernel/File/include/FileFlags.hpp"

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iostream>
#include <iterator>
#include <stdexcept>
#include <string>
#include <vector>
#ifdef _WIN32
#include <fcntl.h>
#include <io.h>
#include <sys/stat.h>
#else
#include <fcntl.h>
#include <unistd.h>
#endif

extern "C" {
int APS5_VABI sceKernelOpen(const char*, int, std::uint16_t);
int APS5_VABI sceKernelClose(int);
std::int64_t APS5_VABI sceKernelRead(int, void*, std::size_t);
int APS5_VABI openat_nid_postfix(int, const char*, int, ...);
int APS5_VABI fstatat_nid_postfix(int, const char*, FileStat*, int);
int APS5_VABI chdir_nid_postfix(const char*);
}

namespace {

void Require(bool condition, const char* message) {
    if (!condition) {
        std::cerr << message << '\n';
        std::exit(1);
    }
}

void Write(const std::filesystem::path& path, const std::string& text) {
    std::filesystem::create_directories(path.parent_path());
    std::ofstream stream(path, std::ios::binary | std::ios::trunc);
    stream << text;
    Require(static_cast<bool>(stream), "a fixture file could not be written");
}

std::string Read(const std::filesystem::path& path) {
    std::ifstream stream(path, std::ios::binary);
    return std::string(std::istreambuf_iterator<char>(stream), {});
}

bool SameResolution(const std::filesystem::path& resolved, const std::filesystem::path& expected) {
#ifdef _WIN32
    const auto fold = [](std::wstring text) {
        for (auto& character : text) {
            if (character >= L'A' && character <= L'Z') character = static_cast<wchar_t>(character + (L'a' - L'A'));
        }
        return text;
    };
    return fold(resolved.wstring()) == fold(expected.wstring());
#else
    return resolved == expected;
#endif
}

std::string ReadDescriptor(int descriptor) {
    std::string text;
    char buffer[64];
    for (std::int64_t count; (count = sceKernelRead(descriptor, buffer, sizeof(buffer))) > 0;) text.append(buffer, static_cast<std::size_t>(count));
    return text;
}

std::string Logged(const std::filesystem::path& file, const std::function<void()>& action) {
    std::fflush(stderr);
#ifdef _WIN32
    const int saved = _dup(2);
    const int target = _wopen(file.c_str(), _O_WRONLY | _O_CREAT | _O_TRUNC | _O_BINARY, _S_IREAD | _S_IWRITE);
    Require(saved >= 0 && target >= 0 && _dup2(target, 2) == 0, "standard error could not be captured");
    _close(target);
#else
    const int saved = ::dup(2);
    const int target = ::open(file.c_str(), O_WRONLY | O_CREAT | O_TRUNC, 0600);
    Require(saved >= 0 && target >= 0 && ::dup2(target, 2) == 2, "standard error could not be captured");
    ::close(target);
#endif
    action();
    std::fflush(stderr);
#ifdef _WIN32
    _dup2(saved, 2);
    _close(saved);
#else
    ::dup2(saved, 2);
    ::close(saved);
#endif
    return Read(file);
}

std::string Refusal(const char* guestPath) {
    try {
        (void)ResolvePath_nid_no_patch(guestPath);
    } catch (const std::runtime_error& error) {
        return error.what();
    }
    return {};
}

bool Contains(const std::string& text, const std::string& part) {
    return text.find(part) != std::string::npos;
}

void Shadow(const std::filesystem::path& root) {
    Write(root / "app0" / "Data" / "Settings.TXT", "base");
    Write(root / "app0" / "Data" / "Level.bin", "level");
    Write(root / "app0-overlay" / "data" / "settings.txt", "overlay");
    const auto shadowed = root / "app0-overlay" / "data" / "settings.txt";
    std::filesystem::path resolved;
    const auto log = Logged(root / "first.log", [&] { resolved = ResolvePath_nid_no_patch("/app0/DATA/settings.txt"); });
    Require(resolved == shadowed, "an overlay file did not shadow the app0 file");
    Require(Read(resolved) == "overlay", "the shadowing file did not read as the overlay copy");
    Require(Contains(log, "app0 overlay: " + (root / "app0-overlay").string() + "\n"), "the overlay directory was not announced");
    Require(Contains(log, "app0 overlay: /app0/DATA/settings.txt -> " + shadowed.string() + "\n"), "the shadowed file was not announced");
    const auto again = Logged(root / "again.log", [] { (void)ResolvePath_nid_no_patch("/app0/Data/Settings.TXT"); });
    Require(again.empty(), "a file was announced twice");
    Require(ResolvePath_nid_no_patch("/app0/Data/Level.bin") == root / "app0" / "Data" / "Level.bin", "a file the overlay lacks did not resolve to app0");
    std::error_code trailing;
    std::string trailingPath;
    try {
        (void)ResolvePath_nid_no_patch("/app0/Data/Settings.TXT/");
    } catch (const std::filesystem::filesystem_error& error) {
        trailing = error.code();
        trailingPath = error.path1().string();
    }
    Require(trailing == std::errc::not_a_directory && !Contains(trailingPath, "app0-overlay"), "a file with a trailing separator was not refused as the app0 file");

    const int file = sceKernelOpen("/app0/Data/Settings.TXT", SCE_KERNEL_O_RDONLY, 0);
    Require(file >= 0 && ReadDescriptor(file) == "overlay" && sceKernelClose(file) == 0, "sceKernelOpen did not read the overlay copy");
    const int directory = sceKernelOpen("/app0/Data", SCE_KERNEL_O_RDONLY | SCE_KERNEL_O_DIRECTORY, 0);
    Require(directory >= 0, "the app0 directory could not be opened");
    const int relative = openat_nid_postfix(directory, "Settings.TXT", SCE_KERNEL_O_RDONLY);
    Require(relative >= 0 && ReadDescriptor(relative) == "overlay" && sceKernelClose(relative) == 0, "openat beside an app0 directory did not read the overlay copy");
    FileStat status{};
    Require(fstatat_nid_postfix(directory, "Settings.TXT", &status, 0) == 0 && status.st_size == 7, "fstatat beside an app0 directory did not see the overlay copy");
    Require(fstatat_nid_postfix(directory, "Level.bin", &status, 0) == 0 && status.st_size == 5, "fstatat of a file the overlay lacks did not see app0");
    Require(sceKernelClose(directory) == 0, "the app0 directory could not be closed");

    Require(chdir_nid_postfix("/app0/Data") == 0, "chdir into app0 failed");
    Require(ResolvePath_nid_no_patch("settings.txt") == shadowed, "a path relative to an app0 directory did not come from the overlay");
}

void Absent(const std::filesystem::path& root) {
    Write(root / "app0" / "Data" / "Settings.TXT", "base");
    const auto log = Logged(root / "absent.log", [] { (void)ResolvePath_nid_no_patch("/app0/Data/Settings.TXT"); });
    Require(log.empty(), "an overlay was announced without an overlay directory");
    Require(SameResolution(ResolvePath_nid_no_patch("/app0/data/settings.txt"), root / "app0" / "Data" / "Settings.TXT"), "resolution changed without an overlay");
    const int directory = sceKernelOpen("/app0/Data", SCE_KERNEL_O_RDONLY | SCE_KERNEL_O_DIRECTORY, 0);
    const int relative = directory < 0 ? -1 : openat_nid_postfix(directory, "Settings.TXT", SCE_KERNEL_O_RDONLY);
    Require(relative >= 0 && ReadDescriptor(relative) == "base", "openat changed without an overlay");
    Require(sceKernelClose(relative) == 0 && sceKernelClose(directory) == 0, "descriptors could not be closed");
}

void Directory(const std::filesystem::path& root) {
    Write(root / "app0" / "Data" / "Settings.TXT", "base");
    Write(root / "app0" / "Data" / "Level.bin", "level");
    Write(root / "app0-overlay" / "Data" / "Settings.TXT", "overlay");
    std::filesystem::create_directories(root / "app0-overlay" / "Extra");
    Require(ResolvePath_nid_no_patch("/app0/Data") == root / "app0" / "Data", "a directory came from the overlay");
    Require(SameResolution(ResolvePath_nid_no_patch("/app0/data/"), root / "app0" / "Data" / ""), "a directory with a trailing separator came from the overlay");
    Require(ResolvePath_nid_no_patch("/app0/Extra") == root / "app0" / "Extra", "a directory only in the overlay came from the overlay");
    Require(chdir_nid_postfix("/app0/Data") == 0, "chdir into an app0 directory failed");
    Require(ResolvePath_nid_no_patch(".") == root / "app0" / "Data" / "", "the working directory came from the overlay");
}

void Added(const std::filesystem::path& root) {
    Write(root / "app0" / "Data" / "Settings.TXT", "base");
    Write(root / "app0-overlay" / "Data" / "New.bin", "new");
    Require(ResolvePath_nid_no_patch("/elsewhere.txt") == root / "elsewhere.txt", "a path outside app0 was refused");
    const auto first = Refusal("/app0/Data/Settings.TXT");
    Require(Contains(first, "New.bin") && Contains(first, "shadows no file in app0 or app0.zar"), "an overlay file without an app0 file was not refused by name");
    Require(Refusal("/app0/Data/Settings.TXT") == first, "a refused overlay was used on a later resolution");
}

void Module(const std::filesystem::path& root, const std::string& kind) {
    std::string header;
    if (kind == "elf") header = std::string("\x7f" "ELF", 4);
    else if (kind == "ps4self") header = std::string("\x4f\x15\x3d\x1d", 4);
    else if (kind == "ps5self") header = std::string("\x54\x14\xf5\xee", 4);
    else Require(false, "unknown executable kind");
    Write(root / "app0" / "sce_module" / "libExample.prx", header + "base");
    Write(root / "app0-overlay" / "sce_module" / "libExample.prx", header + "overlay");
    const auto refusal = Refusal("/app0/sce_module/libExample.prx");
    Require(Contains(refusal, "libExample.prx") && Contains(refusal, "is an ELF or SELF executable"), "an executable in the overlay was not refused by name");
}

void Archive(const std::filesystem::path& root) {
    AppArchiveFixture::WriteArchive(root / "app0.zar", {{"Data/Packed.bin", {'z', 'a', 'r'}}, {"Data/Other.bin", {'o', 't', 'h', 'e', 'r'}}});
    Write(root / "app0-overlay" / "Data" / "Packed.bin", "overlay");
    const auto packed = ResolvePath_nid_no_patch("/app0/data/packed.bin");
    Require(packed == root / "app0-overlay" / "Data" / "Packed.bin" && Read(packed) == "overlay", "an overlay file did not shadow an app0.zar file");
    AppArchiveNode node{};
    Require(AppArchiveLookup_nid_no_patch(ResolveArchivedPath_nid_no_patch("/app0/Data/Other.bin"), &node) && !node.Directory && node.Size == 5, "a file only in app0.zar left the archive");
    const int file = sceKernelOpen("/app0/Data/Packed.bin", SCE_KERNEL_O_RDONLY, 0);
    Require(file >= 0 && ReadDescriptor(file) == "overlay" && sceKernelClose(file) == 0, "sceKernelOpen did not read the overlay copy of an app0.zar file");
    const int other = sceKernelOpen("/app0/Data/Other.bin", SCE_KERNEL_O_RDONLY, 0);
    Require(other >= 0 && ReadDescriptor(other) == "other" && sceKernelClose(other) == 0, "sceKernelOpen did not read a file only in app0.zar");
}

#ifndef _WIN32
void Symlink(const std::filesystem::path& root) {
    Write(root / "app0" / "Data" / "Settings.TXT", "base");
    Write(root / "elsewhere.txt", "elsewhere");
    std::filesystem::create_directories(root / "app0-overlay" / "Data");
    std::filesystem::create_symlink(root / "elsewhere.txt", root / "app0-overlay" / "Data" / "Settings.TXT");
    const auto refusal = Refusal("/app0/Data/Settings.TXT");
    Require(Contains(refusal, "Settings.TXT") && Contains(refusal, "is a symbolic link"), "a symbolic link in the overlay was not refused by name");
}
#endif

}

int main(int argc, char** argv) {
    if (argc < 2) {
        std::cerr << "usage: guest_app_overlay_tests shadow|absent|directory|added|module <elf|ps4self|ps5self>|archive|symlink\n";
        return 2;
    }
    const std::string mode = argv[1];
    const auto root = AppArchiveFixture::EnterFreshRoot("anyps5-app-overlay");
    if (mode == "shadow") Shadow(root);
    else if (mode == "absent") Absent(root);
    else if (mode == "directory") Directory(root);
    else if (mode == "added") Added(root);
    else if (mode == "module" && argc == 3) Module(root, argv[2]);
    else if (mode == "archive") Archive(root);
#ifndef _WIN32
    else if (mode == "symlink") Symlink(root);
#endif
    else return 2;
    std::error_code error;
    std::filesystem::current_path(std::filesystem::temp_directory_path(), error);
    std::filesystem::remove_all(root, error);
    return 0;
}
