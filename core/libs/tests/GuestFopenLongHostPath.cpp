#include "prx/libc/include/General.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include <array>
#include <chrono>
#include <cstdio>
#include <filesystem>
#include <memory>
#include <stdexcept>
#include <string>
#define NOMINMAX
#include <windows.h>

extern "C" {
void* APS5_VABI fopen_nid_postfix(const char*, const char*);
int APS5_VABI fclose_nid_postfix(void*);
std::size_t APS5_VABI fread_nid_postfix(void*, std::size_t, std::size_t, void*);
std::size_t APS5_VABI fwrite_nid_postfix(const void*, std::size_t, std::size_t, void*);
int* APS5_VABI __error_nid_postfix();
}

namespace {

void Require(bool value, const char* message) {
    if (!value) throw std::runtime_error(message);
}

std::filesystem::path Extended(const std::filesystem::path& path) {
    const auto& native = path.native();
    return native.starts_with(L"\\\\") ? L"\\\\?\\UNC\\" + native.substr(2) : L"\\\\?\\" + native;
}

using Stream = std::unique_ptr<void, decltype(&fclose_nid_postfix)>;

Stream Open(const char* path, const char* mode) {
    Stream stream(fopen_nid_postfix(path, mode), fclose_nid_postfix);
    if (!stream) {
        std::fprintf(stderr, "fopen mode %s failed: guest errno=%d, resolved characters=%zu\n", mode, *__error_nid_postfix(), ResolvePath_nid_no_patch(path).native().size());
        throw std::runtime_error("fopen rejected a valid mapped long host path");
    }
    return stream;
}

void Run() {
    auto temp = std::filesystem::absolute(std::filesystem::temp_directory_path()).lexically_normal();
    if (temp.filename().empty()) temp = temp.parent_path();
    const auto root = temp / ("anyps5-fopen-long-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    Require(root.parent_path() == temp && root.filename().string().starts_with("anyps5-fopen-long-"), "invalid fixture root");
    Require(!std::filesystem::exists(root), "fixture root already exists");
    auto deep = root;
    while (deep.native().size() < 290) deep /= std::string(40, 'd');
    Require(deep.native().starts_with(root.native() + L"\\"), "fixture escapes its root");
    struct Cleanup {
        std::filesystem::path root;
        std::filesystem::path deep;
        ~Cleanup() {
            RemovePathAlias_nid_no_patch("/long-host-file");
            RemovePathAlias_nid_no_patch("/extended-host-file");
            RemovePathAlias_nid_no_patch("/relative-long-host-file");
            DeleteFileW(Extended(deep / "payload.bin").c_str());
            DeleteFileW(Extended(deep / "new.bin").c_str());
            for (auto path = deep; ; path = path.parent_path()) {
                RemoveDirectoryW(Extended(path).c_str());
                if (path == root) break;
            }
        }
    } cleanup{root, deep};
    Require(std::filesystem::create_directories(Extended(deep)), "cannot create fixture directories");
    const auto payload = deep / "payload.bin";
    const auto handle = CreateFileW(Extended(payload).c_str(), GENERIC_WRITE, 0, nullptr, CREATE_NEW, FILE_ATTRIBUTE_NORMAL, nullptr);
    Require(handle != INVALID_HANDLE_VALUE, "cannot prepare long-path file");
    DWORD written = 0;
    const bool prepared = WriteFile(handle, "abcd", 4, &written, nullptr) && written == 4;
    CloseHandle(handle);
    Require(prepared, "cannot write fixture payload");
    const auto host = deep.string();
    AddPathAlias_nid_no_patch("/long-host-file", host.c_str());
    AddPathAlias_nid_no_patch("/extended-host-file", Extended(deep).string().c_str());
    Require(ResolvePath_nid_no_patch("/long-host-file/payload.bin").native().size() > MAX_PATH, "fixture does not cross MAX_PATH");
    for (const auto* path : {"/long-host-file/payload.bin", "/extended-host-file/payload.bin"}) {
        auto stream = Open(path, "rb");
        std::array<char, 4> data{};
        Require(fread_nid_postfix(data.data(), 1, data.size(), stream.get()) == 4 && data == std::array{'a', 'b', 'c', 'd'}, "long-path contents differ");
    }
    {
        auto stream = Open("/long-host-file/new.bin", "wbx");
        Require(fwrite_nid_postfix("new", 1, 3, stream.get()) == 3, "long-path exclusive write failed");
    }
    *__error_nid_postfix() = 0;
    Require(!fopen_nid_postfix("/long-host-file/new.bin", "wbx") && *__error_nid_postfix() == 17, "existing long-path exclusive open succeeded");
    {
        auto stream = Open("/long-host-file/new.bin", "ab");
        Require(fwrite_nid_postfix("!", 1, 1, stream.get()) == 1, "long-path append failed");
    }
    {
        auto stream = Open("/long-host-file/new.bin", "rb+");
        std::array<char, 4> data{};
        Require(fread_nid_postfix(data.data(), 2, 2, stream.get()) == 2 && data == std::array{'n', 'e', 'w', '!'}, "long-path append bytes differ");
    }
    *__error_nid_postfix() = 0;
    Require(!fopen_nid_postfix("/long-host-file/missing.bin", "rb") && *__error_nid_postfix() == 2, "missing long-path file did not report ENOENT");
    const auto originalDirectory = std::filesystem::current_path();
    {
        struct RestoreDirectory {
            std::filesystem::path original;
            ~RestoreDirectory() {
                std::error_code error;
                std::filesystem::current_path(original, error);
            }
        } restoreDirectory{originalDirectory};
        std::filesystem::current_path(root);
        const auto relative = deep.lexically_relative(root).string();
        Require(!relative.empty() && !std::filesystem::path(relative).is_absolute(), "fixture alias is not relative");
        AddPathAlias_nid_no_patch("/relative-long-host-file", relative.c_str());
        auto stream = Open("/relative-long-host-file/payload.bin", "rb");
        std::array<char, 4> data{};
        Require(fread_nid_postfix(data.data(), 1, data.size(), stream.get()) == 4 && data == std::array{'a', 'b', 'c', 'd'}, "relative long-path contents differ");
    }
    Require(std::filesystem::current_path() == originalDirectory, "fixture did not restore working directory");
    std::puts("long host path read/create/exclusive/append/update/missing/relative checks passed");
}

}

int main() {
    try { Run(); }
    catch (const std::exception& error) { std::fprintf(stderr, "%s\n", error.what()); return 1; }
    return 0;
}
