#include "prx/libkernel/include/Api.hpp"
#include "prx/libc/include/Api.hpp"
#include "prx/libc/include/ApplicationHeap.hpp"
#include "prx/libScePosixForWebKit/include/Api.hpp"
#include "prx/libc/include/General.hpp"
#include <algorithm>
#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <random>
#include <source_location>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

constexpr int GlobAppend = 0x0001;
constexpr int GlobDooffs = 0x0002;
constexpr int GlobMark = 0x0008;
constexpr int GlobNocheck = 0x0010;
constexpr int GlobNosort = 0x0020;
constexpr int GlobBrace = 0x0080;
constexpr int GlobNomatch = -3;

struct TestGlob {
    std::size_t pathc;
    std::size_t matchc;
    std::size_t offs;
    int flags;
    char** pathv;
    void* errfunc;
    void* closedir;
    void* readdir;
    void* opendir;
    void* lstat;
    void* stat;
};
static_assert(sizeof(TestGlob) == 88);

void Require(bool condition, std::source_location location = std::source_location::current()) {
    if (!condition) {
        std::fprintf(stderr, "glob check failed at line %u (guest errno %d)\n", location.line(), *__error_nid_postfix());
        std::abort();
    }
}

std::vector<std::string> Paths(const TestGlob& glob) {
    std::vector<std::string> paths;
    for (std::size_t index = glob.offs; index < glob.offs + glob.pathc; ++index) paths.emplace_back(glob.pathv[index]);
    return paths;
}

void Create(const std::filesystem::path& path) {
    std::ofstream output(path);
    output << "x";
}

void TestWildcardAndSort(const std::filesystem::path& root) {
    TestGlob glob{};
    Require(glob_nid_postfix("/anyps5-glob-test/*.txt", 0, nullptr, &glob) == 0);
    const std::vector<std::string> expected = {"/anyps5-glob-test/a1.txt", "/anyps5-glob-test/a2.txt"};
    Require(Paths(glob) == expected);
    Require(glob.pathc == 2 && glob.matchc == 2);
    globfree_nid_postfix(&glob);
    Require(glob.pathc == 0 && glob.pathv == nullptr);
    Require(std::filesystem::exists(root / "a1.txt"));
}

void TestHiddenFilesNeedExplicitDot(const std::filesystem::path&) {
    TestGlob star{};
    Require(glob_nid_postfix("/anyps5-glob-test/*", 0, nullptr, &star) == 0);
    for (const std::string& path : Paths(star)) Require(path.find("/.") == std::string::npos);
    globfree_nid_postfix(&star);

    TestGlob hidden{};
    Require(glob_nid_postfix("/anyps5-glob-test/.h*", 0, nullptr, &hidden) == 0);
    const std::vector<std::string> paths = Paths(hidden);
    Require(std::find(paths.begin(), paths.end(), std::string("/anyps5-glob-test/.hidden")) != paths.end());
    globfree_nid_postfix(&hidden);
}

void TestNestedComponents(const std::filesystem::path&) {
    TestGlob glob{};
    Require(glob_nid_postfix("/anyps5-glob-test/*/x.txt", 0, nullptr, &glob) == 0);
    const std::vector<std::string> expected = {"/anyps5-glob-test/dir1/x.txt"};
    Require(Paths(glob) == expected);
    globfree_nid_postfix(&glob);
}

void TestMarkAddsSlashToDirectories(const std::filesystem::path&) {
    TestGlob glob{};
    Require(glob_nid_postfix("/anyps5-glob-test/dir*", GlobMark, nullptr, &glob) == 0);
    const std::vector<std::string> expected = {"/anyps5-glob-test/dir1/"};
    Require(Paths(glob) == expected);
    globfree_nid_postfix(&glob);
}

void TestTrailingSlashKeepsSlash(const std::filesystem::path&) {
    TestGlob glob{};
    Require(glob_nid_postfix("/anyps5-glob-test/dir*/", 0, nullptr, &glob) == 0);
    const std::vector<std::string> expected = {"/anyps5-glob-test/dir1/"};
    Require(Paths(glob) == expected);
    globfree_nid_postfix(&glob);
}

