#include "prx/libSceEsvmWidevine/include/Api.hpp"
#include <cstdio>
#include <stdexcept>

namespace {

int failures = 0;

template <typename TFn>
void RequireThrows(TFn fn, const char *name) {
    try {
        fn();
    } catch (const std::runtime_error &) {
        return;
    }
    std::fprintf(stderr, "%s did not throw\n", name);
    ++failures;
}

}

int main() {
    RequireThrows(sceEsvmCreateMediaKeys2_nid_no_patch, "sceEsvmCreateMediaKeys2");
    RequireThrows(sceEsvmGetMetrics_nid_no_patch, "sceEsvmGetMetrics");
    RequireThrows(sceEsvmGetProvisioningRequest_nid_no_patch, "sceEsvmGetProvisioningRequest");
    RequireThrows(sceEsvmGetServiceCertificateRequest_nid_no_patch, "sceEsvmGetServiceCertificateRequest");
    RequireThrows(sceEsvmHandleProvisioningResponse_nid_no_patch, "sceEsvmHandleProvisioningResponse");
    RequireThrows(sceEsvmIsProvisioned_nid_no_patch, "sceEsvmIsProvisioned");
    RequireThrows(sceEsvmMediaKeysDecryptAudioData_nid_no_patch, "sceEsvmMediaKeysDecryptAudioData");
    RequireThrows(sceEsvmParseAndLoadServiceCertificateResponse_nid_no_patch, "sceEsvmParseAndLoadServiceCertificateResponse");
    RequireThrows(sceEsvmSetMediaKeys_nid_no_patch, "sceEsvmSetMediaKeys");
    RequireThrows(sceEsvmSetServiceCertificate_nid_no_patch, "sceEsvmSetServiceCertificate");
    return failures == 0 ? 0 : 1;
}
