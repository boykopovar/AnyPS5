#include "prx/libSceRudp/include/Api.hpp"
#include <atomic>
#include <cstdint>
#include <cstdio>
#include <cstddef>
#include <cstring>
#include <limits>
#include <map>
#include <set>
#include <mutex>
#include <stdexcept>
#include <utility>
#include <vector>
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
constexpr int RUDP_ERROR_INVALID_MUXMODE = static_cast<int>(0x80770006u);
constexpr int RUDP_ERROR_MEMORY = static_cast<int>(0x80770007u);
constexpr int RUDP_ERROR_CONN_RESET = static_cast<int>(0x80770009u);
constexpr int RUDP_ERROR_CONN_REFUSED = static_cast<int>(0x8077000Au);
constexpr int RUDP_ERROR_NOT_ACCEPTABLE = static_cast<int>(0x80770011u);
constexpr int RUDP_ERROR_CANCELLED = static_cast<int>(0x80770014u);
constexpr int RUDP_ERROR_INVALID_VPORT = static_cast<int>(0x80770015u);
constexpr int RUDP_ERROR_WOULDBLOCK = static_cast<int>(0x80770016u);
constexpr int RUDP_ERROR_VPORT_IN_USE = static_cast<int>(0x80770017u);
constexpr int RUDP_ERROR_ALREADY_BOUND = static_cast<int>(0x8077001Du);
constexpr int RUDP_ERROR_INVALID_POLL_ID = static_cast<int>(0x8077001Fu);
constexpr int RUDP_ERROR_TOO_MANY_CONTEXTS = static_cast<int>(0x80770020u);
constexpr int RUDP_ERROR_IN_PROGRESS = static_cast<int>(0x80770021u);
constexpr int RUDP_ERROR_NO_EVENT_HANDLER = static_cast<int>(0x80770022u);
constexpr uint8_t RUDP_MUXMODE_P2P = 1;
constexpr uint8_t RUDP_MUXMODE_SERVER = 2;
constexpr uint16_t RUDP_AUTO_VPORT_BASE = 0x2710;
constexpr int RUDP_MAX_BACKLOG = 0x80;

constexpr int RUDP_POLL_OP_ADD = 1;
constexpr int RUDP_POLL_OP_MODIFY = 2;
constexpr int RUDP_POLL_OP_REMOVE = 3;
constexpr uint64_t RUDP_POLL_WAIT_MAX_USEC = static_cast<uint64_t>(std::numeric_limits<int64_t>::max() / 2000);

constexpr int RUDP_POOL_MIN_SIZE = 0xC58;
constexpr std::uintptr_t RUDP_POOL_ALIGNMENT = 8;
constexpr std::size_t RUDP_STATUS_SIZE = 0xF8;
constexpr std::size_t RUDP_STATUS_CURRENT_CONTEXTS = 0x64;
constexpr std::size_t RUDP_CONTEXT_BUFFER_STATUS_SIZE = 16;
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
uint16_t g_max_segment_size = RUDP_MAX_SEGMENT_SIZE;
int g_next_ctx = 0;
struct RudpContext {
    bool bound = false;
    int socketId = -1;
    uint16_t vport = 0;
    uint8_t muxMode = 0;
    bool listening = false;
    int backlog = 0;
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
struct RudpSocket {
    uint8_t muxMode = 0;
    int contexts = 0;
    std::set<uint16_t> vports;
    uint16_t lastVport = RUDP_AUTO_VPORT_BASE;
};
std::map<int, RudpSocket> g_sockets;
std::vector<std::pair<int, uint16_t>> g_held_bindings;
TimedWait::Condition g_accept_changed;

struct RudpPoll {
    std::size_t capacity;
    std::map<int, uint16_t> entries;
    int waiters = 0;
    int draining = 0;
    uint64_t cancels = 0;
    bool cancelPending = false;
};

int g_next_poll = 1;
std::map<int, RudpPoll> g_polls;
TimedWait::Condition g_polls_changed;

struct RudpPollWaitRegistration {
    int pollId;
    uint64_t cancels;
    std::unique_lock<std::mutex>& lock;

