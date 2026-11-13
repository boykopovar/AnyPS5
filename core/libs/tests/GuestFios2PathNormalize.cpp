#include "prx/libSceFios2/include/Api.hpp"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>

static void Require(bool condition, int line) {
    if (!condition) {
        std::fprintf(stderr, "guest_fios2_path_normalize check failed at line %d\n", line);
        std::abort();
    }
}

static void CheckNormalize(const char* path, const std::string& expected, int line) {
    char output[256];
    std::memset(output, 0x7f, sizeof(output));
    const std::size_t result = sceFiosPathNormalize(output, path, sizeof(output));
    if (result != expected.size() + 1 || std::memcmp(output, expected.c_str(), expected.size() + 1) != 0) {
        std::fprintf(stderr, "normalize returned %zu for expected size %zu\n", result, expected.size() + 1);
        Require(false, line);
    }
}
#define CHECK_NORMALIZE(path, expected) CheckNormalize((path), (expected), __LINE__)

int main() {
    CHECK_NORMALIZE("/app0/\xff/../a", "/app0/a");
    CHECK_NORMALIZE("/app0/\x83\x66\x81\x5b\x83\x5e.bin", "/app0/\x83\x66\x81\x5b\x83\x5e.bin");
    CHECK_NORMALIZE("/app0/caf\xe9", "/app0/caf\xe9");
    CHECK_NORMALIZE("/app0/\x80/./\xc0\xaf/", "/app0/\x80/\xc0\xaf");

    CHECK_NORMALIZE("x:/../y", "y");
    CHECK_NORMALIZE("a:b/../c", "c");
    CHECK_NORMALIZE("1:/../y", "y");
    CHECK_NORMALIZE("app0:/../y", "y");

    CHECK_NORMALIZE("\\app0\\data\\game.pkg", "/app0/data/game.pkg");
    CHECK_NORMALIZE("/app0/data/../extra/file.dat", "/app0/extra/file.dat");
    CHECK_NORMALIZE("app0/sub/../../../app0", "../app0");
    CHECK_NORMALIZE("/../app0/./data/", "/app0/data");
    CHECK_NORMALIZE("////", "/");
    CHECK_NORMALIZE("a/..", ".");
    CHECK_NORMALIZE("", ".");

    std::puts("guest_fios2_path_normalize ok");
    return 0;
}
