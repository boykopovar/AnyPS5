#include "prx/libc/include/general/VabiMacros.hpp"
#include "prx/libSceNKWebKit/libSceNKWebKit.hpp"
#include "prx/libc/include/General.hpp"

#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <source_location>
#include <string>
#include <string_view>

namespace {

using NKWebKit::WtfAbi::String;

void Require(bool condition, std::source_location location = std::source_location::current()) {
    if (!condition) {
        std::fprintf(stderr, "SQLiteFileSystem check failed at line %u\n", location.line());
        std::abort();
    }
}

String Make(std::string_view text) {
    return _ZN3WTF6String8fromUTF8EPKhm(reinterpret_cast<const unsigned char*>(text.data()), text.size());
}

void TestDirectories() {
    const auto ensure = &_ZN7WebCore16SQLiteFileSystem29ensureDatabaseDirectoryExistsERKN3WTF6StringE;
    Require(!ensure(String()));
    Require(!ensure(Make("")));
    const auto root = std::filesystem::current_path() / ("nkwebkit-sqlite-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    Require(std::filesystem::create_directory(root));
    const auto hostPath = root.u8string();
    AddPathAlias_nid_no_patch("/nkwebkit-sqlite", reinterpret_cast<const char*>(hostPath.c_str()));
    Require(ensure(Make("/nkwebkit-sqlite/databases/origin")));
    Require(std::filesystem::is_directory(root / "databases" / "origin"));
    Require(ensure(Make("/nkwebkit-sqlite/databases/origin")));
    std::error_code error;
    std::filesystem::remove_all(root, error);
}

}

int main() {
    TestDirectories();
    return 0;
}
