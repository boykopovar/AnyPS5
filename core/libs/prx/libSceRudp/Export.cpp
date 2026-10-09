#include <atomic>
#include <cstdint>
#include <cstdio>
#include <cstddef>
#include <cstring>
#include <map>
#include <mutex>
#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"

// Offline reliable-UDP: the library initialises and contexts can be created and bound, but no connection can
// ever be established because there is no network.

namespace {
constexpr int RUDP_ERROR_NOT_INITIALIZED = static_cast<int>(0x80770001u);
constexpr int RUDP_ERROR_ALREADY_INITIALIZED = static_cast<int>(0x80770002u);
constexpr int RUDP_ERROR_INVALID_CONTEXT_ID = static_cast<int>(0x80770003u);
constexpr int RUDP_ERROR_INVALID_ARGUMENT = static_cast<int>(0x80770004u);
constexpr int RUDP_ERROR_CONN_RESET = static_cast<int>(0x80770009u);
constexpr int RUDP_ERROR_CONN_REFUSED = static_cast<int>(0x8077000Au);

using GuestRudpEventHandler = void(APS5_VABI*)(int, int, int, void*);

std::mutex g_mutex;
bool g_inited = false;
int g_next_ctx = 0;
std::map<int, bool> g_ctx;  // id -> bound
GuestRudpEventHandler g_handler = nullptr;
void* g_handler_arg = nullptr;

bool valid_ctx(int ctx) {
    return g_ctx.count(ctx) != 0;
}
}  // namespace

extern "C" {

int APS5_VABI sceRudpInit_nid_postfix(void* mem_pool, int mem_pool_size) {
    (void)mem_pool;
    (void)mem_pool_size;
    std::lock_guard<std::mutex> lk(g_mutex);
    if (g_inited) {
        return RUDP_ERROR_ALREADY_INITIALIZED;
    }
    g_inited = true;
    return 0;
}

int APS5_VABI sceRudpActivate() {
    return 0;
}

int APS5_VABI sceRudpGetStatus(void* status, std::size_t size) {
    std::lock_guard<std::mutex> lk(g_mutex);
    if (!g_inited) {
        return RUDP_ERROR_NOT_INITIALIZED;
    }
    if (status != nullptr && size != 0) {
        std::memset(status, 0, size);  // state 0 = idle, no connections
    }
    return 0;
}

int APS5_VABI sceRudpEnd(int ctx_id) {
    std::lock_guard<std::mutex> lk(g_mutex);
    if (!g_inited) {
        return RUDP_ERROR_NOT_INITIALIZED;
    }
    return g_ctx.erase(ctx_id) != 0 ? 0 : RUDP_ERROR_INVALID_CONTEXT_ID;
}

int APS5_VABI sceRudpTerminate(void) {
    std::lock_guard<std::mutex> lk(g_mutex);
    if (!g_inited) {
        return RUDP_ERROR_NOT_INITIALIZED;
    }
    g_inited = false;
    g_ctx.clear();
    return 0;
}

int APS5_VABI sceRudpEnableInternalIOThread(uint32_t stack_size, uint32_t priority) {
    (void)stack_size;
    (void)priority;
    std::lock_guard<std::mutex> lk(g_mutex);
    return g_inited ? 0 : RUDP_ERROR_NOT_INITIALIZED;
}

int APS5_VABI sceRudpSetEventHandler(GuestRudpEventHandler handler, void* arg) {
    std::lock_guard<std::mutex> lk(g_mutex);
    g_handler = handler;
    g_handler_arg = arg;
    return 0;
}

int APS5_VABI sceRudpCreateContext(GuestRudpEventHandler handler, void* arg, int* ctx_id) {
    (void)handler;
    (void)arg;
    if (ctx_id == nullptr) {
        return RUDP_ERROR_INVALID_ARGUMENT;
    }
    std::lock_guard<std::mutex> lk(g_mutex);
    if (!g_inited) {
        return RUDP_ERROR_NOT_INITIALIZED;
    }
    const int id = g_next_ctx++;
    g_ctx[id] = false;
    *ctx_id = id;
    return 0;
}

int APS5_VABI sceRudpBind(int ctx_id, uint16_t vport, int transport, uint16_t port) {
    (void)vport;
    (void)transport;
    (void)port;
    std::lock_guard<std::mutex> lk(g_mutex);
    if (!g_inited) {
        return RUDP_ERROR_NOT_INITIALIZED;
    }
    if (!valid_ctx(ctx_id)) {
        return RUDP_ERROR_INVALID_CONTEXT_ID;
    }
    g_ctx[ctx_id] = true;
    return 0;
}

int APS5_VABI sceRudpSetOption(int ctx_id, int option, const void* value, uint32_t len) {
    (void)option;
    (void)value;
    (void)len;
    std::lock_guard<std::mutex> lk(g_mutex);
    if (!g_inited) {
        return RUDP_ERROR_NOT_INITIALIZED;
    }
    return valid_ctx(ctx_id) ? 0 : RUDP_ERROR_INVALID_CONTEXT_ID;
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
}
