#include "prx/libkernel/include/Api.hpp"
#include "prx/libSceHttp2/include/Http2.hpp"
#include "SceTypes.hpp"
#include <array>
#include <cstdint>
#include <cstdlib>
#include <stdexcept>
#include <initializer_list>

static void Require(bool value) { if (!value) std::abort(); }

static constexpr int InvalidId = static_cast<int>(0x817B1100);
static constexpr int InvalidValue = static_cast<int>(0x817B11FE);
static constexpr int NotConnected = static_cast<int>(0x817B1065);
static constexpr int UnsupportedScheme = static_cast<int>(0x817B1061);
static constexpr int InvalidUrl = static_cast<int>(0x80433060);

template <typename TCall>
static bool Throws(TCall call) {
    try { call(); } catch (const std::exception&) { return true; }
    return false;
}

static void APS5_VABI WebSocketText(std::int32_t, const char*, std::uint32_t, void*) {}
static void APS5_VABI WebSocketData(std::int32_t, const void*, std::size_t, std::uint32_t, void*) {}
static void APS5_VABI WebSocketClose(std::int32_t, const std::uint16_t*, const char*, std::uint32_t, void*) {}
static void APS5_VABI WebSocketError(std::int32_t, std::int32_t, void*) {}

static std::array<std::uint8_t, 64> g_receive;

static bool ReceiveIs(std::uint8_t value) {
    for (const std::uint8_t byte : g_receive) {
        if (byte != value) return false;
    }
    return true;
}

static int CreateWebSocket(int tmpl, const char* url) {
    g_receive.fill(0xcc);
    return sceHttp2WebSocketCreateRequest(tmpl, url, WebSocketText, WebSocketData, WebSocketClose, WebSocketError, g_receive.data(), g_receive.size(), nullptr, nullptr);
}

static void RequireNullMessageRejected(int id) {
    const Http2AsyncOption asyncOption{};
    Require(sceHttp2WebSocketSendTextMessageAsync(id, nullptr, 0, nullptr, nullptr) == InvalidValue);
    Require(sceHttp2WebSocketSendTextMessageAsync(id, nullptr, 0xff, &asyncOption, nullptr) == InvalidValue);
    Require(sceHttp2WebSocketSendDataMessageAsync(id, nullptr, 2, 0, nullptr, nullptr) == InvalidValue);
    Require(sceHttp2WebSocketSendDataMessageAsync(id, nullptr, 0, 0xff, &asyncOption, nullptr) == InvalidValue);
}

static void RequireNotWebSocket(int id) {
    const std::uint16_t status = 1000;
    RequireNullMessageRejected(id);
    Require(sceHttp2WebSocketSetPingInterval(id, 1000000) == InvalidId);
    Require(sceHttp2WebSocketSetPingTimeout(id, 1000000) == InvalidId);
    Require(sceHttp2WebSocketSendTextMessageAsync(id, "hi", 0, nullptr, nullptr) == InvalidId);
    Require(sceHttp2WebSocketSendDataMessageAsync(id, "hi", 2, 0, nullptr, nullptr) == InvalidId);
    Require(sceHttp2WebSocketCloseAsync(id, &status, "bye", 0, nullptr, nullptr) == InvalidId);
}

