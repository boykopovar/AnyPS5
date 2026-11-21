#include "prx/libSceNKWebKit/include/FileSystemCopyExports.hpp"
#include "prx/libSceNKWebKit/include/WtfExports.hpp"
#include "prx/libc/include/General.hpp"

#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <stdexcept>
#include <string>
#include <string_view>

#ifdef _WIN32
#include <windows.h>
#else
#include <sys/mman.h>
#include <sys/stat.h>
#endif

namespace {

using NKWebKit::WtfAbi::String;
using NKWebKit::WtfAbi::StringImpl;

void Check(bool condition, int line) {
    if (!condition) {
        std::fprintf(stderr, "FileSystem hard-link/copy check failed at line %d\n", line);
        std::abort();
    }
}

#define Require(condition) Check((condition), __LINE__)

String Make(std::string_view value) {
    return _ZN3WTF6String8fromUTF8EPKhm(reinterpret_cast<const unsigned char*>(value.data()), value.size());
}

bool Copy(std::string_view source, std::string_view destination) {
    const auto from = Make(source);
    const auto to = Make(destination);
    return _ZN3WTF14FileSystemImpl18hardLinkOrCopyFileERKNS_6StringES3_(&from, &to);
}

std::string Read(const std::filesystem::path& path) {
    std::ifstream stream(path, std::ios::binary);
    Require(stream.is_open());
    return {std::istreambuf_iterator<char>(stream), std::istreambuf_iterator<char>()};
}

void Write(const std::filesystem::path& path, std::string_view value) {
    std::ofstream stream(path, std::ios::binary);
    stream.write(value.data(), static_cast<std::streamsize>(value.size()));
    Require(stream.good());
}

template <typename Function>
bool Throws(Function function) {
    try {
        function();
    } catch (const std::invalid_argument&) {
        return true;
    }
    return false;
}

void CheckPointers(const String& destination) {
    const auto copy = &_ZN3WTF14FileSystemImpl18hardLinkOrCopyFileERKNS_6StringES3_;
    const auto* unreadable = reinterpret_cast<const String*>(std::uintptr_t{0x10});
    Require(Throws([&] { copy(nullptr, &destination); }));
    Require(Throws([&] { copy(unreadable, &destination); }));
    Require(Throws([&] { copy(&destination, unreadable); }));
    StringImpl* invalidImpl = reinterpret_cast<StringImpl*>(std::uintptr_t{0x10});
    Require(Throws([&] { copy(reinterpret_cast<const String*>(&invalidImpl), &destination); }));
    StringImpl impl{2, 1, reinterpret_cast<const void*>(std::uintptr_t{0x10}), 4};
    StringImpl* pointer = &impl;
    Require(Throws([&] { copy(reinterpret_cast<const String*>(&pointer), &destination); }));
}

void CheckReadOnlySource(const String& source, const String& destination) {
#ifdef _WIN32
    void* page = VirtualAlloc(nullptr, 4096, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
    Require(page != nullptr);
#else
    void* page = mmap(nullptr, 4096, PROT_READ | PROT_WRITE, MAP_ANON | MAP_PRIVATE, -1, 0);
    Require(page != MAP_FAILED);
#endif
    auto* bytes = static_cast<unsigned char*>(page);
    StringImpl* pointer = reinterpret_cast<StringImpl*>(bytes + 16);
    StringImpl impl = *source.impl;
    const auto size = static_cast<std::size_t>(impl.length) * (impl.hashAndFlags & 4 ? 1 : 2);
    Require(size < 4096 - 64);
    std::memcpy(bytes + 64, impl.data, size);
    impl.data = bytes + 64;
    std::memcpy(bytes, &pointer, sizeof(pointer));
    std::memcpy(bytes + 16, &impl, sizeof(impl));
#ifdef _WIN32
    DWORD previous = 0;
    Require(VirtualProtect(page, 4096, PAGE_READONLY, &previous) != 0);
#else
    Require(mprotect(page, 4096, PROT_READ) == 0);
#endif
    Require(_ZN3WTF14FileSystemImpl18hardLinkOrCopyFileERKNS_6StringES3_(reinterpret_cast<const String*>(page), &destination));
    Require(pointer->refCount == impl.refCount);
#ifdef _WIN32
    Require(VirtualFree(page, 0, MEM_RELEASE) != 0);
#else
    Require(munmap(page, 4096) == 0);
#endif
}

void CheckCrossDeviceCopy(const std::filesystem::path& root, const std::string& suffix, std::string_view contents) {
#ifdef __linux__
    struct stat sourceStatus{};
    struct stat sharedStatus{};
    if (::stat(root.c_str(), &sourceStatus) != 0 || ::stat("/dev/shm", &sharedStatus) != 0 || sourceStatus.st_dev == sharedStatus.st_dev) {
        std::puts("Cross-device copy not exercised: no separate /dev/shm filesystem");
        return;
    }
    const auto other = std::filesystem::path("/dev/shm") / suffix;
    Require(std::filesystem::create_directory(other));
    const auto host = other.u8string();
    AddPathAlias_nid_no_patch("/nkwebkit-copy-other", reinterpret_cast<const char*>(host.c_str()));
    Require(Copy("/nkwebkit-copy/source.bin", "/nkwebkit-copy-other/copied.bin"));
    Require(Read(other / "copied.bin") == contents);
    Require(!std::filesystem::equivalent(root / "source.bin", other / "copied.bin"));
    Require(!Copy("/nkwebkit-copy/source.bin", "/nkwebkit-copy-other/copied.bin"));
    Require(Read(other / "copied.bin") == contents);
    RemovePathAlias_nid_no_patch("/nkwebkit-copy-other");
    Require(std::filesystem::remove_all(other) == 2);
#else
    static_cast<void>(root);
    static_cast<void>(suffix);
    static_cast<void>(contents);
    std::puts("Cross-device copy not exercised on this host");
#endif
}

}  // namespace

int main() {
    const std::string suffix = "nkwebkit-copy-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count());
    const auto root = std::filesystem::temp_directory_path() / suffix;
    Require(std::filesystem::create_directory(root));
    const auto host = root.u8string();
    AddPathAlias_nid_no_patch("/nkwebkit-copy", reinterpret_cast<const char*>(host.c_str()));
    std::string contents(131079, '\0');
    for (std::size_t i = 0; i < contents.size(); ++i) contents[i] = static_cast<char>(i % 251);
    Write(root / "source.bin", contents);
    Require(Copy("/nkwebkit-copy/source.bin", "/nkwebkit-copy/linked.bin"));
    Require(Read(root / "linked.bin") == contents);
    Require(std::filesystem::equivalent(root / "source.bin", root / "linked.bin"));
    Require(!Copy("/nkwebkit-copy/source.bin", "/nkwebkit-copy/linked.bin"));
    Require(!Copy("/nkwebkit-copy/source.bin", "/nkwebkit-copy/source.bin"));
    Write(root / "existing.bin", "keep me");
    Require(!Copy("/nkwebkit-copy/source.bin", "/nkwebkit-copy/existing.bin"));
    Require(Read(root / "existing.bin") == "keep me");
    Require(!Copy("/nkwebkit-copy/missing.bin", "/nkwebkit-copy/absent.bin"));
    Require(!std::filesystem::exists(root / "absent.bin"));
    Require(!Copy("/nkwebkit-copy/source.bin", "/nkwebkit-copy/missing/absent.bin"));
    Require(std::filesystem::create_directory(root / "directory"));
    Require(!Copy("/nkwebkit-copy/directory", "/nkwebkit-copy/directory-copy"));
    Require(!Copy("/nkwebkit-copy/source.bin", "/nkwebkit-copy/directory"));
    Require(!Copy("", "/nkwebkit-copy/empty-source"));
    Require(!Copy("/nkwebkit-copy/source.bin", ""));
    const String nullString;
    const auto source = Make("/nkwebkit-copy/source.bin");
    const auto destination = Make("/nkwebkit-copy/readonly.bin");
    Require(!_ZN3WTF14FileSystemImpl18hardLinkOrCopyFileERKNS_6StringES3_(&nullString, &destination));
    Require(!_ZN3WTF14FileSystemImpl18hardLinkOrCopyFileERKNS_6StringES3_(&source, &nullString));
    const std::u8string unicodeName = u8"\u00e4\u4e2d.bin";
    Write(root / unicodeName, contents);
    const auto unicodeSource = std::string("/nkwebkit-copy/") + reinterpret_cast<const char*>(unicodeName.c_str());
    Require(Copy(unicodeSource, "/nkwebkit-copy/unicode-copy.bin"));
    Require(Read(root / "unicode-copy.bin") == contents);
    const std::string withNull = std::string("/nkwebkit-copy/source.bin") + '\0' + "ignored";
    Require(Copy(withNull, "/nkwebkit-copy/nul-copy.bin"));
    Require(Read(root / "nul-copy.bin") == contents);
    std::error_code symlinkError;
    std::filesystem::create_symlink(root / "source.bin", root / "symlink.bin", symlinkError);
    if (!symlinkError) {
        Require(Copy("/nkwebkit-copy/symlink.bin", "/nkwebkit-copy/symlink-copy.bin"));
        Require(!std::filesystem::is_symlink(root / "symlink-copy.bin"));
        Require(std::filesystem::equivalent(root / "source.bin", root / "symlink-copy.bin"));
    } else {
        std::puts("Source symlink not exercised: host did not allow creating it");
    }
    CheckPointers(destination);
    CheckReadOnlySource(source, destination);
    Require(Read(root / "readonly.bin") == contents);
    CheckCrossDeviceCopy(root, suffix, contents);
    RemovePathAlias_nid_no_patch("/nkwebkit-copy");
    Require(std::filesystem::remove_all(root) >= 9);
}
