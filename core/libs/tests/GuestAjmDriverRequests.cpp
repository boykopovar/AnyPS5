#include "prx/libSceAjm/libSceAjm.hpp"
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <stdexcept>
#include <string>

namespace {

void Require(bool condition, const char* message) {
    if (condition) return;
    std::fprintf(stderr, "Ajm driver requests: %s\n", message);
    std::abort();
}

template <typename Call>
void RequireDriverRequestThrows(Call&& call, const char* message) {
    try {
        call();
    } catch (const std::runtime_error& error) {
        Require(std::string(error.what()).find("the request to the AJM driver") != std::string::npos, message);
        return;
    }
    Require(false, message);
}

}

int main() {
    std::uint32_t failed = 0;
    std::uint8_t flag = 0;
    std::uint32_t log = 0;
    RequireDriverRequestThrows([&] { (void)sceAjmInstanceGetSize_nid_disambig1(0, 0, 0); },
                               "sceAjmInstanceGetSize_nid_disambig1 accepted flags reached the unmodeled AJM driver without throwing");
    RequireDriverRequestThrows([&] { (void)sceAjmGetFailedInstance_nid_disambig1(0, &failed); },
                               "sceAjmGetFailedInstance_nid_disambig1 with a valid output reached the unmodeled AJM driver without throwing");
    RequireDriverRequestThrows([&] { (void)sceAjmTelemetryGetLog_nid_disambig1(0, &flag, &log); },
                               "sceAjmTelemetryGetLog_nid_disambig1 with valid outputs reached the unmodeled AJM driver without throwing");
    std::puts("Ajm driver request tests passed");
    return 0;
}
