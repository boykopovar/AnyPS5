#include "prx/libkernel/include/Api.hpp"
#include "prx/libc/include/Api.hpp"
#include "prx/libc/include/ApplicationHeap.hpp"
#include "prx/libScePosixForWebKit/include/Api.hpp"
#include "prx/libc/include/General.hpp"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <map>
#include <random>
#include <source_location>
#include <string>

#ifndef _WIN32
#include <unistd.h>
#endif

namespace {

constexpr int FtwF = 0;
constexpr int FtwD = 1;
constexpr int FtwNs = 4;
constexpr int GuestEinval = 22;

std::map<std::string, int> g_types;
int g_stopAt = 0;
int g_stopReturn = 0;
int g_calls = 0;

void Require(bool condition, std::source_location location = std::source_location::current()) {
    if (!condition) {
        std::fprintf(stderr, "ftw check failed at line %u (guest errno %d)\n", location.line(), *__error_nid_postfix());
        std::abort();
    }
}

int APS5_VABI Recorder(const char* path, const void*, int type) {
    g_types[path] = type;
    ++g_calls;
    if (g_stopAt != 0 && g_calls == g_stopAt) return g_stopReturn;
    return 0;
}

}

int main() {
    ApplicationHeapInitialize_nid_no_patch(ApplicationEmptyProcessParameters_nid_no_patch());
    std::random_device random;
    const auto root = std::filesystem::temp_directory_path() /
        ("anyps5-ftw-test-" + std::to_string(random()) + "-" + std::to_string(random()));
    Require(std::filesystem::create_directory(root));
    std::ofstream(root / "a.txt") << "x";
    Require(std::filesystem::create_directory(root / "d"));
    std::ofstream(root / "d" / "b.txt") << "x";
    Require(std::filesystem::create_directory(root / "d" / "e"));
    std::ofstream(root / "d" / "e" / "c.txt") << "x";
    Require(std::filesystem::create_directory(root / "f"));
    AddPathAlias_nid_no_patch("/anyps5-ftw-test", root.string().c_str());

    g_types.clear();
    g_calls = 0;
    Require(ftw_nid_postfix("/anyps5-ftw-test", Recorder, 4) == 0);
    const std::map<std::string, int> expected = {
        {"/anyps5-ftw-test", FtwD},
        {"/anyps5-ftw-test/a.txt", FtwF},
        {"/anyps5-ftw-test/d", FtwD},
        {"/anyps5-ftw-test/d/b.txt", FtwF},
        {"/anyps5-ftw-test/d/e", FtwD},
        {"/anyps5-ftw-test/d/e/c.txt", FtwF},
        {"/anyps5-ftw-test/f", FtwD},
    };
    Require(g_types == expected);

    g_types.clear();
    g_calls = 0;
    g_stopAt = 2;
    g_stopReturn = 9;
    Require(ftw_nid_postfix("/anyps5-ftw-test", Recorder, 4) == 9);
    Require(g_calls == 2);
    g_stopAt = 0;
    g_stopReturn = 0;

    g_types.clear();
    Require(ftw_nid_postfix("/anyps5-ftw-test/missing", Recorder, 4) == 0);
    Require(g_types.size() == 1 && g_types.begin()->second == FtwNs);

    *__error_nid_postfix() = 0;
    Require(ftw_nid_postfix("/anyps5-ftw-test", Recorder, 0) == -1);
    Require(*__error_nid_postfix() == GuestEinval);

#ifndef _WIN32
    std::filesystem::create_symlink(root / "a.txt", root / "link");
    std::filesystem::create_symlink(root / "missing.txt", root / "dangling");
    g_types.clear();
    Require(ftw_nid_postfix("/anyps5-ftw-test", Recorder, 4) == 0);
    Require(g_types.at("/anyps5-ftw-test/link") == FtwF);
    Require(g_types.at("/anyps5-ftw-test/dangling") == FtwNs);
    std::filesystem::create_symlink(root, root / "d" / "loop");
    g_types.clear();
    *__error_nid_postfix() = 0;
    Require(ftw_nid_postfix("/anyps5-ftw-test", Recorder, 4) == -1);
    Require(*__error_nid_postfix() == 62);
#endif

    RemovePathAlias_nid_no_patch("/anyps5-ftw-test");
    Require(std::filesystem::remove_all(root) > 0);
    return 0;
}
