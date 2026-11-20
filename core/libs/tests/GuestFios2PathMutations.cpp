#include "prx/libSceFios2/include/Api.hpp"
#include "prx/libc/include/General.hpp"
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <source_location>
#include <string>
#include <string_view>

namespace {

using namespace std::string_view_literals;

void Require(bool condition, std::source_location location = std::source_location::current()) {
    if (condition) return;
    std::fprintf(stderr, "guest_fios2_path_mutations check failed at line %u\n", location.line());
    std::abort();
}

std::string Contents(const std::string& path) {
    std::ifstream in(path, std::ios::binary);
    return {std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>()};
}

void TestFileWriteSync(const std::string& directory) {
    const std::string testFile = directory + "/write_test.bin";

    Require(sceFiosFileWriteSync(nullptr, testFile.c_str(), "hello", 5, 0) == 5);
    Require(Contents(testFile) == "hello"sv);

    Require(sceFiosFileWriteSync(nullptr, testFile.c_str(), "world", 5, 6) == 5);
    Require(Contents(testFile) == "hello\x00world"sv);

    Require(sceFiosFileWriteSync(nullptr, testFile.c_str(), "XXX", 3, 2) == 3);
    Require(Contents(testFile) == "heXXX\x00world"sv);

    Require(sceFiosFileWriteSync(nullptr, testFile.c_str(), "", 0, 0) == 0);
    Require(Contents(testFile) == "heXXX\x00world"sv);

    const std::string untouched = directory + "/invalid_write.bin";
    Require(sceFiosFileWriteSync(nullptr, untouched.c_str(), "data", -1, 0) == SCE_FIOS_ERROR_BAD_SIZE);
    Require(sceFiosFileWriteSync(nullptr, untouched.c_str(), "data", 4, -1) == SCE_FIOS_ERROR_BAD_OFFSET);
    Require(sceFiosFileWriteSync(nullptr, untouched.c_str(), nullptr, 4, 0) == SCE_FIOS_ERROR_BAD_PTR);
    Require(sceFiosFileWriteSync(nullptr, untouched.c_str(), "data", -1, -1) == SCE_FIOS_ERROR_BAD_SIZE);
    Require(sceFiosFileWriteSync(nullptr, untouched.c_str(), nullptr, 4, -1) == SCE_FIOS_ERROR_BAD_OFFSET);
    Require(!std::filesystem::exists(untouched));
    Require(sceFiosFileWriteSync(nullptr, untouched.c_str(), nullptr, 0, 3) == 0);
    Require(std::filesystem::exists(untouched) && std::filesystem::file_size(untouched) == 0);
    Require(sceFiosFileWriteSync(nullptr, testFile.c_str(), "data", 4, -1) == SCE_FIOS_ERROR_BAD_OFFSET);
    Require(Contents(testFile) == "heXXX\x00world"sv);

    Require(sceFiosFileWriteSync(nullptr, nullptr, "data", 4, 0) == SCE_FIOS_ERROR_BAD_PTR);
    Require(sceFiosFileWriteSync(nullptr, "", "data", 4, 0) == SCE_FIOS_ERROR_BAD_PATH);
    Require(sceFiosFileWriteSync(nullptr, "", "data", -1, 0) == SCE_FIOS_ERROR_BAD_PATH);
    const std::string longPath(1025, 'x');
    Require(sceFiosFileWriteSync(nullptr, longPath.c_str(), "data", 4, 0) == SCE_FIOS_ERROR_PATH_TOO_LONG);

    const std::string dirAsFile = directory + "/subdir";
    Require(sceFiosDirectoryCreateSync(nullptr, dirAsFile.c_str()) == SCE_FIOS_OK);
    Require(sceFiosFileWriteSync(nullptr, dirAsFile.c_str(), "data", 4, 0) == SCE_FIOS_ERROR_NOT_A_FILE);
    Require(std::filesystem::is_directory(dirAsFile));
}

void TestFileTruncateSync(const std::string& directory) {
    const std::string testFile = directory + "/truncate_test.bin";
    Require(sceFiosFileWriteSync(nullptr, testFile.c_str(), "hello world", 11, 0) == 11);

    Require(sceFiosFileTruncateSync(nullptr, testFile.c_str(), 5) == SCE_FIOS_OK);
    Require(Contents(testFile) == "hello"sv);

    Require(sceFiosFileTruncateSync(nullptr, testFile.c_str(), 10) == SCE_FIOS_OK);
    Require(Contents(testFile) == "hello\x00\x00\x00\x00\x00"sv);

    const std::string missingFile = directory + "/missing.bin";
    Require(sceFiosFileTruncateSync(nullptr, missingFile.c_str(), 10) == SCE_FIOS_ERROR_BAD_PATH);

    Require(sceFiosFileWriteSync(nullptr, testFile.c_str(), "hello world", 11, 0) == 11);
    Require(sceFiosFileTruncateSync(nullptr, testFile.c_str(), -1) == SCE_FIOS_ERROR_BAD_SIZE);
    Require(Contents(testFile) == "hello world"sv);
}

void TestFileDeleteSync(const std::string& directory) {
    const std::string testFile = directory + "/delete_test.bin";
    Require(sceFiosFileWriteSync(nullptr, testFile.c_str(), "data", 4, 0) == 4);
    Require(std::filesystem::exists(testFile));
    Require(sceFiosFileDeleteSync(nullptr, testFile.c_str()) == SCE_FIOS_OK);
    Require(!std::filesystem::exists(testFile));

    const std::string dirAsFile = directory + "/subdir_for_delete";
    Require(sceFiosDirectoryCreateSync(nullptr, dirAsFile.c_str()) == SCE_FIOS_OK);
    Require(sceFiosFileDeleteSync(nullptr, dirAsFile.c_str()) == SCE_FIOS_ERROR_NOT_A_FILE);
    Require(std::filesystem::is_directory(dirAsFile));

    const std::string missingFile = directory + "/missing.bin";
    Require(sceFiosFileDeleteSync(nullptr, missingFile.c_str()) == SCE_FIOS_ERROR_BAD_PATH);
}

void TestDeleteSync(const std::string& directory) {
    const std::string testFile = directory + "/delete_sync_test.bin";
    Require(sceFiosFileWriteSync(nullptr, testFile.c_str(), "data", 4, 0) == 4);
    Require(sceFiosDeleteSync(nullptr, testFile.c_str()) == SCE_FIOS_OK);
    Require(!std::filesystem::exists(testFile));

    const std::string testDir = directory + "/delete_sync_dir";
    Require(sceFiosDirectoryCreateSync(nullptr, testDir.c_str()) == SCE_FIOS_OK);
    Require(sceFiosDeleteSync(nullptr, testDir.c_str()) == SCE_FIOS_OK);
    Require(!std::filesystem::exists(testDir));
}

void TestRenameSync(const std::string& directory) {
    const std::string srcFile = directory + "/rename_src.bin";
    const std::string dstFile = directory + "/rename_dst.bin";
    Require(sceFiosFileWriteSync(nullptr, srcFile.c_str(), "source data", 11, 0) == 11);
    Require(sceFiosRenameSync(nullptr, srcFile.c_str(), dstFile.c_str()) == SCE_FIOS_OK);
    Require(!std::filesystem::exists(srcFile));
    Require(std::filesystem::exists(dstFile));
    Require(Contents(dstFile) == "source data"sv);

    const std::string missingSrc = directory + "/missing.bin";
    Require(sceFiosRenameSync(nullptr, missingSrc.c_str(), dstFile.c_str()) == SCE_FIOS_ERROR_BAD_PATH);

    Require(sceFiosFileWriteSync(nullptr, srcFile.c_str(), "new source", 10, 0) == 10);
    Require(sceFiosFileWriteSync(nullptr, dstFile.c_str(), "old dest", 8, 0) == 8);
    Require(sceFiosRenameSync(nullptr, srcFile.c_str(), dstFile.c_str()) == SCE_FIOS_OK);
    Require(!std::filesystem::exists(srcFile));
    Require(Contents(dstFile) == "new source"sv);

    Require(sceFiosFileWriteSync(nullptr, srcFile.c_str(), "another", 7, 0) == 7);
    Require(sceFiosRenameSync(nullptr, srcFile.c_str(), nullptr) == SCE_FIOS_ERROR_BAD_PTR);
    Require(std::filesystem::exists(srcFile));
    Require(!std::filesystem::exists(directory + "/null_dst"));

    Require(sceFiosRenameSync(nullptr, srcFile.c_str(), "") == SCE_FIOS_ERROR_BAD_PATH);
    Require(std::filesystem::exists(srcFile));

    const std::string longPath(1025, 'y');
    Require(sceFiosRenameSync(nullptr, srcFile.c_str(), longPath.c_str()) == SCE_FIOS_ERROR_PATH_TOO_LONG);
    Require(std::filesystem::exists(srcFile));
}

void TestAsyncWrite(const std::string& directory) {
    const std::string testFile = directory + "/async_write.bin";
    const char* data = "async write test";
    const SceFiosSize length = 16;

    SceFiosOp op = sceFiosFileWrite(nullptr, testFile.c_str(), data, length, 0);
    Require(op > 0);

    Require(sceFiosOpWait(op) == SCE_FIOS_OK);
    Require(sceFiosOpGetError(op) == SCE_FIOS_OK);
    Require(sceFiosOpGetActualCount(op) == length);

    sceFiosOpDelete(op);

    Require(Contents(testFile) == "async write test"sv);
}

} // namespace

int main() {
    const std::filesystem::path tempDir = "fios2_path_mutations_probe";
    std::filesystem::remove_all(tempDir);
    std::filesystem::create_directories(tempDir);

    std::uint32_t params[16] = {};
    params[0] = static_cast<std::uint32_t>(sizeof(params)) << 1;
    params[3] = 0x80000;
    Require(sceFiosInitialize(reinterpret_cast<const SceFiosParams*>(params)) == SCE_FIOS_OK);
    Require(sceFiosIsInitialized(nullptr));

    TestFileWriteSync(tempDir.generic_string());
    TestFileTruncateSync(tempDir.generic_string());
    TestFileDeleteSync(tempDir.generic_string());
    TestDeleteSync(tempDir.generic_string());
    TestRenameSync(tempDir.generic_string());
    TestAsyncWrite(tempDir.generic_string());

    sceFiosTerminate();
    Require(!sceFiosIsInitialized(nullptr));

    std::filesystem::remove_all(tempDir);
    return 0;
}