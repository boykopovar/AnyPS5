#include "prx/libc/include/General.hpp"
#include "prx/libc/include/FileStream.hpp"
#include "prx/libkernel/File/include/FileFlags.hpp"
#include "SceTypes.hpp"

#include <Testing/Test.hpp>

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <string>
#include <system_error>

extern "C" {
int APS5_VABI sceKernelOpen(const char*, int, std::uint16_t);
int APS5_VABI sceKernelClose(int);
std::int64_t APS5_VABI sceKernelRead(int, void*, std::size_t);
std::int64_t APS5_VABI sceKernelWrite(int, const void*, std::size_t);
int APS5_VABI sceKernelStat(const char*, FileStat*);
int APS5_VABI chdir_nid_postfix(const char*);
int APS5_VABI access_nid_postfix(const char*, int);
int APS5_VABI rename_nid_postfix(const char*, const char*);
int APS5_VABI remove_nid_postfix(const char*);
FileStream* APS5_VABI fopen_nid_postfix(const char*, const char*);
std::size_t APS5_VABI fread_nid_postfix(void*, std::size_t, std::size_t, FileStream*);
int APS5_VABI fclose_nid_postfix(FileStream*);
}

namespace {

using Testing::Case;
using Testing::Require;
using Testing::RequireEqual;

constexpr int kernelNotFound = static_cast<int>(0x80020002u);
constexpr int kernelExists = static_cast<int>(0x80020011u);
constexpr int createExclusive = SCE_KERNEL_O_CREAT | SCE_KERNEL_O_EXCL | SCE_KERNEL_O_WRONLY;

class PathCaseFixture {
public:
    PathCaseFixture()
        : host(std::filesystem::canonical(std::filesystem::current_path())),
          name("AnyPS5-Case-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count())),
          lower(Lowercase(name)),
          directory(host / name),
          data(directory / "Data") {
        Require(std::filesystem::create_directories(data / "Nested"), "create the test directories");
        std::ofstream(data / "Settings.ini") << 'x';
    }

    ~PathCaseFixture() {
        chdir_nid_postfix("/");
        AddPathAlias_nid_no_patch("case-mount", data.string().c_str());
        RemovePathAlias_nid_no_patch("case-mount");
        std::error_code ignored;
        std::filesystem::remove_all(directory, ignored);
    }

    PathCaseFixture(const PathCaseFixture&) = delete;
    PathCaseFixture& operator=(const PathCaseFixture&) = delete;

    void EnterLowercase() const {
        RequireEqual(chdir_nid_postfix(lower.c_str()), 0, "enter the test directory by its lowercase name");
    }

    const std::filesystem::path host;
    const std::string name;
    const std::string lower;
    const std::filesystem::path directory;
    const std::filesystem::path data;

private:
    static std::string Lowercase(std::string text) {
        for (auto& character : text) {
            if (character >= 'A' && character <= 'Z') character += 'a' - 'A';
        }
        return text;
    }
};

void RequireRead(const std::string& path, char expected) {
    const int descriptor = sceKernelOpen(path.c_str(), SCE_KERNEL_O_RDONLY, 0);
    Require(descriptor >= 0, "open " + path);
    char value = 0;
    const std::int64_t count = sceKernelRead(descriptor, &value, 1);
    const int closed = sceKernelClose(descriptor);
    RequireEqual(count, std::int64_t{1}, "read one byte from " + path);
    RequireEqual(value, expected, "byte read from " + path);
    RequireEqual(closed, 0, "close " + path);
}

void CreateNewFile() {
    const int created = sceKernelOpen("data/nested/New.DAT", createExclusive, 0600);
    Require(created >= 0, "create data/nested/New.DAT");
    const std::int64_t written = sceKernelWrite(created, "y", 1);
    const int closed = sceKernelClose(created);
    RequireEqual(written, std::int64_t{1}, "write the new file");
    RequireEqual(closed, 0, "close the new file");
}

const Case absoluteMixedCase{"KernelOpen_MixedCaseAbsolutePath_ReadsExistingFile", [] {
    const PathCaseFixture fixture;
    RequireRead("/" + fixture.lower + "/dAtA/SETTINGS.INI", 'x');
}};

const Case statMixedCase{"KernelStat_MixedCaseAbsolutePath_Succeeds", [] {
    const PathCaseFixture fixture;
    FileStat stat{};
    RequireEqual(sceKernelStat(("/" + fixture.lower + "/DATA/settings.INI").c_str(), &stat), 0, "stat");
}};

const Case chdirLowercase{"Chdir_LowercaseDirectoryName_Succeeds", [] {
    const PathCaseFixture fixture;
    RequireEqual(chdir_nid_postfix(fixture.lower.c_str()), 0, "chdir");
}};

const Case accessMixedCase{"Access_MixedCaseRelativePath_Succeeds", [] {
    const PathCaseFixture fixture;
    fixture.EnterLowercase();
    RequireEqual(access_nid_postfix("DATA/SETTINGS.ini", 4), 0, "access");
}};

const Case fopenBackslash{"Fopen_MixedCaseBackslashPath_ReadsExistingFile", [] {
    const PathCaseFixture fixture;
    fixture.EnterLowercase();
    auto* stream = fopen_nid_postfix("data\\SETTINGS.INI", "rb");
    Require(stream != nullptr, "fopen");
    char value = 0;
    const std::size_t count = fread_nid_postfix(&value, 1, 1, stream);
    const int closed = fclose_nid_postfix(stream);
    RequireEqual(count, std::size_t{1}, "fread count");
    RequireEqual(value, 'x', "fread value");
    RequireEqual(closed, 0, "fclose");
}};

const Case chdirDotDot{"Chdir_MixedCaseDotDotPath_EntersExistingDirectory", [] {
    const PathCaseFixture fixture;
    fixture.EnterLowercase();
    RequireEqual(chdir_nid_postfix("DATA/../data"), 0, "chdir DATA/../data");
    RequireRead("settings.INI", 'x');
}};

const Case chdirParent{"Chdir_ParentAfterMixedCaseChdir_ReturnsToTestDirectory", [] {
    const PathCaseFixture fixture;
    fixture.EnterLowercase();
    RequireEqual(chdir_nid_postfix("DATA/../data"), 0, "chdir DATA/../data");
    RequireEqual(chdir_nid_postfix(".."), 0, "chdir ..");
    RequireRead("DATA/SETTINGS.INI", 'x');
}};

const Case createKeepsCase{"KernelOpen_CreateInMixedCaseDirectory_UsesExistingDirectoryCase", [] {
    const PathCaseFixture fixture;
    fixture.EnterLowercase();
    CreateNewFile();
    Require(std::filesystem::exists(fixture.data / "Nested" / "New.DAT"), "file created in Data/Nested as New.DAT");
    RequireRead("DATA/NESTED/new.dat", 'y');
}};

const Case exclusiveDifferentCase{"KernelOpen_ExclusiveCreateWithDifferentCase_FailsWithExists", [] {
    const PathCaseFixture fixture;
    fixture.EnterLowercase();
    CreateNewFile();
    RequireEqual(sceKernelOpen("data/nested/NEW.dat", createExclusive, 0600), kernelExists, "exclusive create");
}};

const Case renameMixedCase{"Rename_MixedCasePaths_Succeeds", [] {
    const PathCaseFixture fixture;
    fixture.EnterLowercase();
    CreateNewFile();
    RequireEqual(rename_nid_postfix("DATA/NESTED/new.dat", "data/nested/Moved.DAT"), 0, "rename");
}};

const Case removeMixedCase{"Remove_MixedCasePathOfRenamedFile_Succeeds", [] {
    const PathCaseFixture fixture;
    fixture.EnterLowercase();
    CreateNewFile();
    RequireEqual(rename_nid_postfix("DATA/NESTED/new.dat", "data/nested/Moved.DAT"), 0, "rename");
    RequireEqual(remove_nid_postfix("DATA/NESTED/moved.dat"), 0, "remove");
}};

const Case openMissing{"KernelOpen_MissingFileInMixedCaseDirectory_FailsWithNotFound", [] {
    const PathCaseFixture fixture;
    fixture.EnterLowercase();
    RequireEqual(sceKernelOpen("data/nested/missing", SCE_KERNEL_O_RDONLY, 0), kernelNotFound, "open missing");
}};

const Case aliasCase{"PathAlias_UppercaseGuestPrefix_ResolvesAlias", [] {
    const PathCaseFixture fixture;
    AddPathAlias_nid_no_patch("case-mount", fixture.data.string().c_str());
    RequireRead("/CASE-MOUNT/settings.INI", 'x');
}};

const Case aliasLongerPrefix{"PathAlias_LongerGuestPrefix_DoesNotMatch", [] {
    const PathCaseFixture fixture;
    AddPathAlias_nid_no_patch("case-mount", fixture.data.string().c_str());
    RequireEqual(access_nid_postfix("/case-mount-other/Settings.ini", 0), -1, "access through a longer prefix");
}};

const Case blockAlias{"BlockPathAlias_DifferentCase_BlocksAlias", [] {
    const PathCaseFixture fixture;
    AddPathAlias_nid_no_patch("case-mount", fixture.data.string().c_str());
    BlockPathAlias_nid_no_patch("CASE-MOUNT");
    RequireEqual(access_nid_postfix("/case-mount/settings.ini", 0), -1, "access through a blocked alias");
}};

const Case readdAlias{"AddPathAlias_AfterBlockWithDifferentCase_ResolvesAlias", [] {
    const PathCaseFixture fixture;
    AddPathAlias_nid_no_patch("case-mount", fixture.data.string().c_str());
    BlockPathAlias_nid_no_patch("CASE-MOUNT");
    AddPathAlias_nid_no_patch("Case-Mount", fixture.data.string().c_str());
    RequireRead("/case-MOUNT/settings.INI", 'x');
}};

const Case removeAlias{"RemovePathAlias_DifferentCase_RemovesAlias", [] {
    const PathCaseFixture fixture;
    AddPathAlias_nid_no_patch("case-mount", fixture.data.string().c_str());
    BlockPathAlias_nid_no_patch("CASE-MOUNT");
    AddPathAlias_nid_no_patch("Case-Mount", fixture.data.string().c_str());
    RemovePathAlias_nid_no_patch("CASE-mount");
    RequireEqual(access_nid_postfix("/case-mount/settings.ini", 0), -1, "access through a removed alias");
}};

const Case returnToRoot{"Chdir_GuestRoot_KeepsHostWorkingDirectory", [] {
    const PathCaseFixture fixture;
    fixture.EnterLowercase();
    RequireEqual(chdir_nid_postfix("/"), 0, "chdir /");
    RequireEqual(std::filesystem::current_path(), fixture.host, "host working directory");
}};

#ifndef _WIN32
const Case symlinkedDirectory{"KernelOpen_MixedCasePathThroughDirectorySymlink_ReadsFile", [] {
    const PathCaseFixture fixture;
    fixture.EnterLowercase();
    std::filesystem::create_directory_symlink("Data", fixture.directory / "Linked");
    RequireRead("linked/SETTINGS.INI", 'x');
}};

const Case exactCasePreferred{"KernelOpen_ExactCaseMatchAmongCaseVariants_OpensExactFile", [] {
    const PathCaseFixture fixture;
    fixture.EnterLowercase();
    std::ofstream(fixture.data / "SETTINGS.INI") << 'z';
    RequireRead("Data/Settings.ini", 'x');
    RequireRead("Data/SETTINGS.INI", 'z');
}};

const Case ambiguousCase{"KernelOpen_AmbiguousCaseVariants_ThrowsRuntimeError", [] {
    const PathCaseFixture fixture;
    fixture.EnterLowercase();
    std::ofstream(fixture.data / "SETTINGS.INI") << 'z';
    Testing::RequireThrows<std::runtime_error>([] {
        const int descriptor = sceKernelOpen("data/settings.ini", SCE_KERNEL_O_RDONLY, 0);
        if (descriptor >= 0) sceKernelClose(descriptor);
    }, "open data/settings.ini with two case variants");
}};

const Case singleVariant{"KernelOpen_SingleCaseVariant_ReadsIt", [] {
    const PathCaseFixture fixture;
    fixture.EnterLowercase();
    std::ofstream(fixture.data / "SETTINGS.INI") << 'z';
    Require(std::filesystem::remove(fixture.data / "SETTINGS.INI"), "remove Data/SETTINGS.INI");
    Require(std::filesystem::remove(fixture.data / "Settings.ini"), "remove Data/Settings.ini");
    std::ofstream(fixture.data / "SETTINGS.ini") << 'n';
    RequireRead("data/settings.ini", 'n');
}};

const Case danglingSymlink{"KernelOpen_DanglingSymlink_FailsWithNotFound", [] {
    const PathCaseFixture fixture;
    fixture.EnterLowercase();
    std::filesystem::create_symlink("absent", fixture.directory / "Dangling");
    RequireEqual(sceKernelOpen("dangling", SCE_KERNEL_O_RDONLY, 0), kernelNotFound, "open dangling");
}};
#endif

} // namespace
