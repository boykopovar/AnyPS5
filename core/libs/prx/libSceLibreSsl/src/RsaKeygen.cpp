#include "prx/libSceLibreSsl/include/Api.hpp"
#include <array>
#include <cstddef>
#include <cstdint>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>
#include "mbedtls/bignum.h"
#include "prx/libSceLibreSsl/include/Error.hpp"
#include "prx/libc/include/General.hpp"
#include "prx/libc/include/HostEntropy.hpp"

namespace {

using Keygen = int (APS5_VABI*)(LibreSslRsa* rsa, int bits, Bignum* exponent, void* callback);

constexpr int BignumLibrary = 3;
constexpr int RsaLibrary = 4;
constexpr int AnyFunction = 0xfff;
constexpr int BitsTooSmall = 117;
constexpr int KeySizeTooSmall = 120;
constexpr std::size_t PrimeCount = 2048;
constexpr std::uint64_t MaximumWord = ~std::uint64_t(0);

const std::array<std::uint16_t, PrimeCount>& SmallPrimes() {
    static const std::array<std::uint16_t, PrimeCount> primes = [] {
        std::array<std::uint16_t, PrimeCount> table {};
        std::size_t count = 0;
        for (std::uint32_t candidate = 2; count < PrimeCount; ++candidate) {
            bool prime = true;
            for (std::size_t index = 0; index < count && std::uint32_t(table[index]) * table[index] <= candidate; ++index) {
                if (candidate % table[index] == 0) {
                    prime = false;
                    break;
                }
            }
            if (prime) table[count++] = static_cast<std::uint16_t>(candidate);
        }
        return table;
    }();
    return primes;
}

struct Mpi {
    mbedtls_mpi value;

    Mpi() {
        mbedtls_mpi_init(&value);
    }

    Mpi(const Mpi&) = delete;
    Mpi& operator=(const Mpi&) = delete;

