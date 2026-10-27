#include "prx/libSceSsl/include/Ssl.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include "ProcessParametersFixture.hpp"
#include <array>
#include <cstring>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <exception>
#include <string>

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

int main() {
    static std::array<std::byte, 64> parameters{};
    const std::uint64_t processSize = parameters.size();
    const std::uint32_t processMagic = 0x4942524f;
    std::memcpy(parameters.data(), &processSize, sizeof(processSize));
    std::memcpy(parameters.data() + 8, &processMagic, sizeof(processMagic));
    Require(ProcessParametersFixture::Install(parameters.data(), parameters.size()));
    constexpr int invalidValue = static_cast<int>(0x8095F007);
    constexpr int notFound = static_cast<int>(0x8095F004);
    constexpr int invalidArg = static_cast<int>(0x8095177A);
    int marker = 0;
    const int context = sceSslInit_nid_postfix(0x10000);
    Require(context > 0);
    SceSsl::MemoryPoolStats stats{1, 1, 1, 1};
    Require(sceSslGetMemoryPoolStats(context, &stats) == 0);
    Require(stats.poolSize == 0x10000 && stats.maxInuseSize == 0 && stats.currentInuseSize == 0 && stats.reserved == 0);
    Require(sceSslGetCaCerts(context, nullptr) == invalidValue);
    Require(sceSslFreeCaCerts(context, nullptr) == invalidArg);

    SceSsl::Data certificateMarker{};
    SceSsl::CaCerts certs{&certificateMarker, 3, &marker};
    Require(sceSslGetCaCerts(context, &certs) == notFound);
    Require(certs.certs == nullptr && certs.num == 0 && certs.pool == nullptr);

    certs = {&certificateMarker, 3, &marker};
    Require(sceSslFreeCaCerts(context, &certs) == 0);
    Require(certs.certs == nullptr && certs.num == 0 && certs.pool == nullptr);

    Require(sceSslGetCaList(context, nullptr) == invalidValue);
    Require(sceSslFreeCaList(context, nullptr) == invalidArg);

    void* handle = &marker;
    SceSsl::CaList list{&handle, 3};
    Require(sceSslGetCaList(context, &list) == notFound);
    Require(list.certs == nullptr && list.num == 0);
    Require(sceSslFreeCaList(context, &list) == 0);
    Require(list.certs == nullptr && list.num == 0);

    SceSsl::CaList foreign{&handle, 1};
    Require(Throws([&] { sceSslFreeCaList(context, &foreign); }));
    foreign = {nullptr, 1};
    Require(Throws([&] { sceSslFreeCaList(context, &foreign); }));
    foreign = {&handle, 0};
    Require(Throws([&] { sceSslFreeCaList(context, &foreign); }));

    Require(sceSslUnloadCert(context) == 0);
    Require(sceSslGetCaList(context, &list) == notFound);
    Require(list.certs == nullptr && list.num == 0);

    constexpr int invalidId = static_cast<int>(0x8095F006);
    constexpr int badLength = static_cast<int>(0x80951773);
    constexpr int badDescriptor = static_cast<int>(0x80410109);
    Require(sceSslInit_nid_postfix(0) == static_cast<int>(0x80020016));
    Require(sceSslCreateConnection(context, 0, nullptr) == invalidValue);
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
    Require(sceSslEnableVerifyOption(connection, 0x40) == invalidValue);
    Require(sceSslDisableVerifyOption(connection, 0x1000) == invalidValue);
    Require(sceSslSetMinSslVersion(connection, 0) == invalidValue);
    Require(sceSslSetMinSslVersion(connection, 1) == invalidValue);
    Require(sceSslSetMinSslVersion(connection, 2) == 0);
    Require(sceSslSetMinSslVersion(connection, 5) == 0);
    Require(sceSslSetMinSslVersion(connection, -1) == invalidValue);
    Require(sceSslSetMinSslVersion(connection, 6) == invalidValue);
    Require(sceSslGetAlpnSelected(connection) == nullptr);
    Require(sceSslSetAlpn(connection, "http/1.1") == 0);
    Require(sceSslSetAlpn(connection, "") == badLength);
    Require(sceSslSetAlpn(connection, ",h2") == badLength);
    Require(sceSslSetAlpn(connection, "h2,") == badLength);
    Require(sceSslSetAlpn(connection, "h2,,http/1.1") == badLength);
    Require(sceSslSetAlpn(connection, std::string(255, 'a').c_str()) == 0);
    Require(sceSslSetAlpn(connection, std::string(256, 'a').c_str()) == badLength);
    Require(sceSslSetAlpn(connection, std::string(65535, 'a').c_str()) == badLength);
    Require(sceSslSetAlpn(0, nullptr) == invalidId);
    Require(sceSslSetAlpn(connection, nullptr) == 0);
    Require(sceSslGetAlpnSelected(connection) == nullptr);
    char buffer[4]{};
    Require(sceSslConnect(connection) == badDescriptor);
    Require(sceSslRead(connection, buffer, sizeof(buffer), 0) == badDescriptor);
    Require(sceSslWrite(connection, buffer, sizeof(buffer), 0) == badDescriptor);
    Require(sceSslWrite(connection, buffer, sizeof(buffer), 1) == invalidValue);
    Require(sceSslClose(connection) == 0);

    const int unknown = connection + 1000;
    Require(sceSslSetVerifyCallback(unknown, nullptr, nullptr) == invalidId);
    Require(sceSslEnableVerifyOption(unknown, serverVerify) == invalidId);
    Require(sceSslDisableVerifyOption(unknown, serverVerify) == invalidId);
    Require(sceSslSetMinSslVersion(unknown, 5) == invalidId);
    Require(sceSslSetAlpn(unknown, "http/1.1") == invalidId);
    Require(sceSslGetAlpnSelected(unknown) == nullptr);
    Require(sceSslConnect(unknown) == invalidId);
    Require(sceSslRead(unknown, buffer, sizeof(buffer), 0) == invalidId);
    Require(sceSslWrite(unknown, buffer, sizeof(buffer), 0) == invalidId);
    Require(sceSslClose(unknown) == invalidId);
    Require(sceSslDeleteConnection(unknown) == invalidId);

    Require(sceSslTerm_nid_postfix(context) == 0);
    Require(sceSslTerm_nid_postfix(context) == invalidId);
    Require(sceSslSetVerifyCallback(connection, nullptr, nullptr) == invalidId);
    Require(sceSslEnableVerifyOption(connection, serverVerify) == invalidId);
    Require(sceSslDisableVerifyOption(connection, serverVerify) == invalidId);
    Require(sceSslSetMinSslVersion(connection, 5) == invalidId);
    Require(sceSslSetAlpn(connection, "http/1.1") == invalidId);
    Require(sceSslGetAlpnSelected(connection) == nullptr);
    Require(sceSslConnect(connection) == invalidId);
    Require(sceSslClose(connection) == invalidId);
    Require(sceSslCreateConnection(context, 0, "example.com") == invalidId);
    Require(sceSslDeleteConnection(connection) == invalidId);

    Require(sceSslGetCaList(context, nullptr) == invalidValue);
    Require(sceSslFreeCaList(context, nullptr) == invalidArg);
    list = {&handle, 3};
    Require(sceSslGetCaList(context, &list) == invalidId);
    Require(list.certs == &handle && list.num == 3);
    Require(sceSslGetCaCerts(context, nullptr) == invalidId);
    certs = {&certificateMarker, 3, &marker};
    Require(sceSslGetCaCerts(context, &certs) == invalidId);
    Require(certs.certs == &certificateMarker && certs.num == 3 && certs.pool == &marker);
    Require(Throws([&] { sceSslFreeCaList(context, &list); }));
    for (const std::uint32_t sdkVersion : {0u, 0x074fffffu, 0x07500000u, 0x09000000u}) {
        std::memcpy(parameters.data() + 16, &sdkVersion, sizeof(sdkVersion));
        Require(sceSslUnloadCert(context) == (sdkVersion < 0x07500000 ? 0 : invalidId));
        Require(sceSslUnloadCert(0) == (sdkVersion < 0x07500000 ? 0 : invalidId));
    }
}
