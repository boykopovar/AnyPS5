// Adapted from the LibreSSL 2.5.4 release tarball (libressl-2.5.4.tar.gz, sha256 107a5b522fbb8318d4c3be668075e5e607296f0a9255d71674caa94571336efa),
// the version identified in docs/dev/debt/libSceLibreSsl.md, which corresponds to the openbsd
// mirror commit 2e2c612066aa993717528a0249c15c51128d61cb. Source file used:
//   ssl/bio_ssl.c - ssl_read, ssl_write, ssl_ctrl, ssl_free
// The original copyright and licence notice of that file follows unchanged.
/* $OpenBSD: bio_ssl.c,v 1.27 2017/02/07 02:08:38 beck Exp $ */
/* Copyright (C) 1995-1998 Eric Young (eay@cryptsoft.com)
 * All rights reserved.
 *
 * This package is an SSL implementation written
 * by Eric Young (eay@cryptsoft.com).
 * The implementation was written so as to conform with Netscapes SSL.
 *
 * This library is free for commercial and non-commercial use as long as
 * the following conditions are aheared to.  The following conditions
 * apply to all code found in this distribution, be it the RC4, RSA,
 * lhash, DES, etc., code; not just the SSL code.  The SSL documentation
 * included with this distribution is covered by the same copyright terms
 * except that the holder is Tim Hudson (tjh@cryptsoft.com).
 *
 * Copyright remains Eric Young's, and as such any Copyright notices in
 * the code are not to be removed.
 * If this package is used in a product, Eric Young should be given attribution
 * as the author of the parts of the library used.
 * This can be in the form of a textual message at program startup or
 * in documentation (online or textual) provided with the package.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *    "This product includes cryptographic software written by
 *     Eric Young (eay@cryptsoft.com)"
 *    The word 'cryptographic' can be left out if the rouines from the library
 *    being used are not cryptographic related :-).
 * 4. If you include any Windows specific code (or a derivative thereof) from
 *    the apps directory (application code) you must include an acknowledgement:
 *    "This product includes software written by Tim Hudson (tjh@cryptsoft.com)"
 *
 * THIS SOFTWARE IS PROVIDED BY ERIC YOUNG ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * The licence and distribution terms for any publically available version or
 * derivative of this code cannot be changed.  i.e. this code cannot simply be
 * copied and put under another distribution licence
 * [including the GNU Public Licence.]
 */

#include "prx/libSceLibreSsl/libSceLibreSsl.hpp"
#include "prx/libc/include/Api.hpp"
#include "prx/libSceLibreSsl/ErrorQueue.hpp"
#include "prx/libSceLibreSsl/include/SslInternal.hpp"
#include "prx/libc/include/General.hpp"
#include <algorithm>
#include <atomic>
#include <cerrno>
#include <cstdint>
#include <cstring>
#include <limits>
#include <stdexcept>
#include <string>
#include <vector>


