#include "prx/libkernel/AppMetadata/include/AppMetadata.hpp"
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <stdexcept>
#include <string_view>

static void Require(bool value) { if (!value) std::abort(); }

int main(int argc, char** argv) {
    Require(argc == 2);
    const std::string_view mode = argv[1];
    Require(std::filesystem::is_directory("download0") == (mode == "declared"));
    if (mode == "invalid") {
        bool rejected = false;
        try {
            GetAppTitleId_nid_postfix();
        } catch (const std::runtime_error&) {
            rejected = true;
        }
        Require(rejected);
        return 0;
    }
    Require(std::strcmp(GetAppTitleId_nid_postfix().value, "PPSA00000") == 0);
    std::filesystem::remove_all("download0");
}
