#include "prx/libc/include/general/VabiMacros.hpp"
#include "prx/libSceNKWebKit/include/PathExports.hpp"

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <source_location>

namespace {

using NKWebKit::WebCoreAbi::Path;

void Check(bool condition, std::source_location location = std::source_location::current()) {
    if (!condition) {
        std::fprintf(stderr, "Path check failed at line %u\n", location.line());
        std::abort();
    }
}

struct PathWithElements {
    Path path;
    void* elementsBuffer;
    std::uint32_t elementsCapacity;
    std::uint32_t elementsSize;
    bool elementsEngaged;
};

void TestNullPlatformPathIsNull() {
    Path path{nullptr};
    Check(_ZNK7WebCore4Path6isNullEv(&path));
}

void TestPlatformPathIsNotNull() {
    int cairo = 0;
    Path path{&cairo};
    Check(!_ZNK7WebCore4Path6isNullEv(&path));
}

void TestOnlyThePlatformPathIsRead() {
    int storage = 0;
    PathWithElements empty{{nullptr}, &storage, 4, 2, true};
    Check(_ZNK7WebCore4Path6isNullEv(&empty.path));
    PathWithElements backed{{&storage}, nullptr, 0, 0, false};
    Check(!_ZNK7WebCore4Path6isNullEv(&backed.path));
}

}  // namespace

int main() {
    TestNullPlatformPathIsNull();
    TestPlatformPathIsNotNull();
    TestOnlyThePlatformPathIsRead();
    std::puts("Path tests passed");
    return 0;
}