    ~RudpPollWaitRegistration() {
        if (!lock.owns_lock()) {
            lock.lock();
        }
        const auto poll = g_polls.find(pollId);
        if (poll == g_polls.end()) {
            return;
        }
        if (poll->second.cancels == cancels) {
            --poll->second.waiters;
        } else {
            --poll->second.draining;
        }
        g_polls_changed.NotifyAll();
    }
};

bool valid_ctx(int ctx) {
    return g_ctx.count(ctx) != 0;
}

void remove_ctx_from_polls(int ctx) {
    for (auto& [id, poll] : g_polls) {
        poll.entries.erase(ctx);
    }
}

void release_binding(int socketId, uint16_t vport) {
    const auto socket = g_sockets.find(socketId);
    socket->second.vports.erase(vport);
    if (--socket->second.contexts == 0) {
        g_sockets.erase(socket);
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
    g_handler = nullptr;
    g_handler_arg = nullptr;
    g_sockets.clear();
    g_held_bindings.clear();
    g_polls_changed.NotifyAll();
    g_accept_changed.NotifyAll();
    return 0;
}

int APS5_VABI sceRudpTerminate(int ctx_id) {
    std::lock_guard<std::mutex> lk(g_mutex);
    if (!g_inited) {
        return RUDP_ERROR_NOT_INITIALIZED;
    }
    const auto context = g_ctx.find(ctx_id);
    if (context == g_ctx.end()) {
        return RUDP_ERROR_INVALID_CONTEXT_ID;
    }
    if (context->second.listening) {
        g_held_bindings.emplace_back(context->second.socketId, context->second.vport);
    } else if (context->second.bound) {
        release_binding(context->second.socketId, context->second.vport);
    }
    g_ctx.erase(context);
    remove_ctx_from_polls(ctx_id);
    g_accept_changed.NotifyAll();
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
    if (!g_inited) {
        return RUDP_ERROR_NOT_INITIALIZED;
    }
    if (handler == nullptr) {
        return RUDP_ERROR_NO_EVENT_HANDLER;
    }
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

int APS5_VABI sceRudpBind(int ctxId, int socketId, uint16_t vport, uint8_t muxMode) {
    std::lock_guard<std::mutex> lk(g_mutex);
    if (!g_inited) {
        return RUDP_ERROR_NOT_INITIALIZED;
    }
    if (ctxId < 0 || socketId < 0) {
        return RUDP_ERROR_INVALID_ARGUMENT;
    }
    if (muxMode != RUDP_MUXMODE_P2P && muxMode != RUDP_MUXMODE_SERVER) {
        return RUDP_ERROR_INVALID_MUXMODE;
    }
    if (!valid_ctx(ctxId)) {
        return RUDP_ERROR_INVALID_CONTEXT_ID;
    }
    auto& context = g_ctx.at(ctxId);
    if (context.listening) {
        return RUDP_ERROR_NOT_ACCEPTABLE;
    }
    if (context.bound) {
        return RUDP_ERROR_ALREADY_BOUND;
    }
    const auto existing = g_sockets.find(socketId);
    if (existing != g_sockets.end() && existing->second.muxMode != muxMode) {
        return RUDP_ERROR_INVALID_MUXMODE;
    }
    if (muxMode == RUDP_MUXMODE_P2P && vport == 0) {
        return RUDP_ERROR_INVALID_VPORT;
    }
    if (muxMode == RUDP_MUXMODE_SERVER && vport != 0 && existing != g_sockets.end() && existing->second.vports.count(vport) != 0) {
        return RUDP_ERROR_VPORT_IN_USE;
    }
    auto& socket = g_sockets[socketId];
    if (muxMode == RUDP_MUXMODE_SERVER) {
        if (vport == 0) {
            vport = socket.lastVport;
            do {
                if (vport == std::numeric_limits<uint16_t>::max()) {
                    throw std::runtime_error("sceRudpBind: an automatic vport past 0xFFFF");
                }
                ++vport;
            } while (socket.vports.count(vport) != 0);
            socket.lastVport = vport;
        }
        socket.vports.insert(vport);
    }
    socket.muxMode = muxMode;
    ++socket.contexts;
    context.bound = true;
    context.socketId = socketId;
    context.vport = vport;
    context.muxMode = muxMode;
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
    (void)address;
    (void)addressSize;
    (void)virtualPort;
    std::lock_guard<std::mutex> lk(g_mutex);
    if (!g_inited) {
        return RUDP_ERROR_NOT_INITIALIZED;
    }
    if (contextId < 0) {
        return RUDP_ERROR_INVALID_ARGUMENT;
    }
    if (!valid_ctx(contextId)) {
        return RUDP_ERROR_INVALID_CONTEXT_ID;
    }
    return RUDP_ERROR_NOT_ACCEPTABLE;
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
    if (!g_inited) {
        return RUDP_ERROR_NOT_INITIALIZED;
    }
    for (const auto& [socketId, vport] : g_held_bindings) {
        release_binding(socketId, vport);
    }
    g_held_bindings.clear();
    return 0;
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
    const auto poll = g_polls.find(pollId);
    if (poll == g_polls.end()) {
        return RUDP_ERROR_INVALID_POLL_ID;
    }
    if (poll->second.cancelPending) {
        poll->second.cancelPending = false;
        return RUDP_ERROR_CANCELLED;
    }
    const uint64_t cancels = poll->second.cancels;
    ++poll->second.waiters;
    const RudpPollWaitRegistration registration{pollId, cancels, lk};
    const auto woken = [pollId, cancels] {
        if (!g_inited) {
            return true;
        }
        const auto current = g_polls.find(pollId);
        return current == g_polls.end() || current->second.cancels != cancels;
    };
    bool signalled = true;
    if (timeout > RUDP_POLL_WAIT_MAX_USEC) {
        g_polls_changed.Wait(lk, woken);
    } else {
        signalled = g_polls_changed.WaitUntil(lk, TimedWait::DeadlineNanos(timeout), woken);
    }
    if (!g_inited) {
        return RUDP_ERROR_NOT_INITIALIZED;
    }
    const auto current = g_polls.find(pollId);
    if (current == g_polls.end()) {
        return RUDP_ERROR_INVALID_POLL_ID;
    }
    return signalled ? RUDP_ERROR_CANCELLED : 0;
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

int APS5_VABI sceRudpAccept(int ctxId, GuestRudpEventHandler handler, void* handlerArg, int* newCtxId, void* address, std::uint32_t* addressLength, std::uint16_t* vport) {
    (void)handler;
    (void)handlerArg;
    (void)address;
    (void)addressLength;
    (void)vport;
    std::unique_lock<std::mutex> lk(g_mutex);
    if (!g_inited) {
        return RUDP_ERROR_NOT_INITIALIZED;
    }
    if (ctxId < 0 || newCtxId == nullptr) {
        return RUDP_ERROR_INVALID_ARGUMENT;
    }
    if (!valid_ctx(ctxId)) {
        return RUDP_ERROR_INVALID_CONTEXT_ID;
    }
    if (!g_ctx.at(ctxId).listening) {
        return RUDP_ERROR_NOT_ACCEPTABLE;
    }
    if (g_ctx.at(ctxId).options.at(RUDP_OPTION_NONBLOCK) != 0) {
        return RUDP_ERROR_WOULDBLOCK;
    }
    g_accept_changed.Wait(lk, [ctxId] { return !g_inited || !valid_ctx(ctxId); });
    if (!g_inited) {
        throw std::runtime_error("sceRudpAccept: a waiting accept ended by sceRudpEnd");
    }
    return RUDP_ERROR_CANCELLED;
}

int APS5_VABI sceRudpFlush(int ctxId) {
    std::lock_guard<std::mutex> lk(g_mutex);
    if (!g_inited) {
        return RUDP_ERROR_NOT_INITIALIZED;
    }
    if (ctxId < 0) {
        return RUDP_ERROR_INVALID_ARGUMENT;
    }
    if (!valid_ctx(ctxId)) {
        return RUDP_ERROR_INVALID_CONTEXT_ID;
    }
    return RUDP_ERROR_NOT_ACCEPTABLE;
}

int APS5_VABI sceRudpGetLocalInfo() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceRudpListen(int ctxId, int backlog) {
    std::lock_guard<std::mutex> lk(g_mutex);
    if (!g_inited) {
        return RUDP_ERROR_NOT_INITIALIZED;
    }
    if (ctxId < 0 || backlog <= 0) {
        return RUDP_ERROR_INVALID_ARGUMENT;
    }
    if (g_handler == nullptr) {
        return RUDP_ERROR_NO_EVENT_HANDLER;
    }
    if (!valid_ctx(ctxId)) {
        return RUDP_ERROR_INVALID_CONTEXT_ID;
    }
    auto& context = g_ctx.at(ctxId);
    if (!context.bound || context.muxMode != RUDP_MUXMODE_SERVER) {
        return RUDP_ERROR_NOT_ACCEPTABLE;
    }
    if (context.listening) {
        return RUDP_ERROR_IN_PROGRESS;
    }
    context.listening = true;
    context.backlog = backlog < RUDP_MAX_BACKLOG ? backlog : RUDP_MAX_BACKLOG;
    return 0;
}

int APS5_VABI sceRudpPollCancel(int pollId) {
    std::unique_lock<std::mutex> lk(g_mutex);
    if (!g_inited) {
        return RUDP_ERROR_NOT_INITIALIZED;
    }
    if (pollId < 0) {
        return RUDP_ERROR_INVALID_ARGUMENT;
    }
    const auto poll = g_polls.find(pollId);
    if (poll == g_polls.end()) {
        return RUDP_ERROR_INVALID_POLL_ID;
    }
    if (poll->second.waiters == 0) {
        poll->second.cancelPending = true;
        return 0;
    }
    ++poll->second.cancels;
    poll->second.draining += poll->second.waiters;
    poll->second.waiters = 0;
    g_polls_changed.NotifyAll();
    g_polls_changed.Wait(lk, [pollId] {
        if (!g_inited) {
            return true;
        }
        const auto current = g_polls.find(pollId);
        return current == g_polls.end() || current->second.draining == 0;
    });
    return 0;
}

int APS5_VABI sceRudpSetMaxSegmentSize(uint16_t mss) {
    std::lock_guard<std::mutex> lk(g_mutex);
    if (!g_inited) {
        return RUDP_ERROR_NOT_INITIALIZED;
    }
    if (!g_ctx.empty()) {
        return RUDP_ERROR_NOT_ACCEPTABLE;
    }
    g_max_segment_size = mss;
    return 0;
}

int APS5_VABI sceRudpGetContextBufferStatus(int ctxId, void* status, std::size_t size) {
    std::lock_guard<std::mutex> lk(g_mutex);
    if (!g_inited) {
        return RUDP_ERROR_NOT_INITIALIZED;
    }
    if (ctxId < 0 || status == nullptr || size == 0 || size > RUDP_CONTEXT_BUFFER_STATUS_SIZE) {
        return RUDP_ERROR_INVALID_ARGUMENT;
    }
    if (!valid_ctx(ctxId)) {
        return RUDP_ERROR_INVALID_CONTEXT_ID;
    }
    std::memset(status, 0, size);
    return 0;
}

}
