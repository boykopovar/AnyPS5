#include <cstdio>
#include <cstdlib>
#include <stdexcept>
#include <string>

extern "C" int RunHostPreload_nid_no_patch(const char* path);

static void Require(bool condition, const char* what) {
    if (!condition) {
        std::fprintf(stderr, "FAIL: %s\n", what);
        std::exit(1);
    }
}

static std::string FailureOf(const char* path) {
    try {
        RunHostPreload_nid_no_patch(path);
    } catch (const std::runtime_error& error) {
        return error.what();
    }
    return {};
}

static void SetResult(const char* value) {
#ifdef _WIN32
    _putenv_s("HOST_PRELOAD_FIXTURE_RESULT", value);
#else
    setenv("HOST_PRELOAD_FIXTURE_RESULT", value, 1);
#endif
}

int main() {
    Require(RunHostPreload_nid_no_patch(HOST_PRELOAD_FIXTURE) == 7, "the fixture's aps5_preload result is returned");
    SetResult("0");
    Require(RunHostPreload_nid_no_patch(HOST_PRELOAD_FIXTURE) == 0, "a zero result is accepted");
    SetResult("-1");
    Require(FailureOf(HOST_PRELOAD_FIXTURE).find("failed with -1") != std::string::npos, "a negative result throws");
    Require(FailureOf("missing-host-preload-library").find("cannot load") != std::string::npos, "a missing library throws");
    Require(FailureOf(HOST_PRELOAD_NO_ENTRY).find("exports no aps5_preload") != std::string::npos, "a library without the entry throws");
    return 0;
}
