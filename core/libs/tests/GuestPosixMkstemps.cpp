#include "prx/libkernel/include/Api.hpp"
#include "prx/libc/include/Api.hpp"
#include "prx/libScePosixForWebKit/include/Api.hpp"
#include "prx/libc/include/General.hpp"
#include "prx/libkernel/File/include/FileFlags.hpp"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <random>
#include <source_location>
#include <string>

static void Require(bool condition, std::source_location location = std::source_location::current()) {
    if (!condition) {
        std::fprintf(stderr, "mkstemps check failed at line %u (guest errno %d)\n", location.line(), *__error_nid_postfix());
        std::abort();
    }
}

static bool IsTemporaryCharacter(char value) {
    return (value >= '0' && value <= '9') || (value >= 'A' && value <= 'Z') || (value >= 'a' && value <= 'z');
}

static void CheckCreated(const std::filesystem::path& root, const char* pattern, const char* prefix, const char* suffix) {
    const std::string name = std::filesystem::path(pattern).filename().string();
    const std::size_t prefixLength = std::strlen(prefix);
    const std::size_t suffixLength = std::strlen(suffix);
    Require(name.size() == prefixLength + 6 + suffixLength);
    Require(name.compare(0, prefixLength, prefix) == 0);
    Require(name.compare(name.size() - suffixLength, suffixLength, suffix) == 0);
    for (std::size_t index = prefixLength; index < prefixLength + 6; ++index) Require(IsTemporaryCharacter(name[index]));
    Require(name.substr(prefixLength, 6) != "XXXXXX");
    Require(std::filesystem::is_regular_file(root / name));
}

static void TestSuffixIsPreserved(const std::filesystem::path& root) {
    char pattern[] = "/anyps5-mkstemps-test/tmpXXXXXX.txt";
    const int descriptor = mkstemps_nid_postfix(pattern, 4);
    Require(descriptor >= 0);
    Require(close_nid_postfix(descriptor) == 0);
    CheckCreated(root, pattern, "tmp", ".txt");
}

static void TestZeroSuffixMatchesMkstemp(const std::filesystem::path& root) {
    char pattern[] = "/anyps5-mkstemps-test/zeroXXXXXX";
    const int descriptor = mkstemps_nid_postfix(pattern, 0);
    Require(descriptor >= 0);
    Require(close_nid_postfix(descriptor) == 0);
    CheckCreated(root, pattern, "zero", "");
}

static void TestDistinctNames(const std::filesystem::path& root) {
    char first[] = "/anyps5-mkstemps-test/uniqXXXXXX.log";
    char second[] = "/anyps5-mkstemps-test/uniqXXXXXX.log";
    const int firstDescriptor = mkstemps_nid_postfix(first, 4);
    const int secondDescriptor = mkstemps_nid_postfix(second, 4);
    Require(firstDescriptor >= 0 && secondDescriptor >= 0);
    Require(close_nid_postfix(firstDescriptor) == 0);
    Require(close_nid_postfix(secondDescriptor) == 0);
    Require(std::strcmp(first, second) != 0);
    CheckCreated(root, first, "uniq", ".log");
    CheckCreated(root, second, "uniq", ".log");
}

static void TestRejectsInvalidSuffixLength() {
    char pattern[] = "/anyps5-mkstemps-test/badXXXXXX.txt";
    *__error_nid_postfix() = 0;
    Require(mkstemps_nid_postfix(pattern, -1) == -1);
    Require(*__error_nid_postfix() == 22);

    *__error_nid_postfix() = 0;
    Require(mkstemps_nid_postfix(pattern, static_cast<int>(std::strlen(pattern)) + 1) == -1);
    Require(*__error_nid_postfix() == 22);

    *__error_nid_postfix() = 0;
    Require(mkstemps_nid_postfix(pattern, static_cast<int>(std::strlen(pattern))) == -1);
    Require(*__error_nid_postfix() == 22);

    char bare[] = "badXXXXXX";
    *__error_nid_postfix() = 0;
    Require(mkstemps_nid_postfix(bare, static_cast<int>(std::strlen(bare))) == -1);
    Require(*__error_nid_postfix() == 22);

    char slashed[] = "/anyps5-mkstemps-test/XXXXXX/b";
    *__error_nid_postfix() = 0;
    Require(mkstemps_nid_postfix(slashed, 2) == -1);
    Require(*__error_nid_postfix() == 22);
    Require(std::strcmp(pattern, "/anyps5-mkstemps-test/badXXXXXX.txt") == 0);
}

static void TestRejectsNullPattern() {
    *__error_nid_postfix() = 0;
    Require(mkstemps_nid_postfix(nullptr, 0) == -1);
    Require(*__error_nid_postfix() == 14);
}

int main() {
    std::random_device random;
    const auto root = std::filesystem::temp_directory_path() /
        ("anyps5-mkstemps-test-" + std::to_string(random()) + "-" + std::to_string(random()));
    Require(std::filesystem::create_directory(root));
    AddPathAlias_nid_no_patch("/anyps5-mkstemps-test", root.string().c_str());

    TestSuffixIsPreserved(root);
    TestZeroSuffixMatchesMkstemp(root);
    TestDistinctNames(root);
    TestRejectsInvalidSuffixLength();
    TestRejectsNullPattern();

    RemovePathAlias_nid_no_patch("/anyps5-mkstemps-test");
    Require(std::filesystem::remove_all(root) > 0);
    return 0;
}
