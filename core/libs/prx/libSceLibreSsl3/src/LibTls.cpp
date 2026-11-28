/* Derived from LibreSSL 4.0.0 libtls tls.c and tls_server.c at
 * https://github.com/libressl/openbsd/blob/ce488b78e0bd994780569df718596b9776ff681a/src/lib/libtls/tls.c
 * https://github.com/libressl/openbsd/blob/ce488b78e0bd994780569df718596b9776ff681a/src/lib/libtls/tls_server.c */
/* Altered: context roles, server configuration validation and accepted contexts are mapped to the mbedTLS transport. */
/* $OpenBSD: tls_server.c,v 1.51 2024/03/26 08:54:48 joshua Exp $ */
/*
 * Copyright (c) 2014 Joel Sing <jsing@openbsd.org>
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */
#include "prx/libSceLibreSsl3/include/LibTls.hpp"

#include <atomic>
#include <cstdlib>
#include <cstring>
#include <initializer_list>
#include <new>
#include <stdexcept>
#include <string>

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
    char* ciphers = nullptr;
    int verifyClient = 0;
    int verifyName = 1;
    const char* error = nullptr;
};

struct ContextStorage {
    ConfigStorage* config;
    TlsRole role;
    std::string error;
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
    if (!source || !length) return 0;
    auto* copy = static_cast<std::uint8_t*>(std::malloc(length));
    if (!copy) return -1;
    std::memcpy(copy, source, length);
    destination = copy;
    destinationLength = length;
    return 0;
}

constexpr const char* CiphersDefault = "TLSv1.3:TLSv1.2+AEAD+ECDHE:TLSv1.2+AEAD+DHE";
constexpr const char* CiphersCompat = "HIGH:!aNULL";
constexpr const char* CiphersLegacy = "HIGH:MEDIUM:!aNULL";
constexpr const char* CiphersAll = "ALL:!aNULL:!eNULL";

char LowerAscii(char value) {
    return value >= 'A' && value <= 'Z' ? static_cast<char>(value - 'A' + 'a') : value;
}

bool EqualsIgnoringCase(const char* left, const char* right) {
    for (; *left && *right; ++left, ++right)
        if (LowerAscii(*left) != LowerAscii(*right)) return false;
    return *left == *right;
}

const char* ResolveCiphers(const char* ciphers) {
    if (!ciphers || EqualsIgnoringCase(ciphers, "default") || EqualsIgnoringCase(ciphers, "secure")) return CiphersDefault;
    if (EqualsIgnoringCase(ciphers, "compat")) return CiphersCompat;
    if (EqualsIgnoringCase(ciphers, "legacy")) return CiphersLegacy;
    if (EqualsIgnoringCase(ciphers, "all") || EqualsIgnoringCase(ciphers, "insecure")) return CiphersAll;
    return ciphers;
}

bool IsPresetCiphers(const char* ciphers) {
    for (const char* preset : {CiphersDefault, CiphersCompat, CiphersLegacy, CiphersAll})
        if (std::strcmp(ciphers, preset) == 0) return true;
    return false;
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
    std::free(config->ciphers);
    delete config;
}

TlsContext* NewContext(TlsRole role) {
    auto* context = new (std::nothrow) ContextStorage{&DefaultConfig(), role, {}};
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
    return NewContext(TlsRole::Client);
}

TlsContext* APS5_VABI tls_server_nid_postfix() {
    return NewContext(TlsRole::Server);
}

void APS5_VABI tls_free_nid_postfix(TlsContext* context) {
    if (!context) return;
    auto* state = Storage(context);
    TlsTransportFree_nid_no_patch(context);
    Release(state->config);
    delete state;
}

const char* APS5_VABI tls_error_nid_postfix(TlsContext* context) {
    if (!context) return nullptr;
    const auto& error = Storage(context)->error;
    return error.empty() ? nullptr : error.c_str();
}

int APS5_VABI tls_configure_nid_postfix(TlsContext* context, TlsConfig* config) {
    if (!context) return -1;
    auto* state = Storage(context);
    auto* replacement = config ? Storage(config) : &DefaultConfig();
    Retain(replacement);
    Release(state->config);
    state->config = replacement;
    if (state->role == TlsRole::Server) {
        TlsTransportConfig settings{};
        settings.cert = replacement->cert;
        settings.certLength = replacement->certLength;
        settings.key = replacement->key;
        settings.keyLength = replacement->keyLength;
        settings.verifyClient = replacement->verifyClient != 0;
        char message[256];
        if (TlsTransportValidateServer_nid_no_patch(&settings, message, sizeof(message)) != 0) {
            state->error = message;
            return -1;
        }
    }
    return 0;
}

int APS5_VABI tls_close_nid_postfix(TlsContext* context) {
    if (!context) return -1;
    auto* state = Storage(context);
    state->error.clear();
    if (state->role == TlsRole::Server) {
        state->error = "invalid operation for context";
        return -1;
    }
    return TlsTransportClose_nid_no_patch(context);
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

int APS5_VABI tls_config_set_ciphers_nid_postfix(TlsConfig* config, const char* ciphers) {
    if (!config) return -1;
    const char* resolved = ResolveCiphers(ciphers);
    if (!IsPresetCiphers(resolved))
        throw std::runtime_error("tls_config_set_ciphers: validating a cipher string other than the libtls presets is not implemented");
    auto& state = *Storage(config);
    std::free(state.ciphers);
    state.ciphers = nullptr;
    const std::size_t size = std::strlen(resolved) + 1;
    auto* copy = static_cast<char*>(std::malloc(size));
    if (!copy) return -1;
    std::memcpy(copy, resolved, size);
    state.ciphers = copy;
    return 0;
}

void APS5_VABI tls_config_verify_client_nid_postfix(TlsConfig* config) {
    if (config) Storage(config)->verifyClient = 1;
}

void APS5_VABI tls_config_insecure_noverifyname_nid_postfix(TlsConfig* config) {
    if (config) Storage(config)->verifyName = 0;
}

bool TlsTransportGetConfig_nid_no_patch(TlsContext* context, TlsTransportConfig* config) {
    if (!context || !config) return false;
    const auto* state = Storage(context);
    config->ca = state->config->ca;
    config->caLength = state->config->caLength;
    config->cert = state->config->cert;
    config->certLength = state->config->certLength;
    config->key = state->config->key;
    config->keyLength = state->config->keyLength;
    config->ciphers = state->config->ciphers;
    config->protocols = state->config->protocols;
    config->role = state->role;
    config->verifyClient = state->config->verifyClient != 0;
    config->verifyName = state->config->verifyName != 0;
    return true;
}

void TlsTransportSetError_nid_no_patch(TlsContext* context, const char* error) {
    if (context) Storage(context)->error = error ? error : "TLS transport failed";
}

void TlsTransportClearError_nid_no_patch(TlsContext* context) {
    if (context) Storage(context)->error.clear();
}

TlsContext* TlsTransportAcceptContext_nid_no_patch(TlsContext* context) {
    if (!context) return nullptr;
    auto* state = Storage(context);
    if (state->role != TlsRole::Server) return nullptr;
    auto* accepted = new (std::nothrow) ContextStorage{state->config, TlsRole::ServerConnection, {}};
    if (!accepted) return nullptr;
    Retain(accepted->config);
    return Public(accepted);
}

}
