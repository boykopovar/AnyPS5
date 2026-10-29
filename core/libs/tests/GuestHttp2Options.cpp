#include "prx/libSceHttp2/include/Http2.hpp"
#include "SceTypes.hpp"
#include <cstdint>
#include <cstdlib>
#include <stdexcept>

static void Require(bool value) { if (!value) std::abort(); }

static constexpr int InvalidId = static_cast<int>(0x817B1100);
static constexpr int NotFound = static_cast<int>(0x817B1025);
static constexpr int InvalidValue = static_cast<int>(0x817B11FE);
static constexpr int HttpInvalidValue = static_cast<int>(0x804311FE);

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
    int value = -2;
    Require(sceHttp2GetAuthEnabled(id, &value) == 0);
    return value;
}

static int AutoRedirect(int id) {
    int value = -2;
    Require(sceHttp2GetAutoRedirect(id, &value) == 0);
    return value;
}

int main() {
    const int context = sceHttp2Init(1, 1, 0x10000, 4);
    Require(context > 0);
    const int tmpl = sceHttp2CreateTemplate(context, "agent", 3, 0);
    Require(tmpl > 0);
    const int box = sceHttp2CreateCookieBox(0x10000);
    Require(box > 0);
    Require(AuthEnabled(tmpl) == 1 && AutoRedirect(tmpl) == 1);
    Require(sceHttp2SetAuthEnabled(tmpl, 0) == 0);
    Require(sceHttp2SetAutoRedirect(tmpl, 0) == 0);
    Require(AuthEnabled(tmpl) == 0 && AutoRedirect(tmpl) == 0);

    int tag = 0;
    Require(sceHttp2SetAuthInfoCallback(tmpl, nullptr, &tag) == 0);
    Require(sceHttp2SetPreSendCallback(tmpl, &tag, &tag) == 0);
    Require(sceHttp2AddRequestHeader(tmpl, "X-Test", "one", 0) == 0);
    Require(sceHttp2AddRequestHeader(tmpl, "x-test", "two", 1) == 0);

    const int request = sceHttp2CreateRequestWithURL(tmpl, "GET", "https://localhost/", 0);
    Require(request > 0);
    Require(AuthEnabled(request) == 0 && AutoRedirect(request) == 0);
    Require(sceHttp2SetAuthEnabled(request, 7) == 0);
    Require(sceHttp2SetAutoRedirect(request, 7) == 0);
    Require(AuthEnabled(request) == 1 && AutoRedirect(request) == 7);
    Require(sceHttp2SetAutoRedirect(request, -1) == 0);
    Require(AutoRedirect(request) == -1);
    Require(AuthEnabled(tmpl) == 0 && AutoRedirect(tmpl) == 0);

    Require(sceHttp2SetAuthEnabled(tmpl, 1) == 0);
    Require(sceHttp2AddRequestHeader(tmpl, "X-Late", "1", 0) == 0);
    Require(AuthEnabled(request) == 1);
    Require(sceHttp2RemoveRequestHeader(request, "X-Late") == NotFound);
    const int later = sceHttp2CreateRequestWithURL(tmpl, "GET", "https://localhost/", 0);
    Require(later > 0);
    Require(AuthEnabled(later) == 1 && AutoRedirect(later) == 0);
    Require(sceHttp2RemoveRequestHeader(later, "x-late") == 0);
    Require(sceHttp2RemoveRequestHeader(tmpl, "X-LATE") == 0);
    Require(sceHttp2SetAuthEnabled(tmpl, 0) == 0);
    Require(AuthEnabled(request) == 1 && AuthEnabled(later) == 1);

    Require(sceHttp2RemoveRequestHeader(request, "X-TEST") == 0);
    Require(sceHttp2RemoveRequestHeader(request, "x-test") == NotFound);
    Require(sceHttp2RemoveRequestHeader(later, "X-Test") == 0);
    Require(sceHttp2RemoveRequestHeader(tmpl, "X-Test") == 0);
    Require(sceHttp2RemoveRequestHeader(request, "absent") == NotFound);
    Require(sceHttp2RemoveRequestHeader(request, nullptr) == NotFound);
    Require(sceHttp2RemoveRequestHeader(0, nullptr) == NotFound);

    Require(sceHttp2AddRequestHeader(request, "A", "1", 1) == 0);
    Require(sceHttp2AddRequestHeader(request, "a", "2", 1) == 0);
    Require(sceHttp2AddRequestHeader(request, "A", "3", 0) == 0);
    Require(sceHttp2RemoveRequestHeader(request, "a") == 0);
    Require(sceHttp2RemoveRequestHeader(request, "a") == NotFound);
    Require(sceHttp2AddRequestHeader(0, nullptr, nullptr, 2) == InvalidValue);
    Require(sceHttp2AddRequestHeader(0, "connection", "close", 0) == HttpInvalidValue);
    Require(sceHttp2AddRequestHeader(request, "Connection", "close", 1) == HttpInvalidValue);
    Require(sceHttp2RemoveRequestHeader(request, "Connection") == NotFound);
    Require(sceHttp2AddRequestHeader(request, nullptr, "value", 0) == InvalidValue);
    Require(sceHttp2AddRequestHeader(request, "B", "1", 0) == 0);
    Require(sceHttp2AddRequestHeader(request, "B", nullptr, 0) == InvalidValue);
    Require(sceHttp2RemoveRequestHeader(request, "B") == NotFound);
    Require(sceHttp2AddRequestHeader(0, nullptr, "value", 0) == InvalidId);

    Require(sceHttp2SetAuthInfoCallback(request, nullptr, nullptr) == 0);
    Require(sceHttp2SetPreSendCallback(request, nullptr, nullptr) == 0);
    Require(sceHttp2GetAutoRedirect(request, nullptr) == InvalidValue);
    Require(sceHttp2GetAutoRedirect(0, nullptr) == InvalidId);
    Require(sceHttp2GetAuthEnabled(0, nullptr) == InvalidId);
    Require(Throws([&] { sceHttp2GetAuthEnabled(request, nullptr); }));

    Require(sceHttp2AuthCacheFlush(context) == 0);
    Require(sceHttp2AuthCacheFlush(tmpl) == InvalidId);
    Require(sceHttp2AuthCacheFlush(request) == InvalidId);
    Require(sceHttp2RedirectCacheFlush(context) == 0);
    Require(sceHttp2RedirectCacheFlush(request) == 0);
    Require(sceHttp2RedirectCacheFlush(0x7fffffff) == 0);

    const int sent = sceHttp2CreateRequestWithURL(tmpl, "GET", "https://127.0.0.1:1/", 0);
    Require(sent > 0);
    Require(sceHttp2AddRequestHeader(sent, "X-Sent", "1", 0) == 0);
    try {
        sceHttp2SendRequest(sent, nullptr, 0);
    } catch (const std::exception&) {
    }
    Require(AuthEnabled(sent) == 0 && AutoRedirect(sent) == 0);
    Require(Throws([&] { sceHttp2SetAuthEnabled(sent, 1); }));
    Require(Throws([&] { sceHttp2SetAutoRedirect(sent, 1); }));
    Require(Throws([&] { sceHttp2AddRequestHeader(sent, "X-Other", "1", 0); }));
    Require(Throws([&] { sceHttp2RemoveRequestHeader(sent, "X-Sent"); }));
    Require(Throws([&] { sceHttp2SetAuthInfoCallback(sent, nullptr, nullptr); }));
    Require(Throws([&] { sceHttp2SetPreSendCallback(sent, nullptr, nullptr); }));
    Require(sceHttp2DeleteRequest(sent) == 0);

    Require(sceHttp2DeleteRequest(request) == 0);
    const int invalidIds[] = {0, -1, 0x7fffffff, request, context, box};
    for (const int invalid : invalidIds) {
        int value = -2;
        Require(sceHttp2SetAuthEnabled(invalid, 1) == InvalidId);
        Require(sceHttp2SetAutoRedirect(invalid, 1) == InvalidId);
        Require(sceHttp2GetAuthEnabled(invalid, &value) == InvalidId && value == -2);
        Require(sceHttp2GetAutoRedirect(invalid, &value) == InvalidId && value == -2);
        Require(sceHttp2AddRequestHeader(invalid, "X-Test", "value", 0) == InvalidId);
        Require(sceHttp2RemoveRequestHeader(invalid, "X-Test") == InvalidId);
        Require(sceHttp2SetAuthInfoCallback(invalid, nullptr, nullptr) == InvalidId);
        Require(sceHttp2SetPreSendCallback(invalid, nullptr, nullptr) == InvalidId);
        if (invalid != context) Require(sceHttp2AuthCacheFlush(invalid) == InvalidId);
        Require(sceHttp2RedirectCacheFlush(invalid) == 0);
    }

    Require(sceHttp2DeleteCookieBox(box) == 0);
    Require(sceHttp2Term(context) == 0);
    Require(sceHttp2AuthCacheFlush(context) == InvalidId);
    Require(sceHttp2SetAuthEnabled(tmpl, 1) == InvalidId);
}
