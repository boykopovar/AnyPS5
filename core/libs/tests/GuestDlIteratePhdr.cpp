#include "prx/libc/include/Api.hpp"
#include <cstdio>
#include <cstdlib>
#include <stdexcept>

namespace {

void Require(bool condition, const char* message) {
    if (condition) return;
    std::fprintf(stderr, "guest_dl_iterate_phdr: %s\n", message);
    std::abort();
}

int RecordInvocation(dl_phdr_info*, std::size_t, void* data) {
    auto* count = static_cast<int*>(data);
    ++(*count);
    return 42;
}

void CheckNullCallbackThrowsInvalidArgument() {
    bool caught = false;
    try {
        dl_iterate_phdr_nid_postfix(nullptr, nullptr);
    } catch (const std::invalid_argument&) {
        caught = true;
    } catch (const std::exception&) {
        Require(false, "expected std::invalid_argument on null callback, caught another std::exception");
    }
    Require(caught, "expected std::invalid_argument on null callback");
}

void CheckNonNullCallbackDispatches() {
    int count = 0;
#if defined(__linux__) || defined(__APPLE__) || defined(__EMSCRIPTEN__)
    const int result = dl_iterate_phdr_nid_postfix(&RecordInvocation, &count);
    Require(count == 0 ? result == 0 : result == 42, "unexpected return value from dl_iterate_phdr");
#else
    bool caughtRuntime = false;
    try {
        dl_iterate_phdr_nid_postfix(&RecordInvocation, &count);
    } catch (const std::runtime_error&) {
        caughtRuntime = true;
    }
    Require(caughtRuntime, "expected std::runtime_error for non-null callback on unsupported host");
#endif
}

}

int main() {
    CheckNullCallbackThrowsInvalidArgument();
    CheckNonNullCallbackDispatches();
    return 0;
}
