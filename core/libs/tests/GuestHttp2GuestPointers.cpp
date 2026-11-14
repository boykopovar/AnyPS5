#include "prx/libSceHttp2/include/Http2.hpp"
#include <cerrno>
#include <cstdint>
#include <cstdio>
#include <cstdlib>

namespace {

constexpr int nullPointer = static_cast<int>(0x817B1225);
constexpr int invalidValue = static_cast<int>(0x817B11FE);
constexpr int savedErrno = 71;

void Check(bool value, int line) {
    if (!value) {
        std::fprintf(stderr, "HTTP2 guest pointer check failed at line %d\n", line);
        std::abort();
    }
}
#define Require(value) Check((value), __LINE__)

#define RequireErrno(call, expected)  \
    do {                              \
        errno = savedErrno;           \
        Require((call) == (expected)); \
        Require(errno == savedErrno); \
    } while (0)

void APS5_VABI TextCallback(std::int32_t, const char*, std::uint32_t, void*) {}
void APS5_VABI DataCallback(std::int32_t, const void*, std::size_t, std::uint32_t, void*) {}
void APS5_VABI CloseCallback(std::int32_t, const std::uint16_t*, const char*, std::uint32_t, void*) {}
void APS5_VABI ErrorCallback(std::int32_t, std::int32_t, void*) {}

}

int main() {
    const int context = sceHttp2Init(1, 1, 0x10000, 4);
    Require(context > 0);
    const int tmpl = sceHttp2CreateTemplate(context, "agent", 3, 0);
    Require(tmpl > 0);
    const int box = sceHttp2CreateCookieBox(0x10000);
    Require(box > 0);

    auto* const unmapped = reinterpret_cast<void*>(0x10);
    int value = 0;

    RequireErrno(sceHttp2GetAuthEnabled(tmpl, static_cast<int*>(unmapped)), nullPointer);
    RequireErrno(sceHttp2GetAutoRedirect(tmpl, static_cast<int*>(unmapped)), nullPointer);
    RequireErrno(sceHttp2GetCookieBox(tmpl, static_cast<int*>(unmapped)), nullPointer);
    RequireErrno(sceHttp2GetCookieStats(box, static_cast<Http2CookieStats*>(unmapped)), invalidValue);
    RequireErrno(sceHttp2WebSocketCreateRequest(tmpl, "ws://127.0.0.1:8080/", TextCallback, DataCallback, CloseCallback, ErrorCallback, unmapped, 16, nullptr, nullptr), invalidValue);

    Require(sceHttp2GetAuthEnabled(tmpl, &value) == 0);
    Require(sceHttp2GetAutoRedirect(tmpl, &value) == 0);
    Http2CookieStats stats{};
    Require(sceHttp2GetCookieStats(box, &stats) == 0);

    std::puts("guest_http2_guest_pointers ok");
    return 0;
}
