#include "SceTypes.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include "prx/libkernel/File/include/FileFlags.hpp"
#include <windows.h>
#include <winioctl.h>
#include <array>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <map>
#include <string>
#include <vector>

extern "C" {
int APS5_VABI sceKernelOpen(const char*, int, std::uint16_t);
int APS5_VABI sceKernelClose(int);
int APS5_VABI sceKernelGetdents(int, char*, int);
int APS5_VABI lstat_nid_postfix(const char*, FileStat*);
}

static void Check(bool value, int line) {
    if (!value) {
        std::fprintf(stderr, "Directory link type check failed at line %d\n", line);
        std::exit(1);
    }
}

#define Require(value) Check((value), __LINE__)

static void Junction(const std::filesystem::path& link, const std::filesystem::path& target) {
    Require(std::filesystem::create_directory(link));
    const auto printName = std::filesystem::absolute(target).make_preferred().wstring();
    const auto substituteName = L"\\??\\" + printName;
    struct MountPoint {
        ULONG tag;
        USHORT dataLength;
        USHORT reserved;
        USHORT substituteOffset;
        USHORT substituteLength;
        USHORT printOffset;
        USHORT printLength;
        wchar_t names[2048];
    } data{};
    Require(substituteName.size() + printName.size() + 2 <= std::size(data.names));
    data.tag = IO_REPARSE_TAG_MOUNT_POINT;
    data.substituteLength = static_cast<USHORT>(substituteName.size() * sizeof(wchar_t));
    data.printOffset = static_cast<USHORT>((substituteName.size() + 1) * sizeof(wchar_t));
    data.printLength = static_cast<USHORT>(printName.size() * sizeof(wchar_t));
    data.dataLength = static_cast<USHORT>(8 + data.printOffset + data.printLength + sizeof(wchar_t));
    std::memcpy(data.names, substituteName.c_str(), data.substituteLength + sizeof(wchar_t));
    std::memcpy(reinterpret_cast<char*>(data.names) + data.printOffset, printName.c_str(), data.printLength + sizeof(wchar_t));
    const auto handle = CreateFileW(link.c_str(), GENERIC_WRITE, 0, nullptr, OPEN_EXISTING,
        FILE_FLAG_OPEN_REPARSE_POINT | FILE_FLAG_BACKUP_SEMANTICS, nullptr);
    Require(handle != INVALID_HANDLE_VALUE);
    DWORD returned = 0;
    const auto result = DeviceIoControl(handle, FSCTL_SET_REPARSE_POINT, &data, 8 + data.dataLength, nullptr, 0, &returned, nullptr);
    Require(CloseHandle(handle) != 0);
    Require(result != 0);
}

