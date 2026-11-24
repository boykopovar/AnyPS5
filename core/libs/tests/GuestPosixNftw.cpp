#include "prx/libkernel/include/Api.hpp"
#include "prx/libc/include/Api.hpp"
#include "prx/libScePosixForWebKit/include/Api.hpp"
#include "prx/libc/include/General.hpp"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <iterator>
#include <fstream>
#include <map>
#include <random>
#include <source_location>
#include <stdexcept>
#include <string>
#include <vector>

#ifdef _WIN32
#include <windows.h>
#include <winioctl.h>
#else
#include <unistd.h>
#endif

namespace {

constexpr int FtwF = 0;
constexpr int FtwD = 1;
constexpr int FtwNs = 4;
constexpr int FtwSl = 5;
constexpr int FtwSln = 6;
constexpr int FtwDp = 3;
constexpr int FtwPhys = 0x01;
constexpr int FtwDepth = 0x04;
constexpr int FtwChdir = 0x08;
constexpr int GuestEnoent = 2;
constexpr int GuestEinval = 22;

struct GuestFtw {
    int base;
    int level;
};

struct Record {
    std::string path;
    int type;
    int base;
    int level;
};

std::vector<Record> g_records;
int g_stopAt = 0;
int g_stopReturn = 0;

void Require(bool condition, std::source_location location = std::source_location::current()) {
    if (!condition) {
        std::fprintf(stderr, "nftw check failed at line %u (guest errno %d)\n", location.line(), *__error_nid_postfix());
        std::abort();
    }
}

int APS5_VABI Recorder(const char* path, const void*, int type, void* info) {
    const auto* ftw = static_cast<const GuestFtw*>(info);
    g_records.push_back(Record{path, type, ftw->base, ftw->level});
    if (g_stopAt != 0 && static_cast<int>(g_records.size()) == g_stopAt) return g_stopReturn;
    return 0;
}

std::map<std::string, std::pair<int, int>> ByPath(const std::vector<Record>& records) {
    std::map<std::string, std::pair<int, int>> result;
    for (const Record& record : records) result[record.path] = {record.type, record.level};
    return result;
}

std::size_t IndexOf(const std::vector<Record>& records, const std::string& path) {
    for (std::size_t index = 0; index < records.size(); ++index)
        if (records[index].path == path) return index;
    Require(false);
    return 0;
}

void TestPreorderLogical() {
    g_records.clear();
    g_stopAt = 0;
    Require(nftw_nid_postfix("/anyps5-nftw-test", Recorder, 4, 0) == 0);
    const std::map<std::string, std::pair<int, int>> expected = {
        {"/anyps5-nftw-test", {FtwD, 0}},
        {"/anyps5-nftw-test/a.txt", {FtwF, 1}},
        {"/anyps5-nftw-test/d", {FtwD, 1}},
        {"/anyps5-nftw-test/d/b.txt", {FtwF, 2}},
        {"/anyps5-nftw-test/d/e", {FtwD, 2}},
        {"/anyps5-nftw-test/d/e/c.txt", {FtwF, 3}},
        {"/anyps5-nftw-test/f", {FtwD, 1}},
    };
    Require(g_records.size() == expected.size());
    Require(ByPath(g_records) == expected);
    for (const Record& record : g_records) Require(record.base == static_cast<int>(record.path.rfind('/') + 1));
}

void TestPostorderDepth() {
    g_records.clear();
    Require(nftw_nid_postfix("/anyps5-nftw-test", Recorder, 4, FtwDepth) == 0);
    Require(g_records.size() == 7);
    for (const Record& record : g_records) Require(record.type != FtwD);
    Require(IndexOf(g_records, "/anyps5-nftw-test/d/b.txt") < IndexOf(g_records, "/anyps5-nftw-test/d"));
    Require(IndexOf(g_records, "/anyps5-nftw-test/d/e/c.txt") < IndexOf(g_records, "/anyps5-nftw-test/d/e"));
    Require(IndexOf(g_records, "/anyps5-nftw-test/d/e") < IndexOf(g_records, "/anyps5-nftw-test/d"));
    Require(g_records.back().path == "/anyps5-nftw-test");
    Require(g_records.back().type == FtwDp);
}

void TestCallbackStopsWalk() {
    g_records.clear();
    g_stopAt = 1;
    g_stopReturn = 7;
    Require(nftw_nid_postfix("/anyps5-nftw-test", Recorder, 4, 0) == 7);
    Require(g_records.size() == 1);
    g_stopAt = 0;
    g_stopReturn = 0;
}

void TestMissingRootReportsNs() {
    g_records.clear();
    Require(nftw_nid_postfix("/anyps5-nftw-test/missing", Recorder, 4, 0) == 0);
    Require(g_records.size() == 1);
    Require(g_records[0].type == FtwNs && g_records[0].level == 0);

    g_records.clear();
    *__error_nid_postfix() = 0;
    Require(nftw_nid_postfix("", Recorder, 4, 0) == -1);
    Require(*__error_nid_postfix() == GuestEnoent);
    Require(g_records.empty());
}

void TestRejectsBadArguments() {
    g_records.clear();
    *__error_nid_postfix() = 0;
    Require(nftw_nid_postfix("/anyps5-nftw-test", Recorder, 0, 0) == -1);
    Require(*__error_nid_postfix() == GuestEinval);
    Require(g_records.empty());

    bool thrown = false;
    try {
        nftw_nid_postfix("/anyps5-nftw-test", Recorder, 4, FtwChdir);
    } catch (const std::exception&) {
        thrown = true;
    }
    Require(thrown);
}

#ifndef _WIN32
void TestSymlinks(const std::filesystem::path& root) {
    std::filesystem::create_symlink(root / "a.txt", root / "link");
    std::filesystem::create_symlink(root / "missing.txt", root / "dangling");

    g_records.clear();
    Require(nftw_nid_postfix("/anyps5-nftw-test", Recorder, 4, 0) == 0);
    const auto logical = ByPath(g_records);
    Require(logical.at("/anyps5-nftw-test/link").first == FtwF);
    Require(logical.at("/anyps5-nftw-test/dangling").first == FtwSln);

    g_records.clear();
    Require(nftw_nid_postfix("/anyps5-nftw-test", Recorder, 4, FtwPhys) == 0);
    const auto physical = ByPath(g_records);
    Require(physical.at("/anyps5-nftw-test/link").first == FtwSl);
    Require(physical.at("/anyps5-nftw-test/dangling").first == FtwSl);
}
#endif

}