namespace {

using LibreSsl::Bio;
using LibreSsl::BioBuffer;
using LibreSsl::BioMethod;

constexpr int ReadOnly = 0x200;
constexpr int RetryFlags = 0xf;
constexpr int RetryRead = 0x9;
constexpr std::size_t MaximumGrowth = 0x5ffffffc;
constexpr int BioLibrary = 32;
constexpr int BufferLibrary = 7;
constexpr int AllocationFailure = 65;
constexpr int NullParameter = 115;
constexpr int Uninitialized = 120;
constexpr int UnsupportedMethod = 121;
constexpr int ReadOnlyWrite = 126;
constexpr int SslLibrary = 20;
constexpr int SslBioType = 0x207;
constexpr int SslBioSetSsl = 109;
constexpr int SslBioGetSsl = 110;
constexpr int SslBioGetRenegotiates = 126;
constexpr int SslBioGetClose = 8;
constexpr int SslBioSetClose = 9;
constexpr int SslBioInfo = 3;
constexpr int SslBioSetCallback = 14;
constexpr int CtrlPending = 10;
constexpr int CtrlFlush = 11;
constexpr int CtrlWritePending = 13;
constexpr int SslBioDoStateMachine = 101;
constexpr int RetryX509Lookup = 0x01;
constexpr int RetryConnect = 0x02;
constexpr int RetryAccept = 0x03;

struct BioSslData {
    void* ssl;
    int renegotiates;
    std::uint64_t renegotiateCount;
    std::uint64_t byteCount;
    std::uint64_t renegotiateTimeout;
    std::int64_t lastTime;
};

static_assert(sizeof(BioSslData) == 48);
static_assert(offsetof(BioSslData, ssl) == 0);
static_assert(offsetof(BioSslData, renegotiates) == 8);
static_assert(offsetof(BioSslData, renegotiateCount) == 16);
static_assert(offsetof(BioSslData, byteCount) == 24);
static_assert(offsetof(BioSslData, renegotiateTimeout) == 32);
static_assert(offsetof(BioSslData, lastTime) == 40);

int queueError(int library, int reason, int result) {
    LibreSslPutError_nid_no_patch(library, 0xfff, reason, __FILE__, __LINE__);
    return result;
}

bool CopyGuestStringPreservingErrno(const char* source, std::string& destination) {
    constexpr std::uintptr_t Chunk = 4096;
    char bytes[Chunk];
    destination.clear();
    for (auto address = reinterpret_cast<std::uintptr_t>(source);;) {
        const std::size_t count = Chunk - address % Chunk;
        if (!GuestCopyIn_nid_no_patch(bytes, reinterpret_cast<const void*>(address), count)) return false;
        const auto* end = static_cast<const char*>(std::memchr(bytes, 0, count));
        destination.append(bytes, end ? static_cast<std::size_t>(end - bytes) : count);
        if (end) return true;
        address += count;
    }
}

void clearBuffer(BioBuffer& buffer) {
    auto* bytes = reinterpret_cast<volatile unsigned char*>(buffer.data);
    for (std::size_t i = 0; i < buffer.capacity; ++i) bytes[i] = 0;
}

BioBuffer& memoryBuffer(Bio* bio) {
    if (!bio || !bio->data) throw std::invalid_argument("BIO: missing memory buffer");
    return *static_cast<BioBuffer*>(bio->data);
}

int APS5_VABI createMemory(Bio* bio) {
    auto* buffer = static_cast<BioBuffer*>(calloc_nid_postfix(1, sizeof(BioBuffer)));
    if (!buffer) return queueError(BufferLibrary, AllocationFailure, 0);
    bio->data = buffer;
    bio->initialized = 1;
    bio->shutdown = 1;
    bio->number = -1;
    return 1;
}

int APS5_VABI destroyMemory(Bio* bio) {
    if (!bio) return 0;
    if (bio->shutdown && bio->initialized && bio->data) {
        auto* buffer = static_cast<BioBuffer*>(bio->data);
        if (!(bio->flags & ReadOnly)) {
            clearBuffer(*buffer);
            free_nid_postfix(buffer->data);
        }
        free_nid_postfix(buffer);
        bio->data = nullptr;
    }
    return 1;
}

BioSslData& sslData(Bio* bio) {
    if (!bio || !bio->data) throw std::invalid_argument("SSL BIO: missing state");
    return *static_cast<BioSslData*>(bio->data);
}

void requireNoRenegotiation(const BioSslData& state, int result, const char* function) {
    if (result > 0 && (state.renegotiateCount || state.renegotiateTimeout)) NotImplemented_nid_no_patch(function);
}

Bio& nextBio(Bio& bio) {
    if (!bio.next) throw std::invalid_argument("SSL BIO: missing read BIO");
    return *bio.next;
}

int applySslRetryFlags(Bio* bio, const BioSslData& state, LibreSslSsl* ssl, int result, bool handleAccept, const char* function) {
    int retryReason = 0;
    switch (SSL_get_error_nid_postfix(ssl, result)) {
    case SslLayout::SslErrorNone:
        requireNoRenegotiation(state, result, function);
        break;
    case SslLayout::SslErrorWantRead:
        bio->flags |= SslLayout::BioFlagRead | SslLayout::BioFlagShouldRetry;
        break;
    case SslLayout::SslErrorWantWrite:
        bio->flags |= SslLayout::BioFlagWrite | SslLayout::BioFlagShouldRetry;
        break;
    case SslLayout::SslErrorWantX509Lookup:
        bio->flags |= SslLayout::BioFlagIoSpecial | SslLayout::BioFlagShouldRetry;
        retryReason = RetryX509Lookup;
        break;
    case SslLayout::SslErrorWantAccept:
        if (handleAccept) {
            bio->flags |= SslLayout::BioFlagIoSpecial | SslLayout::BioFlagShouldRetry;
            retryReason = RetryAccept;
        }
        break;
    case SslLayout::SslErrorWantConnect:
        bio->flags |= SslLayout::BioFlagIoSpecial | SslLayout::BioFlagShouldRetry;
        retryReason = RetryConnect;
        break;
    default:
        break;
    }
    bio->retryReason = retryReason;
    return result;
}

int APS5_VABI readSsl(Bio* bio, char* destination, int size) {
    if (!destination) return 0;
    auto& state = sslData(bio);
    bio->flags &= ~RetryFlags;
    auto* ssl = static_cast<LibreSslSsl*>(state.ssl);
    return applySslRetryFlags(bio, state, ssl, SSL_read_nid_postfix(ssl, destination, size), true, __func__);
}

int APS5_VABI writeSsl(Bio* bio, const char* source, int size) {
    if (!source) return 0;
    auto& state = sslData(bio);
    bio->flags &= ~RetryFlags;
    auto* ssl = static_cast<LibreSslSsl*>(state.ssl);
    return applySslRetryFlags(bio, state, ssl, SSL_write_nid_postfix(ssl, source, size), false, __func__);
}

int APS5_VABI putsSsl(Bio* bio, const char* source) {
    if (!source) throw std::invalid_argument("SSL BIO puts: null string");
    std::string text;
    if (!CopyGuestStringPreservingErrno(source, text)) NotImplementedAt_nid_no_patch(__func__, __FILE__, __LINE__);
    return writeSsl(bio, text.c_str(), static_cast<int>(text.size()));
}

std::int64_t APS5_VABI controlSsl(Bio* bio, int command, std::int64_t argument, void* pointer) {
    auto& state = sslData(bio);
    if (!state.ssl && command != SslBioSetSsl) return 0;
    auto* ssl = static_cast<LibreSslSsl*>(state.ssl);
    switch (command) {
    case SslBioSetSsl:
        if (state.ssl || bio->next) NotImplemented_nid_no_patch(__func__);
        bio->shutdown = static_cast<int>(argument);
        if (!pointer) throw std::invalid_argument("SSL BIO control: null SSL");
        ssl = static_cast<LibreSslSsl*>(pointer);
        state.ssl = ssl;
        bio->initialized = 1;
        if (auto* readBio = static_cast<Bio*>(SSL_get_rbio_nid_postfix(ssl))) {
            bio->next = readBio;
            std::atomic_ref(readBio->references).fetch_add(1);
        }
        return 1;
    case SslBioInfo:
        return 0;
    case CtrlPending: {
        const int pending = SSL_pending_nid_postfix(ssl);
        if (pending) return pending;
        return static_cast<int>(BIO_ctrl_nid_postfix(static_cast<Bio*>(ssl->rbio), CtrlPending, 0, nullptr));
    }
    case CtrlWritePending:
        return BIO_ctrl_nid_postfix(static_cast<Bio*>(ssl->wbio), command, argument, pointer);
    case CtrlFlush: {
        bio->flags &= ~RetryFlags;
        const std::int64_t result = BIO_ctrl_nid_postfix(static_cast<Bio*>(ssl->wbio), command, argument, pointer);
        const Bio& next = nextBio(*bio);
        bio->flags |= next.flags & RetryFlags;
        bio->retryReason = next.retryReason;
        return result;
    }
    case SslBioGetSsl:
        if (!pointer) return 0;
        if (!GuestCopyOut_nid_no_patch(pointer, &state.ssl, sizeof(state.ssl))) NotImplementedAt_nid_no_patch(__func__, __FILE__, __LINE__);
        return 1;
    case SslBioGetClose:
        return bio->shutdown;
    case SslBioSetClose:
        bio->shutdown = static_cast<int>(argument);
        return 1;
    case SslBioGetRenegotiates:
        return state.renegotiates;
    case SslBioDoStateMachine: {
        bio->flags &= ~RetryFlags;
        bio->retryReason = 0;
        const int result = LibreSslDoHandshake_nid_no_patch(ssl);
        switch (SSL_get_error_nid_postfix(ssl, result)) {
        case SslLayout::SslErrorWantRead:
            bio->flags |= SslLayout::BioFlagRead | SslLayout::BioFlagShouldRetry;
            break;
        case SslLayout::SslErrorWantWrite:
            bio->flags |= SslLayout::BioFlagWrite | SslLayout::BioFlagShouldRetry;
            break;
        case SslLayout::SslErrorWantConnect:
            bio->flags |= SslLayout::BioFlagIoSpecial | SslLayout::BioFlagShouldRetry;
            bio->retryReason = nextBio(*bio).retryReason;
            break;
        default:
            break;
        }
        return result;
    }
    default:
        NotImplemented_nid_no_patch(__func__);
    }
}

int APS5_VABI createSsl(Bio* bio) {
    auto* state = static_cast<BioSslData*>(calloc_nid_postfix(1, sizeof(BioSslData)));
    if (!state) return queueError(SslLibrary, AllocationFailure, 0);
    bio->initialized = 0;
    bio->data = state;
    bio->flags = 0;
    return 1;
}

int APS5_VABI destroySsl(Bio* bio) {
    if (!bio) return 0;
    auto& state = sslData(bio);
    if (state.ssl) {
        auto* ssl = static_cast<LibreSslSsl*>(state.ssl);
        SSL_shutdown_nid_postfix(ssl);
        if (bio->shutdown && bio->initialized) SSL_free_nid_postfix(ssl);
    }
    if (bio->shutdown) {
        bio->initialized = 0;
        bio->flags = 0;
    }
    free_nid_postfix(&state);
    return 1;
}

std::int64_t forwardCallbackControl(Bio* bio, int command, LibreSsl::BioInfoCallback callback) {
    if (!bio) return 0;
    if (!bio->method || !bio->method->callbackControl) return queueError(BioLibrary, UnsupportedMethod, -2);
    const auto bioCallback = bio->callback;
    if (bioCallback) {
        const auto result = bioCallback(bio, 6, reinterpret_cast<const char*>(&callback), command, 0, 1);
        if (result <= 0) return result;
    }
    const auto result = bio->method->callbackControl(bio, command, callback);
    return bioCallback ? bioCallback(bio, 0x86, reinterpret_cast<const char*>(&callback), command, 0, result) : result;
}

std::int64_t APS5_VABI callbackControlSsl(Bio* bio, int command, LibreSsl::BioInfoCallback callback) {
    auto& state = sslData(bio);
    if (!state.ssl) throw std::invalid_argument("SSL BIO callback control: null SSL");
    auto& ssl = *static_cast<LibreSslSsl*>(state.ssl);
    if (command != SslBioSetCallback) return forwardCallbackControl(static_cast<Bio*>(ssl.rbio), command, callback);
    if (!ssl.internal) throw std::invalid_argument("SSL BIO callback control: null internal state");
    ssl.internal->infoCallback = reinterpret_cast<void*>(callback);
    return 1;
}

void consumeMemory(Bio* bio, BioBuffer& buffer, std::size_t count) {
    buffer.length -= count;
    if (bio->flags & ReadOnly) buffer.data += count;
    else std::memmove(buffer.data, buffer.data + count, buffer.length);
}

int APS5_VABI readMemory(Bio* bio, char* destination, int size) {
    auto& buffer = memoryBuffer(bio);
    bio->flags &= ~RetryFlags;
    const int count = size >= 0 ? static_cast<int>(std::min<std::size_t>(size, buffer.length)) : size;
    if (destination && count > 0) {
        std::vector<char> bytes(static_cast<std::size_t>(count));
        if (!GuestCopyIn_nid_no_patch(bytes.data(), buffer.data, bytes.size())) NotImplementedAt_nid_no_patch(__func__, __FILE__, __LINE__);
        if (!GuestCopyOut_nid_no_patch(destination, bytes.data(), bytes.size())) NotImplementedAt_nid_no_patch(__func__, __FILE__, __LINE__);
        consumeMemory(bio, buffer, static_cast<std::size_t>(count));
        return count;
    }
    if (buffer.length != 0) return count;
    if (bio->number != 0) bio->flags |= RetryRead;
    return bio->number;
}

int APS5_VABI writeMemory(Bio* bio, const char* source, int size) {
    auto& buffer = memoryBuffer(bio);
    if (!source) return queueError(BioLibrary, NullParameter, -1);
    if (bio->flags & ReadOnly) return queueError(BioLibrary, ReadOnlyWrite, -1);
    if (size < 0) throw std::invalid_argument("BIO: negative memory write size");
    bio->flags &= ~RetryFlags;
    const auto count = static_cast<std::size_t>(size);
    if (buffer.length > std::numeric_limits<std::size_t>::max() - count) NotImplemented_nid_no_patch(__func__);
    const std::size_t length = buffer.length + count;
    if (length > buffer.capacity) {
        if (length > MaximumGrowth) return queueError(BufferLibrary, AllocationFailure, -1);
        const std::size_t capacity = (length + 3) / 3 * 4;
        auto* data = static_cast<char*>(calloc_nid_postfix(capacity, 1));
        if (!data) return queueError(BufferLibrary, AllocationFailure, -1);
        if (count != 0 && !GuestCopyIn_nid_no_patch(data + buffer.length, source, count)) {
            BioBuffer rejected{0, data, capacity};
            clearBuffer(rejected);
            free_nid_postfix(data);
            NotImplementedAt_nid_no_patch(__func__, __FILE__, __LINE__);
        }
        if (buffer.length != 0) std::memcpy(data, buffer.data, buffer.length);
        clearBuffer(buffer);
        free_nid_postfix(buffer.data);
        buffer.data = data;
        buffer.capacity = capacity;
    } else if (count != 0) {
        const auto start = reinterpret_cast<std::uintptr_t>(source);
        const auto begin = reinterpret_cast<std::uintptr_t>(buffer.data);
        if (start >= begin && start - begin <= buffer.capacity && count <= buffer.capacity - (start - begin))
            std::memmove(buffer.data + buffer.length, buffer.data + (start - begin), count);
        else if (!GuestCopyIn_nid_no_patch(buffer.data + buffer.length, source, count))
            NotImplementedAt_nid_no_patch(__func__, __FILE__, __LINE__);
    }
    buffer.length = length;
    return size;
}

int APS5_VABI putsMemory(Bio* bio, const char* source) {
    if (!source) throw std::invalid_argument("BIO_puts: null string");
    std::string text;
    if (!CopyGuestStringPreservingErrno(source, text)) NotImplementedAt_nid_no_patch(__func__, __FILE__, __LINE__);
    if (text.size() > static_cast<std::size_t>(std::numeric_limits<int>::max())) NotImplementedAt_nid_no_patch(__func__, __FILE__, __LINE__);
    return writeMemory(bio, text.data(), static_cast<int>(text.size()));
}

int APS5_VABI getsMemory(Bio* bio, char* destination, int size) {
    auto& buffer = memoryBuffer(bio);
    if (!destination || size == std::numeric_limits<int>::min()) throw std::invalid_argument("BIO: invalid line buffer");
    bio->flags &= ~RetryFlags;
    constexpr char Terminator = '\0';
    if (size <= 1 || buffer.length == 0) {
        if (!GuestCopyOut_nid_no_patch(destination, &Terminator, 1)) NotImplementedAt_nid_no_patch(__func__, __FILE__, __LINE__);
        return 0;
    }
    const auto limit = std::min<std::size_t>(size - 1, buffer.length);
    std::vector<char> line(limit + 1);
    if (!GuestCopyIn_nid_no_patch(line.data(), buffer.data, limit)) NotImplementedAt_nid_no_patch(__func__, __FILE__, __LINE__);
    const void* newline = std::memchr(line.data(), '\n', limit);
    const std::size_t count = newline ? static_cast<const char*>(newline) - line.data() + 1 : limit;
    line[count] = Terminator;
    if (!GuestCopyOut_nid_no_patch(destination, line.data(), count + 1)) NotImplementedAt_nid_no_patch(__func__, __FILE__, __LINE__);
    consumeMemory(bio, buffer, count);
    return static_cast<int>(count);
}

std::int64_t APS5_VABI controlMemory(Bio* bio, int command, std::int64_t argument, void* pointer) {
    if (!bio) throw std::invalid_argument("BIO: null memory BIO");
    switch (command) {
    case 1: {
        auto& buffer = memoryBuffer(bio);
        if (buffer.data) {
            if (bio->flags & ReadOnly) {
                if (buffer.capacity < buffer.length) throw std::invalid_argument("BIO: invalid read-only buffer length");
                buffer.data -= buffer.capacity - buffer.length;
                buffer.length = buffer.capacity;
            } else {
                clearBuffer(buffer);
                buffer.length = 0;
            }
        }
        return 1;
    }
    case 2:
        return memoryBuffer(bio).length == 0;
    case 3:
        if (pointer && !GuestCopyOut_nid_no_patch(pointer, &memoryBuffer(bio).data, sizeof(char*))) NotImplementedAt_nid_no_patch(__func__, __FILE__, __LINE__);
        return static_cast<std::int64_t>(memoryBuffer(bio).length);
    case 8:
        return bio->shutdown;
    case 9:
        bio->shutdown = static_cast<int>(argument);
        return 1;
    case 10:
        return static_cast<std::int64_t>(memoryBuffer(bio).length);
    case 11:
    case 12:
        return 1;
    case 13:
        return 0;
    case 114:
        destroyMemory(bio);
        bio->data = pointer;
        bio->shutdown = static_cast<int>(argument);
        return 1;
    case 115:
        if (pointer && !GuestCopyOut_nid_no_patch(pointer, &bio->data, sizeof(bio->data))) NotImplementedAt_nid_no_patch(__func__, __FILE__, __LINE__);
        return 1;
    case 130:
        bio->number = static_cast<int>(argument);
        return 1;
    default:
        return 0;
    }
}

const BioMethod MemoryMethod{0x401, "memory buffer", writeMemory, readMemory, putsMemory, getsMemory, controlMemory, createMemory, destroyMemory, nullptr};
const BioMethod SslMethod{SslBioType, "ssl", writeSsl, readSsl, putsSsl, nullptr, controlSsl, createSsl, destroySsl, callbackControlSsl};

}  // namespace

