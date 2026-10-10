#include "prx/libc/include/general/VabiMacros.hpp"
#include "prx/libc/include/FileStream.hpp"
#include "SceTypes.hpp"
#include "GuestSaveDataFixture.hpp"

#include <Testing/Test.hpp>

#include <array>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>
#include <system_error>
#include <vector>

extern "C" {
int APS5_VABI sceSaveDataDelete(const SaveDataDelete*);
int APS5_VABI sceSaveDataInitialize3(const void*);
int APS5_VABI sceSaveDataMount3(const SaveDataMount3*, SaveDataMountResult*);
int APS5_VABI sceSaveDataUmount2(std::uint32_t, const SaveDataMountPoint*);
FileStream* APS5_VABI fopen_nid_postfix(const char*, const char*);
std::size_t APS5_VABI fread_nid_postfix(void*, std::size_t, std::size_t, FileStream*);
std::size_t APS5_VABI fwrite_nid_postfix(const void*, std::size_t, std::size_t, FileStream*);
int APS5_VABI fclose_nid_postfix(FileStream*);
}

namespace {

using Testing::Case;
using Testing::Require;
using Testing::RequireEqual;

constexpr int SaveDataErrorParameter = -2137063424;
constexpr int SaveDataErrorBusy = -2137063421;
constexpr char mountedPayload[] = "live mounted save";
constexpr char updatedPayload[] = "still mounted";

int Delete(const char* data, std::size_t size) {
    SceSaveDataDirName name{};
    std::memcpy(name.data, data, size);
    SaveDataDelete del{};
    del.dir_name = &name;
    return sceSaveDataDelete(&del);
}

void Write(const std::filesystem::path& path) {
    std::filesystem::create_directories(path.parent_path());
    std::ofstream(path) << "data";
}

std::vector<char> Read(const std::filesystem::path& path) {
    std::ifstream stream(path, std::ios::binary);
    if (!stream) return {};
    return {std::istreambuf_iterator<char>(stream), std::istreambuf_iterator<char>()};
}

std::vector<char> Bytes(const char* payload, std::size_t size) {
    return std::vector<char>(payload, payload + size);
}

bool WriteMountedFile(const char* payload, std::size_t size) {
    auto* stream = fopen_nid_postfix("/savedata0/data.bin", "wb");
    if (stream == nullptr) return false;
    const bool written = fwrite_nid_postfix(payload, 1, size, stream) == size;
    return fclose_nid_postfix(stream) == 0 && written;
}

void RequireMountedContent(const char* payload, std::size_t size, const std::string& message) {
    std::array<char, 32> buffer{};
    auto* stream = fopen_nid_postfix("/savedata0/data.bin", "rb");
    Require(stream != nullptr, message + ": open /savedata0/data.bin");
    const bool read = fread_nid_postfix(buffer.data(), 1, size, stream) == size;
    const bool closed = fclose_nid_postfix(stream) == 0;
    Require(read && closed, message + ": read /savedata0/data.bin");
    Require(std::memcmp(buffer.data(), payload, size) == 0, message + ": content");
}

class SaveRoot {
public:
    SaveRoot() : kept(directory.Path() / "_sd" / "kept" / "data.bin"), victim(directory.Path() / "victim" / "important.txt") {
        Write(kept);
        Write(victim);
    }

    const SaveDataWorkingDirectory directory;
    const std::filesystem::path kept;
    const std::filesystem::path victim;
};

class MountedSave {
public:
    MountedSave() {
        RequireEqual(sceSaveDataInitialize3(nullptr), 0, "SaveData initializes");
        SceSaveDataDirName dirName{};
        std::memcpy(dirName.data, "kept", sizeof("kept"));
        SaveDataMount3 mount{};
        mount.dir_name = &dirName;
        mount.mount_mode = 2;
        RequireEqual(sceSaveDataMount3(&mount, &result), 0, "the existing save mounts read/write");
        mounted = true;
        Require(WriteMountedFile(mountedPayload, sizeof(mountedPayload)), "the guest alias writes the mounted save");
    }

