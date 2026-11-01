#include "prx/libSceNKWebKitRequirements/libSceNKWebKitRequirements.hpp"

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace {

void AssertTrue(bool condition, const char* message) {
    if (!condition) {
        std::fprintf(stderr, "Assertion failed: %s\n", message);
        std::abort();
    }
}

void TestBuiltinMetadata() {
    const auto* psl = psl_builtin_nid_postfix();
    AssertTrue(psl != nullptr, "psl_builtin returned null");

    const char* version = psl_get_version_nid_postfix();
    AssertTrue(version != nullptr && std::strncmp(version, "0.21.5", 6) == 0, "psl_get_version mismatch");

    int verCheck = psl_check_version_number_nid_postfix(0x001505);
    AssertTrue(verCheck == 0x001505, "psl_check_version_number failed");
    AssertTrue(psl_check_version_number_nid_postfix(0x001000) == 0, "psl_check_version_number older version should return 0");

    int64_t fileTime = psl_builtin_file_time_nid_postfix();
    AssertTrue(fileTime >= 0, "psl_builtin_file_time negative");

    const char* fn = psl_builtin_filename_nid_postfix();
    AssertTrue(fn != nullptr, "psl_builtin_filename null");

    const char* sha = psl_builtin_sha1sum_nid_postfix();
    AssertTrue(sha != nullptr, "psl_builtin_sha1sum null");

    const char* distFn = psl_dist_filename_nid_postfix();
    AssertTrue(distFn != nullptr, "psl_dist_filename null");

    int outdated = psl_builtin_outdated_nid_postfix();
    (void)outdated;

    int totalCount = psl_suffix_count_nid_postfix(psl);
    AssertTrue(totalCount > 1000, "psl_suffix_count unexpectedly small");

    int wildCount = psl_suffix_wildcard_count_nid_postfix(psl);
    AssertTrue(wildCount >= 0, "psl_suffix_wildcard_count negative");

    int excCount = psl_suffix_exception_count_nid_postfix(psl);
    AssertTrue(excCount >= 0, "psl_suffix_exception_count negative");
}

void TestSuffixAndRegistrableDomain() {
    const auto* psl = psl_builtin_nid_postfix();
    AssertTrue(psl != nullptr, "psl_builtin returned null");

    AssertTrue(psl_is_public_suffix_nid_postfix(psl, "com") == 1, "'com' should be public suffix");
    AssertTrue(psl_is_public_suffix_nid_postfix(psl, "co.uk") == 1, "'co.uk' should be public suffix");
    AssertTrue(psl_is_public_suffix_nid_postfix(psl, "org") == 1, "'org' should be public suffix");
    AssertTrue(psl_is_public_suffix_nid_postfix(psl, "example.com") == 0, "'example.com' should not be public suffix");

    AssertTrue(psl_is_public_suffix2_nid_postfix(psl, "com", 1) == 1, "'com' should be public suffix type 1");
    AssertTrue(psl_is_public_suffix2_nid_postfix(psl, "example.com", 1) == 0, "'example.com' should not be public suffix type 1");

    const char* reg1 = psl_registrable_domain_nid_postfix(psl, "www.example.com");
    AssertTrue(reg1 != nullptr && std::strcmp(reg1, "example.com") == 0, "registrable domain of www.example.com mismatch");

    const char* reg2 = psl_registrable_domain_nid_postfix(psl, "sub.domain.co.uk");
    AssertTrue(reg2 != nullptr && std::strcmp(reg2, "domain.co.uk") == 0, "registrable domain of sub.domain.co.uk mismatch");

    const char* unreg1 = psl_unregistrable_domain_nid_postfix(psl, "www.example.com");
    AssertTrue(unreg1 != nullptr && std::strcmp(unreg1, "com") == 0, "unregistrable domain of www.example.com mismatch");

    AssertTrue(psl_is_cookie_domain_acceptable_nid_postfix(psl, "www.example.com", "example.com") == 1, "cookie domain example.com should be acceptable");
    AssertTrue(psl_is_cookie_domain_acceptable_nid_postfix(psl, "www.example.com", "com") == 0, "cookie domain com should not be acceptable");
    AssertTrue(psl_is_cookie_domain_acceptable_nid_postfix(psl, "www.example.com", "other.com") == 0, "cookie domain other.com should not be acceptable");
}

void TestStringConversionAndFileLoading() {
    char* lower = nullptr;
    int ret = psl_str_to_utf8lower_nid_postfix("EXAMPLE.COM", nullptr, nullptr, &lower);
    AssertTrue(ret == 0, "psl_str_to_utf8lower failed");
    AssertTrue(lower != nullptr && std::strcmp(lower, "example.com") == 0, "lowercase mismatch");
    psl_free_string_nid_postfix(lower);

    const char* datPath = "3rdparty/libpsl/list/public_suffix_list.dat";
    NKWebKit::GuestPslCtx* loadedPsl = psl_load_file_nid_postfix(datPath);
    if (loadedPsl) {
        AssertTrue(psl_is_public_suffix_nid_postfix(loadedPsl, "com") == 1, "loaded psl 'com' failed");
        AssertTrue(psl_is_public_suffix_nid_postfix(loadedPsl, "example.com") == 0, "loaded psl 'example.com' failed");
        psl_free_nid_postfix(loadedPsl);
    }

    FILE* fp = std::fopen(datPath, "r");
    if (fp) {
        NKWebKit::GuestPslCtx* fpPsl = psl_load_fp_nid_postfix(fp);
        std::fclose(fp);
        if (fpPsl) {
            AssertTrue(psl_is_public_suffix_nid_postfix(fpPsl, "org") == 1, "fp psl 'org' failed");
            psl_free_nid_postfix(fpPsl);
        }
    }

    NKWebKit::GuestPslCtx* latestPsl = psl_latest_nid_postfix(datPath);
    if (latestPsl) {
        AssertTrue(psl_is_public_suffix_nid_postfix(latestPsl, "net") == 1, "latest psl 'net' failed");
        psl_free_nid_postfix(latestPsl);
    }
}

} // namespace

int main() {
    TestBuiltinMetadata();
    TestSuffixAndRegistrableDomain();
    TestStringConversionAndFileLoading();
    std::printf("Psl tests passed.\n");
    return 0;
}
