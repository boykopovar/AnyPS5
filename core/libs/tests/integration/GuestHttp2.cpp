#include "SceTypes.hpp"

#include <Testing/Test.hpp>

#include <cstddef>
#include <cstdint>
#include <stdexcept>

extern "C" {
int APS5_VABI sceHttp2Init(int, int, std::size_t, int);
int APS5_VABI sceHttp2CreateTemplate(int, const char*, int, int);
int APS5_VABI sceHttp2CreateRequestWithURL(int, const char*, const char*, std::uint64_t);
int APS5_VABI sceHttp2CreateCookieBox(int);
int APS5_VABI sceHttp2SetCookieBox(int, int);
int APS5_VABI sceHttp2CookieFlush(int);
int APS5_VABI sceHttp2SetRequestNoContentLength(int);
int APS5_VABI sceHttp2SendRequest(int, const void*, std::size_t);
int APS5_VABI sceHttp2SendRequestAsync(int, const void*, std::size_t, Http2AsyncOption*, void*);
int APS5_VABI sceHttp2WaitAsync(int, Http2AsyncResult*, std::uint32_t*, void*);
int APS5_VABI sceHttp2DeleteRequest(int);
int APS5_VABI sceHttp2Term(int);
int APS5_VABI sceKernelCreateEqueue(KernelEqueue* eq, const char* name);
int APS5_VABI sceKernelDeleteEqueue(KernelEqueue eq);
int APS5_VABI sceKernelWaitEqueue(KernelEqueue eq, KernelEvent* ev, int num, int* out, const KernelUseconds* timo);
int APS5_VABI sceKernelAddUserEventEdge(KernelEqueue eq, int id);
int APS5_VABI sceKernelDeleteUserEvent(KernelEqueue eq, int id);
uintptr_t APS5_VABI sceKernelGetEventId(const KernelEvent* ev);
void* APS5_VABI sceKernelGetEventUserData(const KernelEvent* ev);
}

struct Http2MemoryPoolStats {
    std::size_t pool_size;
    std::size_t max_inuse_size;
    std::size_t current_inuse_size;
    std::int32_t reserved;
};

extern "C" {
int APS5_VABI sceHttp2GetMemoryPoolStats(int, Http2MemoryPoolStats*);
int APS5_VABI sceHttp2SetResolveRetry(int, std::int32_t);
}

namespace {

using Testing::Case;
using Testing::Require;
using Testing::RequireEqual;
using Testing::RequireThrows;

constexpr int networkError = static_cast<int>(0x80436063);
constexpr std::size_t poolSize = 0x10000;

class Http2Context {
public:
    Http2Context() : id(sceHttp2Init(1, 1, poolSize, 4)) {
        Require(id > 0, "sceHttp2Init returned a non-positive context id");
    }

    ~Http2Context() {
        sceHttp2Term(id);
    }

    Http2Context(const Http2Context&) = delete;
    Http2Context& operator=(const Http2Context&) = delete;

    const int id;
};

class Http2Request {
public:
    Http2Request() {
        templateId = sceHttp2CreateTemplate(context.id, "agent", 2, 0);
        Require(templateId > 0, "template creation");
        id = sceHttp2CreateRequestWithURL(templateId, "POST", "https://example.com/", 16);
        Require(id > 0, "request creation");
    }

    ~Http2Request() {
        if (id > 0) sceHttp2DeleteRequest(id);
    }

    Http2Request(const Http2Request&) = delete;
    Http2Request& operator=(const Http2Request&) = delete;

    Http2Context context;
    int templateId = 0;
    int id = 0;
};

class UserEventQueue {
public:
    explicit UserEventQueue(int eventId) : eventId(eventId) {
        RequireEqual(sceKernelCreateEqueue(&queue, "http2"), 0, "create the event queue");
        RequireEqual(sceKernelAddUserEventEdge(queue, eventId), 0, "add the user event");
    }

    ~UserEventQueue() {
        if (eventRegistered) sceKernelDeleteUserEvent(queue, eventId);
        sceKernelDeleteEqueue(queue);
    }

    UserEventQueue(const UserEventQueue&) = delete;
    UserEventQueue& operator=(const UserEventQueue&) = delete;

    int DeleteUserEvent() {
        eventRegistered = false;
        return sceKernelDeleteUserEvent(queue, eventId);
    }

    Http2AsyncOption Option(void* userData) const {
        Http2AsyncOption option{};
        option.equeue = queue;
        option.user_event_id = eventId;
        option.user_data = userData;
        return option;
    }

