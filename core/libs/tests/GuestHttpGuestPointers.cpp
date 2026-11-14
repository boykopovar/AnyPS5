#include "prx/libSceHttp/include/Http.hpp"
#include "ProcessParametersFixture.hpp"
#include <array>
#include <cerrno>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace {

constexpr int invalidValue = static_cast<int>(0x804311FE);
constexpr int savedErrno = 71;

void Check(bool value, int line) {
    if (!value) {
        std::fprintf(stderr, "HTTP guest pointer check failed at line %d\n", line);
        std::abort();
    }
}
#define Require(value) Check((value), __LINE__)

#define RequireErrno(call, expected)       \
    do {                                    \
        errno = savedErrno;                 \
        Require((call) == (expected));      \
        Require(errno == savedErrno);       \
    } while (0)

alignas(16) std::array<std::byte, 0x40> processParameters{};

}

int main() {
    const std::uint64_t processSize = processParameters.size();
    const std::uint32_t processMagic = 0x4942524f;
    const std::uint32_t sdkVersion = 0x1000000;
    std::memcpy(processParameters.data(), &processSize, sizeof(processSize));
    std::memcpy(processParameters.data() + 8, &processMagic, sizeof(processMagic));
    std::memcpy(processParameters.data() + 16, &sdkVersion, sizeof(sdkVersion));
    Require(ProcessParametersFixture::Install(processParameters.data(), processParameters.size()));

    const int context = sceHttpInit_nid_postfix(0, 0, 0x10000);
    Require(context > 0);
    const int tmpl = sceHttpCreateTemplate(context, "guest-pointer-test", 1, 0);
    Require(tmpl > 0);
    const int conn = sceHttpCreateConnectionWithURL(tmpl, "http://127.0.0.1:8080", 0);
    Require(conn > 0);

    auto* const unmapped = reinterpret_cast<void*>(0x10);
    int value = 0;
    int other = 0;

    RequireErrno(sceHttpGetMemoryPoolStats(context, static_cast<HttpMemoryPoolStats*>(unmapped)), invalidValue);
    RequireErrno(sceHttpsGetCaList(context, static_cast<HttpsCaList*>(unmapped)), invalidValue);
    RequireErrno(sceHttpsFreeCaList(context, static_cast<HttpsCaList*>(unmapped)), invalidValue);
    RequireErrno(sceHttpGetAuthEnabled(tmpl, static_cast<int*>(unmapped)), invalidValue);
    RequireErrno(sceHttpGetCookieEnabled(tmpl, static_cast<int*>(unmapped)), invalidValue);
    RequireErrno(sceHttpGetRegisteredCtxIds(context, static_cast<int*>(unmapped), &other), invalidValue);
    RequireErrno(sceHttpGetRegisteredCtxIds(context, &value, static_cast<int*>(unmapped)), invalidValue);
    RequireErrno(sceHttpGetConnectionStat(conn, static_cast<HttpConnectionStat*>(unmapped)), invalidValue);
    RequireErrno(sceHttpGetAutoRedirect(tmpl, static_cast<int*>(unmapped)), invalidValue);
    RequireErrno(sceHttpGetNonblock(tmpl, static_cast<int*>(unmapped)), invalidValue);
    RequireErrno(sceHttpGetAcceptEncodingGZIPEnabled(tmpl, static_cast<int*>(unmapped)), invalidValue);
    RequireErrno(sceHttpGetEpoll(tmpl, static_cast<HttpEpollHandle*>(unmapped), nullptr), invalidValue);
    RequireErrno(sceHttpGetCookieStats(context, static_cast<HttpCookieStats*>(unmapped)), invalidValue);

    Require(sceHttpGetAutoRedirect(tmpl, &value) == 0);
    Require(sceHttpGetAuthEnabled(tmpl, &value) == 0);
    HttpConnectionStat stat{};
    Require(sceHttpGetConnectionStat(conn, &stat) == 0);
    HttpCookieStats cookieStats{};
    Require(sceHttpGetCookieStats(context, &cookieStats) == 0);

    std::puts("guest_http_guest_pointers ok");
    return 0;
}
