#include "SceTypes.hpp"
#include <cstdint>
#include <cstdlib>
#include <stdexcept>

extern "C" {
int APS5_VABI sceHttp2Init(int, int, std::size_t, int);
int APS5_VABI sceHttp2Term(int);
int APS5_VABI sceHttp2CreateTemplate(int, const char*, int, int);
int APS5_VABI sceHttp2DeleteTemplate(int);
int APS5_VABI sceHttp2CreateRequestWithURL(int, const char*, const char*, std::uint64_t);
int APS5_VABI sceHttp2DeleteRequest(int);
int APS5_VABI sceHttp2AddRequestHeader(int, const char*, const char*, std::uint32_t);
int APS5_VABI sceHttp2RemoveRequestHeader(int, const char*);
int APS5_VABI sceHttp2SetAuthEnabled(int, int);
int APS5_VABI sceHttp2GetAuthEnabled(int, int*);
int APS5_VABI sceHttp2SetAutoRedirect(int, int);
int APS5_VABI sceHttp2GetAutoRedirect(int, int*);
int APS5_VABI sceHttp2AuthCacheFlush(int);
int APS5_VABI sceHttp2RedirectCacheFlush(int);
int APS5_VABI sceHttp2SetAuthInfoCallback(int, void*, void*);
int APS5_VABI sceHttp2SetPreSendCallback(int, void*, void*);
}

static void Require(bool value) { if (!value) std::abort(); }

static constexpr int InvalidId = static_cast<int>(0x817B1100);

template <typename TCall>
static bool Throws(TCall call) {
    try {
        call();
    } catch (const std::exception&) {
        return true;
    }
    return false;
}

static int AuthEnabled(int id) {
    int value = -1;
    Require(sceHttp2GetAuthEnabled(id, &value) == 0);
    return value;
}

static int AutoRedirect(int id) {
    int value = -1;
    Require(sceHttp2GetAutoRedirect(id, &value) == 0);
    return value;
}

int main() {
    const int context = sceHttp2Init(1, 1, 0x10000, 4);
    Require(context > 0);
    const int tmpl = sceHttp2CreateTemplate(context, "agent", 3, 0);
    Require(tmpl > 0);
    Require(AuthEnabled(tmpl) == 1 && AutoRedirect(tmpl) == 1);
    Require(sceHttp2SetAuthEnabled(tmpl, 0) == 0);
    Require(sceHttp2SetAutoRedirect(tmpl, 0) == 0);
    Require(AuthEnabled(tmpl) == 0 && AutoRedirect(tmpl) == 0);

    int tag = 0;
    Require(sceHttp2SetAuthInfoCallback(tmpl, &tag, &tag) == 0);
    Require(sceHttp2SetPreSendCallback(tmpl, &tag, &tag) == 0);
    Require(sceHttp2AddRequestHeader(tmpl, "X-Test", "one", 0) == 0);
    Require(sceHttp2AddRequestHeader(tmpl, "x-test", "two", 1) == 0);

    const int request = sceHttp2CreateRequestWithURL(tmpl, "GET", "https://example.com/", 0);
    Require(request > 0);
    Require(AuthEnabled(request) == 0 && AutoRedirect(request) == 0);
    Require(sceHttp2SetAuthEnabled(request, 7) == 0);
    Require(sceHttp2SetAutoRedirect(request, 1) == 0);
    Require(AuthEnabled(request) == 1 && AutoRedirect(request) == 1);
    Require(AuthEnabled(tmpl) == 0 && AutoRedirect(tmpl) == 0);
    Require(sceHttp2RemoveRequestHeader(request, "X-TEST") == 0);
    Require(sceHttp2RemoveRequestHeader(request, "absent") == 0);
    Require(sceHttp2SetAuthInfoCallback(request, nullptr, nullptr) == 0);
    Require(sceHttp2SetPreSendCallback(request, nullptr, nullptr) == 0);

    Require(Throws([&] { sceHttp2GetAuthEnabled(request, nullptr); }));
    Require(Throws([&] { sceHttp2GetAutoRedirect(request, nullptr); }));
    Require(Throws([&] { sceHttp2RemoveRequestHeader(request, nullptr); }));
    Require(Throws([&] { sceHttp2AddRequestHeader(request, nullptr, "value", 0); }));
    Require(Throws([&] { sceHttp2AddRequestHeader(request, "X-Test", "value", 2); }));

    Require(sceHttp2AuthCacheFlush(context) == 0);
    Require(sceHttp2RedirectCacheFlush(context) == 0);
    Require(sceHttp2AuthCacheFlush(tmpl) == InvalidId);
    Require(sceHttp2RedirectCacheFlush(request) == InvalidId);

    Require(sceHttp2DeleteRequest(request) == 0);
    const int invalidIds[] = {0, -1, 0x7fffffff, request};
    for (const int invalid : invalidIds) {
        int value = -1;
        Require(sceHttp2SetAuthEnabled(invalid, 1) == InvalidId);
        Require(sceHttp2SetAutoRedirect(invalid, 1) == InvalidId);
        Require(sceHttp2GetAuthEnabled(invalid, &value) == InvalidId && value == -1);
        Require(sceHttp2GetAutoRedirect(invalid, &value) == InvalidId && value == -1);
        Require(sceHttp2AddRequestHeader(invalid, "X-Test", "value", 0) == InvalidId);
        Require(sceHttp2RemoveRequestHeader(invalid, "X-Test") == InvalidId);
        Require(sceHttp2SetAuthInfoCallback(invalid, nullptr, nullptr) == InvalidId);
        Require(sceHttp2SetPreSendCallback(invalid, nullptr, nullptr) == InvalidId);
        Require(sceHttp2AuthCacheFlush(invalid) == InvalidId);
        Require(sceHttp2RedirectCacheFlush(invalid) == InvalidId);
    }

    Require(sceHttp2Term(context) == 0);
    Require(sceHttp2AuthCacheFlush(context) == InvalidId);
    Require(sceHttp2SetAuthEnabled(tmpl, 1) == InvalidId);
}
