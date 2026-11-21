#include "prx/libSceHubAppUtil/libSceHubAppUtil.hpp"

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <source_location>

namespace {

void Require(bool condition, std::source_location location = std::source_location::current()) {
    if (condition) return;
    std::fprintf(stderr, "libSceHubAppUtil not-available check failed at line %u\n", location.line());
    std::abort();
}

constexpr int NotAvailable = static_cast<int>(0x818D0001u);

} // namespace

int main() {
    Require(sceHubAppUtilDeeplinkToLiveDetailView() == NotAvailable);
    Require(sceHubAppUtilDeeplinkToPSSubscriptions() == NotAvailable);
    Require(sceHubAppUtilGetInstallationStatus() == NotAvailable);
    return 0;
}
