#include "prx/libkernel/include/Api.hpp"
#include "prx/libSceHttp2/include/Http2.hpp"
#include "SceTypes.hpp"
#include <cstdint>
#include <cstdlib>
#include <stdexcept>
#include <initializer_list>

static void Require(bool value) { if (!value) std::abort(); }

static constexpr int InvalidId = static_cast<int>(0x817B1100);

template <typename TCall>
static bool Throws(TCall call) {
    try { call(); } catch (const std::exception&) { return true; }
    return false;
}

int main() {
    Require(sceHttp2CreateCookieBox(0xffff) == static_cast<int>(0x817b121du));
    const int box = sceHttp2CreateCookieBox(0x10000);
    Require(sceHttp2Init(1, 1, 0x10000, 0) == static_cast<int>(0x817B11FE));
    Require(Throws([] { sceHttp2Init(1, 1, 0x10000, -1); }));
    const int context = sceHttp2Init(1, 1, 0x10000, 4);
    const int tmpl = sceHttp2CreateTemplate(context, "agent", 3, 0);
    const int proxyTemplate = sceHttp2CreateTemplate(context, "agent", 3, 1);
    Require(proxyTemplate > 0 && sceHttp2DeleteTemplate(proxyTemplate) == 0);
    Require(Throws([&] { sceHttp2CreateTemplate(context, "agent", 3, 2); }));
    Require(Throws([&] { sceHttp2CreateTemplate(context, "agent", 3, -1); }));
    const int request = sceHttp2CreateRequestWithURL(tmpl, "GET", "https://localhost/", 0);
    Require(box > 0 && context > 0 && tmpl > 0 && request > 0);
    Require(sceHttp2SetCookieBox(tmpl, 0) == 0);
    Require(Throws([&] { sceHttp2SetCookieBox(tmpl, box); }));
    Require(sceHttp2CookieFlush(box) == 0);
    Require(sceHttp2SetResolveRetry(tmpl, 0) == 0);
    Require(sceHttp2SetResolveRetry(tmpl, 3) == 0);
    Require(Throws([&] { sceHttp2SetResolveRetry(tmpl, -1); }));
    Require(sceHttp2SetRequestNoContentLength(request) == 0);
    Require(Throws([&] { sceHttp2AddRequestHeader(request, "bad:name", "value", 0); }));
    Require(Throws([&] { sceHttp2AddRequestHeader(request, "x-header", "value\r\ninjected: yes", 0); }));
    Require(Throws([&] { sceHttp2AddRequestHeader(request, "keep-alive", "timeout=5", 0); }));
    Require(sceHttp2AddRequestHeader(request, "connection", "keep-alive", 0) == static_cast<int>(0x804311FE));
    Require(Throws([] { sceHttp2WebSocketSetPingInterval(0, 1000000); }));
    for (const int id : {tmpl, request}) {
        Require(Throws([&] { sceHttp2SetMinSslVersion(id, 0); }));
        Require(sceHttp2SslEnableOption(id, 0x3d) == 0);
        Require(sceHttp2SslDisableOption(id, 0x20) == 0);
        Require(Throws([&] { sceHttp2SetMinSslVersion(id, 99); }));
        Require(Throws([&] { sceHttp2SslEnableOption(id, 0x80000000); }));
    }
    for (const int id : {context, box}) {
        Require(Throws([&] { sceHttp2SslEnableOption(id, 0x3d); }));
        Require(Throws([&] { sceHttp2SslDisableOption(id, 0x20); }));
    }
    Http2MemoryPoolStats stats{1, 2, 3, 4};
    Require(sceHttp2GetMemoryPoolStats(context, &stats) == 0);
    Require(stats.pool_size == 0x10000 && stats.max_inuse_size == 0 && stats.current_inuse_size == 0 && stats.reserved == 0);
    KernelEqueue eq = 0;
    Require(sceKernelCreateEqueue(&eq, "http2-unsupported-async") == 0);
    Require(sceKernelAddUserEventEdge(eq, request) == 0);
    Http2AsyncOption option{};
    option.equeue = eq;
    option.user_event_id = request;
    Require(Throws([&] { sceHttp2SendRequestAsync(request, nullptr, 0, &option, nullptr); }));
    Http2AsyncResult result{};
    result.req_id = 123;
    std::uint32_t timeout = 500;
    Require(Throws([&] { sceHttp2WaitAsync(request, &result, &timeout, nullptr); }));
    Require(result.req_id == 123 && timeout == 500);
    KernelEvent event{};
    int count = 123;
    const KernelUseconds poll = 0;
    Require(sceKernelWaitEqueue(eq, &event, 1, &count, &poll) != 0);
    Require(sceKernelDeleteEqueue(eq) == 0);
    const int sibling = sceHttp2CreateTemplate(context, "other", 2, 0);
    const int child = sceHttp2CreateRequestWithURL(sibling, "GET", "https://localhost/", 0);
    Require(Throws([&] { sceHttp2SendRequest(child, nullptr, 0); }));
    Require(sceHttp2DeleteTemplate(tmpl) == 0);
    Require(sceHttp2SslEnableOption(sibling, 0) == 0);
    Require(sceHttp2DeleteRequest(request) == InvalidId);
    Require(sceHttp2SendRequest(request, nullptr, 0) == InvalidId);
    Require(sceHttp2Term(context) == 0);
    Require(sceHttp2DeleteRequest(child) == InvalidId);
    Require(sceHttp2DeleteTemplate(sibling) == InvalidId);
    Require(sceHttp2SetMinSslVersion(context, 0) == InvalidId);
    Require(sceHttp2DeleteCookieBox(box) == 0);
    Require(sceHttp2DeleteCookieBox(box) == InvalidId);
    Require(sceHttp2Term(context) == InvalidId);
}
