#include "prx/libSceRudp/include/Api.hpp"
#include <atomic>
#include <cstdint>
#include <cstdio>
#include <cstddef>
#include <cstring>
#include <limits>
#include <map>
#include <mutex>
#include <stdexcept>
#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"
#include "prx/libkernel/Time/include/TimedWait.hpp"

// Offline reliable-UDP: the library initialises and contexts can be created and bound, but no connection can
// ever be established because there is no network.

namespace {
constexpr int RUDP_ERROR_NOT_INITIALIZED = static_cast<int>(0x80770001u);
constexpr int RUDP_ERROR_ALREADY_INITIALIZED = static_cast<int>(0x80770002u);
constexpr int RUDP_ERROR_INVALID_CONTEXT_ID = static_cast<int>(0x80770003u);
constexpr int RUDP_ERROR_INVALID_ARGUMENT = static_cast<int>(0x80770004u);
constexpr int RUDP_ERROR_INVALID_OPTION = static_cast<int>(0x80770005u);
constexpr int RUDP_ERROR_MEMORY = static_cast<int>(0x80770007u);
constexpr int RUDP_ERROR_CONN_RESET = static_cast<int>(0x80770009u);
constexpr int RUDP_ERROR_CONN_REFUSED = static_cast<int>(0x8077000Au);
constexpr int RUDP_ERROR_NOT_ACCEPTABLE = static_cast<int>(0x80770011u);
constexpr int RUDP_ERROR_INVALID_POLL_ID = static_cast<int>(0x8077001Fu);
constexpr int RUDP_ERROR_TOO_MANY_CONTEXTS = static_cast<int>(0x80770020u);

constexpr int RUDP_POLL_OP_ADD = 1;
constexpr int RUDP_POLL_OP_MODIFY = 2;
constexpr int RUDP_POLL_OP_REMOVE = 3;
constexpr uint64_t RUDP_POLL_WAIT_MAX_USEC = static_cast<uint64_t>(std::numeric_limits<int64_t>::max() / 2000);

constexpr int RUDP_POOL_MIN_SIZE = 0xC58;
constexpr std::uintptr_t RUDP_POOL_ALIGNMENT = 8;
constexpr std::size_t RUDP_STATUS_SIZE = 0xF8;
constexpr std::size_t RUDP_STATUS_CURRENT_CONTEXTS = 0x64;
constexpr uint16_t RUDP_MAX_SEGMENT_SIZE = 1410;

enum RudpOption {
    RUDP_OPTION_MAX_PAYLOAD = 1,
    RUDP_OPTION_SNDBUF = 2,
    RUDP_OPTION_RCVBUF = 3,
    RUDP_OPTION_NODELAY = 4,
    RUDP_OPTION_DELIVERY_CRITICAL = 5,
    RUDP_OPTION_ORDER_CRITICAL = 6,
    RUDP_OPTION_NONBLOCK = 7,
    RUDP_OPTION_STREAM = 8,
    RUDP_OPTION_CONNECTION_TIMEOUT = 9,
    RUDP_OPTION_CLOSE_WAIT_TIMEOUT = 10,
    RUDP_OPTION_AGGREGATION_TIMEOUT = 11,
    RUDP_OPTION_READ_TIMEOUT = 15,
    RUDP_OPTION_WRITE_TIMEOUT = 16,
    RUDP_OPTION_FLUSH_TIMEOUT = 17,
    RUDP_OPTION_KEEP_ALIVE_INTERVAL = 18,
    RUDP_OPTION_KEEP_ALIVE_TIMEOUT = 19,
};

std::mutex g_mutex;
bool g_inited = false;
int g_next_ctx = 0;
struct RudpContext {
    bool bound = false;
    std::map<int, uint32_t> options = {
        {RUDP_OPTION_MAX_PAYLOAD, 1346},
        {RUDP_OPTION_SNDBUF, 65536},
        {RUDP_OPTION_RCVBUF, 65536},
        {RUDP_OPTION_NODELAY, 0},
        {RUDP_OPTION_DELIVERY_CRITICAL, 1},
        {RUDP_OPTION_ORDER_CRITICAL, 1},
        {RUDP_OPTION_NONBLOCK, 1},
        {RUDP_OPTION_STREAM, 0},
        {RUDP_OPTION_CONNECTION_TIMEOUT, 60000},
        {RUDP_OPTION_CLOSE_WAIT_TIMEOUT, 0},
        {RUDP_OPTION_AGGREGATION_TIMEOUT, 30},
        {RUDP_OPTION_READ_TIMEOUT, 0},
        {RUDP_OPTION_WRITE_TIMEOUT, 0},
        {RUDP_OPTION_FLUSH_TIMEOUT, 0},
        {RUDP_OPTION_KEEP_ALIVE_INTERVAL, 0},
        {RUDP_OPTION_KEEP_ALIVE_TIMEOUT, 0},
    };
};
std::map<int, RudpContext> g_ctx;
GuestRudpEventHandler g_handler = nullptr;
void* g_handler_arg = nullptr;

struct RudpPoll {
    std::size_t capacity;
    std::map<int, uint16_t> entries;
};

int g_next_poll = 1;
std::map<int, RudpPoll> g_polls;
TimedWait::Condition g_polls_changed;

bool valid_ctx(int ctx) {
    return g_ctx.count(ctx) != 0;
}

void remove_ctx_from_polls(int ctx) {
    for (auto& [id, poll] : g_polls) {
        poll.entries.erase(ctx);
    }
}
}  // namespace

