#include "prx/libc/include/general/VabiMacros.hpp"

#include <Testing/Test.hpp>

#include <cstddef>
#include <cstdint>
#include <string>

struct SslMemoryPoolStats {
    std::size_t pool_size;
    std::size_t max_inuse_size;
    std::size_t current_inuse_size;
    std::int32_t reserved;
};

extern "C" {
int APS5_VABI sceSslInit_nid_postfix(std::size_t);
int APS5_VABI sceSslTerm_nid_postfix(int);
int APS5_VABI sceSslGetCaCerts(int, void*);
int APS5_VABI sceSslFreeCaCerts(int, void*);
int APS5_VABI sceSslGetMemoryPoolStats(int, SslMemoryPoolStats*);
}

namespace {

using Testing::Case;
using Testing::Require;
using Testing::RequireEqual;

constexpr int notFound = static_cast<int>(0x8095F004);
constexpr int invalidArg = static_cast<int>(0x8095177A);
constexpr std::size_t poolSize = 0x10000;

struct SslCaCerts {
    void* certs;
    std::size_t num;
    void* pool;
};

class SslContext {
public:
    SslContext() : id(sceSslInit_nid_postfix(poolSize)) {
        Require(id > 0, "sceSslInit returned a non-positive context id");
    }

    ~SslContext() {
        sceSslTerm_nid_postfix(id);
    }

    SslContext(const SslContext&) = delete;
    SslContext& operator=(const SslContext&) = delete;

    const int id;
};

void RequireEmpty(const SslCaCerts& certs, const char* message) {
    Require(certs.certs == nullptr, std::string(message) + ": certs pointer not cleared");
    RequireEqual(certs.num, std::size_t{0}, std::string(message) + ": certificate count");
    Require(certs.pool == nullptr, std::string(message) + ": pool pointer not cleared");
}

const Case poolStats{"GetMemoryPoolStats_FreshContext_ReportsPoolSizeAndNoUsage", [] {
    const SslContext context;
    SslMemoryPoolStats stats{1, 1, 1, 1};
    RequireEqual(sceSslGetMemoryPoolStats(context.id, &stats), 0, "get memory pool stats");
    RequireEqual(stats.pool_size, poolSize, "pool size");
    RequireEqual(stats.max_inuse_size, std::size_t{0}, "max in-use size");
    RequireEqual(stats.current_inuse_size, std::size_t{0}, "current in-use size");
    RequireEqual(stats.reserved, 0, "reserved field");
}};

const Case nullCerts{"CaCerts_NullOutput_ReturnsInvalidArgument", [] {
    const SslContext context;
    RequireEqual(sceSslGetCaCerts(context.id, nullptr), invalidArg, "get with a null output");
    RequireEqual(sceSslFreeCaCerts(context.id, nullptr), invalidArg, "free with a null output");
}};

const Case getCerts{"GetCaCerts_NoCertificates_ReturnsNotFoundAndClearsOutput", [] {
    const SslContext context;
    int marker = 0;
    SslCaCerts certs{&marker, 3, &marker};
    RequireEqual(sceSslGetCaCerts(context.id, &certs), notFound, "get certificates");
    RequireEmpty(certs, "after get");
}};

const Case freeCerts{"FreeCaCerts_PopulatedOutput_SucceedsAndClearsOutput", [] {
    const SslContext context;
    int marker = 0;
    SslCaCerts certs{&marker, 3, &marker};
    RequireEqual(sceSslFreeCaCerts(context.id, &certs), 0, "free certificates");
    RequireEmpty(certs, "after free");
}};

} // namespace