extern "C" {

const LibreSsl::BioMethod* APS5_VABI BIO_s_mem_nid_postfix() {
    return &MemoryMethod;
}

const LibreSsl::BioMethod* APS5_VABI BIO_f_ssl_nid_postfix() {
    return &SslMethod;
}

LibreSsl::Bio* APS5_VABI BIO_new_nid_postfix(const LibreSsl::BioMethod* method) {
    if (!method) throw std::invalid_argument("BIO_new: null method");
    auto* bio = static_cast<Bio*>(calloc_nid_postfix(1, sizeof(Bio)));
    if (!bio) {
        queueError(BioLibrary, AllocationFailure, 0);
        return nullptr;
    }
    bio->method = method;
    bio->shutdown = 1;
    bio->references = 1;
    try {
        if (!method->create || method->create(bio)) return bio;
    } catch (...) {
        free_nid_postfix(bio);
        throw;
    }
    free_nid_postfix(bio);
    return nullptr;
}

LibreSsl::Bio* APS5_VABI BIO_new_mem_buf_nid_postfix(const void* source, int size) {
    if (!source) {
        queueError(BioLibrary, NullParameter, 0);
        return nullptr;
    }
    auto length = static_cast<std::size_t>(size);
    if (size < 0) {
        std::string text;
        if (!CopyGuestStringPreservingErrno(static_cast<const char*>(source), text)) NotImplementedAt_nid_no_patch(__func__, __FILE__, __LINE__);
        length = text.size();
    }
    auto* bio = BIO_new_nid_postfix(&MemoryMethod);
    if (!bio) return nullptr;
    auto& buffer = memoryBuffer(bio);
    buffer.data = const_cast<char*>(static_cast<const char*>(source));
    buffer.length = length;
    buffer.capacity = buffer.length;
    bio->flags |= ReadOnly;
    bio->number = 0;
    return bio;
}

int APS5_VABI BIO_free_nid_postfix(LibreSsl::Bio* bio) {
    if (!bio) return 0;
    if (bio->extraData) NotImplemented_nid_no_patch(__func__);
    if (std::atomic_ref(bio->references).fetch_sub(1) > 1) return 1;
    if (bio->callback) {
        const int result = static_cast<int>(bio->callback(bio, 1, nullptr, 0, 0, 1));
        if (result <= 0) return result;
    }
    if (bio->extraData) NotImplemented_nid_no_patch(__func__);
    if (bio->method && bio->method->destroy) bio->method->destroy(bio);
    free_nid_postfix(bio);
    return 1;
}

std::int64_t APS5_VABI BIO_ctrl_nid_postfix(LibreSsl::Bio* bio, int command, std::int64_t argument, void* pointer) {
    if (!bio) return 0;
    if (!bio->method || !bio->method->control) return queueError(BioLibrary, UnsupportedMethod, -2);
    const auto callback = bio->callback;
    if (callback) {
        const auto result = callback(bio, 6, static_cast<const char*>(pointer), command, argument, 1);
        if (result <= 0) return result;
    }
    const auto result = bio->method->control(bio, command, argument, pointer);
    return callback ? callback(bio, 0x86, static_cast<const char*>(pointer), command, argument, result) : result;
}

int LibreSslBioWrite_nid_no_patch(LibreSsl::Bio* bio, const void* source, int size) {
    if (!bio || size < 0) return 0;
    if (!bio->method || !bio->method->write) return queueError(BioLibrary, UnsupportedMethod, -2);
    const auto callback = bio->callback;
    if (callback) {
        const int result = static_cast<int>(callback(bio, 3, static_cast<const char*>(source), size, 0, 1));
        if (result <= 0) return result;
    }
    if (!bio->initialized) return queueError(BioLibrary, Uninitialized, -2);
    const int result = bio->method->write(bio, static_cast<const char*>(source), size);
    if (result > 0) bio->bytesWritten += static_cast<std::uint64_t>(result);
    return callback ? static_cast<int>(callback(bio, 0x83, static_cast<const char*>(source), size, 0, result)) : result;
}

int APS5_VABI BIO_puts_nid_postfix(LibreSsl::Bio* bio, const char* source) {
    if (!bio || !bio->method || !bio->method->puts) return queueError(BioLibrary, UnsupportedMethod, -2);
    const auto callback = bio->callback;
    if (callback) {
        const int result = static_cast<int>(callback(bio, 4, source, 0, 0, 1));
        if (result <= 0) return result;
    }
    if (!bio->initialized) return queueError(BioLibrary, Uninitialized, -2);
    const int result = bio->method->puts(bio, source);
    if (result > 0) bio->bytesWritten += static_cast<std::uint64_t>(result);
    return callback ? static_cast<int>(callback(bio, 0x84, source, 0, 0, result)) : result;
}

}
