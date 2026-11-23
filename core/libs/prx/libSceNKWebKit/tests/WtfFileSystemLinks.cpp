#include "prx/libc/include/general/VabiMacros.hpp"
#include "prx/libSceNKWebKit/libSceNKWebKit.hpp"
#include "prx/libc/include/ApplicationHeap.hpp"
#include "prx/libc/include/General.hpp"
#include "prx/libkernel/include/Api.hpp"
#include "prx/libkernel/File/include/FileFlags.hpp"

#include <array>
#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>
#include <system_error>

#ifdef _WIN32
#include <windows.h>
#else
#include <sys/stat.h>
#include <unistd.h>
#endif

namespace {

using NKWebKit::WtfAbi::String;

void Check(bool condition, int line) {
    if (!condition) {
        std::fprintf(stderr, "WTF FileSystemImpl hardLinkOrCopyFile check failed at line %d\n", line);
        std::abort();
    }
}
#define Require(value) Check((value), __LINE__)

String Make(const std::string& text) {
    return _ZN3WTF6String8fromUTF8EPKhm(reinterpret_cast<const unsigned char*>(text.data()), text.size());
}

std::string GuestPath(const char* prefix, const char* name) {
    return std::string(prefix) + "/" + name;
}

std::string NativeText(const std::filesystem::path& path) {
    return std::string(reinterpret_cast<const char*>(path.u8string().c_str()));
}

void Alias(const char* prefix, const std::filesystem::path& path) {
    const auto text = path.u8string();
    AddPathAlias_nid_no_patch(prefix, reinterpret_cast<const char*>(text.c_str()));
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

std::string ReadFile(const std::filesystem::path& path) {
    std::ifstream input(path, std::ios::binary);
    Require(input.good());
    return std::string(std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>());
}

void WriteFile(const std::filesystem::path& path, const std::string& contents) {
    std::ofstream output(path, std::ios::binary | std::ios::trunc);
    Require(output.good());
    output.write(contents.data(), static_cast<std::streamsize>(contents.size()));
    output.close();
    Require(output.good());
}

void* APS5_VABI HeapAllocate(std::size_t size) { return std::malloc(size == 0 ? 1 : size); }
void APS5_VABI FreeTestHeap(void* pointer) { std::free(pointer); }
void* APS5_VABI HeapCallocate(std::size_t count, std::size_t size) {
    return std::calloc(count == 0 ? 1 : count, size == 0 ? 1 : size);
}
void* APS5_VABI HeapReallocate(void* pointer, std::size_t size) { return std::realloc(pointer, size == 0 ? 1 : size); }
void* APS5_VABI UnexpectedAlign(std::size_t, std::size_t) { std::abort(); }
void* APS5_VABI UnexpectedRealign(void*, std::size_t, std::size_t) { std::abort(); }
int APS5_VABI UnexpectedPosixAlign(void**, std::size_t, std::size_t) { std::abort(); }

}  // namespace

int main() {
    std::array<void*, 10> heapApi{
        reinterpret_cast<void*>(HeapAllocate),
        reinterpret_cast<void*>(FreeTestHeap),
        reinterpret_cast<void*>(HeapCallocate),
        reinterpret_cast<void*>(HeapReallocate),
        reinterpret_cast<void*>(UnexpectedAlign),
        reinterpret_cast<void*>(UnexpectedRealign),
        reinterpret_cast<void*>(UnexpectedPosixAlign),
    };
    ApplicationHeapRegister_nid_no_patch(heapApi.data());
    const auto hardLinkOrCopyFile = &_ZN3WTF14FileSystemImpl18hardLinkOrCopyFileERKNS_6StringES3_;
    std::error_code error;
    auto tempRoot = std::filesystem::temp_directory_path(error);
    Require(!error);
    const auto rootPath = TempDirectory(tempRoot, "nkwebkit-hard-link-or-copy");
    const auto otherPath = TempDirectory(tempRoot, "nkwebkit-hard-link-or-copy-other");
    Alias("/nkwebkit-hard-link-or-copy", rootPath);
    Alias("/nkwebkit-hard-link-or-copy-other", otherPath);

    const std::string contents = "hard link or copy\n";
    WriteFile(rootPath / "source.bin", contents);
    Require(hardLinkOrCopyFile(Make(GuestPath("/nkwebkit-hard-link-or-copy", "source.bin")), Make(GuestPath("/nkwebkit-hard-link-or-copy", "linked.bin"))));
    Require(ReadFile(rootPath / "linked.bin") == contents);
    Require(std::filesystem::equivalent(rootPath / "source.bin", rootPath / "linked.bin"));
    Require(ReadFile(rootPath / "source.bin") == contents);

    Require(hardLinkOrCopyFile(Make(GuestPath("/nkwebkit-hard-link-or-copy", "source.bin")), Make(GuestPath("/nkwebkit-hard-link-or-copy-other", "linked.bin"))));
    Require(ReadFile(otherPath / "linked.bin") == contents);
    Require(std::filesystem::equivalent(rootPath / "source.bin", otherPath / "linked.bin"));

    Require(!hardLinkOrCopyFile(Make(GuestPath("/nkwebkit-hard-link-or-copy", "missing.bin")), Make(GuestPath("/nkwebkit-hard-link-or-copy", "created.bin"))));
    Require(!std::filesystem::exists(rootPath / "created.bin"));
    Require(!hardLinkOrCopyFile(Make(GuestPath("/nkwebkit-hard-link-or-copy", "missing.bin")), Make(GuestPath("/nkwebkit-hard-link-or-copy-other", "created.bin"))));
    Require(!std::filesystem::exists(otherPath / "created.bin"));

    WriteFile(rootPath / "existing.bin", "keep me\n");
    Require(!hardLinkOrCopyFile(Make(GuestPath("/nkwebkit-hard-link-or-copy", "source.bin")), Make(GuestPath("/nkwebkit-hard-link-or-copy", "existing.bin"))));
    Require(ReadFile(rootPath / "existing.bin") == "keep me\n");

    WriteFile(rootPath / "empty.bin", "");
    Require(hardLinkOrCopyFile(Make(GuestPath("/nkwebkit-hard-link-or-copy", "empty.bin")), Make(GuestPath("/nkwebkit-hard-link-or-copy", "empty-linked.bin"))));
    Require(ReadFile(rootPath / "empty-linked.bin").empty());

    NKWebKit::WtfAbi::String nullPath;
    Require(!hardLinkOrCopyFile(nullPath, Make(GuestPath("/nkwebkit-hard-link-or-copy", "null-target.bin"))));
    Require(!std::filesystem::exists(rootPath / "null-target.bin"));
    Require(!hardLinkOrCopyFile(Make(GuestPath("/nkwebkit-hard-link-or-copy", "source.bin")), nullPath));

#ifndef _WIN32
    struct stat tempStatus {};
    Require(::stat(NativeText(rootPath).c_str(), &tempStatus) == 0);
    struct stat shmStatus {};
    if (::stat("/dev/shm", &shmStatus) == 0 && shmStatus.st_dev != tempStatus.st_dev) {
        const auto copiedPath = TempDirectory("/dev/shm", "nkwebkit-hard-link-or-copy-tmpfs");
        Alias("/nkwebkit-hard-link-or-copy-tmpfs", copiedPath);
        WriteFile(otherPath / "permissions.bin", "copied\n");
        std::filesystem::permissions(otherPath / "permissions.bin", std::filesystem::perms::owner_read | std::filesystem::perms::owner_write, std::filesystem::perm_options::replace);
        Require(hardLinkOrCopyFile(Make(GuestPath("/nkwebkit-hard-link-or-copy-other", "permissions.bin")), Make(GuestPath("/nkwebkit-hard-link-or-copy-tmpfs", "copied.bin"))));
        Require(ReadFile(copiedPath / "copied.bin") == "copied\n");
        Require(!std::filesystem::equivalent(otherPath / "permissions.bin", copiedPath / "copied.bin"));
        Require((std::filesystem::status(copiedPath / "copied.bin").permissions() & std::filesystem::perms::mask) == (std::filesystem::perms::owner_read | std::filesystem::perms::owner_write));
        Require(!hardLinkOrCopyFile(Make(GuestPath("/nkwebkit-hard-link-or-copy-other", "permissions.bin")), Make(GuestPath("/nkwebkit-hard-link-or-copy-tmpfs", "copied.bin"))));

        Require(::mkfifo(NativeText(otherPath / "source.fifo").c_str(), 0600) == 0);
        Require(!hardLinkOrCopyFile(Make(GuestPath("/nkwebkit-hard-link-or-copy-other", "source.fifo")), Make(GuestPath("/nkwebkit-hard-link-or-copy-tmpfs", "fifo-copy"))));
        Require(!std::filesystem::exists(copiedPath / "fifo-copy"));
        std::filesystem::remove_all(copiedPath, error);
    }
#endif

    std::filesystem::remove_all(rootPath, error);
    std::filesystem::remove_all(otherPath, error);
    return 0;
}