#include "SceTypes.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include "prx/libc/include/RemoveTree.hpp"
#include "prx/libSceLibcInternal/include/Api.hpp"
#include "prx/libScePosixForWebKit/include/Api.hpp"
#include "prx/libkernel/include/Api.hpp"
#include "prx/libc/include/Api.hpp"
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <source_location>
#include <string>

namespace {

constexpr int Enoent = 2;
constexpr int Ebadf = 9;
constexpr int Eexist = 17;
constexpr int Einval = 22;
constexpr std::uint32_t Unchanged = 0xffffffffu;

void Require(bool condition, std::source_location location = std::source_location::current()) {
    if (!condition) {
        std::fprintf(stderr, "WebKit wrapper check failed at line %u (guest errno %d)\n", location.line(), *__error_nid_postfix());
        std::abort();
    }
}

bool Fails(std::int64_t result, int error) {
    return result == -1 && *__error_nid_postfix() == error;
}

}

int main() {
    const auto root = std::filesystem::path("anyps5-posix-wrap-test-" +
        std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    Require(std::filesystem::create_directory(root));
    const auto file = (root / "data.bin").string();
    std::ofstream(file, std::ios::binary) << "abcdefgh";
    const auto missing = (root / "missing").string();

    const int descriptor = open_nid_postfix(file.c_str(), 0, 0);
    Require(descriptor >= 0);
    Require(__wrap_fchown(descriptor, Unchanged, Unchanged) == 0);
    Require(close_nid_postfix(descriptor) == 0);
    Require(Fails(__wrap_fchown(-1, Unchanged, Unchanged), Ebadf));

    const auto linked = (root / "linked.bin").string();
    Require(__wrap_link(file.c_str(), linked.c_str()) == 0 && std::filesystem::hard_link_count(file) == 2);
    Require(Fails(__wrap_link(file.c_str(), linked.c_str()), Eexist));
    Require(Fails(__wrap_link(missing.c_str(), (root / "other.bin").string().c_str()), Enoent));

    char target[64] = {};
    Require(Fails(__wrap_readlink(file.c_str(), target, sizeof(target)), Einval));
    Require(Fails(__wrap_readlink(missing.c_str(), target, sizeof(target)), Enoent));

    LibcStatvfs wrapped{};
    LibcStatvfs plain{};
    Require(__wrap_statvfs(root.string().c_str(), &wrapped) == 0 && statvfs_nid_postfix(root.string().c_str(), &plain) == 0);
    Require(wrapped.f_bsize != 0 && wrapped.f_bsize == plain.f_bsize && wrapped.f_namemax == plain.f_namemax && wrapped.f_fsid == plain.f_fsid);
    Require(Fails(__wrap_statvfs(missing.c_str(), &wrapped), Enoent));

    Require(Fails(__wrap_symlink("data.bin", linked.c_str()), Eexist));
    const auto symbolic = (root / "symbolic").string();
    if (__wrap_symlink("data.bin", symbolic.c_str()) == 0) {
        FileStat status{};
        Require(__wrap_readlink(symbolic.c_str(), target, sizeof(target)) == 8 && std::memcmp(target, "data.bin", 8) == 0);
        Require(lstat_nid_postfix(symbolic.c_str(), &status) == 0 && (status.st_mode & 0170000) == 0120000);
    }

    std::error_code error;
    RemoveTree(root, error);
    return 0;
}
