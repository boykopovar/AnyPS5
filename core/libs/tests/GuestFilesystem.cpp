#include "SceTypes.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <cstdio>
extern "C" {
int APS5_VABI remove_nid_postfix(const char*);
int APS5_VABI rename_nid_postfix(const char*, const char*);
int APS5_VABI sceKernelChmod_nid_postfix(const char*, unsigned short);
int APS5_VABI sceKernelFsync(int);
int APS5_VABI sceKernelFtruncate(int, long long);
int APS5_VABI sceKernelTruncate_nid_postfix(const char*, long long);
int APS5_VABI sceKernelUtimes_nid_postfix(const char*, const void*);
int APS5_VABI open_nid_postfix(const char*, int, int);
int APS5_VABI _open_nid_postfix(const char*, int, ...);
int APS5_VABI close_nid_postfix(int);
int APS5_VABI stat_nid_postfix(const char*, FileStat*);
int APS5_VABI unlink_nid_postfix(const char*);
int APS5_VABI rmdir_nid_postfix(const char*);
int APS5_VABI sceKernelOpen(const char*, int, unsigned short);
int APS5_VABI sceKernelStat(const char*, FileStat*);
int APS5_VABI sceKernelUnlink(const char*);
int APS5_VABI sceKernelRmdir(const char*);
int* APS5_VABI __error_nid_postfix();
}
static void Check(bool value, int line) {
    if (!value) {
        std::fprintf(stderr, "Filesystem check failed at line %d (guest errno %d)\n", line, *__error_nid_postfix());
        std::abort();
    }
}
#define Require(value) Check((value), __LINE__)
int main() {
    const auto root = std::filesystem::path("anyps5-filesystem-test-" +
        std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    Require(std::filesystem::create_directory(root));
    const auto file = root / "file.txt";
    { std::ofstream stream(file); stream << "retained until removal"; }
    Require(remove_nid_postfix(root.string().c_str()) == -1);
    Require(*__error_nid_postfix() == 66);
    Require(std::filesystem::is_regular_file(file));
    Require(remove_nid_postfix((file / "invalid").string().c_str()) == -1);
    Require(remove_nid_postfix("") == -1 && *__error_nid_postfix() == 2);
    Require(remove_nid_postfix(nullptr) == -1 && *__error_nid_postfix() == 14);
    const auto renamed = root / "renamed.txt";
    { std::ofstream stream(renamed); stream << "old contents"; }
    Require(rename_nid_postfix(file.string().c_str(), renamed.string().c_str()) == 0);
    Require(!std::filesystem::exists(file));
    { std::ifstream stream(renamed); std::string contents; std::getline(stream, contents);
      Require(contents == "retained until removal"); }
    Require(rename_nid_postfix(renamed.string().c_str(), renamed.string().c_str()) == 0);
    Require(rename_nid_postfix(file.string().c_str(), renamed.string().c_str()) == -1);
    Require(*__error_nid_postfix() == 2);
    Require(rename_nid_postfix(renamed.string().c_str(), file.string().c_str()) == 0);
    Require(remove_nid_postfix(file.string().c_str()) == 0);
    Require(!std::filesystem::exists(file));
    Require(remove_nid_postfix(file.string().c_str()) == -1 && *__error_nid_postfix() == 2);
    const auto sized = root / "sized.txt";
    { std::ofstream stream(sized); stream << "0123456789abcdef"; }
    Require(sceKernelChmod_nid_postfix(sized.string().c_str(), 0600) == 0);
    Require(sceKernelTruncate_nid_postfix(sized.string().c_str(), 6) == 0);
    Require(std::filesystem::file_size(sized) == 6);
    { std::ifstream stream(sized); std::string contents; std::getline(stream, contents);
      Require(contents == "012345"); }
    Require(sceKernelTruncate_nid_postfix((sized / "missing").string().c_str(), 6) == static_cast<int>(0x80020002u));
    Require(sceKernelUtimes_nid_postfix(sized.string().c_str(), nullptr) == 0);
    std::FILE* native = std::fopen(sized.string().c_str(), "r+b");
    Require(native != nullptr);
#ifdef _WIN32
    const int descriptor = _fileno(native);
#else
    const int descriptor = ::fileno(native);
#endif
    Require(descriptor >= 0 && sceKernelFsync(descriptor) == 0);
    Require(sceKernelFtruncate(descriptor, 3) == 0);
    Require(std::fclose(native) == 0);
    Require(std::filesystem::file_size(sized) == 3);
    Require(remove_nid_postfix(sized.string().c_str()) == 0);
    const auto present = root / "present.txt";
    const auto presentName = present.string();
    const auto missingName = (root / "missing.txt").string();
    const auto rootName = root.string();
    { std::ofstream stream(present); stream << "posix"; }
    FileStat status{};
    Require(stat_nid_postfix(presentName.c_str(), &status) == 0 && status.st_size == 5);
    Require(stat_nid_postfix(missingName.c_str(), &status) == -1 && *__error_nid_postfix() == 2);
    Require(sceKernelStat(missingName.c_str(), &status) == static_cast<int>(0x80020002u));
    Require(stat_nid_postfix("", &status) == -1 && *__error_nid_postfix() == 2);
    Require(stat_nid_postfix(nullptr, &status) == -1 && *__error_nid_postfix() == 14);
    Require(stat_nid_postfix(presentName.c_str(), nullptr) == -1 && *__error_nid_postfix() == 14);
    const int opened = open_nid_postfix(presentName.c_str(), 0, 0);
    Require(opened >= 0 && close_nid_postfix(opened) == 0);
    const int reopened = _open_nid_postfix(presentName.c_str(), 0);
    Require(reopened >= 0 && close_nid_postfix(reopened) == 0);
    Require(open_nid_postfix(missingName.c_str(), 0, 0) == -1 && *__error_nid_postfix() == 2);
    Require(_open_nid_postfix(missingName.c_str(), 0) == -1 && *__error_nid_postfix() == 2);
    Require(sceKernelOpen(missingName.c_str(), 0, 0) == static_cast<int>(0x80020002u));
    Require(open_nid_postfix(presentName.c_str(), 0x0a02, 0644) == -1 && *__error_nid_postfix() == 17);
    Require(_open_nid_postfix(presentName.c_str(), 0x0a02, 0644) == -1 && *__error_nid_postfix() == 17);
    Require(open_nid_postfix("", 0, 0) == -1 && *__error_nid_postfix() == 2);
    Require(_open_nid_postfix("", 0) == -1 && *__error_nid_postfix() == 2);
    Require(open_nid_postfix(nullptr, 0, 0) == -1 && *__error_nid_postfix() == 14);
    Require(_open_nid_postfix(nullptr, 0) == -1 && *__error_nid_postfix() == 14);
    Require(rmdir_nid_postfix(rootName.c_str()) == -1 && *__error_nid_postfix() == 66);
    Require(sceKernelRmdir(rootName.c_str()) == static_cast<int>(0x80020042u));
    Require(rmdir_nid_postfix(presentName.c_str()) == -1 && *__error_nid_postfix() == 20);
    Require(rmdir_nid_postfix(missingName.c_str()) == -1 && *__error_nid_postfix() == 2);
    Require(rmdir_nid_postfix("") == -1 && *__error_nid_postfix() == 2);
    Require(rmdir_nid_postfix(nullptr) == -1 && *__error_nid_postfix() == 14);
    Require(unlink_nid_postfix(missingName.c_str()) == -1 && *__error_nid_postfix() == 2);
    Require(sceKernelUnlink(missingName.c_str()) == static_cast<int>(0x80020002u));
    Require(unlink_nid_postfix("") == -1 && *__error_nid_postfix() == 2);
    Require(unlink_nid_postfix(nullptr) == -1 && *__error_nid_postfix() == 14);
    Require(unlink_nid_postfix(presentName.c_str()) == 0 && !std::filesystem::exists(present));
    const auto empty = root / "empty";
    Require(std::filesystem::create_directory(empty));
    Require(rmdir_nid_postfix(empty.string().c_str()) == 0 && !std::filesystem::exists(empty));
    Require(remove_nid_postfix(root.string().c_str()) == 0);
    Require(!std::filesystem::exists(root));
}
