#include "prx/libc/include/Api.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"

#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <string>
#include <unistd.h>

namespace {

void Require(bool value, int line) {
    if (!value) {
        std::fprintf(stderr, "fopen error check failed at line %d, errno %d\n", line, *__error_nid_postfix());
        std::exit(1);
    }
}
#define Check(value) Require((value), __LINE__)

constexpr int GuestEnoent = 2;
constexpr int GuestEisdir = 21;
constexpr int GuestEinval = 22;
constexpr int GuestEloop = 62;
constexpr int GuestEnametoolong = 63;

const std::string LongName(300, 'n');

std::filesystem::path EnterFreshRoot() {
    const auto root = std::filesystem::temp_directory_path() /
        ("anyps5-fopen-errors-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    std::filesystem::create_directories(root / "folder");
    std::filesystem::current_path(root);
    Check(::symlink("loop", "loop") == 0);
    { std::ofstream file("plain.txt"); file << "plain"; }
    return root;
}

bool OpenFails(const char* name, const char* mode, int error) {
    *__error_nid_postfix() = 0;
    const bool failed = fopen_nid_postfix(name, mode) == nullptr && *__error_nid_postfix() == error;
    *__error_nid_postfix() = 0;
    return failed;
}

void Open() {
    Check(OpenFails("loop", "r", GuestEloop));
    Check(OpenFails("loop", "w", GuestEloop));
    Check(OpenFails("loop/inside", "r", GuestEloop));
    Check(OpenFails(LongName.c_str(), "r", GuestEnametoolong));
    Check(OpenFails(LongName.c_str(), "w", GuestEnametoolong));
    Check(OpenFails((LongName + "/inside").c_str(), "a", GuestEnametoolong));
    Check(OpenFails("missing.txt", "r", GuestEnoent));
    Check(OpenFails("folder", "w", GuestEisdir));
    FileStream* plain = fopen_nid_postfix("plain.txt", "r");
    Check(plain != nullptr && fclose_nid_postfix(plain) == 0);
}

bool ReopenFails(const char* name, const char* mode, int error) {
    FileStream* stream = fopen_nid_postfix("plain.txt", "r");
    Check(stream != nullptr);
    *__error_nid_postfix() = 0;
    const bool failed = freopen_nid_postfix(name, mode, stream) == nullptr && *__error_nid_postfix() == error;
    *__error_nid_postfix() = 0;
    return failed;
}

void Reopen() {
    Check(ReopenFails("loop", "r", GuestEloop));
    Check(ReopenFails("loop/inside", "w", GuestEloop));
    Check(ReopenFails(LongName.c_str(), "r", GuestEnametoolong));
    Check(ReopenFails(LongName.c_str(), "w", GuestEnametoolong));
    Check(ReopenFails("missing.txt", "r", GuestEnoent));
    FileStream* stream = fopen_nid_postfix("plain.txt", "r");
    Check(stream != nullptr);
    FileStream* reopened = freopen_nid_postfix("plain.txt", "r", stream);
    Check(reopened != nullptr && fclose_nid_postfix(reopened) == 0);
}

void CheckedOpen() {
    FileStream* stream = reinterpret_cast<FileStream*>(1);
    Check(fopen_s_nid_postfix(&stream, "loop", "r") == GuestEloop && stream == nullptr);
    Check(fopen_s_nid_postfix(&stream, LongName.c_str(), "w") == GuestEnametoolong && stream == nullptr);
    Check(fopen_s_nid_postfix(&stream, "folder", "w") == GuestEisdir && stream == nullptr);
    Check(fopen_s_nid_postfix(&stream, "plain.txt", "z") == GuestEinval && stream == nullptr);
    Check(fopen_s_nid_postfix(&stream, "missing.txt", "r") == GuestEnoent && stream == nullptr);
    Check(fopen_s_nid_postfix(&stream, "plain.txt", "r") == 0 && stream != nullptr && fclose_nid_postfix(stream) == 0);
}

}

int main() {
    const auto root = EnterFreshRoot();
    Open();
    Reopen();
    CheckedOpen();
    std::error_code error;
    std::filesystem::current_path(std::filesystem::temp_directory_path(), error);
    std::filesystem::remove_all(root, error);
    return 0;
}
