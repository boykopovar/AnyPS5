#include "prx/libc/include/general/VabiMacros.hpp"
#include "prx/libSceNKWebKit/libSceNKWebKit.hpp"
#include "prx/libc/include/General.hpp"

#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <stdexcept>
#include <string>

using NKWebKit::WebCoreAbi::FileHandle;
using NKWebKit::WtfAbi::String;

static void Check(bool value, int line) {
    if (!value) {
        std::fprintf(stderr, "FileHandle check failed at line %d\n", line);
        std::abort();
    }
}
#define Require(value) Check((value), __LINE__)

constexpr int Read = 0;
constexpr int Truncate = 1;
constexpr unsigned Exclusive = 2;

template <typename Action>
static bool ThrowsInvalidArgument(Action action) {
    try {
        action();
    } catch (const std::invalid_argument&) {
        return true;
    }
    return false;
}

static String Guest(const char* path) {
    return _ZN3WTF6String8fromUTF8EPKh(reinterpret_cast<const unsigned char*>(path));
}

static std::string Contents(const std::filesystem::path& path) {
    std::ifstream file(path, std::ios::binary);
    return {std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>()};
}

struct Storage {
    alignas(FileHandle) unsigned char bytes[sizeof(FileHandle)];
    FileHandle* get() { return reinterpret_cast<FileHandle*>(bytes); }
};

static void DefaultHandleIsClosed() {
    Storage storage;
    std::memset(storage.bytes, 0xAB, sizeof(storage.bytes));
    auto* handle = storage.get();
    _ZN7WebCore10FileHandleC1Ev(handle);
    Require(!handle->path.impl && handle->mode == Read && handle->fileHandle == -1 && handle->lockMode == 0 && !handle->shouldLock);
    Require(!_ZNK7WebCore10FileHandlecvbEv(handle) && !_ZN7WebCore10FileHandle4openEv(handle));
    char byte = 0;
    Require(_ZN7WebCore10FileHandle4readEPvi(handle, &byte, 1) == -1);
    _ZN7WebCore10FileHandleD2Ev(handle);
}

static void WritesReadsAndReopens(const std::filesystem::path& host) {
    auto path = Guest("/nkwebkit-filehandle/written.txt");
    Storage writer;
    _ZN7WebCore10FileHandleC2ERKN3WTF6StringENS1_14FileSystemImpl12FileOpenModeE(writer.get(), &path, Truncate);
    Require(!_ZNK7WebCore10FileHandlecvbEv(writer.get()));
    Require(_ZN7WebCore10FileHandle5writeEPKvi(writer.get(), "hello", 5) == 5);
    Require(_ZNK7WebCore10FileHandlecvbEv(writer.get()));
    Require(_ZN7WebCore10FileHandle6printfEPKcz(writer.get(), " %d-%s %.2f", 42, "x", 1.5));
    const std::string longLine(1500, 'z');
    Require(_ZN7WebCore10FileHandle6printfEPKcz(writer.get(), "|%s", longLine.c_str()));
    const int descriptor = writer.get()->fileHandle;
    Require(_ZN7WebCore10FileHandle4openERKN3WTF6StringENS1_14FileSystemImpl12FileOpenModeE(writer.get(), &path, Truncate) && writer.get()->fileHandle == descriptor);
    _ZN7WebCore10FileHandle5closeEv(writer.get());
    Require(!_ZNK7WebCore10FileHandlecvbEv(writer.get()));
    _ZN7WebCore10FileHandleD1Ev(writer.get());
    const std::string expected = "hello 42-x 1.50|" + longLine;
    Require(Contents(host / "written.txt") == expected);

    Storage reader;
    _ZN7WebCore10FileHandleC1ERKN3WTF6StringENS1_14FileSystemImpl12FileOpenModeE(reader.get(), &path, Read);
    std::string buffer(expected.size() + 8, '\0');
    Require(_ZN7WebCore10FileHandle4readEPvi(reader.get(), buffer.data(), static_cast<int>(buffer.size())) == static_cast<int>(expected.size()));
    Require(buffer.compare(0, expected.size(), expected) == 0);
    _ZN7WebCore10FileHandleD2Ev(reader.get());

    auto missing = Guest("/nkwebkit-filehandle/missing.txt");
    Storage absent;
    _ZN7WebCore10FileHandleC1ERKN3WTF6StringENS1_14FileSystemImpl12FileOpenModeE(absent.get(), &missing, Read);
    char byte = 0;
    Require(_ZN7WebCore10FileHandle4readEPvi(absent.get(), &byte, 1) == -1 && !_ZNK7WebCore10FileHandlecvbEv(absent.get()));
    _ZN7WebCore10FileHandleD1Ev(absent.get());
}

