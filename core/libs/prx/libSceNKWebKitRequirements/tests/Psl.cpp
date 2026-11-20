#include "prx/libSceNKWebKitRequirements/libSceNKWebKitRequirements.hpp"
#include "prx/libc/include/Api.hpp"
#include "prx/libc/include/FileStream.hpp"

#include <libpsl.h>

#include <cstdint>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <string>

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
    AssertTrue(sha != nullptr && std::strlen(sha) == 40 && std::strspn(sha, "0123456789abcdef") == 40, "psl_builtin_sha1sum should be 40 lowercase hex digits");

    const char* distFn = psl_dist_filename_nid_postfix();
    AssertTrue(distFn != nullptr && std::strcmp(distFn, "public_suffix_list.dat") == 0, "psl_dist_filename should be the configured PSL_DISTFILE");

    int outdated = psl_builtin_outdated_nid_postfix();
    AssertTrue(outdated == 0 || outdated == 1, "psl_builtin_outdated should be 0 or 1");

    int totalCount = psl_suffix_count_nid_postfix(psl);
    AssertTrue(totalCount > 1000, "psl_suffix_count unexpectedly small");

    int wildCount = psl_suffix_wildcard_count_nid_postfix(psl);
    AssertTrue(wildCount > 0, "the public suffix list has wildcard rules such as *.ck");

    int excCount = psl_suffix_exception_count_nid_postfix(psl);
    AssertTrue(excCount > 0, "the public suffix list has exception rules such as !www.ck");
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

    const char* datPath = "guest_nk_req_psl.dat";
    FILE* out = std::fopen(datPath, "wb");
    AssertTrue(out != nullptr, "cannot create the test list");
    std::fputs("// ===BEGIN ICANN DOMAINS===\ncom\norg\nnet\n*.ck\n!www.ck\n// ===END ICANN DOMAINS===\n", out);
    std::fclose(out);

    NKWebKit::GuestPslCtx* loadedPsl = psl_load_file_nid_postfix(datPath);
    AssertTrue(loadedPsl != nullptr, "psl_load_file failed");
    AssertTrue(psl_is_public_suffix_nid_postfix(loadedPsl, "com") == 1, "loaded psl 'com' failed");
    AssertTrue(psl_is_public_suffix_nid_postfix(loadedPsl, "example.com") == 0, "loaded psl 'example.com' failed");
    AssertTrue(psl_is_public_suffix_nid_postfix(loadedPsl, "foo.ck") == 1, "loaded psl wildcard failed");
    AssertTrue(psl_is_public_suffix_nid_postfix(loadedPsl, "www.ck") == 0, "loaded psl exception failed");
    AssertTrue(psl_suffix_count_nid_postfix(loadedPsl) == 4, "loaded psl suffix count mismatch");
    AssertTrue(psl_suffix_exception_count_nid_postfix(loadedPsl) == 1, "loaded psl exception count mismatch");
    AssertTrue(psl_suffix_wildcard_count_nid_postfix(loadedPsl) == 1, "loaded psl wildcard count mismatch");
    psl_free_nid_postfix(loadedPsl);

    FileStream* fp = fopen_nid_postfix(datPath, "r");
    AssertTrue(fp != nullptr, "cannot reopen the test list through the guest stdio");
    NKWebKit::GuestPslCtx* fpPsl = psl_load_fp_nid_postfix(fp);
    AssertTrue(feof_nid_postfix(fp) != 0, "psl_load_fp should read the guest stream to its end");
    AssertTrue(fclose_nid_postfix(fp) == 0, "guest fclose failed");
    AssertTrue(psl_load_fp_nid_postfix(nullptr) == nullptr, "psl_load_fp of a null stream should fail");
    AssertTrue(fpPsl != nullptr, "psl_load_fp failed");
    AssertTrue(psl_is_public_suffix_nid_postfix(fpPsl, "org") == 1, "fp psl 'org' failed");
    psl_free_nid_postfix(fpPsl);

    NKWebKit::GuestPslCtx* latestPsl = psl_latest_nid_postfix(datPath);
    AssertTrue(latestPsl != nullptr, "psl_latest failed");
    AssertTrue(psl_is_public_suffix_nid_postfix(latestPsl, "net") == 1, "latest psl 'net' failed");
    psl_free_nid_postfix(latestPsl);
    std::remove(datPath);
}

void SetListTime(const std::filesystem::path& path, int64_t seconds) {
    using Clock = std::filesystem::file_time_type::clock;
    std::filesystem::last_write_time(path, Clock::from_sys(std::chrono::sys_seconds{std::chrono::seconds{seconds}}));
}

