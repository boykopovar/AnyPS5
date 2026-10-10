#include "prx/libc/include/general/VabiMacros.hpp"
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <stdexcept>
#include <string>

extern "C" {
int APS5_VABI sceSslInit_nid_postfix(std::size_t);
int APS5_VABI sceSslGetCaCerts(int, void*);
int APS5_VABI sceSslFreeCaCerts(int, void*);
int APS5_VABI sceSslLoadCert();
int APS5_VABI sceSslUnloadCert();
}

struct SslMemoryPoolStats {
    std::size_t pool_size;
    std::size_t max_inuse_size;
    std::size_t current_inuse_size;
    std::int32_t reserved;
};

extern "C" int APS5_VABI sceSslGetMemoryPoolStats(int, SslMemoryPoolStats*);

static void Require(bool value) { if (!value) std::abort(); }

struct SslCaCerts {
    void* certs;
    std::size_t num;
    void* pool;
};

int main() {
    constexpr int notFound = static_cast<int>(0x8095F004);
    constexpr int invalidArg = static_cast<int>(0x8095177A);
    int marker = 0;

    const int context = sceSslInit_nid_postfix(0x10000);
    Require(context > 0);
    SslMemoryPoolStats stats{1, 1, 1, 1};
    Require(sceSslGetMemoryPoolStats(context, &stats) == 0);
    Require(stats.pool_size == 0x10000 && stats.max_inuse_size == 0 && stats.current_inuse_size == 0 && stats.reserved == 0);
    Require(sceSslGetCaCerts(context, nullptr) == invalidArg);
    Require(sceSslFreeCaCerts(context, nullptr) == invalidArg);

    SslCaCerts certs{&marker, 3, &marker};
    Require(sceSslGetCaCerts(context, &certs) == notFound);
    Require(certs.certs == nullptr && certs.num == 0 && certs.pool == nullptr);

    certs = {&marker, 3, &marker};
    Require(sceSslFreeCaCerts(context, &certs) == 0);
    Require(certs.certs == nullptr && certs.num == 0 && certs.pool == nullptr);

    bool loadNotImplemented = false;
    try {
        sceSslLoadCert();
    } catch (const std::runtime_error& error) {
        loadNotImplemented = std::string(error.what()) == "sceSslLoadCert not implemented";
    }
    Require(loadNotImplemented);

    bool unloadNotImplemented = false;
    try {
        sceSslUnloadCert();
    } catch (const std::runtime_error& error) {
        unloadNotImplemented = std::string(error.what()) == "sceSslUnloadCert not implemented";
    }
    Require(unloadNotImplemented);
}
