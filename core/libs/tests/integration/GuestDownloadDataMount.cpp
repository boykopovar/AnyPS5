#include "prx/libkernel/AppMetadata/include/AppMetadata.hpp"

#include <Testing/Test.hpp>

#include <filesystem>
#include <stdexcept>
#include <string>
#include <string_view>
#include <system_error>

namespace {

using Testing::Case;
using Testing::Require;
using Testing::RequireEqual;
using Testing::RequireThrows;

class DownloadDirectoryCleanup {
public:
    DownloadDirectoryCleanup() = default;
    ~DownloadDirectoryCleanup() {
        std::error_code ignored;
        std::filesystem::remove_all("download0", ignored);
    }
    DownloadDirectoryCleanup(const DownloadDirectoryCleanup&) = delete;
    DownloadDirectoryCleanup& operator=(const DownloadDirectoryCleanup&) = delete;
};

void RequireMode(std::string_view mode) {
    const std::string& actual = Testing::RequireArgument(0, "param.json fixture mode (declared, none or invalid)");
    if (actual != mode) Testing::Skip("needs the " + std::string(mode) + " param.json working directory");
}

const Case declared{"DownloadDataMount_DeclaredSize_MountsDownloadDirectoryAndReadsTitle", [] {
    RequireMode("declared");
    const DownloadDirectoryCleanup cleanup;
    Require(std::filesystem::is_directory("download0"), "download0 is mounted");
    RequireEqual(std::string_view(GetAppTitleId_nid_postfix().value), std::string_view("PPSA00000"), "title id");
}};

const Case zeroSize{"DownloadDataMount_ZeroSize_DoesNotMountDownloadDirectory", [] {
    RequireMode("none");
    const DownloadDirectoryCleanup cleanup;
    Require(!std::filesystem::is_directory("download0"), "download0 is not mounted");
    RequireEqual(std::string_view(GetAppTitleId_nid_postfix().value), std::string_view("PPSA00000"), "title id");
}};

const Case invalid{"DownloadDataMount_InvalidParamJson_RejectsMetadataWithoutMounting", [] {
    RequireMode("invalid");
    const DownloadDirectoryCleanup cleanup;
    Require(!std::filesystem::is_directory("download0"), "download0 is not mounted");
    RequireThrows<std::runtime_error>([] { GetAppTitleId_nid_postfix(); }, "title id lookup");
}};

} // namespace
