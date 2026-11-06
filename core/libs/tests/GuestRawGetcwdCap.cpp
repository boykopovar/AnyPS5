#include "prx/libc/include/Api.hpp"
#include "prx/libkernel_sys/include/Api.hpp"

#include <algorithm>
#include <array>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <exception>
#include <filesystem>
#include <string>
#include <system_error>

static bool Check(bool condition, const char* description) {
    if (!condition) std::fprintf(stderr, "Raw getcwd cap check failed: %s\n", description);
    return condition;
}

static bool CheckPath(const std::string& expected) {
    bool passed = true;
    std::array<char, 4096> libcBuffer;
    libcBuffer.fill('#');
    const auto libcResult = getcwd_nid_postfix(libcBuffer.data(), libcBuffer.size());
    passed &= Check(libcResult == libcBuffer.data() &&
        std::memcmp(libcBuffer.data(), expected.c_str(), expected.size() + 1) == 0,
        "libc returns the exact guest path and NUL");

    std::array<char, 4096> rawBuffer;
    rawBuffer.fill('#');
    *__error_nid_postfix() = 0;
    const int rawResult = __getcwd_nid_postfix(rawBuffer.data(), rawBuffer.size());
    const int rawError = *__error_nid_postfix();
    if (expected.size() == 1023) {
        passed &= Check(rawResult == 0, "length 1023 succeeds");
        passed &= Check(std::memcmp(rawBuffer.data(), expected.c_str(), expected.size() + 1) == 0,
            "length 1023 returns the exact path and NUL");
        passed &= Check(std::all_of(rawBuffer.begin() + expected.size() + 1, rawBuffer.end(),
            [](char value) { return value == '#'; }), "length 1023 preserves every byte after the NUL");
    } else {
        passed &= Check(rawResult == -1, "length 1024 fails");
        passed &= Check(rawError == 12, "length 1024 reports ENOMEM");
        passed &= Check(std::all_of(rawBuffer.begin(), rawBuffer.end(),
            [](char value) { return value == '#'; }), "length 1024 preserves all 4096 output bytes");
        *__error_nid_postfix() = 0;
        const int nullResult = __getcwd_nid_postfix(nullptr, 4096);
        const int nullError = *__error_nid_postfix();
        passed &= Check(nullResult == -1, "null output at length 1024 fails");
        passed &= Check(nullError == 12, "length 1024 reports ENOMEM before checking the output pointer");
    }
    return passed;
}

static bool RunFixture() {
    const std::array<std::string, 4> parents = {
        std::string(200, 'a'), std::string(200, 'b'), std::string(200, 'c'), std::string(200, 'd')
    };
    const std::array<std::string, 2> leaves = {std::string(218, 'e'), std::string(219, 'f')};
    auto parent = std::filesystem::current_path();
    for (const auto& name : parents) {
        parent /= name;
        if (!Check(std::filesystem::create_directory(parent), "create a parent directory")) return false;
    }
    for (const auto& name : leaves) {
        if (!Check(std::filesystem::create_directory(parent / name), "create a boundary directory")) return false;
    }

    if (!Check(chdir_nid_postfix("/") == 0, "initialize the isolated guest root")) return false;
    std::string expected;
    for (const auto& name : parents) {
        if (!Check(chdir_nid_postfix(name.c_str()) == 0, "enter a parent through a short relative path")) return false;
        expected += "/" + name;
    }
    bool passed = true;
    for (std::size_t index = 0; index < leaves.size(); ++index) {
        if (!Check(chdir_nid_postfix(leaves[index].c_str()) == 0,
            "enter a boundary through a short relative path")) return false;
        const auto guestPath = expected + "/" + leaves[index];
        passed &= Check(guestPath.size() == 1023 + index, "construct the exact boundary length");
        passed &= CheckPath(guestPath);
        if (!Check(chdir_nid_postfix("..") == 0, "return from a boundary directory")) return false;
    }
    return passed;
}

int main() {
    bool passed = true;
    bool rootCreated = false;
    bool hostChanged = false;
    bool guestStarted = false;
    char root[] = "/tmp/anyps5-cwd-XXXXXX";
    std::filesystem::path original;
    try {
        original = std::filesystem::current_path();
        if (Check(::mkdtemp(root) != nullptr, "create an isolated host root")) {
            rootCreated = true;
            std::filesystem::current_path(root);
            hostChanged = true;
            guestStarted = true;
            passed = RunFixture();
        } else {
            passed = false;
        }
    } catch (const std::exception& error) {
        std::fprintf(stderr, "Raw getcwd cap exception: %s\n", error.what());
        passed = false;
    }

    if (guestStarted) {
        try {
            passed &= Check(chdir_nid_postfix("/") == 0, "restore the guest cwd");
        } catch (const std::exception& error) {
            std::fprintf(stderr, "Guest cwd cleanup exception: %s\n", error.what());
            passed = false;
        }
    }
    if (hostChanged) {
        std::error_code error;
        std::filesystem::current_path(original, error);
        passed &= Check(!error, "restore the original host cwd");
    }
    if (rootCreated) {
        std::error_code error;
        const auto removed = std::filesystem::remove_all(root, error);
        passed &= Check(removed != 0 && !error, "remove the complete fixture");
    }
    return passed ? 0 : 1;
}
