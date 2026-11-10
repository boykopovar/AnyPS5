#include "prx/libSceEsvmWidevine/include/Api.hpp"
#include "prx/libc/include/General.hpp"

#include <cstdio>
#include <cstdlib>
#include <stdexcept>
#include <string>

namespace {

void Fail(const std::string& message, int line) {
    std::fprintf(stderr, "libSceEsvmWidevine catalog export check failed at line %d: %s\n", line, message.c_str());
    std::abort();
}

template <typename TFn>
void ExpectNotImplemented(TFn stub, const char* name, int line) {
    const auto before = NotImplementedCalls_nid_no_patch();
    std::string what;
    bool threw = false;
    try {
        stub();
    } catch (const std::runtime_error& error) {
        threw = true;
        what = error.what();
    }
    if (!threw) Fail(std::string(name) + " returned instead of reporting that it is not implemented", line);
    if (NotImplementedCalls_nid_no_patch() != before + 1)
        Fail(std::string(name) + " did not reach libc's shared not-implemented path", line);
    if (what != std::string(name) + " not implemented")
        Fail(std::string(name) + " reported \"" + what + "\"", line);
}

}

#define RequireNotImplemented(stub) ExpectNotImplemented(stub, #stub, __LINE__)

int main() {
    RequireNotImplemented(sceEsvmCreateMediaKeys2_nid_no_patch);
    RequireNotImplemented(sceEsvmGetMetrics_nid_no_patch);
    RequireNotImplemented(sceEsvmGetProvisioningRequest_nid_no_patch);
    RequireNotImplemented(sceEsvmGetServiceCertificateRequest_nid_no_patch);
    RequireNotImplemented(sceEsvmHandleProvisioningResponse_nid_no_patch);
    RequireNotImplemented(sceEsvmIsProvisioned_nid_no_patch);
    RequireNotImplemented(sceEsvmMediaKeysDecryptAudioData_nid_no_patch);
    RequireNotImplemented(sceEsvmParseAndLoadServiceCertificateResponse_nid_no_patch);
    RequireNotImplemented(sceEsvmSetMediaKeys_nid_no_patch);
    RequireNotImplemented(sceEsvmSetServiceCertificate_nid_no_patch);
    return 0;
}
