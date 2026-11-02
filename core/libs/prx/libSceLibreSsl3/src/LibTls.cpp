#include "prx/libSceLibreSsl3/include/LibTls.hpp"

#include <atomic>
#include <cstdlib>
#include <cstring>
#include <new>

namespace {

struct ConfigStorage {
    std::atomic<std::uint32_t> references{1};
    std::uint8_t* key = nullptr;
    std::size_t keyLength = 0;
    const char* error = nullptr;
};

enum class Role { Client, Server };

struct ContextStorage {
    ConfigStorage* config;
    Role role;
    const char* error;
};

ConfigStorage* Storage(TlsConfig* config) {
    return reinterpret_cast<ConfigStorage*>(config);
}

ContextStorage* Storage(TlsContext* context) {
    return reinterpret_cast<ContextStorage*>(context);
}

TlsConfig* Public(ConfigStorage* config) {
    return reinterpret_cast<TlsConfig*>(config);
}

TlsContext* Public(ContextStorage* context) {
    return reinterpret_cast<TlsContext*>(context);
}

ConfigStorage& DefaultConfig() {
    static ConfigStorage config;
    return config;
}

void ClearKey(ConfigStorage& config) {
    if (config.key) {
        volatile auto* bytes = config.key;
        for (std::size_t index = 0; index < config.keyLength; ++index)
            bytes[index] = 0;
        std::free(config.key);
    }
    config.key = nullptr;
    config.keyLength = 0;
}

void Retain(ConfigStorage* config) {
    config->references.fetch_add(1, std::memory_order_relaxed);
}

void Release(ConfigStorage* config) {
    if (!config) return;
    if (config->references.fetch_sub(1, std::memory_order_acq_rel) != 1 || config == &DefaultConfig())
        return;
    ClearKey(*config);
    delete config;
}

TlsContext* NewContext(Role role) {
    auto* context = new (std::nothrow) ContextStorage{&DefaultConfig(), role, nullptr};
    if (!context) return nullptr;
    Retain(context->config);
    return Public(context);
}

int SetKey(TlsConfig* config, const std::uint8_t* key, std::size_t length) {
    if (!config) return -1;
    auto& state = *Storage(config);
    ClearKey(state);
    if (!key || length == 0) return 0;
    auto* copy = static_cast<std::uint8_t*>(std::malloc(length));
    if (!copy) {
        state.error = "out of memory";
        return -1;
    }
    std::memcpy(copy, key, length);
    state.key = copy;
    state.keyLength = length;
    return 0;
}

}

extern "C" {

TlsConfig* APS5_VABI tls_config_new_nid_postfix() {
    return Public(new (std::nothrow) ConfigStorage);
}

void APS5_VABI tls_config_free_nid_postfix(TlsConfig* config) {
    Release(config ? Storage(config) : nullptr);
}

const char* APS5_VABI tls_config_error_nid_postfix(TlsConfig* config) {
    return config ? Storage(config)->error : nullptr;
}

int APS5_VABI tls_config_set_key_mem_nid_postfix(TlsConfig* config, const std::uint8_t* key,
                                                  std::size_t length) {
    return SetKey(config, key, length);
}

TlsContext* APS5_VABI tls_client_nid_postfix() {
    return NewContext(Role::Client);
}

TlsContext* APS5_VABI tls_server_nid_postfix() {
    return NewContext(Role::Server);
}

void APS5_VABI tls_free_nid_postfix(TlsContext* context) {
    if (!context) return;
    auto* state = Storage(context);
    Release(state->config);
    delete state;
}

const char* APS5_VABI tls_error_nid_postfix(TlsContext* context) {
    return context ? Storage(context)->error : nullptr;
}

int APS5_VABI tls_configure_nid_postfix(TlsContext* context, TlsConfig* config) {
    if (!context) return -1;
    auto* state = Storage(context);
    auto* replacement = config ? Storage(config) : &DefaultConfig();
    Retain(replacement);
    Release(state->config);
    state->config = replacement;
    if (state->role == Role::Server) {
        state->error = replacement->keyLength ? "failed to read private key" : "private/public key mismatch";
        return -1;
    }
    return 0;
}

int APS5_VABI tls_close_nid_postfix(TlsContext* context) {
    if (!context) return -1;
    auto* state = Storage(context);
    state->error = nullptr;
    if (state->role == Role::Server) {
        state->error = "invalid operation for context";
        return -1;
    }
    return 0;
}

}
