#include "AppArchiveFixture.hpp"
#include "prx/libc/include/AppArchive.hpp"
#include "prx/libc/include/General.hpp"

#include <algorithm>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <set>
#include <stdexcept>
#include <string>

namespace {

void Require(bool condition, const char* message) {
    if (!condition) {
        std::cerr << message << '\n';
        std::exit(1);
    }
}

bool Lookup(const char* guestPath, AppArchiveNode& node) {
    return AppArchiveLookup_nid_no_patch(ResolveArchivedPath_nid_no_patch(guestPath), &node);
}

bool GuardRefuses(const char* guestPath) {
    try {
        (void)ResolvePath_nid_no_patch(guestPath);
    } catch (const std::runtime_error& error) {
        return std::strstr(error.what(), "not implemented") != nullptr && std::strstr(error.what(), guestPath) != nullptr;
    }
    return false;
}

constexpr std::size_t BigSize = 200 * 1024 + 123;

std::map<std::string, std::vector<char>> Contents() {
    return {
        {"Data/Big.bin", AppArchiveFixture::Pattern(BigSize)},
        {"Data/Empty.txt", {}},
        {"Data/Sub/Deep.txt", {'d', 'e', 'e', 'p'}},
        {"Readme.TXT", {'r', 'e', 'a', 'd'}},
    };
}

void Present(const std::filesystem::path& root) {
    AppArchiveFixture::WriteArchive(root / "app0.zar", Contents(), {"Data/Unused"});
    { std::ofstream eboot(root / "app0" / "eboot.bin"); eboot << "elf"; }
    Require(AppArchivePath_nid_no_patch() == root / "app0.zar", "the archive beside app0 was not opened");

    AppArchiveNode node{};
    Require(Lookup("/app0/data/BIG.BIN", node) && !node.Directory && node.Size == BigSize, "a file was not found case-insensitively with its size");
    const auto big = node.Handle;
    Require(Lookup("/APP0/Data/Empty.txt", node) && !node.Directory && node.Size == 0, "an empty file was not found");
    Require(Lookup("/app0/Data", node) && node.Directory, "a directory was not found");
    const auto data = node.Handle;
    Require(Lookup("/app0/", node) && node.Directory, "the archive root was not app0");
    Require(Lookup("/app0/Data/Sub/../Sub/deep.txt", node) && node.Size == 4, "a normalised path was not found");
    Require(!Lookup("/app0/Data/Missing.bin", node), "a missing file was found");
    Require(!Lookup("/app0/Readme.TXT/x", node), "a path through a file was found");
    Require(!Lookup("/app0x/Readme.TXT", node), "a sibling of app0 was looked up in the archive");
    Require(!Lookup("/Readme.TXT", node), "a path outside app0 was looked up in the archive");

    Require(GuardRefuses("/app0/Data/Big.bin"), "an archive-only file resolved through an unaware call");
    Require(GuardRefuses("/app0/Data"), "an archive-only directory resolved through an unaware call");
    Require(ResolvePath_nid_no_patch("/app0/eboot.bin") == root / "app0" / "eboot.bin", "a file on disk was refused");
    Require(ResolvePath_nid_no_patch("/app0/Data/Missing.bin") == root / "app0" / "Data" / "Missing.bin", "a path missing everywhere was refused");
    Require(Lookup("/app0/sce_sys", node) == false && ResolvePath_nid_no_patch("/app0/sce_sys") == root / "app0" / "sce_sys", "a directory only on disk was not left to the disk");

    bool rejected = false;
    const auto expected = AppArchiveFixture::Pattern(BigSize);
    std::vector<char> buffer(BigSize + 64, 0);
    Require(AppArchiveRead_nid_no_patch(big, 0, buffer.data(), BigSize) == BigSize && std::equal(expected.begin(), expected.end(), buffer.begin()), "the whole file did not read back");
    for (const std::uint64_t offset : {std::uint64_t{1}, std::uint64_t{65535}, std::uint64_t{65536}, std::uint64_t{131071}, std::uint64_t{BigSize - 5}}) {
        std::fill(buffer.begin(), buffer.end(), 0);
        const auto read = AppArchiveRead_nid_no_patch(big, offset, buffer.data(), 70000);
        const auto wanted = std::min<std::uint64_t>(70000, BigSize - offset);
        Require(read == wanted && std::equal(expected.begin() + static_cast<std::ptrdiff_t>(offset), expected.begin() + static_cast<std::ptrdiff_t>(offset + wanted), buffer.begin()), "a read across block boundaries returned wrong bytes");
    }
    Require(AppArchiveRead_nid_no_patch(big, BigSize, buffer.data(), 10) == 0, "a read at the end returned bytes");
    Require(AppArchiveRead_nid_no_patch(big, 10, buffer.data(), 0) == 0, "a zero-length read returned bytes");

    AppArchiveNode bigNode{};
    Require(Lookup("/app0/Data/Big.bin", bigNode), "the big file vanished");
    constexpr int Descriptor = 1000003;
    constexpr int Shared = 1000004;
    Require(!AppArchiveDescriptor_nid_no_patch(Descriptor, nullptr, nullptr), "a descriptor was attached before attaching");
    AppArchiveAttach_nid_no_patch(Descriptor, bigNode);
    AppArchiveShare_nid_no_patch(Descriptor, Shared);
    std::uint64_t done = 0;
    std::vector<char> first(65530), second(20);
    const AppArchiveBuffer split[] = {{first.data(), first.size()}, {second.data(), second.size()}};
    Require(AppArchiveDescriptorRead_nid_no_patch(Descriptor, split, 2, nullptr, &done) && done == first.size() + second.size(), "a vector read returned the wrong length");
    Require(std::equal(first.begin(), first.end(), expected.begin()) && std::equal(second.begin(), second.end(), expected.begin() + 65530), "a vector read returned wrong bytes");
    std::uint64_t position = 0;
    Require(AppArchiveDescriptor_nid_no_patch(Shared, nullptr, &position) && position == 65550, "a shared descriptor did not share the position");
    const std::int64_t at = 131000;
    const AppArchiveBuffer single[] = {{second.data(), second.size()}};
    Require(AppArchiveDescriptorRead_nid_no_patch(Shared, single, 1, &at, &done) && done == 20 && std::equal(second.begin(), second.end(), expected.begin() + at), "a positioned read returned wrong bytes");
    Require(AppArchiveDescriptor_nid_no_patch(Descriptor, nullptr, &position) && position == 65550, "a positioned read moved the position");
    std::int64_t sought = 0;
    Require(AppArchiveDescriptorSeek_nid_no_patch(Shared, -3, 2, &sought) && sought == static_cast<std::int64_t>(BigSize - 3), "seeking from the end failed");
    Require(AppArchiveDescriptorRead_nid_no_patch(Descriptor, single, 1, nullptr, &done) && done == 3, "a read at the end was not short");
    Require(AppArchiveDescriptorRead_nid_no_patch(Descriptor, single, 1, nullptr, &done) && done == 0, "a read past the end returned bytes");
    Require(AppArchiveDescriptorSeek_nid_no_patch(Descriptor, -1, 0, &sought) && sought == -1, "a negative seek was accepted");
    Require(AppArchiveDescriptorSeek_nid_no_patch(Descriptor, 10, 1, &sought) && sought == static_cast<std::int64_t>(BigSize + 10), "seeking past the end failed");
    AppArchiveDetach_nid_no_patch(Descriptor);
    Require(!AppArchiveDescriptor_nid_no_patch(Descriptor, nullptr, nullptr) && AppArchiveDescriptor_nid_no_patch(Shared, nullptr, nullptr), "detaching one descriptor detached its duplicate");
    AppArchiveShare_nid_no_patch(Descriptor, Shared);
    Require(!AppArchiveDescriptor_nid_no_patch(Shared, nullptr, nullptr), "duplicating a plain descriptor over an archived one kept the archive");
    rejected = false;
    try { AppArchiveAttach_nid_no_patch(Descriptor, AppArchiveNode{0, true, 0}); } catch (const std::invalid_argument&) { rejected = true; }
    Require(rejected, "a directory was attached to a descriptor");

    std::set<std::pair<std::string, bool>> names;
    for (const auto& entry : AppArchiveList_nid_no_patch(data)) names.insert({entry.Name, entry.Directory});
    Require(names == std::set<std::pair<std::string, bool>>{{"Big.bin", false}, {"Empty.txt", false}, {"Sub", true}, {"Unused", true}}, "the directory listing is wrong");

    rejected = false;
    try { (void)AppArchiveList_nid_no_patch(big); } catch (const std::invalid_argument&) { rejected = true; }
    Require(rejected, "a file was listed as a directory");
    rejected = false;
    try { (void)AppArchiveRead_nid_no_patch(data, 0, buffer.data(), 1); } catch (const std::invalid_argument&) { rejected = true; }
    Require(rejected, "a directory was read as a file");
}

void Absent() {
    AppArchiveNode node{};
    Require(AppArchivePath_nid_no_patch().empty(), "an archive was reported without app0.zar");
    Require(!Lookup("/app0/Readme.TXT", node), "a path was found without an archive");
    Require(ResolvePath_nid_no_patch("/app0/Readme.TXT").filename() == "Readme.TXT", "resolution changed without an archive");
}

void Damaged(const std::filesystem::path& root) {
    AppArchiveFixture::WriteArchive(root / "app0.zar", Contents());
    {
        std::fstream archive(root / "app0.zar", std::ios::binary | std::ios::in | std::ios::out);
        const char garbage[8] = {0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 0x00};
        archive.seekp(0);
        archive.write(garbage, sizeof(garbage));
    }
    AppArchiveNode node{};
    Require(Lookup("/app0/Data/Big.bin", node), "the damaged archive's index was not read");
    std::vector<char> buffer(16);
    bool thrown = false;
    try { (void)AppArchiveRead_nid_no_patch(node.Handle, 0, buffer.data(), buffer.size()); } catch (const std::runtime_error& error) { thrown = std::strstr(error.what(), "damaged") != nullptr; }
    Require(thrown, "a damaged block did not fail the read");
}

void Invalid(const std::filesystem::path& root) {
    { std::ofstream archive(root / "app0.zar", std::ios::binary); archive << "not an archive"; }
    AppArchiveNode node{};
    bool thrown = false;
    try { (void)Lookup("/app0/Readme.TXT", node); } catch (const std::runtime_error& error) { thrown = std::strstr(error.what(), "app0.zar") != nullptr; }
    Require(thrown, "an invalid app0.zar was not reported");
}

}

int main(int argc, char** argv) {
    if (argc != 2) {
        std::cerr << "usage: guest_app_archive_tests present|absent|damaged|invalid\n";
        return 2;
    }
    const std::string mode = argv[1];
    const auto root = AppArchiveFixture::EnterFreshRoot("anyps5-app-archive");
    if (mode == "present") Present(root);
    else if (mode == "absent") Absent();
    else if (mode == "damaged") Damaged(root);
    else if (mode == "invalid") Invalid(root);
    else return 2;
    std::error_code error;
    std::filesystem::current_path(std::filesystem::temp_directory_path(), error);
    std::filesystem::remove_all(root, error);
    return 0;
}
