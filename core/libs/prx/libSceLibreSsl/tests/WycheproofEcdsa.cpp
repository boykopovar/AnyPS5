#include "prx/libSceLibreSsl/include/Api.hpp"
#include "prx/libc/include/ApplicationHeap.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include "WycheproofEcdsaVectors.hpp"
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>

using Bytes = std::vector<unsigned char>;

static void Require(bool value, const char* what) {
    if (value) return;
    std::fprintf(stderr, "failed: %s\n", what);
    std::abort();
}

static Bytes FromHex(const char* hex) {
    Bytes out;
    for (const char* digit = hex; digit[0] && digit[1]; digit += 2) {
        out.push_back(static_cast<unsigned char>(std::stoi(std::string(digit, 2), nullptr, 16)));
    }
    return out;
}

int main() {
    void* const heapApi[10]{};
    ApplicationHeapRegister_nid_no_patch(heapApi);

    for (const auto& vector : LibreSslWycheproofEcdsa::Vectors) {
        EcKey* const key = EC_KEY_new_by_curve_name_nid_postfix(415);
        Require(key != nullptr, "EC_KEY_new_by_curve_name");

        const EcGroup* const group = EC_KEY_get0_group_nid_postfix(key);
        Require(group != nullptr, "EC_KEY_get0_group");

        EcPoint* const point = EC_POINT_new_nid_postfix(group);
        Require(point != nullptr, "EC_POINT_new");

        const Bytes publicKeyBytes = FromHex(vector.publicHex);
        Require(EC_POINT_oct2point_nid_postfix(group, point, publicKeyBytes.data(), publicKeyBytes.size(), nullptr) == 1, "EC_POINT_oct2point");

        Require(EC_KEY_set_public_key_nid_postfix(key, point) == 1, "EC_KEY_set_public_key");
        EC_POINT_free_nid_postfix(point);

        const Bytes digestBytes = FromHex(vector.digestHex);
        const Bytes signatureBytes = FromHex(vector.signatureHex);

        const int verify = ECDSA_verify_nid_postfix(672, digestBytes.data(), static_cast<int>(digestBytes.size()), signatureBytes.data(), static_cast<int>(signatureBytes.size()), key);

        if (vector.expectedVerify == 1) {
            Require(verify == 1, "ECDSA_verify valid");
        } else {
            Require(verify <= 0, "ECDSA_verify invalid");
        }

        EC_KEY_free_nid_postfix(key);
        ERR_clear_error_nid_postfix();
    }

    Require(ERR_peek_error_nid_postfix() == 0, "no error is queued");
    std::puts("guest libressl wycheproof ecdsa tests passed");
    return 0;
}