bool TestGuestFilePaths() {
    std::filesystem::create_directories("app0");
    const std::string names[]{"guest-suffix-list.dat", "guest-suffix-\xC5\xBE-\xE6\xBC\xA2.dat"};
    int matched = 0;
    int checked = 0;
    for (const auto& name : names) {
        const auto disk = std::filesystem::path("app0") / std::filesystem::path(std::u8string(name.begin(), name.end()));
        {
            std::ofstream file(disk, std::ios::binary);
            file << "// ===BEGIN ICANN DOMAINS===\ncom\norg\n*.ck\n!www.ck\n// ===END ICANN DOMAINS===\n";
            AssertTrue(static_cast<bool>(file), "cannot create the guest PSL path fixture");
        }
        SetListTime(disk, psl_builtin_file_time_nid_postfix() + 3600);
        const std::string guest = "/app0/" + name;
        FileStream* stream = fopen_nid_postfix(guest.c_str(), "rb");
        AssertTrue(stream != nullptr, "the guest filesystem cannot open the PSL control path");
        auto* control = psl_load_fp_nid_postfix(stream);
        AssertTrue(fclose_nid_postfix(stream) == 0 && control != nullptr, "the guest PSL stream control failed");
        AssertTrue(psl_suffix_count_nid_postfix(control) == 3, "the guest PSL stream control loaded the wrong data");
        psl_free_nid_postfix(control);

        for (const bool latest : {false, true}) {
            ++checked;
            auto* loaded = latest ? psl_latest_nid_postfix(guest.c_str()) : psl_load_file_nid_postfix(guest.c_str());
            const bool passed = loaded && psl_suffix_count_nid_postfix(loaded) == 3
                && psl_suffix_wildcard_count_nid_postfix(loaded) == 1 && psl_suffix_exception_count_nid_postfix(loaded) == 1;
            if (passed) ++matched;
            else std::fprintf(stderr, "PSL %s failed for guest path %s\n", latest ? "latest" : "load_file", guest.c_str());
            psl_free_nid_postfix(loaded);
        }
        std::filesystem::remove(disk);
    }
    std::printf("PSL guest file paths: %d/%d cases passed\n", matched, checked);
    return matched == checked;
}

bool TestLatestSelection() {
    const char* input = "app0/guest-psl-order.dat";
    const char* guestInput = "/app0/guest-psl-order.dat";
    const char* distribution = psl_dist_filename();
    AssertTrue(std::strcmp(distribution, "public_suffix_list.dat") == 0, "unexpected configured PSL distribution path");
    const auto write = [](const char* path, const char* data) {
        std::ofstream file(path, std::ios::binary);
        file << data;
        AssertTrue(static_cast<bool>(file), "cannot write the PSL selection fixture");
    };
    const auto createInput = [&] { write(input, "com\norg\n*.ck\n!www.ck\n"); };
    const auto createDistribution = [&] { write(distribution, "net\nedu\n"); };
    int checked = 0;
    int matched = 0;
    const auto compare = [&](const char* name, const char* nativePath, const char* guestPath) {
        ++checked;
        auto* expected = psl_latest(nativePath);
        auto* observed = psl_latest_nid_postfix(guestPath);
        const bool passed = expected && observed
            && psl_suffix_count(expected) == psl_suffix_count_nid_postfix(observed)
            && psl_suffix_wildcard_count(expected) == psl_suffix_wildcard_count_nid_postfix(observed)
            && psl_suffix_exception_count(expected) == psl_suffix_exception_count_nid_postfix(observed);
        if (passed) ++matched;
        else std::fprintf(stderr, "PSL latest source selection differs from bundled libpsl: %s\n", name);
        psl_free(expected);
        psl_free_nid_postfix(observed);
    };
    const int64_t baseline = psl_builtin_file_time();
    createInput();
    createDistribution();
    SetListTime(input, baseline + 7200);
    SetListTime(distribution, baseline + 3600);
    compare("provided file is newest", input, guestInput);
    SetListTime(input, baseline + 3600);
    SetListTime(distribution, baseline + 7200);
    compare("distribution file is newest", input, guestInput);
    SetListTime(distribution, baseline + 3600);
    compare("equal times prefer provided file", input, guestInput);
    std::filesystem::remove(distribution);
    SetListTime(input, baseline - 1);
    compare("older file uses builtin", input, guestInput);
    SetListTime(input, baseline);
    compare("equal builtin time uses builtin", input, guestInput);
    std::filesystem::remove(input);
    compare("missing files use builtin", input, guestInput);
    createDistribution();
    SetListTime(distribution, baseline + 3600);
    compare("null input uses available defaults", nullptr, nullptr);
    compare("empty input uses available defaults", "", "");
    compare("missing input uses available defaults", input, guestInput);
    std::filesystem::remove(distribution);
    AssertTrue(psl_load_file_nid_postfix(nullptr) == nullptr, "null PSL filename should fail");
    AssertTrue(psl_load_file_nid_postfix("") == nullptr, "empty PSL filename should fail");
    AssertTrue(psl_load_file_nid_postfix(guestInput) == nullptr, "missing PSL filename should fail");
    std::printf("PSL latest source selection: %d/%d cases match bundled libpsl\n", matched, checked);
    return matched == checked;
}

} // namespace

int main() {
    const bool paths = TestGuestFilePaths();
    const bool selection = TestLatestSelection();
    if (!paths || !selection) return 1;
    TestBuiltinMetadata();
    TestSuffixAndRegistrableDomain();
    TestStringConversionAndFileLoading();
    std::printf("Psl tests passed.\n");
    return 0;
}
