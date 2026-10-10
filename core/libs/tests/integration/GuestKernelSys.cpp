#include "prx/libc/include/general/VabiMacros.hpp"

#include <Testing/Test.hpp>

#include <chrono>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <string>
#include <system_error>

extern "C" {
std::int64_t APS5_VABI readlink_nid_postfix(const char*, char*, std::size_t);
int* APS5_VABI __error_nid_postfix();
}

namespace {

using Testing::Case;
using Testing::Require;
using Testing::RequireEqual;

constexpr int enoent = 2;
constexpr int efault = 14;
constexpr int einval = 22;

class ReadlinkFixture {
public:
    ReadlinkFixture()
        : root("anyps5-kernel-sys-test-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count())),
          file((root / "file.txt").string()),
          link((root / "link").string()) {
        Require(std::filesystem::create_directory(std::filesystem::current_path() / root), "create the test directory");
        std::ofstream(file).put('x');
    }

    ~ReadlinkFixture() {
        std::error_code ignored;
        std::filesystem::remove_all(std::filesystem::current_path() / root, ignored);
    }

    ReadlinkFixture(const ReadlinkFixture&) = delete;
    ReadlinkFixture& operator=(const ReadlinkFixture&) = delete;

    void CreateLinkOrSkip() const {
        std::error_code error;
        std::filesystem::create_symlink("sub/target.txt", link, error);
        if (error) Testing::Skip("symbolic links unavailable: " + error.message());
    }

    const std::filesystem::path root;
    const std::string file;
    const std::string link;
};

void RequireFailure(std::int64_t result, int expectedError, const std::string& message) {
    const int error = *__error_nid_postfix();
    RequireEqual(result, std::int64_t {-1}, message);
    RequireEqual(error, expectedError, message + " guest errno");
}

const Case regularFile{"Readlink_RegularFile_FailsWithEinval", [] {
    const ReadlinkFixture fixture;
    char buffer[64];
    RequireFailure(readlink_nid_postfix(fixture.file.c_str(), buffer, sizeof(buffer)), einval, "regular file");
}};

const Case directory{"Readlink_Directory_FailsWithEinval", [] {
    const ReadlinkFixture fixture;
    char buffer[64];
    RequireFailure(readlink_nid_postfix(fixture.root.string().c_str(), buffer, sizeof(buffer)), einval, "directory");
}};

const Case missing{"Readlink_MissingPath_FailsWithEnoent", [] {
    const ReadlinkFixture fixture;
    char buffer[64];
    RequireFailure(readlink_nid_postfix((fixture.root / "missing").string().c_str(), buffer, sizeof(buffer)), enoent, "missing path");
}};

const Case emptyPath{"Readlink_EmptyPath_FailsWithEnoent", [] {
    char buffer[64];
    RequireFailure(readlink_nid_postfix("", buffer, sizeof(buffer)), enoent, "empty path");
}};

const Case nullPath{"Readlink_NullPath_FailsWithEfault", [] {
    char buffer[64];
    RequireFailure(readlink_nid_postfix(nullptr, buffer, sizeof(buffer)), efault, "null path");
}};

const Case symbolicLink{"Readlink_SymbolicLink_ReturnsTargetWithoutTerminator", [] {
    const ReadlinkFixture fixture;
    fixture.CreateLinkOrSkip();
    char buffer[64];
    std::memset(buffer, '#', sizeof(buffer));
    RequireEqual(readlink_nid_postfix(fixture.link.c_str(), buffer, sizeof(buffer)), std::int64_t {14}, "returned length");
    RequireEqual(std::string(buffer, 14), std::string("sub/target.txt"), "link target");
    RequireEqual(buffer[14], '#', "byte after the target");
}};

const Case smallBuffer{"Readlink_SmallBuffer_TruncatesTarget", [] {
    const ReadlinkFixture fixture;
    fixture.CreateLinkOrSkip();
    char buffer[64];
    std::memset(buffer, '#', sizeof(buffer));
    RequireEqual(readlink_nid_postfix(fixture.link.c_str(), buffer, 3), std::int64_t {3}, "returned length");
    RequireEqual(std::string(buffer, 3), std::string("sub"), "truncated target");
    RequireEqual(buffer[3], '#', "byte after the truncated target");
}};

const Case nullBufferZeroSize{"Readlink_NullBufferWithZeroSize_ReturnsZero", [] {
    const ReadlinkFixture fixture;
    fixture.CreateLinkOrSkip();
    RequireEqual(readlink_nid_postfix(fixture.link.c_str(), nullptr, 0), std::int64_t {0}, "returned length");
}};

const Case nullBuffer{"Readlink_NullBufferWithNonZeroSize_FailsWithEfault", [] {
    const ReadlinkFixture fixture;
    fixture.CreateLinkOrSkip();
    RequireFailure(readlink_nid_postfix(fixture.link.c_str(), nullptr, 64), efault, "null buffer");
}};

const Case differentCase{"Readlink_LinkNameInDifferentCase_ReturnsTarget", [] {
    const ReadlinkFixture fixture;
    fixture.CreateLinkOrSkip();
    char buffer[64];
    RequireEqual(readlink_nid_postfix((fixture.root / "LINK").string().c_str(), buffer, sizeof(buffer)), std::int64_t {14}, "returned length");
}};

} // namespace
