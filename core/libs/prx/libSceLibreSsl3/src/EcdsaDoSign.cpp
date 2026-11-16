#include "prx/libSceLibreSsl3/libSceLibreSsl3.hpp"
#include "prx/libSceLibreSsl/include/Ec.hpp"
#include "prx/libc/include/General.hpp"
#include "prx/libc/include/HostEntropy.hpp"
#include "mbedtls/ecdsa.h"
#include <cstddef>
#include <cstdint>
#include <stdexcept>
#include <vector>

namespace {

int HostRandom(void*, unsigned char* buffer, std::size_t size) {
    HostEntropy_nid_no_patch(buffer, size);
    return 0;
}

struct Mpi {
    Mpi() {
        mbedtls_mpi_init(&value);
    }

    ~Mpi() {
        mbedtls_mpi_free(&value);
    }

    Mpi(const Mpi&) = delete;
    Mpi& operator=(const Mpi&) = delete;

    mbedtls_mpi value;
};

Bignum* ToBignum(const mbedtls_mpi& value) {
    const std::size_t size = mbedtls_mpi_size(&value);
    if (size == 0) return BN_bin2bn_nid_postfix(nullptr, 0, nullptr);
    std::vector<unsigned char> bytes(size);
    if (mbedtls_mpi_write_binary(&value, bytes.data(), bytes.size()) != 0) return nullptr;
    return BN_bin2bn_nid_postfix(bytes.data(), static_cast<int>(bytes.size()), nullptr);
}

}

extern "C" {

LibreSsl3EcdsaSig* APS5_VABI ECDSA_do_sign_nid_postfix(const unsigned char* digest, int digestLength, EcKey* key) {
    if (key == nullptr) throw std::invalid_argument("ECDSA_do_sign requires a key");
    if (key->group == nullptr || key->privateKey == nullptr) return nullptr;
    if (digestLength < 0) return nullptr;
    if (digest == nullptr && digestLength > 0) throw std::invalid_argument("ECDSA_do_sign requires a digest");

    Mpi r;
    Mpi s;
    if (mbedtls_ecdsa_sign(&key->group->group, &r.value, &s.value, &key->privateKey->value,
                           digest, static_cast<std::size_t>(digestLength), HostRandom, nullptr) != 0)
        return nullptr;

    Bignum* rValue = ToBignum(r.value);
    if (rValue == nullptr) return nullptr;
    Bignum* sValue = ToBignum(s.value);
    if (sValue == nullptr) {
        BN_free_nid_postfix(rValue);
        return nullptr;
    }

    LibreSsl3EcdsaSig* signature = ECDSA_SIG_new_nid_postfix();
    if (signature == nullptr) {
        BN_free_nid_postfix(rValue);
        BN_free_nid_postfix(sValue);
        return nullptr;
    }
    if (ECDSA_SIG_set0_nid_postfix(signature, rValue, sValue) != 1) {
        BN_free_nid_postfix(rValue);
        BN_free_nid_postfix(sValue);
        ECDSA_SIG_free_nid_postfix(signature);
        return nullptr;
    }
    return signature;
}

}