    KernelEqueue queue = 0;

private:
    int eventId;
    bool eventRegistered = true;
};

void CompleteAsyncSend(const Http2Request& request, UserEventQueue& queue, int& tag) {
    auto option = queue.Option(&tag);
    RequireEqual(sceHttp2SendRequestAsync(request.id, nullptr, 0, &option, nullptr), 0, "asynchronous send");
    Http2AsyncResult result{};
    RequireEqual(sceHttp2WaitAsync(request.id, &result, nullptr, nullptr), 0, "wait for the asynchronous send");
}

const Case poolStats{"GetMemoryPoolStats_FreshContext_ReportsPoolSizeAndNoUsage", [] {
    const Http2Context context;
    Http2MemoryPoolStats stats{1, 1, 1, 1};
    RequireEqual(sceHttp2GetMemoryPoolStats(context.id, &stats), 0, "get memory pool stats");
    RequireEqual(stats.pool_size, poolSize, "pool size");
    RequireEqual(stats.max_inuse_size, std::size_t{0}, "max in-use size");
    RequireEqual(stats.current_inuse_size, std::size_t{0}, "current in-use size");
    RequireEqual(stats.reserved, 0, "reserved field");
}};

const Case cookieBoxes{"CreateCookieBox_TwoBoxes_ReturnDistinctPositiveIds", [] {
    const Http2Context context;
    const int box = sceHttp2CreateCookieBox(context.id);
    const int other = sceHttp2CreateCookieBox(context.id);
    Require(box > 0 && other > 0, "cookie box ids must be positive");
    Require(box != other, "cookie box ids must differ");
    Require(box != context.id, "cookie box id must differ from the context id");
}};

const Case templateCookieBox{"SetCookieBox_OnTemplate_AcceptsBoxAndZero", [] {
    const Http2Context context;
    const int box = sceHttp2CreateCookieBox(context.id);
    const int other = sceHttp2CreateCookieBox(context.id);
    const int templateId = sceHttp2CreateTemplate(context.id, "agent", 2, 0);
    Require(templateId > 0, "template id must be positive");
    Require(templateId != box && templateId != other, "template id must differ from the cookie box ids");
    RequireEqual(sceHttp2SetResolveRetry(templateId, 3), 0, "set resolve retry");
    RequireEqual(sceHttp2SetCookieBox(templateId, box), 0, "set a cookie box");
    RequireEqual(sceHttp2SetCookieBox(templateId, 0), 0, "clear the cookie box");
}};

const Case requestSettings{"RequestSettings_CookieBoxAndNoContentLength_Succeed", [] {
    const Http2Request request;
    const int other = sceHttp2CreateCookieBox(request.context.id);
    RequireEqual(sceHttp2SetCookieBox(request.id, other), 0, "set the request cookie box");
    RequireEqual(sceHttp2SetRequestNoContentLength(request.id), 0, "disable the content length");
    RequireEqual(sceHttp2CookieFlush(request.context.id), 0, "flush cookies");
}};

const Case sendRequest{"SendRequest_NoNetwork_ReturnsNetworkError", [] {
    const Http2Request request;
    RequireEqual(sceHttp2SendRequest(request.id, nullptr, 0), networkError, "synchronous send");
}};

const Case asyncEvent{"SendRequestAsync_WithUserEvent_TriggersEventWithUserData", [] {
    const Http2Request request;
    UserEventQueue queue(request.id);
    int tag = 0;
    auto option = queue.Option(&tag);
    RequireEqual(sceHttp2SendRequestAsync(request.id, nullptr, 0, &option, nullptr), 0, "asynchronous send");
    KernelEvent event{};
    int count = 0;
    const KernelUseconds timeout = 1000000;
    RequireEqual(sceKernelWaitEqueue(queue.queue, &event, 1, &count, &timeout), 0, "wait for the event");
    RequireEqual(count, 1, "event count");
    RequireEqual(sceKernelGetEventId(&event), static_cast<uintptr_t>(request.id), "event id");
    Require(sceKernelGetEventUserData(&event) == &tag, "event user data");
}};

const Case asyncResult{"WaitAsync_AfterAsyncSend_ReportsNetworkErrorOnce", [] {
    const Http2Request request;
    UserEventQueue queue(request.id);
    int tag = 0;
    auto option = queue.Option(&tag);
    RequireEqual(sceHttp2SendRequestAsync(request.id, nullptr, 0, &option, nullptr), 0, "asynchronous send");
    Http2AsyncResult result{};
    RequireEqual(sceHttp2WaitAsync(request.id, &result, nullptr, nullptr), 0, "wait for the result");
    RequireEqual(result.req_id, request.id, "result request id");
    RequireEqual(result.result, networkError, "result code");
    RequireThrows<std::runtime_error>([&] { sceHttp2WaitAsync(request.id, &result, nullptr, nullptr); }, "second wait");
}};

const Case asyncWithoutOption{"SendRequestAsync_NullOption_Throws", [] {
    const Http2Request request;
    UserEventQueue queue(request.id);
    int tag = 0;
    CompleteAsyncSend(request, queue, tag);
    RequireThrows<std::runtime_error>([&] { sceHttp2SendRequestAsync(request.id, nullptr, 0, nullptr, nullptr); }, "send without an option");
}};

const Case asyncDeletedEvent{"SendRequestAsync_DeletedUserEvent_ThrowsAndQueuesNothing", [] {
    Http2Request request;
    UserEventQueue queue(request.id);
    int tag = 0;
    CompleteAsyncSend(request, queue, tag);
    auto option = queue.Option(&tag);
    Http2AsyncResult result{};
    RequireEqual(queue.DeleteUserEvent(), 0, "delete the user event");
    RequireThrows<std::runtime_error>([&] { sceHttp2SendRequestAsync(request.id, nullptr, 0, &option, nullptr); }, "send to a deleted user event");
    RequireThrows<std::runtime_error>([&] { sceHttp2WaitAsync(request.id, &result, nullptr, nullptr); }, "wait after the failed send");
    RequireEqual(sceHttp2DeleteRequest(request.id), 0, "delete the request");
    request.id = 0;
}};

const Case deleteAndTerm{"DeleteRequestAndTerm_CreatedObjects_Succeed", [] {
    const int context = sceHttp2Init(1, 1, poolSize, 4);
    Require(context > 0, "sceHttp2Init returned a non-positive context id");
    const int templateId = sceHttp2CreateTemplate(context, "agent", 2, 0);
    const int request = sceHttp2CreateRequestWithURL(templateId, "POST", "https://example.com/", 16);
    RequireEqual(sceHttp2DeleteRequest(request), 0, "delete the request");
    RequireEqual(sceHttp2Term(context), 0, "terminate the context");
}};

} // namespace