    ~Mpi() {
        mbedtls_mpi_free(&value);
    }
};

void Check(int status, const char* function) {
    if (status) NotImplemented_nid_no_patch((std::string(function) + ": a number over the mbedtls size limits").c_str());
}

int Entropy(void*, unsigned char* buffer, std::size_t size) {
    HostEntropy_nid_no_patch(buffer, size);
    return 0;
}

int PrimeChecks(int bits) {
    if (bits >= 1300) return 2;
    if (bits >= 850) return 3;
    if (bits >= 650) return 4;
    if (bits >= 550) return 5;
    if (bits >= 450) return 6;
    if (bits >= 400) return 7;
    if (bits >= 350) return 8;
    if (bits >= 300) return 9;
    if (bits >= 250) return 12;
    if (bits >= 200) return 15;
    if (bits >= 150) return 18;
    return 27;
}

void RandomOdd(Mpi& number, int bits, const char* function) {
    const int bytes = (bits + 7) / 8;
    const int bit = (bits - 1) % 8;
    const int mask = 0xff << (bit + 1);
    std::vector<unsigned char> buffer(static_cast<std::size_t>(bytes));
    HostEntropy_nid_no_patch(buffer.data(), buffer.size());
    if (bit == 0) {
        buffer[0] = 1;
        buffer[1] |= 0x80;
    } else {
        buffer[0] |= static_cast<unsigned char>(3 << (bit - 1));
    }
    buffer[0] &= static_cast<unsigned char>(~mask);
    buffer[buffer.size() - 1] |= 1;
    Check(mbedtls_mpi_read_binary(&number.value, buffer.data(), buffer.size()), function);
}

void ProbablePrime(Mpi& number, int bits, const char* function) {
    const auto& primes = SmallPrimes();
    std::array<std::uint16_t, PrimeCount> mods {};
    const std::uint64_t maximumDelta = MaximumWord - primes[PrimeCount - 1];
    for (;;) {
        RandomOdd(number, bits, function);
        for (std::size_t index = 1; index < PrimeCount; ++index) {
            mbedtls_mpi_uint remainder = 0;
            Check(mbedtls_mpi_mod_int(&remainder, &number.value, primes[index]), function);
            mods[index] = static_cast<std::uint16_t>(remainder);
        }
        std::uint64_t delta = 0;
        bool restart = false;
        for (std::size_t index = 1; index < PrimeCount;) {
            if ((mods[index] + delta) % primes[index] <= 1) {
                delta += 2;
                if (delta > maximumDelta) {
                    restart = true;
                    break;
                }
                index = 1;
                continue;
            }
            ++index;
        }
        if (restart) continue;
        Mpi step;
        std::array<unsigned char, 8> bytes {};
        for (std::size_t index = 0; index < bytes.size(); ++index) bytes[index] = static_cast<unsigned char>(delta >> (8 * (7 - index)));
        Check(mbedtls_mpi_read_binary(&step.value, bytes.data(), bytes.size()), function);
        Check(mbedtls_mpi_add_mpi(&number.value, &number.value, &step.value), function);
        return;
    }
}

bool GeneratePrime(Mpi& number, int bits, const char* function) {
    if (bits < 2) {
        LibreSslPutError_nid_no_patch(BignumLibrary, AnyFunction, BitsTooSmall, __FILE__, __LINE__);
        return false;
    }
    const int checks = PrimeChecks(bits);
    for (;;) {
        ProbablePrime(number, bits, function);
        const int status = mbedtls_mpi_is_prime_ext(&number.value, checks, Entropy, nullptr);
        if (!status) return true;
        if (status != MBEDTLS_ERR_MPI_NOT_ACCEPTABLE) Check(status, function);
    }
}

bool Store(void* target, const Mpi& number, const char* function) {
    std::vector<unsigned char> bytes(mbedtls_mpi_size(&number.value));
    Check(mbedtls_mpi_write_binary(&number.value, bytes.data(), bytes.size()), function);
    return BN_bin2bn_nid_postfix(bytes.data(), static_cast<int>(bytes.size()), static_cast<Bignum*>(target)) != nullptr;
}

bool CoprimeToExponent(const Mpi& prime, const Mpi& exponent, const char* function) {
    Mpi less, divisor;
    Check(mbedtls_mpi_sub_int(&less.value, &prime.value, 1), function);
    Check(mbedtls_mpi_gcd(&divisor.value, &less.value, &exponent.value), function);
    return mbedtls_mpi_cmp_int(&divisor.value, 1) == 0;
}

void LoadExponent(const Bignum* exponent, Mpi& number, const char* function) {
    if (!exponent) throw std::invalid_argument(std::string(function) + ": null public exponent");
    if (exponent->top < 0 || (exponent->top && !exponent->d)) throw std::invalid_argument(std::string(function) + ": malformed public exponent");
    std::vector<unsigned char> bytes(static_cast<std::size_t>(exponent->top) * 8);
    for (std::size_t word = 0; word < static_cast<std::size_t>(exponent->top); ++word)
        for (std::size_t byte = 0; byte < 8; ++byte)
            bytes[bytes.size() - 1 - (word * 8 + byte)] = static_cast<unsigned char>(exponent->d[word] >> (8 * byte));
    Check(mbedtls_mpi_read_binary(&number.value, bytes.data(), bytes.size()), function);
    if (exponent->neg && mbedtls_mpi_cmp_int(&number.value, 0) != 0)
        NotImplemented_nid_no_patch((std::string(function) + ": a negative public exponent").c_str());
    if (mbedtls_mpi_get_bit(&number.value, 0) == 0)
        NotImplemented_nid_no_patch((std::string(function) + ": an even public exponent, for which LibreSSL never returns").c_str());
}

int BuiltinKeygen(LibreSslRsa& rsa, int bits, const Bignum* exponentValue, const char* function) {
    Mpi exponent;
    LoadExponent(exponentValue, exponent, function);
    const int bitsP = static_cast<int>((static_cast<std::int64_t>(bits) + 1) / 2);
    const int bitsQ = bits - bitsP;
    if (bitsP > 8 * MBEDTLS_MPI_MAX_SIZE) NotImplemented_nid_no_patch((std::string(function) + ": a prime over the mbedtls size limits").c_str());
    const auto fail = [] {
        LibreSslPutError_nid_no_patch(RsaLibrary, AnyFunction, BignumLibrary, __FILE__, __LINE__);
        return 0;
    };
    for (void** field : {&rsa.n, &rsa.d, &rsa.e, &rsa.p, &rsa.q, &rsa.dmp1, &rsa.dmq1, &rsa.iqmp}) {
        if (!*field && !(*field = BN_new_nid_postfix())) return fail();
    }
    Store(rsa.e, exponent, function);
    Mpi p, q;
    do {
        if (!GeneratePrime(p, bitsP, function) || !Store(rsa.p, p, function)) return fail();
    } while (!CoprimeToExponent(p, exponent, function));
    for (;;) {
        unsigned int degenerate = 0;
        do {
            if (!GeneratePrime(q, bitsQ, function) || !Store(rsa.q, q, function)) return fail();
        } while (mbedtls_mpi_cmp_mpi(&p.value, &q.value) == 0 && ++degenerate < 3);
        if (degenerate == 3) {
            LibreSslPutError_nid_no_patch(RsaLibrary, AnyFunction, KeySizeTooSmall, __FILE__, __LINE__);
            return 0;
        }
        if (CoprimeToExponent(q, exponent, function)) break;
    }
    if (mbedtls_mpi_cmp_mpi(&p.value, &q.value) < 0) {
        std::swap(rsa.p, rsa.q);
        mbedtls_mpi_swap(&p.value, &q.value);
    }
    Mpi modulus, lessP, lessQ, order, privateExponent, exponentP, exponentQ, coefficient;
    Check(mbedtls_mpi_mul_mpi(&modulus.value, &p.value, &q.value), function);
    if (!Store(rsa.n, modulus, function)) return fail();
    Check(mbedtls_mpi_sub_int(&lessP.value, &p.value, 1), function);
    Check(mbedtls_mpi_sub_int(&lessQ.value, &q.value, 1), function);
    Check(mbedtls_mpi_mul_mpi(&order.value, &lessP.value, &lessQ.value), function);
    Check(mbedtls_mpi_inv_mod(&privateExponent.value, &exponent.value, &order.value), function);
    if (!Store(rsa.d, privateExponent, function)) return fail();
    Check(mbedtls_mpi_mod_mpi(&exponentP.value, &privateExponent.value, &lessP.value), function);
    if (!Store(rsa.dmp1, exponentP, function)) return fail();
    Check(mbedtls_mpi_mod_mpi(&exponentQ.value, &privateExponent.value, &lessQ.value), function);
    if (!Store(rsa.dmq1, exponentQ, function)) return fail();
    Check(mbedtls_mpi_inv_mod(&coefficient.value, &q.value, &p.value), function);
    if (!Store(rsa.iqmp, coefficient, function)) return fail();
    return 1;
}

}

extern "C" {

int APS5_VABI RSA_generate_key_ex_nid_postfix(LibreSslRsa* rsa, int bits, Bignum* exponent, void* callback) {
    if (!rsa) throw std::invalid_argument(std::string(__func__) + ": null RSA");
    if (!rsa->meth) throw std::invalid_argument(std::string(__func__) + ": null RSA method");
    if (rsa->meth->keygen) return reinterpret_cast<Keygen>(rsa->meth->keygen)(rsa, bits, exponent, callback);
    if (callback) NotImplemented_nid_no_patch("RSA_generate_key_ex: a BN_GENCB progress callback");
    return BuiltinKeygen(*rsa, bits, exponent, __func__);
}

}