void TestStopBelowRoot() {
    g_records.clear();
    g_stopAt = 3;
    g_stopReturn = 7;
    Require(nftw_nid_postfix("/anyps5-nftw-test", Recorder, 4, 0) == 7);
    Require(g_records.size() == 3);
    g_stopAt = 0;
    g_stopReturn = 0;
}

void TestTrailingSlashBase() {
    g_records.clear();
    Require(nftw_nid_postfix("/anyps5-nftw-test/", Recorder, 4, 0) == 0);
    Require(!g_records.empty());
    Require(g_records[0].base == static_cast<int>(g_records[0].path.size()));
}

void DirectoryLink(const std::filesystem::path& link, const std::filesystem::path& target) {
#ifdef _WIN32
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
#else
    std::filesystem::create_directory_symlink(target, link);
#endif
}

void RemoveDirectoryLink(const std::filesystem::path& link) {
#ifdef _WIN32
    Require(RemoveDirectoryW(link.c_str()) != 0);
#else
    Require(std::filesystem::remove(link));
#endif
}

void TestPhysicalFollowsRootLink(const std::filesystem::path& root) {
    const std::filesystem::path links = root.string() + "-links";
    Require(std::filesystem::create_directory(links));
    DirectoryLink(links / "d", root / "d");
    DirectoryLink(links / "gone", root / "missing");
    AddPathAlias_nid_no_patch("/anyps5-nftw-links", links.string().c_str());

    g_records.clear();
    Require(nftw_nid_postfix("/anyps5-nftw-links/d", Recorder, 4, FtwPhys) == 0);
    Require(g_records.size() == 4);
    Require(g_records[0].path == "/anyps5-nftw-links/d" && g_records[0].type == FtwD && g_records[0].level == 0);
    const auto followed = ByPath(g_records);
    Require(followed.at("/anyps5-nftw-links/d/b.txt") == std::make_pair(FtwF, 1));
    Require(followed.at("/anyps5-nftw-links/d/e") == std::make_pair(FtwD, 1));
    Require(followed.at("/anyps5-nftw-links/d/e/c.txt") == std::make_pair(FtwF, 2));

    g_records.clear();
    Require(nftw_nid_postfix("/anyps5-nftw-links/gone", Recorder, 4, FtwPhys) == 0);
    Require(g_records.size() == 1);
    Require(g_records[0].type == FtwSln && g_records[0].level == 0);

    RemovePathAlias_nid_no_patch("/anyps5-nftw-links");
    RemoveDirectoryLink(links / "d");
    RemoveDirectoryLink(links / "gone");
    Require(std::filesystem::remove(links));
}

int main() {
    std::random_device random;
    const auto root = std::filesystem::temp_directory_path() /
        ("anyps5-nftw-test-" + std::to_string(random()) + "-" + std::to_string(random()));
    Require(std::filesystem::create_directory(root));
    std::ofstream(root / "a.txt") << "x";
    Require(std::filesystem::create_directory(root / "d"));
    std::ofstream(root / "d" / "b.txt") << "x";
    Require(std::filesystem::create_directory(root / "d" / "e"));
    std::ofstream(root / "d" / "e" / "c.txt") << "x";
    Require(std::filesystem::create_directory(root / "f"));
    AddPathAlias_nid_no_patch("/anyps5-nftw-test", root.string().c_str());

    TestPreorderLogical();
    TestPostorderDepth();
    TestCallbackStopsWalk();
    TestMissingRootReportsNs();
    TestRejectsBadArguments();
    TestStopBelowRoot();
    TestTrailingSlashBase();
    TestPhysicalFollowsRootLink(root);
#ifndef _WIN32
    TestSymlinks(root);
#endif
#ifndef _WIN32
    std::filesystem::create_symlink(root, root / "d" / "loop");
    g_records.clear();
    Require(nftw_nid_postfix("/anyps5-nftw-test", Recorder, 4, 0) == 0);
    for (const Record& record : g_records) Require(record.path.find("/loop") == std::string::npos);
#endif

    RemovePathAlias_nid_no_patch("/anyps5-nftw-test");
    Require(std::filesystem::remove_all(root) > 0);
    return 0;
}