void TestRepeatedSeparatorsAreKept(const std::filesystem::path&) {
    TestGlob glob{};
    Require(glob_nid_postfix("/anyps5-glob-test//dir*/x.txt", 0, nullptr, &glob) == 0);
    const std::vector<std::string> expected = {"/anyps5-glob-test//dir1/x.txt"};
    Require(Paths(glob) == expected);
    globfree_nid_postfix(&glob);
}

void TestTrailingSlashMatchesDirectoriesOnly(const std::filesystem::path&) {
    TestGlob glob{};
    Require(glob_nid_postfix("/anyps5-glob-test/a*/", 0, nullptr, &glob) == GlobNomatch);
    Require(glob.pathc == 0);
    globfree_nid_postfix(&glob);
}

void TestNoMatchAndNocheck() {
    TestGlob none{};
    Require(glob_nid_postfix("/anyps5-glob-test/*.none", 0, nullptr, &none) == GlobNomatch);
    Require(none.pathc == 0);
    globfree_nid_postfix(&none);

    TestGlob check{};
    Require(glob_nid_postfix("/anyps5-glob-test/*.none", GlobNocheck, nullptr, &check) == 0);
    const std::vector<std::string> expected = {"/anyps5-glob-test/*.none"};
    Require(Paths(check) == expected);
    globfree_nid_postfix(&check);
}

void TestAppendAndDooffs() {
    TestGlob glob{};
    Require(glob_nid_postfix("/anyps5-glob-test/a*.txt", 0, nullptr, &glob) == 0);
    Require(glob_nid_postfix("/anyps5-glob-test/b*.log", GlobAppend, nullptr, &glob) == 0);
    const std::vector<std::string> expected = {"/anyps5-glob-test/a1.txt", "/anyps5-glob-test/a2.txt", "/anyps5-glob-test/b.log"};
    Require(Paths(glob) == expected);
    globfree_nid_postfix(&glob);

    TestGlob offset{};
    offset.offs = 2;
    Require(glob_nid_postfix("/anyps5-glob-test/a1.txt", GlobDooffs, nullptr, &offset) == 0);
    Require(offset.pathv[0] == nullptr && offset.pathv[1] == nullptr);
    Require(std::strcmp(offset.pathv[2], "/anyps5-glob-test/a1.txt") == 0);
    globfree_nid_postfix(&offset);
}

void TestNosortKeepsAllMatches() {
    TestGlob glob{};
    Require(glob_nid_postfix("/anyps5-glob-test/*.txt", GlobNosort, nullptr, &glob) == 0);
    Require(glob.pathc == 2);
    globfree_nid_postfix(&glob);
}

void TestUnsupportedFlagThrows() {
    TestGlob glob{};
    bool thrown = false;
    try {
        glob_nid_postfix("/anyps5-glob-test/{a1,a2}.txt", GlobBrace, nullptr, &glob);
    } catch (const std::exception&) {
        thrown = true;
    }
    Require(thrown);
}

}

int main() {
    ApplicationHeapInitialize_nid_no_patch(ApplicationEmptyProcessParameters_nid_no_patch());
    std::random_device random;
    const auto root = std::filesystem::temp_directory_path() /
        ("anyps5-glob-test-" + std::to_string(random()) + "-" + std::to_string(random()));
    Require(std::filesystem::create_directory(root));
    Create(root / "a1.txt");
    Create(root / "a2.txt");
    Create(root / "b.log");
    Create(root / ".hidden");
    Require(std::filesystem::create_directory(root / "dir1"));
    Create(root / "dir1" / "x.txt");
    AddPathAlias_nid_no_patch("/anyps5-glob-test", root.string().c_str());

    TestWildcardAndSort(root);
    TestHiddenFilesNeedExplicitDot(root);
    TestNestedComponents(root);
    TestMarkAddsSlashToDirectories(root);
    TestTrailingSlashKeepsSlash(root);
    TestRepeatedSeparatorsAreKept(root);
    TestTrailingSlashMatchesDirectoriesOnly(root);
    TestNoMatchAndNocheck();
    TestAppendAndDooffs();
    TestNosortKeepsAllMatches();
    TestUnsupportedFlagThrows();

    RemovePathAlias_nid_no_patch("/anyps5-glob-test");
    Require(std::filesystem::remove_all(root) > 0);
    return 0;
}
