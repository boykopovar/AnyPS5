#include "prx/libSceAgcDriver/Execution/include/DescriptorHeapVersions.hpp"
#include <cstdio>

namespace {

struct Case {
    const char* name;
    std::uint32_t extensionRevision;
    std::optional<std::uint32_t> validationVersion;
    bool supported;
};

constexpr auto ReleasedRevision = VK_EXT_DESCRIPTOR_HEAP_SPEC_VERSION;
constexpr auto Layer275 = VK_MAKE_API_VERSION(0, 1, 3, 275);
constexpr auto Layer362 = VK_MAKE_API_VERSION(0, 1, 4, 362);
constexpr auto Layer363 = VK_MAKE_API_VERSION(0, 1, 4, 363);

constexpr Case Cases[] = {
    {"revision below release without validation", ReleasedRevision - 1u, std::nullopt, false},
    {"revision below release with current validation", ReleasedRevision - 1u, Layer363, false},
    {"released revision without validation", ReleasedRevision, std::nullopt, true},
    {"released revision with old validation", ReleasedRevision, Layer275, false},
    {"validation below minimum", ReleasedRevision, Layer362, false},
    {"first supported revision and validation", ReleasedRevision, Layer363, true},
    {"later revision without validation", ReleasedRevision + 1u, std::nullopt, true},
    {"later revision with old validation", ReleasedRevision + 1u, Layer362, false},
    {"later revision and current validation", ReleasedRevision + 1u, Layer363, true},
};

}

int main() {
    for (const auto& test : Cases) {
        if (AgcDriver::DescriptorHeapVersionsSupported(test.extensionRevision, test.validationVersion) != test.supported) {
            std::fprintf(stderr, "descriptor heap version policy failed: %s\n", test.name);
            return 1;
        }
    }
    std::puts("descriptor heap version policy tests passed");
    return 0;
}