    ~MountedSave() {
        if (mounted) sceSaveDataUmount2(0, &result.mount_point);
    }

    MountedSave(const MountedSave&) = delete;
    MountedSave& operator=(const MountedSave&) = delete;

    void Unmount() {
        mounted = false;
        RequireEqual(sceSaveDataUmount2(0, &result.mount_point), 0, "the mounted save unmounts");
    }

private:
    SaveDataMountResult result{};
    bool mounted = false;
};

const Case invalidNames{"Delete_TraversalOrReservedName_FailsParameterAndDeletesNothing", [] {
    const SaveRoot root;
    for (const char* invalid : {"../victim", "", ".", "..", "../..", "kept/..", "a\\b", "c:d"}) {
        const auto name = std::string("\"") + invalid + "\"";
        RequireEqual(Delete(invalid, std::strlen(invalid) + 1), SaveDataErrorParameter, name + " is rejected");
        Require(std::filesystem::exists(root.kept) && std::filesystem::exists(root.victim), name + " deletes nothing");
    }
}};

const Case unterminated{"Delete_UnterminatedName_FailsParameter", [] {
    const SaveRoot root;
    char name[sizeof(SceSaveDataDirName::data)];
    std::memset(name, 'a', sizeof(name));
    RequireEqual(Delete(name, sizeof(name)), SaveDataErrorParameter, "an unterminated name is rejected");
}};

const Case caseVariantMounted{"Delete_CaseVariantOfMountedSave_FailsBusyWhenHostIsCaseInsensitive", [] {
    const SaveRoot root;
    const MountedSave mounted;
    std::error_code caseAliasError;
    const bool caseAlias = std::filesystem::equivalent(root.kept.parent_path(), root.directory.Path() / "_sd" / "Kept", caseAliasError);
    Require(!caseAliasError || caseAliasError == std::errc::no_such_file_or_directory,
            "case-alias host identity probe has no unexpected filesystem error: " + caseAliasError.message());
    if (!caseAlias) Testing::Skip("host paths are case sensitive, so the case variant names another save");
    RequireEqual(Delete("Kept", sizeof("Kept")), SaveDataErrorBusy, "case-variant mounted delete returns BUSY");
    RequireEqual(Read(root.kept), Bytes(mountedPayload, sizeof(mountedPayload)), "case-variant mounted delete preserves backing data");
    RequireMountedContent(mountedPayload, sizeof(mountedPayload), "case-variant mounted delete preserves the live guest alias");
}};

const Case mounted{"Delete_MountedSave_FailsBusyAndKeepsAliasUsable", [] {
    const SaveRoot root;
    MountedSave save;
    RequireEqual(Delete("kept", sizeof("kept")), SaveDataErrorBusy, "mounted delete returns BUSY");
    RequireEqual(Read(root.kept), Bytes(mountedPayload, sizeof(mountedPayload)), "mounted data remains on disk after delete");
    RequireMountedContent(mountedPayload, sizeof(mountedPayload), "mounted data remains accessible through /savedata0");
    Require(WriteMountedFile(updatedPayload, sizeof(updatedPayload)), "the live /savedata0 alias remains writable after delete");
    RequireEqual(Read(root.kept), Bytes(updatedPayload, sizeof(updatedPayload)), "writes through /savedata0 remain attached to the save");
    save.Unmount();
}};

const Case valid{"Delete_UnmountedSave_RemovesOnlyThatSave", [] {
    const SaveRoot root;
    RequireEqual(Delete("kept", 5), 0, "a valid name is deleted");
    Require(!std::filesystem::exists(root.kept.parent_path()), "the valid save directory is gone");
    Require(std::filesystem::exists(root.victim), "the directory beside the save root is untouched");
}};

} // namespace