static void TestWebSockets() {
    const int context = sceHttp2Init(1, 1, 0x10000, 4);
    const int tmpl = sceHttp2CreateTemplate(context, "agent", 2, 0);
    Require(context > 0 && tmpl > 0);
    const char* url = "ws://example.com/socket";
    const int marker = 0;
    const auto create = [&](Http2WebSocketTextCallback text, Http2WebSocketDataCallback data, Http2WebSocketCloseCallback close, Http2WebSocketErrorCallback error, void* buffer, std::size_t size, const void* option) {
        g_receive.fill(0xcc);
        return sceHttp2WebSocketCreateRequest(tmpl, url, text, data, close, error, buffer, size, nullptr, option);
    };
    Require(create(nullptr, WebSocketData, WebSocketClose, WebSocketError, g_receive.data(), g_receive.size(), nullptr) == InvalidValue && ReceiveIs(0xcc));
    Require(create(WebSocketText, nullptr, WebSocketClose, WebSocketError, g_receive.data(), g_receive.size(), nullptr) == InvalidValue && ReceiveIs(0xcc));
    Require(create(WebSocketText, WebSocketData, nullptr, WebSocketError, g_receive.data(), g_receive.size(), nullptr) == InvalidValue && ReceiveIs(0xcc));
    Require(create(WebSocketText, WebSocketData, WebSocketClose, nullptr, g_receive.data(), g_receive.size(), nullptr) == InvalidValue && ReceiveIs(0xcc));
    Require(create(WebSocketText, WebSocketData, WebSocketClose, WebSocketError, nullptr, g_receive.size(), nullptr) == InvalidValue && ReceiveIs(0xcc));
    Require(create(WebSocketText, WebSocketData, WebSocketClose, WebSocketError, g_receive.data(), 0, nullptr) == InvalidValue && ReceiveIs(0xcc));
    Require(create(WebSocketText, WebSocketData, WebSocketClose, WebSocketError, g_receive.data(), g_receive.size(), &marker) == InvalidValue && ReceiveIs(0xcc));
    Require(CreateWebSocket(0x7777, url) == InvalidId && ReceiveIs(0));
    Require(CreateWebSocket(0x7777, nullptr) == InvalidId && ReceiveIs(0));
    Require(CreateWebSocket(tmpl, nullptr) == InvalidUrl && ReceiveIs(0));
    Require(CreateWebSocket(tmpl, "http://example.com/socket") == UnsupportedScheme && ReceiveIs(0));
    Require(CreateWebSocket(tmpl, "") == UnsupportedScheme);
    const int accepted[] = {CreateWebSocket(tmpl, "ws:/bad"), CreateWebSocket(tmpl, "wss://example.com:9/probe"), CreateWebSocket(tmpl, "WS://EXAMPLE.COM/")};
    for (const int id : accepted) {
        Require(id > 0);
        Require(sceHttp2DeleteRequest(id) == 0);
    }

    const int socket = CreateWebSocket(tmpl, url);
    Require(socket > 0 && ReceiveIs(0));
    const int plain = sceHttp2CreateRequestWithURL(tmpl, "GET", "https://example.com/", 0);
    Require(plain > 0);
    Require(sceHttp2WebSocketSetPingInterval(socket, 0) == 0);
    Require(sceHttp2WebSocketSetPingInterval(socket, 1) == InvalidValue);
    Require(sceHttp2WebSocketSetPingInterval(socket, 999999) == InvalidValue);
    Require(sceHttp2WebSocketSetPingInterval(socket, 1000000) == 0);
    Require(sceHttp2WebSocketSetPingInterval(socket, 0xffffffffu) == 0);
    Require(sceHttp2WebSocketSetPingTimeout(socket, 0) == InvalidValue);
    Require(sceHttp2WebSocketSetPingTimeout(socket, 999999) == InvalidValue);
    Require(sceHttp2WebSocketSetPingTimeout(socket, 1000000) == 0);
    Require(sceHttp2WebSocketSetPingTimeout(socket, 0xffffffffu) == 0);
    Require(sceHttp2WebSocketSetPingInterval(0x7777, 5) == InvalidValue);
    Require(sceHttp2WebSocketSetPingTimeout(0x7777, 0) == InvalidValue);
    for (const int invalid : {0, -1, 0x7777, plain, tmpl, context}) RequireNotWebSocket(invalid);

    const std::uint16_t status = 1000;
    RequireNullMessageRejected(socket);
    Require(sceHttp2WebSocketSendTextMessageAsync(socket, "hi", 0, nullptr, nullptr) == NotConnected);
    Require(sceHttp2WebSocketSendTextMessageAsync(socket, "hi", 0, nullptr, &marker) == InvalidValue);
    Require(sceHttp2WebSocketSendDataMessageAsync(socket, "hi", 2, 0, nullptr, nullptr) == NotConnected);
    Require(sceHttp2WebSocketSendDataMessageAsync(socket, "hi", 2, 0, nullptr, &marker) == InvalidValue);
    Require(sceHttp2WebSocketCloseAsync(socket, &status, "bye", 0, nullptr, nullptr) == NotConnected);
    Require(sceHttp2WebSocketCloseAsync(socket, nullptr, nullptr, 0, nullptr, nullptr) == NotConnected);
    Require(sceHttp2WebSocketCloseAsync(socket, nullptr, "bye", 0, nullptr, nullptr) == InvalidValue);
    Require(sceHttp2WebSocketCloseAsync(socket, &status, nullptr, 0, nullptr, &marker) == InvalidValue);
    Require(sceHttp2WebSocketCloseAsync(0x7777, nullptr, "bye", 0, nullptr, nullptr) == InvalidValue);

    Require(sceHttp2DeleteRequest(socket) == 0);
    RequireNotWebSocket(socket);
    const int orphan = CreateWebSocket(tmpl, "wss://example.com/");
    Require(orphan > 0);
    Require(sceHttp2DeleteTemplate(tmpl) == 0);
    RequireNotWebSocket(orphan);
    Require(CreateWebSocket(tmpl, url) == InvalidId);
    Require(sceHttp2Term(context) == 0);
}

int main() {
    TestWebSockets();
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
    Require(sceHttp2WebSocketSetPingInterval(request, 1000000) == InvalidId);
    for (const int id : {tmpl, request}) {
        Require(Throws([&] { sceHttp2SetMinSslVersion(id, 0); }));
        Require(sceHttp2SslEnableOption(id, 0x3d) == 0);
        Require(sceHttp2SslDisableOption(id, 0x20) == 0);
        Require(Throws([&] { sceHttp2SetMinSslVersion(id, 99); }));
        Require(Throws([&] { sceHttp2SslEnableOption(id, 0x80000000); }));
    }
    Require(sceHttp2SslEnableOption(context, 0x3d) == 0);
    Require(sceHttp2SslDisableOption(context, 0x20) == 0);
    Require(Throws([&] { sceHttp2SslEnableOption(context, 0x80000000); }));
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
    Require(sceHttp2SslEnableOption(context, 0x3d) == InvalidId);
    Require(sceHttp2DeleteCookieBox(box) == 0);
    Require(sceHttp2DeleteCookieBox(box) == InvalidId);
    Require(sceHttp2Term(context) == InvalidId);
}