static void OutsideApp0() {
    const auto root = std::filesystem::absolute("anyps5-directory-links-" +
        std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    Require(std::filesystem::create_directory(root));
    Require(std::filesystem::create_directory(root / "directory"));
    { std::ofstream file(root / "regular"); file << "data"; }
    std::vector<std::string> links{"junction", "dangling-junction", "loop-junction"};
    Junction(root / links[0], root / "directory");
    Junction(root / links[1], root / "missing");
    Junction(root / links[2], root / links[2]);
    if (CreateSymbolicLinkW((root / "file-link").c_str(), L"regular", SYMBOLIC_LINK_FLAG_ALLOW_UNPRIVILEGED_CREATE)) {
        links.push_back("file-link");
        Require(CreateSymbolicLinkW((root / "directory-link").c_str(), L"directory",
            SYMBOLIC_LINK_FLAG_DIRECTORY | SYMBOLIC_LINK_FLAG_ALLOW_UNPRIVILEGED_CREATE) != 0);
        links.push_back("directory-link");
        Require(CreateSymbolicLinkW((root / "dangling-link").c_str(), L"missing", SYMBOLIC_LINK_FLAG_ALLOW_UNPRIVILEGED_CREATE) != 0);
        links.push_back("dangling-link");
    } else {
        Require(GetLastError() == ERROR_PRIVILEGE_NOT_HELD);
        std::fprintf(stderr, "Symbolic-link cases require Windows developer mode or symbolic-link privilege; junction cases still run\n");
    }
    const int descriptor = sceKernelOpen(root.string().c_str(), SCE_KERNEL_O_RDONLY | SCE_KERNEL_O_DIRECTORY, 0);
    Require(descriptor >= 0);
    std::map<std::string, std::uint8_t> entries;
    std::array<char, 64> buffer{};
    for (;;) {
        const int bytes = sceKernelGetdents(descriptor, buffer.data(), static_cast<int>(buffer.size()));
        Require(bytes >= 0 && bytes <= static_cast<int>(buffer.size()));
        if (bytes == 0) break;
        for (int offset = 0; offset < bytes;) {
            std::uint16_t length = 0;
            std::memcpy(&length, buffer.data() + offset + 4, sizeof(length));
            const auto nameLength = static_cast<std::uint8_t>(buffer[offset + 7]);
            Require(length >= 8 + nameLength + 1 && offset + length <= bytes);
            const std::string name(buffer.data() + offset + 8, nameLength);
            Require(entries.emplace(name, static_cast<std::uint8_t>(buffer[offset + 6])).second);
            offset += length;
        }
    }
    Require(sceKernelClose(descriptor) == 0);
    Require(entries.size() == links.size() + 4);
    Require(entries.at(".") == 4 && entries.at("..") == 4);
    Require(entries.at("directory") == 4 && entries.at("regular") == 8);
    for (const auto& name : links) {
        if (entries.at(name) != 10)
            std::fprintf(stderr, "%s: getdents type %u, expected DT_LNK (10)\n", name.c_str(), entries.at(name));
        Require(entries.at(name) == 10);
        const auto path = root / name;
        FileStat status{};
        Require(lstat_nid_postfix(path.string().c_str(), &status) == 0);
        Require((status.st_mode & 0170000) == 0120000);
    }
    for (const auto& name : links) Require(std::filesystem::remove(root / name));
    Require(std::filesystem::remove_all(root) == 3);
}


static std::map<std::string, std::uint8_t> Types(const char* path) {
    const int descriptor = sceKernelOpen(path, SCE_KERNEL_O_RDONLY | SCE_KERNEL_O_DIRECTORY, 0);
    Require(descriptor >= 0);
    std::map<std::string, std::uint8_t> entries;
    std::array<char, 64> buffer{};
    for (;;) {
        const int bytes = sceKernelGetdents(descriptor, buffer.data(), static_cast<int>(buffer.size()));
        Require(bytes >= 0 && bytes <= static_cast<int>(buffer.size()));
        if (bytes == 0) break;
        for (int offset = 0; offset < bytes;) {
            std::uint16_t length = 0;
            std::memcpy(&length, buffer.data() + offset + 4, sizeof(length));
            const auto nameLength = static_cast<std::uint8_t>(buffer[offset + 7]);
            Require(length >= 8 + nameLength + 1 && offset + length <= bytes);
            Require(entries.emplace(std::string(buffer.data() + offset + 8, nameLength),
                static_cast<std::uint8_t>(buffer[offset + 6])).second);
            offset += length;
        }
    }
    Require(sceKernelClose(descriptor) == 0);
    return entries;
}

int main() {
    const auto previous = std::filesystem::current_path();
    const auto root = previous / ("anyps5-app0-links-" +
        std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    Require(std::filesystem::create_directories(root / "app0" / "real"));
    Require(std::filesystem::create_directories(root / "external"));
    Require(std::filesystem::create_directories(root / "data"));
    Require(std::filesystem::create_directories(root / "app0-sibling"));
    std::filesystem::current_path(root);
    OutsideApp0();
    std::vector<std::filesystem::path> links;
    const auto junction = [&](const std::filesystem::path& link, const std::filesystem::path& target) {
        Junction(root / link, root / target);
        links.push_back(link);
    };
    junction("app0/packed", "external");
    junction("app0/chain", "app0/packed");
    junction("app0/dangling", "missing");
    junction("app0/loop", "app0/loop");
    junction("app0/real/nested", "external");
    junction("external/nested", "app0/real");
    junction("data/saved", "external");
    junction("app0-sibling/saved", "external");
    { std::ofstream file(root / "external/regular"); file << "data"; }
    const bool symbolic = CreateSymbolicLinkW((root / "app0/file-link").c_str(), L"../external/regular",
        SYMBOLIC_LINK_FLAG_ALLOW_UNPRIVILEGED_CREATE) != 0;
    if (symbolic) {
        links.push_back("app0/file-link");
        Require(CreateSymbolicLinkW((root / "app0/relative").c_str(), L"../external",
            SYMBOLIC_LINK_FLAG_DIRECTORY | SYMBOLIC_LINK_FLAG_ALLOW_UNPRIVILEGED_CREATE) != 0);
        links.push_back("app0/relative");
    } else {
        Require(GetLastError() == ERROR_PRIVILEGE_NOT_HELD);
        std::fprintf(stderr, "app0 symbolic-link subcases skipped: privilege unavailable\n");
    }
    auto app0 = Types("/app0");
    std::fprintf(stderr, "app0 packed/chain/real types: %u/%u/%u (expected 4/4/4)\n", app0.at("packed"), app0.at("chain"), app0.at("real"));
    Require(app0.at("real") == 4 && app0.at("packed") == 4 && app0.at("chain") == 4);
    Require(app0.at("dangling") == 10 && app0.at("loop") == 10);
    if (symbolic) Require(app0.at("file-link") == 8 && app0.at("relative") == 4);
    Require(Types("/app0/real").at("nested") == 4);
    Require(Types("/app0/packed").at("nested") == 10);
    Require(Types("/data").at("saved") == 10);
    Require(Types((root / "app0-sibling").string().c_str()).at("saved") == 10);
    FileStat status{};
    Require(lstat_nid_postfix("/app0/packed", &status) == 0 && (status.st_mode & 0170000) == 0120000);
    for (auto it = links.rbegin(); it != links.rend(); ++it) Require(std::filesystem::remove(root / *it));
    std::filesystem::rename(root / "app0", root / "application");
    Junction(root / "app0", root / "application");
    Junction(root / "application/packed", root / "external");
    Require(Types("/app0").at("packed") == 4);
    Require(std::filesystem::remove(root / "application/packed"));
    Require(std::filesystem::remove(root / "app0"));
    std::filesystem::current_path(previous);
    Require(root.parent_path() == previous && root.filename().string().starts_with("anyps5-app0-links-"));
    std::filesystem::remove_all(root);
    std::puts("Windows directory link type tests passed");
}
