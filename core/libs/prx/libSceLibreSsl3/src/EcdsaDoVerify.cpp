#include "prx/libSceLibreSsl3/libSceLibreSsl3.hpp"
#include "prx/libSceLibreSsl/include/Ec.hpp"
#include "mbedtls/ecdsa.h"
#include <cstddef>
#include <cstdint>
#include <stdexcept>

namespace {

struct Mpi {
    Mpi() { mbedtls_mpi_init(&value); }

    ~Mpi() { mbedtls_mpi_free(&value); }

    Mpi(const Mpi&) = delete;
    Mpi& operator=(const Mpi&) = delete;

    mbedtls_mpi value;
};

int LoadComponent(const Bignum* value, const mbedtls_mpi& order, mbedtls_mpi& out) {
    if (!value) throw std::invalid_argument("ECDSA_do_verify requires signature components");
    if (value->top <= 0 || value->neg) return 0;
    if (!value->d) throw std::invalid_argument("ECDSA_do_verify: BIGNUM with words but no storage");
    const std::size_t orderWords = (mbedtls_mpi_size(&order) + sizeof(std::uint64_t) - 1) / sizeof(std::uint64_t);
    if (static_cast<std::size_t>(value->top) > orderWords) return 0;
    if (mbedtls_mpi_read_binary_le(&out, reinterpret_cast<const unsigned char*>(value->d),
                                   static_cast<std::size_t>(value->top) * sizeof(std::uint64_t)) != 0)
        return -1;
    return mbedtls_mpi_cmp_int(&out, 0) > 0 && mbedtls_mpi_cmp_mpi(&out, &order) < 0 ? 1 : 0;
}

}

extern "C" {

int APS5_VABI ECDSA_do_verify_nid_postfix(const unsigned char* digest, int digestLength,
                                           const LibreSsl3EcdsaSig* signature, EcKey* key) {
    if (!key) throw std::invalid_argument("ECDSA_do_verify requires a key");
    if (!key->group || !key->publicKey || !signature) return -1;
    const mbedtls_mpi& order = key->group->group.N;
    Mpi r;
    Mpi s;
    int loaded = LoadComponent(static_cast<const Bignum*>(signature->r), order, r.value);
    if (loaded <= 0) return loaded;
    loaded = LoadComponent(static_cast<const Bignum*>(signature->s), order, s.value);
    if (loaded <= 0) return loaded;
    if (digestLength < 0) return -1;
    if (!digest && digestLength > 0) throw std::invalid_argument("ECDSA_do_verify requires a digest");
    const int result = mbedtls_ecdsa_verify(&key->group->group, digest, static_cast<std::size_t>(digestLength),
                                            &key->publicKey->point, &r.value, &s.value);
    if (result == 0) return 1;
    if (result == MBEDTLS_ERR_ECP_VERIFY_FAILED) return 0;
    return -1;
}

}
