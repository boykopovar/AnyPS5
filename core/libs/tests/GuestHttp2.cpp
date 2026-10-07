#include "SceTypes.hpp"
#include <cstdint>
#include <cstdlib>

extern "C" {
int APS5_VABI sceHttp2Init(int, int, std::size_t, int);
int APS5_VABI sceHttp2CreateTemplate(int, const char*, int, int);
int APS5_VABI sceHttp2CreateRequestWithURL(int, const char*, const char*, std::uint64_t);
int APS5_VABI sceHttp2CreateCookieBox(int);
int APS5_VABI sceHttp2SetCookieBox(int, int);
int APS5_VABI sceHttp2CookieFlush(int);
int APS5_VABI sceHttp2SetRequestNoContentLength(int);
int APS5_VABI sceHttp2SendRequest(int, const void*, std::size_t);
int APS5_VABI sceHttp2Term(int);
}

static void Require(bool value) {
    if (!value)
        std::abort();
}

int main() {
    const int context = sceHttp2Init(1, 1, 0x10000, 4);
    Require(context > 0);
    const int box = sceHttp2CreateCookieBox(context);
    const int other = sceHttp2CreateCookieBox(context);
    Require(box > 0 && other > 0 && box != other && box != context);
    const int tmpl = sceHttp2CreateTemplate(context, "agent", 2, 0);
    Require(tmpl > 0 && tmpl != box && tmpl != other);
    Require(sceHttp2SetCookieBox(tmpl, box) == 0);
    Require(sceHttp2SetCookieBox(tmpl, 0) == 0);
    const int request = sceHttp2CreateRequestWithURL(tmpl, "POST", "https://example.com/", 16);
    Require(request > 0);
    Require(sceHttp2SetCookieBox(request, other) == 0);
    Require(sceHttp2SetRequestNoContentLength(request) == 0);
    Require(sceHttp2CookieFlush(context) == 0);
    Require(sceHttp2SendRequest(request, nullptr, 0) == static_cast<int>(0x80436063));
    Require(sceHttp2Term(context) == 0);
}