extern "C" {

int APS5_VABI sceRudpInit_nid_postfix(void* mem_pool, int mem_pool_size) {
    std::lock_guard<std::mutex> lk(g_mutex);
    if (g_inited) {
        return RUDP_ERROR_ALREADY_INITIALIZED;
    }
    if (mem_pool == nullptr || mem_pool_size <= 0) {
        return RUDP_ERROR_INVALID_ARGUMENT;
    }
    if (mem_pool_size < RUDP_POOL_MIN_SIZE || reinterpret_cast<std::uintptr_t>(mem_pool) % RUDP_POOL_ALIGNMENT != 0) {
        return RUDP_ERROR_MEMORY;
    }
    g_inited = true;
    return 0;
}

int APS5_VABI sceRudpActivate(std::int32_t contextId, const void* address, std::uint32_t addressLength) {
    std::lock_guard lock(g_mutex);
    if (!g_inited) return RUDP_ERROR_NOT_INITIALIZED;
    if (contextId < 0 || !address) return RUDP_ERROR_INVALID_ARGUMENT;
    static_cast<void>(addressLength);
    throw std::runtime_error(__func__);
}

int APS5_VABI sceRudpGetStatus(void* status, std::size_t size) {
    std::lock_guard<std::mutex> lk(g_mutex);
    if (!g_inited) {
        return RUDP_ERROR_NOT_INITIALIZED;
    }
    if (status == nullptr || size == 0 || size > RUDP_STATUS_SIZE) {
        return RUDP_ERROR_INVALID_ARGUMENT;
    }
    unsigned char report[RUDP_STATUS_SIZE] = {};
    const auto contexts = static_cast<uint32_t>(g_ctx.size());
    std::memcpy(report + RUDP_STATUS_CURRENT_CONTEXTS, &contexts, sizeof(contexts));
    std::memcpy(status, report, size);
    return 0;
}

int APS5_VABI sceRudpEnd(void) {
    std::lock_guard<std::mutex> lk(g_mutex);
    if (!g_inited) {
        return RUDP_ERROR_NOT_INITIALIZED;
    }
    g_inited = false;
    g_ctx.clear();
    g_polls.clear();
    g_polls_changed.NotifyAll();
    return 0;
}

int APS5_VABI sceRudpTerminate(int ctx_id) {
    std::lock_guard<std::mutex> lk(g_mutex);
    if (!g_inited) {
        return RUDP_ERROR_NOT_INITIALIZED;
    }
    if (g_ctx.erase(ctx_id) == 0) {
        return RUDP_ERROR_INVALID_CONTEXT_ID;
    }
    remove_ctx_from_polls(ctx_id);
    return 0;
}

int APS5_VABI sceRudpEnableInternalIOThread(uint32_t stack_size, uint32_t priority) {
    (void)stack_size;
    (void)priority;
    std::lock_guard<std::mutex> lk(g_mutex);
    return g_inited ? 0 : RUDP_ERROR_NOT_INITIALIZED;
}

int APS5_VABI sceRudpEnableInternalIOThread2(uint32_t stack_size, uint32_t priority, uint64_t affinity) {
    (void)stack_size;
    (void)priority;
    (void)affinity;
    std::lock_guard<std::mutex> lk(g_mutex);
    return g_inited ? 0 : RUDP_ERROR_NOT_INITIALIZED;
}

int APS5_VABI sceRudpSetEventHandler(GuestRudpEventHandler handler, void* arg) {
    std::lock_guard<std::mutex> lk(g_mutex);
    g_handler = handler;
    g_handler_arg = arg;
    return 0;
}

int APS5_VABI sceRudpCreateContext(GuestRudpEventHandler handler, void* arg, int* ctxId) {
    (void)handler;
    (void)arg;
    if (ctxId == nullptr) {
        return RUDP_ERROR_INVALID_ARGUMENT;
    }
    std::lock_guard<std::mutex> lk(g_mutex);
    if (!g_inited) {
        return RUDP_ERROR_NOT_INITIALIZED;
    }
    const int id = g_next_ctx++;
    g_ctx.emplace(id, RudpContext{});
    *ctxId = id;
    return 0;
}

int APS5_VABI sceRudpBind(int ctxId, uint16_t vport, int transport, uint16_t port) {
    (void)vport;
    (void)transport;
    (void)port;
    std::lock_guard<std::mutex> lk(g_mutex);
    if (!g_inited) {
        return RUDP_ERROR_NOT_INITIALIZED;
    }
    if (!valid_ctx(ctxId)) {
        return RUDP_ERROR_INVALID_CONTEXT_ID;
    }
    g_ctx.at(ctxId).bound = true;
    return 0;
}

int APS5_VABI sceRudpSetOption(int ctxId, int option, const void* value, uint32_t len) {
    std::lock_guard<std::mutex> lk(g_mutex);
    if (!g_inited) {
        return RUDP_ERROR_NOT_INITIALIZED;
    }
    const auto context = g_ctx.find(ctxId);
    if (context == g_ctx.end()) return RUDP_ERROR_INVALID_CONTEXT_ID;
    if (option < RUDP_OPTION_MAX_PAYLOAD || option > RUDP_OPTION_KEEP_ALIVE_TIMEOUT) return RUDP_ERROR_INVALID_OPTION;
    uint32_t optionValue;
    if (value == nullptr || len != sizeof(optionValue)) return RUDP_ERROR_INVALID_ARGUMENT;
    std::memcpy(&optionValue, value, sizeof(optionValue));
    context->second.options.insert_or_assign(option, optionValue);
    return 0;
}

int APS5_VABI sceRudpGetOption(int ctxId, int option, void* value, uint32_t len) {
    std::lock_guard<std::mutex> lk(g_mutex);
    if (!g_inited) {
        return RUDP_ERROR_NOT_INITIALIZED;
    }
    const auto context = g_ctx.find(ctxId);
    if (context == g_ctx.end()) return RUDP_ERROR_INVALID_CONTEXT_ID;
    if (option < RUDP_OPTION_MAX_PAYLOAD || option > RUDP_OPTION_KEEP_ALIVE_TIMEOUT) return RUDP_ERROR_INVALID_OPTION;
    const auto entry = context->second.options.find(option);
    if (entry == context->second.options.end() ||
        (option > RUDP_OPTION_AGGREGATION_TIMEOUT && option < RUDP_OPTION_READ_TIMEOUT)) {
        NotImplemented_nid_no_patch(__func__);
    }
    if (value == nullptr || len != sizeof(entry->second)) return RUDP_ERROR_INVALID_ARGUMENT;
    std::memcpy(value, &entry->second, sizeof(entry->second));
    return 0;
}

int APS5_VABI sceRudpInitiate(int ctx_id, const void* to, uint32_t tolen, uint16_t vport) {
    (void)to;
    (void)tolen;
    (void)vport;
    std::lock_guard<std::mutex> lk(g_mutex);
    if (!g_inited) {
        return RUDP_ERROR_NOT_INITIALIZED;
    }
    if (!valid_ctx(ctx_id)) {
        return RUDP_ERROR_INVALID_CONTEXT_ID;
    }
    return RUDP_ERROR_CONN_REFUSED;
}

int APS5_VABI sceRudpGetContextStatus(int ctx_id, void* status, uint32_t size) {
    std::lock_guard<std::mutex> lk(g_mutex);
    if (!g_inited) {
        return RUDP_ERROR_NOT_INITIALIZED;
    }
    if (!valid_ctx(ctx_id)) {
        return RUDP_ERROR_INVALID_CONTEXT_ID;
    }
    if (status != nullptr && size != 0) {
        std::memset(status, 0, size);  // state 0 = idle, nothing pending
    }
    return 0;
}

int APS5_VABI sceRudpGetRemoteInfo(int contextId, void* address, std::uint32_t* addressSize, std::uint16_t* virtualPort) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceRudpGetNumberOfPacketsToRead(int ctx_id) {
    std::lock_guard<std::mutex> lk(g_mutex);
    if (!g_inited) {
        return RUDP_ERROR_NOT_INITIALIZED;
    }
    if (ctx_id < 0) {
        return RUDP_ERROR_INVALID_ARGUMENT;
    }
    if (!valid_ctx(ctx_id)) {
        return RUDP_ERROR_INVALID_CONTEXT_ID;
    }
    return 0;
}

int APS5_VABI sceRudpGetSizeWritable(int ctx_id) {
    std::lock_guard<std::mutex> lk(g_mutex);
    if (!g_inited) {
        return RUDP_ERROR_NOT_INITIALIZED;
    }
    if (ctx_id < 0) {
        return RUDP_ERROR_INVALID_ARGUMENT;
    }
    if (!valid_ctx(ctx_id)) {
        return RUDP_ERROR_INVALID_CONTEXT_ID;
    }
    return RUDP_ERROR_NOT_ACCEPTABLE;
}

int APS5_VABI sceRudpGetSizeReadable(int ctx_id) {
    std::lock_guard<std::mutex> lk(g_mutex);
    if (!g_inited) {
        return RUDP_ERROR_NOT_INITIALIZED;
    }
    if (ctx_id < 0) {
        return RUDP_ERROR_INVALID_ARGUMENT;
    }
    if (!valid_ctx(ctx_id)) {
        return RUDP_ERROR_INVALID_CONTEXT_ID;
    }
    return RUDP_ERROR_NOT_ACCEPTABLE;
}

int APS5_VABI sceRudpWrite(int ctx_id, const void* data, uint32_t len, int flags) {
    (void)data;
    (void)len;
    (void)flags;
    std::lock_guard<std::mutex> lk(g_mutex);
    if (!g_inited) {
        return RUDP_ERROR_NOT_INITIALIZED;
    }
    return valid_ctx(ctx_id) ? RUDP_ERROR_CONN_RESET : RUDP_ERROR_INVALID_CONTEXT_ID;
}

int APS5_VABI sceRudpRead(int ctx_id, void* data, uint32_t len, int flags, void* status) {
    (void)data;
    (void)len;
    (void)flags;
    (void)status;
    std::lock_guard<std::mutex> lk(g_mutex);
    if (!g_inited) {
        return RUDP_ERROR_NOT_INITIALIZED;
    }
    return valid_ctx(ctx_id) ? RUDP_ERROR_CONN_RESET : RUDP_ERROR_INVALID_CONTEXT_ID;
}

int APS5_VABI sceRudpNetFlush(void) {
    std::lock_guard<std::mutex> lk(g_mutex);
    return g_inited ? 0 : RUDP_ERROR_NOT_INITIALIZED;
}

int APS5_VABI sceRudpProcessEvents(std::uint64_t timeout) {
    std::lock_guard<std::mutex> lk(g_mutex);
    return g_inited ? 0 : RUDP_ERROR_NOT_INITIALIZED;
}

int APS5_VABI sceRudpNetReceived(int socketId, const void* data, std::size_t dataSize, const void* sourceAddress, std::uint32_t sourceAddressSize) {
    std::lock_guard<std::mutex> lk(g_mutex);
    return g_inited ? 0 : RUDP_ERROR_NOT_INITIALIZED;
}

int APS5_VABI sceRudpGetMaxSegmentSize(uint16_t* size) {
    std::lock_guard<std::mutex> lk(g_mutex);
    if (!g_inited) {
        return RUDP_ERROR_NOT_INITIALIZED;
    }
    if (size == nullptr) {
        return RUDP_ERROR_INVALID_ARGUMENT;
    }
    *size = RUDP_MAX_SEGMENT_SIZE;
    return 0;
}

int APS5_VABI sceRudpPollCreate(std::size_t size) {
    std::lock_guard<std::mutex> lk(g_mutex);
    if (!g_inited) {
        return RUDP_ERROR_NOT_INITIALIZED;
    }
    if (size == 0) {
        return RUDP_ERROR_INVALID_ARGUMENT;
    }
    if (g_next_poll == std::numeric_limits<int>::max()) {
        throw std::runtime_error("sceRudpPollCreate: poll ids exhausted");
    }
    const int id = g_next_poll++;
    g_polls[id] = RudpPoll{size, {}};
    return id;
}

int APS5_VABI sceRudpPollControl(int pollId, int op, int ctxId, uint16_t events) {
    std::lock_guard<std::mutex> lk(g_mutex);
    if (!g_inited) {
        return RUDP_ERROR_NOT_INITIALIZED;
    }
    if (pollId < 0 || ctxId < 0 || (op != RUDP_POLL_OP_ADD && op != RUDP_POLL_OP_MODIFY && op != RUDP_POLL_OP_REMOVE)) {
        return RUDP_ERROR_INVALID_ARGUMENT;
    }
    const auto poll = g_polls.find(pollId);
    if (poll == g_polls.end()) {
        return RUDP_ERROR_INVALID_POLL_ID;
    }
    if (!valid_ctx(ctxId)) {
        return RUDP_ERROR_INVALID_CONTEXT_ID;
    }
    auto& entries = poll->second.entries;
    const auto entry = entries.find(ctxId);
    if (op == RUDP_POLL_OP_ADD) {
        if (entries.size() >= poll->second.capacity) {
            return RUDP_ERROR_TOO_MANY_CONTEXTS;
        }
        if (entry != entries.end()) {
            return RUDP_ERROR_INVALID_CONTEXT_ID;
        }
        entries.emplace(ctxId, events);
        return 0;
    }
    if (entry == entries.end()) {
        return RUDP_ERROR_INVALID_CONTEXT_ID;
    }
    if (op == RUDP_POLL_OP_MODIFY) {
        entry->second = events;
    } else {
        entries.erase(entry);
    }
    return 0;
}

int APS5_VABI sceRudpPollWait(int pollId, RudpPollEvent* events, std::size_t eventLen, uint64_t timeout) {
    std::unique_lock<std::mutex> lk(g_mutex);
    if (!g_inited) {
        return RUDP_ERROR_NOT_INITIALIZED;
    }
    if (pollId < 0 || events == nullptr || eventLen == 0) {
        return RUDP_ERROR_INVALID_ARGUMENT;
    }
    if (g_polls.count(pollId) == 0) {
        return RUDP_ERROR_INVALID_POLL_ID;
    }
    const auto closed = [pollId] { return !g_inited || g_polls.count(pollId) == 0; };
    if (timeout > RUDP_POLL_WAIT_MAX_USEC) {
        g_polls_changed.Wait(lk, closed);
    } else if (!g_polls_changed.WaitUntil(lk, TimedWait::DeadlineNanos(timeout), closed)) {
        return 0;
    }
    return g_inited ? RUDP_ERROR_INVALID_POLL_ID : RUDP_ERROR_NOT_INITIALIZED;
}

int APS5_VABI sceRudpPollDestroy(int pollId) {
    std::lock_guard<std::mutex> lk(g_mutex);
    if (!g_inited) {
        return RUDP_ERROR_NOT_INITIALIZED;
    }
    if (pollId < 0) {
        return RUDP_ERROR_INVALID_ARGUMENT;
    }
    if (g_polls.erase(pollId) == 0) {
        return RUDP_ERROR_INVALID_POLL_ID;
    }
    g_polls_changed.NotifyAll();
    return 0;
}

int APS5_VABI sceRudpAccept() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceRudpFlush() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceRudpGetLocalInfo() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceRudpListen() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceRudpPollCancel() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceRudpSetMaxSegmentSize() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

}
