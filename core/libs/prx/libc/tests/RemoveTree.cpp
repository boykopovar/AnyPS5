#include "prx/libc/include/RemoveTree.hpp"

#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <string>
#include <system_error>

namespace {

void Require(bool condition, const char* message) {
    if (condition) return;
    std::fprintf(stderr, "RemoveTree: %s\n", message);
    std::abort();
}

void WriteTestFile(const std::filesystem::path& path, const char* content) {
    std::ofstream output(path, std::ios::binary);
    output << content;
    Require(static_cast<bool>(output), "test file could not be written");
}

void VerifyRemoveTree(const std::filesystem::path& base) {
    const auto root = base / ("removetree-test-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    Require(std::filesystem::create_directory(root), "test root could not be created");

    std::error_code ec;
    RemoveTree(root / "missing-entry", ec);
    Require(!ec, "RemoveTree failed on a missing path");

    const auto nested = root / "nested";
    const auto sub = nested / "sub";
    Require(std::filesystem::create_directories(sub), "nested directories could not be created");
    WriteTestFile(sub / "leaf.txt", "leaf");
    const std::filesystem::path nestedWithSep(nested.native() + std::filesystem::path::preferred_separator);
    RemoveTree(nestedWithSep, ec);
    Require(!ec, "RemoveTree failed on a nested directory with trailing separator");
    Require(!std::filesystem::exists(nested), "RemoveTree did not remove nested directory");

#ifdef _WIN32
    const auto roHiddenFile = root / "ro_hidden.bin";
    WriteTestFile(roHiddenFile, "ro_hidden");
    Require(SetFileAttributesW(roHiddenFile.c_str(), FILE_ATTRIBUTE_READONLY | FILE_ATTRIBUTE_HIDDEN) != 0, "failed to set readonly+hidden attributes");
    const HANDLE roHandle = CreateFileW(
        roHiddenFile.c_str(),
        GENERIC_READ,
        FILE_SHARE_READ | FILE_SHARE_WRITE,
        nullptr,
        OPEN_EXISTING,
        FILE_ATTRIBUTE_NORMAL,
        nullptr);
    Require(roHandle != INVALID_HANDLE_VALUE, "failed to open locked readonly+hidden file");
    RemoveTree(roHiddenFile, ec);
    Require(static_cast<bool>(ec), "RemoveTree succeeded on a locked readonly+hidden file");
    const DWORD roAttrs = GetFileAttributesW(roHiddenFile.c_str());
    Require(roAttrs != INVALID_FILE_ATTRIBUTES, "locked readonly+hidden file disappeared");
    Require((roAttrs & (FILE_ATTRIBUTE_READONLY | FILE_ATTRIBUTE_HIDDEN)) == (FILE_ATTRIBUTE_READONLY | FILE_ATTRIBUTE_HIDDEN), "RemoveTree did not preserve readonly+hidden attributes on failure");

    const auto hiddenFile = root / "hidden.bin";
    WriteTestFile(hiddenFile, "hidden");
    Require(SetFileAttributesW(hiddenFile.c_str(), FILE_ATTRIBUTE_HIDDEN) != 0, "failed to set hidden attribute");
    const HANDLE hiddenHandle = CreateFileW(
        hiddenFile.c_str(),
        GENERIC_READ,
        FILE_SHARE_READ | FILE_SHARE_WRITE,
        nullptr,
        OPEN_EXISTING,
        FILE_ATTRIBUTE_NORMAL,
        nullptr);
    Require(hiddenHandle != INVALID_HANDLE_VALUE, "failed to open locked hidden file");
    RemoveTree(hiddenFile, ec);
    Require(static_cast<bool>(ec), "RemoveTree succeeded on a locked hidden file");
    const DWORD hiddenAttrs = GetFileAttributesW(hiddenFile.c_str());
    Require(hiddenAttrs != INVALID_FILE_ATTRIBUTES, "locked hidden file disappeared");
    Require((hiddenAttrs & FILE_ATTRIBUTE_HIDDEN) != 0, "RemoveTree stripped hidden attribute from locked hidden file");
    Require((hiddenAttrs & FILE_ATTRIBUTE_READONLY) == 0, "locked hidden file unexpectedly gained readonly attribute");

    CloseHandle(roHandle);
    CloseHandle(hiddenHandle);

    RemoveTree(roHiddenFile, ec);
    Require(!ec && !std::filesystem::exists(roHiddenFile), "RemoveTree failed to remove unlocked readonly+hidden file");
    RemoveTree(hiddenFile, ec);
    Require(!ec && !std::filesystem::exists(hiddenFile), "RemoveTree failed to remove unlocked hidden file");

    const auto roOnlyFile = root / "ro_only.bin";
    WriteTestFile(roOnlyFile, "ro_only");
    Require(SetFileAttributesW(roOnlyFile.c_str(), FILE_ATTRIBUTE_READONLY) != 0, "failed to set readonly attribute");
    RemoveTree(roOnlyFile, ec);
    Require(!ec && !std::filesystem::exists(roOnlyFile), "RemoveTree failed to remove readonly-only file");

    const auto roDir = root / "ro_dir";
    Require(std::filesystem::create_directory(roDir), "failed to create readonly directory");
    const auto roChild = roDir / "child.txt";
    WriteTestFile(roChild, "child");
    Require(SetFileAttributesW(roChild.c_str(), FILE_ATTRIBUTE_READONLY) != 0, "failed to set readonly on child file");
    Require(SetFileAttributesW(roDir.c_str(), FILE_ATTRIBUTE_READONLY) != 0, "failed to set readonly on directory");
    RemoveTree(std::filesystem::path(roDir.wstring() + L"/"), ec);
    Require(!ec && !std::filesystem::exists(roDir), "RemoveTree failed on readonly directory with forward slash");
#endif

    RemoveTree(root, ec);
    Require(!ec && !std::filesystem::exists(root), "RemoveTree failed to clean up test root");
}

}

int main() {
    VerifyRemoveTree(std::filesystem::temp_directory_path());
    std::puts("libc RemoveTree tests passed");
    return EXIT_SUCCESS;
}