static void RejectsNegativeLengths(const std::filesystem::path& host) {
    auto path = Guest("/nkwebkit-filehandle/negative.txt");
    Storage writer;
    _ZN7WebCore10FileHandleC1ERKN3WTF6StringENS1_14FileSystemImpl12FileOpenModeE(writer.get(), &path, Truncate);
    *__error_nid_postfix() = 0;
    Require(_ZN7WebCore10FileHandle5writeEPKvi(writer.get(), "data", -1) == -1 && *__error_nid_postfix() == 22);
    Require(_ZNK7WebCore10FileHandlecvbEv(writer.get()));
    Require(_ZN7WebCore10FileHandle5writeEPKvi(writer.get(), "data", 4) == 4);
    _ZN7WebCore10FileHandleD1Ev(writer.get());
    Require(Contents(host / "negative.txt") == "data");

    Storage reader;
    _ZN7WebCore10FileHandleC1ERKN3WTF6StringENS1_14FileSystemImpl12FileOpenModeE(reader.get(), &path, Read);
    char buffer[4] = {'q', 'q', 'q', 'q'};
    *__error_nid_postfix() = 0;
    Require(_ZN7WebCore10FileHandle4readEPvi(reader.get(), buffer, -1) == -1 && *__error_nid_postfix() == 22);
    Require(std::memcmp(buffer, "qqqq", 4) == 0);
    Require(_ZN7WebCore10FileHandle4readEPvi(reader.get(), buffer, 4) == 4 && std::memcmp(buffer, "data", 4) == 0);
    _ZN7WebCore10FileHandleD2Ev(reader.get());
}

static void MovesAsTheSourceDoes() {
    auto path = Guest("/nkwebkit-filehandle/moved.txt");
    Storage locked;
    _ZN7WebCore10FileHandleC1ERKN3WTF6StringENS1_14FileSystemImpl12FileOpenModeENS1_9OptionSetINS5_12FileLockModeEEE(locked.get(), &path, Truncate, Exclusive);
    Require(locked.get()->shouldLock && locked.get()->lockMode == Exclusive && _ZN7WebCore10FileHandle4openEv(locked.get()));
    const int descriptor = locked.get()->fileHandle;
    Storage moved;
    _ZN7WebCore10FileHandleC2EOS0_(moved.get(), locked.get());
    Require(moved.get()->fileHandle == descriptor && moved.get()->mode == Truncate && !moved.get()->shouldLock && moved.get()->lockMode == 0);
    Require(locked.get()->fileHandle == -1 && !locked.get()->path.impl);
    _ZN7WebCore10FileHandleD1Ev(locked.get());

    Storage target;
    auto other = Guest("/nkwebkit-filehandle/other.txt");
    _ZN7WebCore10FileHandleC1ERKN3WTF6StringENS1_14FileSystemImpl12FileOpenModeENS1_9OptionSetINS5_12FileLockModeEEE(target.get(), &other, Truncate, Exclusive);
    Require(_ZN7WebCore10FileHandle4openEv(target.get()));
    auto& assigned = _ZN7WebCore10FileHandleaSEOS0_(target.get(), moved.get());
    Require(&assigned == target.get() && target.get()->fileHandle == descriptor && !target.get()->shouldLock && moved.get()->fileHandle == -1);
    Require(_ZN7WebCore10FileHandle5writeEPKvi(target.get(), "moved", 5) == 5);
    _ZN7WebCore10FileHandleD2Ev(moved.get());
    _ZN7WebCore10FileHandleD2Ev(target.get());
}

static void LockingCloseKeepsTheDescriptor() {
    auto path = Guest("/nkwebkit-filehandle/locked.txt");
    Storage locked;
    _ZN7WebCore10FileHandleC2ERKN3WTF6StringENS1_14FileSystemImpl12FileOpenModeENS1_9OptionSetINS5_12FileLockModeEEE(locked.get(), &path, Truncate, Exclusive);
    Require(_ZN7WebCore10FileHandle5writeEPKvi(locked.get(), "lock", 4) == 4);
    _ZN7WebCore10FileHandle5closeEv(locked.get());
    Require(_ZNK7WebCore10FileHandlecvbEv(locked.get()));
    locked.get()->fileHandle = -1;
    _ZN7WebCore10FileHandleD1Ev(locked.get());
}

static void RefusesNull() {
    auto path = Guest("/nkwebkit-filehandle/null.txt");
    Storage storage;
    Require(ThrowsInvalidArgument([] { _ZN7WebCore10FileHandleC1Ev(nullptr); }));
    Require(ThrowsInvalidArgument([&] { _ZN7WebCore10FileHandleC1ERKN3WTF6StringENS1_14FileSystemImpl12FileOpenModeE(storage.get(), nullptr, Read); }));
    Require(ThrowsInvalidArgument([] { _ZNK7WebCore10FileHandlecvbEv(nullptr); }));
    Require(ThrowsInvalidArgument([] { _ZN7WebCore10FileHandle4readEPvi(nullptr, nullptr, 0); }));
    Require(ThrowsInvalidArgument([&] { _ZN7WebCore10FileHandle4openERKN3WTF6StringENS1_14FileSystemImpl12FileOpenModeE(nullptr, &path, Read); }));
}

int main() {
    const auto root = std::filesystem::current_path() / ("nkwebkit-filehandle-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    Require(std::filesystem::create_directory(root));
    const auto hostPath = root.u8string();
    AddPathAlias_nid_no_patch("/nkwebkit-filehandle", reinterpret_cast<const char*>(hostPath.c_str()));
    DefaultHandleIsClosed();
    WritesReadsAndReopens(root);
    RejectsNegativeLengths(root);
    MovesAsTheSourceDoes();
    LockingCloseKeepsTheDescriptor();
    RefusesNull();
    std::filesystem::remove_all(root);
    std::puts("FileHandle passed");
    return 0;
}
