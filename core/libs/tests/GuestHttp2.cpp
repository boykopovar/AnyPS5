#include "prx/libkernel/include/Api.hpp"
#include "prx/libSceHttp2/include/Http2.hpp"
#include "SceTypes.hpp"
#include <array>
#include <atomic>
#include <cstdint>
#include <cstdlib>
#include <stdexcept>
#include <thread>
#include <initializer_list>

static void Require(bool value) { if (!value) std::abort(); }

static constexpr int InvalidId = static_cast<int>(0x817B1100);
static constexpr int InvalidValue = static_cast<int>(0x817B11FE);
static constexpr int NotConnected = static_cast<int>(0x817B1065);
static constexpr int UnsupportedScheme = static_cast<int>(0x817B1061);
static constexpr int InvalidUrl = static_cast<int>(0x80433060);
static constexpr int NotFound = static_cast<int>(0x817B1025);
static constexpr int NullPointer = static_cast<int>(0x817B1225);

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

static int callbackCalls = 0;

static int APS5_VABI RejectRedirect(int, int, const char**, const char*, void*) {
    ++callbackCalls;
    return -1;
}

static int APS5_VABI RejectSsl(int, unsigned int, void* const*, int, void*) {
    ++callbackCalls;
    return -1;
}

int main() {
    TestWebSockets();
    Require(sceHttp2CreateCookieBox(0xffff) == static_cast<int>(0x817b121du));
    const int box = sceHttp2CreateCookieBox(0x10000);
    Require(sceHttp2Init(1, 1, 0x10000, 0) == static_cast<int>(0x817B11FE));
    Require(Throws([] { sceHttp2Init(1, 1, 0x10000, -1); }));
    Require(sceHttp2Init(1, 0, 0x40000, 1) == static_cast<int>(0x8095F006));
    const int context = sceHttp2Init(1, 1, 0x10000, 4);
    const int tmpl = sceHttp2CreateTemplate(context, "agent", 3, 0);
    const int proxyTemplate = sceHttp2CreateTemplate(context, "agent", 3, 1);
    Require(proxyTemplate > 0 && sceHttp2DeleteTemplate(proxyTemplate) == 0);
    Require(Throws([&] { sceHttp2CreateTemplate(context, "agent", 3, 2); }));
    Require(Throws([&] { sceHttp2CreateTemplate(context, "agent", 3, -1); }));
    const int request = sceHttp2CreateRequestWithURL(tmpl, "GET", "https://localhost/", 0);
    Require(box > 0 && context > 0 && tmpl > 0 && request > 0);
    Require(sceHttp2SetRedirectCallback(-1, RejectRedirect, nullptr) == InvalidId);
    for (const int id : {tmpl, request}) {
        Require(sceHttp2SetRedirectCallback(id, RejectRedirect, &callbackCalls) == 0);
        Require(sceHttp2SetRedirectCallback(id, nullptr, &callbackCalls) == 0);
        Require(sceHttp2SetRedirectCallback(id, RejectRedirect, nullptr) == 0);
    }
    Require(sceHttp2SetSslCallback(-1, RejectSsl, nullptr) == InvalidId);
    for (const int id : {tmpl, request}) {
        Require(sceHttp2SetSslCallback(id, RejectSsl, &callbackCalls) == 0);
        Require(sceHttp2SetSslCallback(id, nullptr, &callbackCalls) == 0);
        Require(sceHttp2SetSslCallback(id, RejectSsl, nullptr) == 0);
    }
    Require(callbackCalls == 0);

    Require(sceHttp2SetCookieBox(tmpl, 0) == 0);
    int bound = -7;
    Require(sceHttp2GetCookieBox(tmpl, &bound) == NotFound && bound == 0);
    Require(sceHttp2GetCookieBox(tmpl, nullptr) == NullPointer && sceHttp2GetCookieBox(context, nullptr) == NullPointer);
    Require(sceHttp2SetCookieBox(tmpl, box) == 0);
    bound = -7;
    Require(sceHttp2GetCookieBox(tmpl, &bound) == 0 && bound == box);
    bound = -7;
    Require(sceHttp2GetCookieBox(request, &bound) == NotFound && bound == 0);
    const int copied = sceHttp2CreateRequestWithURL(tmpl, "GET", "https://localhost/", 0);
    bound = -7;
    Require(sceHttp2GetCookieBox(copied, &bound) == 0 && bound == box);
    Require(sceHttp2SetCookieBox(tmpl, 0) == 0);
    bound = -7;
    Require(sceHttp2GetCookieBox(tmpl, &bound) == NotFound && bound == 0);
    bound = -7;
    Require(sceHttp2GetCookieBox(copied, &bound) == 0 && bound == box);
    for (const int id : {context, 0, -1, 0x7fffffff}) {
        Require(sceHttp2SetCookieBox(id, box) == InvalidId && sceHttp2SetCookieBox(id, 0) == InvalidId);
        bound = -7;
        Require(sceHttp2GetCookieBox(id, &bound) == InvalidId && bound == 0);
    }
    const int deletedBox = sceHttp2CreateCookieBox(0x10000);
    Require(deletedBox > 0 && sceHttp2DeleteCookieBox(deletedBox) == 0);
    for (const int value : {-1, 0x7fffffff, context, tmpl, deletedBox}) Require(sceHttp2SetCookieBox(request, value) == InvalidId);
    const int heldBox = sceHttp2CreateCookieBox(0x10000);
    Require(heldBox > 0 && sceHttp2SetCookieBox(copied, heldBox) == 0 && sceHttp2DeleteCookieBox(heldBox) == 0);
    bound = -7;
    Require(sceHttp2GetCookieBox(copied, &bound) == 0 && bound == heldBox);
    Require(sceHttp2SetCookieBox(copied, 0) == 0 && sceHttp2DeleteRequest(copied) == 0);
    Require(sceHttp2CookieFlush(box) == 0);
    Require(sceHttp2SetResolveRetry(tmpl, 0) == 0);
    Require(sceHttp2SetResolveRetry(tmpl, 3) == 0);
    Require(Throws([&] { sceHttp2SetResolveRetry(tmpl, -1); }));
    Require(sceHttp2SetConnectionWaitTimeOut(-1, 1000000) == InvalidId);
    for (const int id : {tmpl, request}) {
        Require(sceHttp2SetConnectionWaitTimeOut(id, 1000000) == 0);
        Require(sceHttp2SetConnectionWaitTimeOut(id, 0xffffffffu) == 0);
    }
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
    Require(sceKernelCreateEqueue(&eq, "http2-async") == 0);
    const int failed = sceHttp2CreateRequestWithURL(tmpl, "POST", "https://localhost/", 0);
    Require(failed > 0);
    Require(sceKernelAddUserEventEdge(eq, failed) == 0);
    int tag = 0;
    Http2AsyncOption option{};
    option.equeue = eq;
    option.user_event_id = failed;
    option.user_data = &tag;
    const char body = 'x';
    Require(sceHttp2SendRequestAsync(failed, &body, 1, &option, nullptr) == 0);
    KernelEvent event{};
    int count = 0;
    const KernelUseconds timeout = 2000000;
    Require(sceKernelWaitEqueue(eq, &event, 1, &count, &timeout) == 0 && count == 1);
    Require(event.ident == static_cast<std::uintptr_t>(failed) && event.udata == &tag);
    Http2AsyncResult result{};
    Require(Throws([&] { sceHttp2WaitAsync(failed, &result, nullptr, nullptr); }));
    Require(sceHttp2WaitAsync(0, &result, nullptr, nullptr) == InvalidId);
    Require(sceHttp2WaitAsync(failed, nullptr, nullptr, nullptr) == static_cast<int>(0x817B1225));
    Http2AsyncResult reserved{};
    reserved.reserved = &tag;
    Require(sceHttp2WaitAsync(failed, &reserved, nullptr, nullptr) == static_cast<int>(0x817B11FE));
    Http2AsyncResult clean{};
    Require(sceHttp2WaitAsync(failed, &clean, nullptr, &tag) == static_cast<int>(0x817B11FE));
    {
#ifdef _WIN32
        _putenv("APS5_NET_DISCONNECTED=1");
#else
        setenv("APS5_NET_DISCONNECTED", "1", 1);
#endif
        const KernelUseconds poll = 0;
        Http2AsyncOption bad_option = option;
        bad_option.equeue = -1;
        Require(sceHttp2SendRequestAsync(failed, nullptr, 0, &bad_option, nullptr) == 0);
        Require(Throws([&] { sceHttp2WaitAsync(failed, &result, nullptr, nullptr); }));
        Require(sceHttp2SendRequestAsync(failed, nullptr, 0, nullptr, nullptr) == 0);
        std::uint32_t zero_timeout = 0;
        Http2AsyncResult polled{};
        Require(sceHttp2WaitAsync(failed, &polled, &zero_timeout, nullptr) == 0);
        Require(polled.req_id == failed && polled.result == static_cast<int>(0x80431063) && zero_timeout == 0);
        KernelEvent extra{};
        int extra_count = 123;
        Require(sceKernelWaitEqueue(eq, &extra, 1, &extra_count, &poll) != 0);
        std::atomic<bool> waiter_consumed{false};
        std::thread waiter([&] {
            KernelEvent event_wait{};
            int count_wait = 0;
            const KernelUseconds poll_wait = 1000000;
            if (sceKernelWaitEqueue(eq, &event_wait, 1, &count_wait, &poll_wait) == 0 && count_wait == 1) {
                Http2AsyncResult result_wait{};
                std::uint32_t timeout_wait = 500;
                if (sceHttp2WaitAsync(failed, &result_wait, &timeout_wait, nullptr) == 0) {
                    if (result_wait.req_id == failed && result_wait.result == static_cast<int>(0x80431063)) {
                        waiter_consumed = true;
                    }
                }
            }
        });
        Require(sceHttp2SendRequestAsync(failed, nullptr, 0, &option, nullptr) == 0);
        waiter.join();
        Require(waiter_consumed);
        Require(sceKernelWaitEqueue(eq, &extra, 1, &extra_count, &poll) != 0);
        Require(sceHttp2SendRequest(failed, nullptr, 0) == static_cast<int>(0x80431063));
        Require(sceHttp2SendRequest(failed + 1000, nullptr, 0) == InvalidId);
#ifdef _WIN32
        _putenv("APS5_NET_DISCONNECTED=");
#else
        unsetenv("APS5_NET_DISCONNECTED");
#endif
    }
    Require(sceKernelDeleteUserEvent(eq, failed) == 0);
    Require(sceKernelDeleteEqueue(eq) == 0);
    const int sibling = sceHttp2CreateTemplate(context, "other", 2, 0);
    const int child = sceHttp2CreateRequestWithURL(sibling, "CONNECT", "https://localhost/", 0);
    Require(sceHttp2DeleteTemplate(tmpl) == 0);
    Require(sceHttp2SslEnableOption(sibling, 0) == 0);
    Require(sceHttp2WaitAsync(request, &result, nullptr, nullptr) == InvalidId);
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
