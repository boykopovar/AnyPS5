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
    std::uint8_t* ca = nullptr;
    std::size_t caLength = 0;
    std::uint8_t* cert = nullptr;
    std::size_t certLength = 0;
    std::uint32_t protocols = 24;
    int verifyClient = 0;
    int verifyName = 1;
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

void ClearMemory(std::uint8_t*& bytes, std::size_t& length) {
    std::free(bytes);
    bytes = nullptr;
    length = 0;
}

int SetMemory(std::uint8_t*& destination, std::size_t& destinationLength,
              const std::uint8_t* source, std::size_t length) {
    ClearMemory(destination, destinationLength);
    if (length && !source) return -1;
    if (!length) return 0;
    auto* copy = static_cast<std::uint8_t*>(std::malloc(length));
    if (!copy) return -1;
    std::memcpy(copy, source, length);
    destination = copy;
    destinationLength = length;
    return 0;
}

bool EqualsAscii(const char* text, std::size_t length, const char* name) {
    for (std::size_t i = 0; i < length; ++i) {
        if (!name[i]) return false;
        const auto c = static_cast<unsigned char>(text[i]);
        const auto n = static_cast<unsigned char>(name[i]);
        const auto lower = [](unsigned char value) -> unsigned char {
            return (value >= 'A' && value <= 'Z') ? static_cast<unsigned char>(value + ('a' - 'A')) : value;
        };
        if (lower(c) != lower(n)) return false;
    }
    return name[length] == '\0';
}

int ParseProtocols(std::uint32_t* protocols, const char* text) {
    if (!protocols) return -1;
    constexpr std::uint32_t all = (1U << 3) | (1U << 4);
    if (!text) {
        *protocols = all;
        return 0;
    }
    std::uint32_t result = 0;
    const char* start = text;
    for (;;) {
        const char* end = start;
        while (*end && *end != ',' && *end != ':') ++end;
        const char* word = start;
        while (word != end && (*word == ' ' || *word == '\t')) ++word;
        const bool negate = word != end && *word == '!';
        if (negate) ++word;
        if (negate && result == 0) result = all;
        const auto length = static_cast<std::size_t>(end - word);
        std::uint32_t flags = 0;
        if (EqualsAscii(word, length, "all") || EqualsAscii(word, length, "legacy") ||
            EqualsAscii(word, length, "default") || EqualsAscii(word, length, "secure") ||
            EqualsAscii(word, length, "tlsv1")) flags = all;
        else if (EqualsAscii(word, length, "tlsv1.0")) flags = 1U << 1;
        else if (EqualsAscii(word, length, "tlsv1.1")) flags = 1U << 2;
        else if (EqualsAscii(word, length, "tlsv1.2")) flags = 1U << 3;
        else if (EqualsAscii(word, length, "tlsv1.3")) flags = 1U << 4;
        if (!flags) return -1;
        if (negate) result &= ~flags;
        else result |= flags;
        if (!*end) break;
        start = end + 1;
    }
    *protocols = result;
    return 0;
}

void Retain(ConfigStorage* config) {
    config->references.fetch_add(1, std::memory_order_relaxed);
}

void Release(ConfigStorage* config) {
    if (!config) return;
    if (config->references.fetch_sub(1, std::memory_order_acq_rel) != 1 || config == &DefaultConfig())
        return;
    ClearKey(*config);
    ClearMemory(config->ca, config->caLength);
    ClearMemory(config->cert, config->certLength);
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
        state->error = replacement->certLength ? "failed to load certificate" :
                       replacement->keyLength ? "failed to read private key" : "private/public key mismatch";
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

int APS5_VABI tls_config_set_ca_mem_nid_postfix(TlsConfig* config, const std::uint8_t* ca, std::size_t length) {
    if (!config) return -1;
    auto* storage = Storage(config);
    return SetMemory(storage->ca, storage->caLength, ca, length);
}

int APS5_VABI tls_config_set_cert_mem_nid_postfix(TlsConfig* config, const std::uint8_t* cert, std::size_t length) {
    if (!config) return -1;
    auto* storage = Storage(config);
    return SetMemory(storage->cert, storage->certLength, cert, length);
}

int APS5_VABI tls_config_set_protocols_nid_postfix(TlsConfig* config, std::uint32_t protocols) {
    if (!config) return -1;
    Storage(config)->protocols = protocols;
    return 0;
}

void APS5_VABI tls_config_verify_client_nid_postfix(TlsConfig* config) {
    if (config) Storage(config)->verifyClient = 1;
}

int APS5_VABI tls_config_parse_protocols_nid_postfix(std::uint32_t* protocols, const char* text) {
    return ParseProtocols(protocols, text);
}

void APS5_VABI tls_config_insecure_noverifyname_nid_postfix(TlsConfig* config) {
    if (config) Storage(config)->verifyName = 0;
}

}
