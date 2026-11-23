#include "prx/libc/include/general/VabiMacros.hpp"
#include "prx/libSceNKWebKit/libSceNKWebKit.hpp"
#include "prx/libc/include/ApplicationHeap.hpp"
#include "prx/libc/include/General.hpp"

#include <array>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <string>
#include <system_error>

#ifdef _WIN32
#include <windows.h>
#else
#include <unistd.h>
#endif

namespace {

using NKWebKit::WtfAbi::String;

void Check(bool condition, int line) {
    if (!condition) {
        std::fprintf(stderr, "SQLiteFileSystem database file check failed at line %d\n", line);
        std::abort();
    }
}
#define Require(value) Check((value), __LINE__)

String Make(const std::string& text) {
    return _ZN3WTF6String8fromUTF8EPKhm(reinterpret_cast<const unsigned char*>(text.data()), text.size());
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

void WriteFile(const std::filesystem::path& path, const std::string& contents) {
    std::ofstream output(path, std::ios::binary | std::ios::trunc);
    Require(output.good());
    output.write(contents.data(), static_cast<std::streamsize>(contents.size()));
    output.close();
    Require(output.good());
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
    const auto creationTime = &_ZN7WebCore16SQLiteFileSystem20databaseCreationTimeERKN3WTF6StringE;
    const auto deleteDatabaseFile = &_ZN7WebCore16SQLiteFileSystem18deleteDatabaseFileERKN3WTF6StringE;
    std::error_code error;
    auto tempRoot = std::filesystem::temp_directory_path(error);
    Require(!error);
    const auto root = TempDirectory(tempRoot, "nkwebkit-sqlite-database");
    Alias("/nkwebkit-sqlite-database", root);
    const auto guest = [](const std::string& name) { return Make("/nkwebkit-sqlite-database/" + name); };

    WriteFile(root / "fresh.db", "sqlite");
    const auto fresh = creationTime(guest("fresh.db"));
    Require(fresh.hasValue);
    const double now = std::chrono::duration<double>(std::chrono::system_clock::now().time_since_epoch()).count();
    Require(fresh.value > 1.5e9);
    Require(std::abs(fresh.value - now) < 300.0);
    Require(!creationTime(guest("missing.db")).hasValue);
    Require(!creationTime(Make("")).hasValue);
    Require(!creationTime(NKWebKit::WtfAbi::String()).hasValue);
    std::filesystem::create_directory(root / "directory.db");
    Require(creationTime(guest("directory.db")).hasValue);

    WriteFile(root / "main.db", "sqlite");
    WriteFile(root / "main.db-wal", "wal");
    WriteFile(root / "main.db-shm", "shm");
    Require(deleteDatabaseFile(guest("main.db")));
    Require(!std::filesystem::exists(root / "main.db"));
    Require(!std::filesystem::exists(root / "main.db-wal"));
    Require(!std::filesystem::exists(root / "main.db-shm"));
    Require(deleteDatabaseFile(guest("main.db")));

    WriteFile(root / "journal.db-wal", "wal");
    Require(deleteDatabaseFile(guest("journal.db")));
    Require(!std::filesystem::exists(root / "journal.db-wal"));
    Require(deleteDatabaseFile(guest("nothing-here.db")));

    Require(!deleteDatabaseFile(guest("directory.db")));
    Require(std::filesystem::is_directory(root / "directory.db"));
    Require(deleteDatabaseFile(NKWebKit::WtfAbi::String()));
    Require(deleteDatabaseFile(Make("")));

    WriteFile(root / std::filesystem::path(u8"caf\u00e9.db"), "latin1");
    WriteFile(root / std::filesystem::path(u8"caf\u00e9.db-wal"), "latin1 wal");
    WriteFile(root / std::filesystem::path(u8"caf\u00e9.db-shm"), "latin1 shm");
    Require(deleteDatabaseFile(guest("caf\u00e9.db")));
    Require(!std::filesystem::exists(root / std::filesystem::path(u8"caf\u00e9.db-wal")));
    Require(!std::filesystem::exists(root / std::filesystem::path(u8"caf\u00e9.db-shm")));

    WriteFile(root / std::filesystem::path(u8"\U0001F600.db"), "surrogate pair");
    WriteFile(root / std::filesystem::path(u8"\U0001F600.db-wal"), "surrogate pair wal");
    Require(deleteDatabaseFile(guest("\U0001F600.db")));
    Require(!std::filesystem::exists(root / std::filesystem::path(u8"\U0001F600.db")));
    Require(!std::filesystem::exists(root / std::filesystem::path(u8"\U0001F600.db-wal")));

    std::filesystem::remove_all(root, error);
    return 0;
}
