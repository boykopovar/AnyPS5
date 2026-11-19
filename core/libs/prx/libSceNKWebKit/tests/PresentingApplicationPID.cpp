#include "prx/libSceNKWebKit/libSceNKWebKit.hpp"
#include "prx/libkernel/include/Api.hpp"

#include <cstdio>
#include <cstdlib>
#include <source_location>
#include <type_traits>

namespace {

void Require(bool condition, std::source_location location = std::source_location::current()) {
    if (!condition) {
        std::fprintf(stderr, "WebCore::presentingApplicationPID check failed at line %u\n", location.line());
        std::abort();
    }
}

static_assert(std::is_same_v<decltype(&_ZN7WebCore24presentingApplicationPIDEv), int (APS5_VABI *)()>);

}

int main() {
    int processID = getpid_nid_postfix();
    Require(processID > 0);
    Require(_ZN7WebCore24presentingApplicationPIDEv() == processID);
    _ZN7WebCore27setPresentingApplicationPIDEi(1234);
    Require(_ZN7WebCore24presentingApplicationPIDEv() == 1234);
    _ZN7WebCore27setPresentingApplicationPIDEi(0);
    Require(_ZN7WebCore24presentingApplicationPIDEv() == 0);
    std::printf("WebCore::presentingApplicationPID: the current process ID until an override is set, then the override\n");
}
