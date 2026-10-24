#include "prx/libkernel/include/Api.hpp"
#include "prx/libc/include/Api.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <string>
#ifndef _WIN32
#include <unistd.h>
#endif

namespace {

constexpr int ErrorNoEntry = 2;
constexpr int ErrorNotDirectory = 20;
constexpr int ErrorInvalid = 22;
constexpr int ErrorLoop = 62;
constexpr char Source[] = "anyps5_rename_dot_source";
constexpr char Target[] = "anyps5_rename_dot_target";
constexpr char Moved[] = "anyps5_rename_dot_moved";
constexpr char PlainFile[] = "anyps5_rename_dot_file";
constexpr char Loop[] = "anyps5_rename_dot_loop";

void Cleanup() {
    rmdir_nid_postfix(Source);
    rmdir_nid_postfix(Target);
    rmdir_nid_postfix(Moved);

    std::remove(PlainFile);
    std::remove(Loop);
}

void Require(bool condition, const char* what) {
    if (condition) return;
    std::fprintf(stderr, "%s (errno %d)\n", what, *__error_nid_postfix());
    Cleanup();
    std::abort();
}

bool Fails(long result, int error) { return result == -1 && *__error_nid_postfix() == error; }

bool Exists(const char* path) {
    unsigned char status[256];
    return stat_nid_postfix(path, reinterpret_cast<FileStat*>(status)) == 0;
}

}

int main() {
    Cleanup();
    Require(mkdir_nid_postfix(Source, 0755) == 0 && mkdir_nid_postfix(Target, 0755) == 0, "mkdir failed");
    const std::string dotted = std::string(Source) + "/.";
    const std::string dottedSlash = std::string(Source) + "/./";
    const std::string intoTarget = std::string(Target) + "/.";
    Require(Fails(rename_nid_postfix(dotted.c_str(), Moved), ErrorInvalid), "rename from dir/. must fail with EINVAL (22)");
    Require(Fails(rename_nid_postfix(dottedSlash.c_str(), Moved), ErrorInvalid), "rename from dir/./ must fail with EINVAL (22)");
    Require(Exists(Source) && !Exists(Moved), "a failed rename from dir/. must leave the directory in place");
    Require(Fails(rename_nid_postfix(Source, intoTarget.c_str()), ErrorInvalid), "rename to dir/. must fail with EINVAL (22)");
    Require(Fails(rename_nid_postfix(".", Moved), ErrorInvalid), "rename of . must fail with EINVAL (22)");
    Require(Fails(rename_nid_postfix(Source, "."), ErrorInvalid), "rename onto . must fail with EINVAL (22)");
    Require(Fails(rename_nid_postfix("anyps5_rename_dot_missing/.", Moved), ErrorNoEntry), "rename from missing/. must fail with ENOENT (2)");
    const std::string dotDot = std::string(Source) + "/..";
    const std::string dotDotSlash = std::string(Source) + "/../";
    Require(Fails(rename_nid_postfix(dotDot.c_str(), Moved), ErrorInvalid), "rename from dir/.. must fail with EINVAL (22)");
    Require(Fails(rename_nid_postfix(dotDotSlash.c_str(), Moved), ErrorInvalid), "rename from dir/../ must fail with EINVAL (22)");
    Require(Fails(rename_nid_postfix(Source, (std::string(Target) + "/..").c_str()), ErrorInvalid), "rename to dir/.. must fail with EINVAL (22)");
    Require(Fails(rename_nid_postfix("..", Moved), ErrorInvalid), "rename of .. must fail with EINVAL (22)");
    Require(Fails(rename_nid_postfix("anyps5_rename_dot_missing/..", Moved), ErrorNoEntry), "rename from missing/.. must fail with ENOENT (2)");
    const std::string backslashDot = std::string(Source) + "\\.";
    const std::string backslashDotDot = std::string(Source) + "\\..";
    Require(Fails(rename_nid_postfix(backslashDot.c_str(), Moved), ErrorInvalid), "rename from dir\\. must fail with EINVAL (22), the guest resolver turns the backslash into a separator");
    Require(Fails(rename_nid_postfix(backslashDotDot.c_str(), Moved), ErrorInvalid), "rename from dir\\.. must fail with EINVAL (22)");
    Require(Exists(Source) && !Exists(Moved), "failed renames from dir\\. and dir\\.. must leave the directory in place");
    Require(sceKernelRename(dotDot.c_str(), Moved) == static_cast<int>(0x80020016u), "sceKernelRename from dir/.. must return EINVAL");
    Require(sceKernelRename(dotted.c_str(), Moved) == static_cast<int>(0x80020016u), "sceKernelRename from dir/. must return EINVAL");
    Require(sceKernelRename(Source, intoTarget.c_str()) == static_cast<int>(0x80020016u), "sceKernelRename to dir/. must return EINVAL");
    Require(Exists(Source) && Exists(Target) && !Exists(Moved), "failed renames must change nothing");

    {
        std::FILE* file = std::fopen(PlainFile, "w");
        Require(file != nullptr && std::fclose(file) == 0, "creating the plain file failed");
    }
    const std::string throughFile = std::string(PlainFile) + "/.";
    Require(Fails(rename_nid_postfix(throughFile.c_str(), Moved), ErrorNotDirectory), "rename from file/. must fail with ENOTDIR (20)");
    Require(Fails(rename_nid_postfix(Source, throughFile.c_str()), ErrorNotDirectory), "rename to file/. must fail with ENOTDIR (20)");
    std::remove(PlainFile);
#ifndef _WIN32
    Require(::symlink(Loop, Loop) == 0, "creating the symbolic link loop failed");
    const std::string throughLoop = std::string(Loop) + "/.";
    Require(Fails(rename_nid_postfix(throughLoop.c_str(), Moved), ErrorLoop), "rename from loop/. must fail with ELOOP (62)");
    std::remove(Loop);
#endif

    Require(rename_nid_postfix(Source, Moved) == 0 && Exists(Moved) && !Exists(Source), "an ordinary rename must still work");
    Require(sceKernelRename(Moved, Source) == 0 && Exists(Source) && !Exists(Moved), "an ordinary sceKernelRename must still work");
    Cleanup();
    return 0;
}
