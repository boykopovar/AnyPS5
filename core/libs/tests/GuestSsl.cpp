#include "prx/libc/include/general/VabiMacros.hpp"
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <exception>

extern "C" {
int APS5_VABI sceSslInit_nid_postfix(std::size_t);
int APS5_VABI sceSslTerm_nid_postfix(int);
int APS5_VABI sceSslGetCaCerts(int, void*);
int APS5_VABI sceSslFreeCaCerts(int, void*);
int APS5_VABI sceSslGetCaList(int, void*);
int APS5_VABI sceSslFreeCaList(int, void*);
int APS5_VABI sceSslUnloadCert(int);
int APS5_VABI sceSslCreateConnection(int, int, const char*);
int APS5_VABI sceSslDeleteConnection(int);
int APS5_VABI sceSslSetVerifyCallback(int, int (APS5_VABI *)(int, unsigned int, void**, int, void*), void*);
int APS5_VABI sceSslConnect(int);
int APS5_VABI sceSslRead(int, void*, std::size_t, int);
int APS5_VABI sceSslWrite(int, void*, std::size_t, int);
int APS5_VABI sceSslEnableVerifyOption(int, std::uint32_t);
int APS5_VABI sceSslDisableVerifyOption(int, std::uint32_t);
int APS5_VABI sceSslSetMinSslVersion(int, int);
int APS5_VABI sceSslSetAlpn(int, const char*);
const char* APS5_VABI sceSslGetAlpnSelected(int);
}

struct SslMemoryPoolStats {
    std::size_t pool_size;
    std::size_t max_inuse_size;
    std::size_t current_inuse_size;
    std::int32_t reserved;
};

extern "C" int APS5_VABI sceSslGetMemoryPoolStats(int, SslMemoryPoolStats*);

static void Require(bool value) { if (!value) std::abort(); }

template <typename TCall>
static bool Throws(TCall call) {
    try {
        call();
    } catch (const std::exception&) {
        return true;
    }
    return false;
}

static int APS5_VABI VerifyCallback(int, unsigned int, void**, int, void*) { return 0; }

struct SslCaCerts {
    void* certs;
    std::size_t num;
    void* pool;
};

struct SslCaList {
    void** certs;
    int num;
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

    Require(sceSslGetCaList(context, nullptr) == invalidArg);
    Require(sceSslFreeCaList(context, nullptr) == invalidArg);

    void* handle = &marker;
    SslCaList list{&handle, 3};
    Require(sceSslGetCaList(context, &list) == notFound);
    Require(list.certs == nullptr && list.num == 0);
    Require(sceSslFreeCaList(context, &list) == 0);
    Require(list.certs == nullptr && list.num == 0);

    SslCaList foreign{&handle, 1};
    Require(Throws([&] { sceSslFreeCaList(context, &foreign); }));
    foreign = {nullptr, 1};
    Require(Throws([&] { sceSslFreeCaList(context, &foreign); }));
    foreign = {&handle, 0};
    Require(Throws([&] { sceSslFreeCaList(context, &foreign); }));

    Require(sceSslUnloadCert(context) == 0);
    Require(sceSslGetCaList(context, &list) == notFound);
    Require(list.certs == nullptr && list.num == 0);

    constexpr int invalidId = static_cast<int>(0x8095F006);
    Require(Throws([&] { sceSslCreateConnection(context, 0, nullptr); }));
    Require(sceSslCreateConnection(context + 1000, 0, "example.com") == invalidId);

    const int connection = sceSslCreateConnection(context, 0, "example.com");
    Require(connection > 0 && connection != context);
    Require(sceSslSetVerifyCallback(connection, VerifyCallback, &marker) == 0);
    Require(sceSslSetVerifyCallback(connection, nullptr, nullptr) == 0);
    constexpr std::uint32_t serverVerify = 0x1;
    constexpr std::uint32_t cnCheck = 0x4;
    Require(sceSslEnableVerifyOption(connection, serverVerify | cnCheck) == 0);
    Require(sceSslDisableVerifyOption(connection, cnCheck) == 0);
    Require(sceSslDisableVerifyOption(connection, serverVerify | cnCheck) == 0);
    Require(sceSslSetMinSslVersion(connection, 0) == 0);
    Require(sceSslSetMinSslVersion(connection, 5) == 0);
    Require(Throws([&] { sceSslSetMinSslVersion(connection, -1); }));
    Require(Throws([&] { sceSslSetMinSslVersion(connection, 6); }));
    Require(sceSslGetAlpnSelected(connection) == nullptr);
    Require(sceSslSetAlpn(connection, "http/1.1") == 0);
    Require(sceSslSetAlpn(connection, "") == 0);
    Require(Throws([&] { sceSslSetAlpn(connection, nullptr); }));
    Require(sceSslGetAlpnSelected(connection) == nullptr);
    char buffer[4]{};
    Require(Throws([&] { sceSslConnect(connection); }));
    Require(Throws([&] { sceSslRead(connection, buffer, sizeof(buffer), 0); }));
    Require(Throws([&] { sceSslWrite(connection, buffer, sizeof(buffer), 0); }));

    const int unknown = connection + 1000;
    Require(Throws([&] { sceSslSetVerifyCallback(unknown, nullptr, nullptr); }));
    Require(Throws([&] { sceSslEnableVerifyOption(unknown, serverVerify); }));
    Require(Throws([&] { sceSslDisableVerifyOption(unknown, serverVerify); }));
    Require(Throws([&] { sceSslSetMinSslVersion(unknown, 5); }));
    Require(Throws([&] { sceSslSetAlpn(unknown, "http/1.1"); }));
    Require(Throws([&] { sceSslGetAlpnSelected(unknown); }));
    Require(Throws([&] { sceSslDeleteConnection(unknown); }));

    Require(sceSslTerm_nid_postfix(context) == 0);
    Require(Throws([&] { sceSslSetVerifyCallback(connection, nullptr, nullptr); }));
    Require(Throws([&] { sceSslEnableVerifyOption(connection, serverVerify); }));
    Require(Throws([&] { sceSslDisableVerifyOption(connection, serverVerify); }));
    Require(Throws([&] { sceSslSetMinSslVersion(connection, 5); }));
    Require(Throws([&] { sceSslSetAlpn(connection, "http/1.1"); }));
    Require(Throws([&] { sceSslGetAlpnSelected(connection); }));
    Require(sceSslCreateConnection(context, 0, "example.com") == invalidId);
    Require(sceSslDeleteConnection(connection) == 0);
    Require(Throws([&] { sceSslDeleteConnection(connection); }));

    Require(sceSslGetCaList(context, nullptr) == invalidArg);
    Require(sceSslFreeCaList(context, nullptr) == invalidArg);
    list = {&handle, 3};
    Require(Throws([&] { sceSslGetCaList(context, &list); }));
    Require(Throws([&] { sceSslFreeCaList(context, &list); }));
    Require(Throws([&] { sceSslUnloadCert(context); }));
}
