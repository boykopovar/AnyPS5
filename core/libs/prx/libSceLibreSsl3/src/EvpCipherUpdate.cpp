/* $OpenBSD: evp_cipher.c,v 1.23 2024/04/10 15:00:38 beck Exp $ */
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
/* ====================================================================
 * Copyright (c) 2015 The OpenSSL Project.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the
 *    distribution.
 *
 * 3. All advertising materials mentioning features or use of this
 *    software must display the following acknowledgment:
 *    "This product includes software developed by the OpenSSL Project
 *    for use in the OpenSSL Toolkit. (http://www.OpenSSL.org/)"
 *
 * 4. The names "OpenSSL Toolkit" and "OpenSSL Project" must not be used to
 *    endorse or promote products derived from this software without
 *    prior written permission. For written permission, please contact
 *    licensing@OpenSSL.org.
 *
 * 5. Products derived from this software may not be called "OpenSSL"
 *    nor may "OpenSSL" appear in their names without prior written
 *    permission of the OpenSSL Project.
 *
 * 6. Redistributions of any form whatsoever must retain the following
 *    acknowledgment:
 *    "This product includes software developed by the OpenSSL Project
 *    for use in the OpenSSL Toolkit (http://www.OpenSSL.org/)"
 *
 * THIS SOFTWARE IS PROVIDED BY THE OpenSSL PROJECT ``AS IS'' AND ANY
 * EXPRESSED OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE OpenSSL PROJECT OR
 * ITS CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED
 * OF THE POSSIBILITY OF SUCH DAMAGE.
 * ====================================================================
 *
 * This product includes cryptographic software written by Eric Young
 * (eay@cryptsoft.com).  This product includes software written by Tim
 * Hudson (tjh@cryptsoft.com).
 *
 */
// Derived from LibreSSL 4.0.0 src/lib/libcrypto/evp/evp_cipher.c: https://github.com/libressl/openbsd/blob/ce488b78e0bd994780569df718596b9776ff681a/src/lib/libcrypto/evp/evp_cipher.c

#include "prx/libSceLibreSsl3/libSceLibreSsl3.hpp"
#include "prx/libSceLibreSsl/include/Error.hpp"
#include "prx/libSceLibreSsl/include/Evp.hpp"

#include "prx/libc/include/General.hpp"

#include <cerrno>
#include <climits>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <limits>
#include <stdexcept>
#include <string>
#ifdef _WIN32
#include <windows.h>
#else
#include <unistd.h>
#endif

namespace {

constexpr std::uint64_t CipherModeCcm = 0x7;
constexpr int EvpLibrary = 6;
constexpr int AnyFunction = 0xfff;
constexpr int BadBlockLength = 136;
constexpr int TooLarge = 164;

struct ErrnoGuard {
    int saved = errno;
    ~ErrnoGuard() { errno = saved; }
};

std::size_t HostPageSize() {
    static const std::size_t size = [] {
#ifdef _WIN32
        SYSTEM_INFO info{};
        GetSystemInfo(&info);
        const std::size_t page = info.dwPageSize;
#else
        const long raw = sysconf(_SC_PAGESIZE);
        const std::size_t page = raw > 0 ? static_cast<std::size_t>(raw) : 4096u;
#endif
        return page != 0 && (page & (page - 1)) == 0 ? page : 4096u;
    }();
    return size;
}

int Mask(int blockSize) {
    return static_cast<int>(static_cast<unsigned>(blockSize) - 1u);
}

bool GuestReadableRange(const void* source, std::size_t bytes) {
    if (bytes == 0) return true;
    const auto start = reinterpret_cast<std::uintptr_t>(source);
    if (start == 0 || bytes > std::numeric_limits<std::uintptr_t>::max() - start) return false;
    const std::size_t pageSize = HostPageSize();
    const std::uintptr_t end = start + bytes;
    unsigned char probe = 0;
    if (!GuestCopyIn_nid_no_patch(&probe, reinterpret_cast<const void*>(start), 1)) return false;
    std::uintptr_t page = start & ~(static_cast<std::uintptr_t>(pageSize) - 1u);
    while (end - page > pageSize) {
        page += pageSize;
        if (!GuestCopyIn_nid_no_patch(&probe, reinterpret_cast<const void*>(page), 1)) return false;
    }
    return true;
}

bool GuestWritableRange(void* destination, std::size_t bytes) {
    if (bytes == 0) return true;
    const auto start = reinterpret_cast<std::uintptr_t>(destination);
    if (start == 0 || bytes > std::numeric_limits<std::uintptr_t>::max() - start) return false;
    const std::size_t pageSize = HostPageSize();
    const std::uintptr_t end = start + bytes;
    auto probeAt = [](std::uintptr_t address) {
        unsigned char probe = 0;
        return GuestCopyInUnsynced_nid_no_patch(&probe, reinterpret_cast<const void*>(address), 1) &&
               GuestCopyOut_nid_no_patch(reinterpret_cast<void*>(address), &probe, 1);
    };
    if (!probeAt(start)) return false;
    std::uintptr_t page = start & ~(static_cast<std::uintptr_t>(pageSize) - 1u);
    while (end - page > pageSize) {
        page += pageSize;
        if (!probeAt(page)) return false;
    }
    return true;
}

struct BufferExtents {
    std::size_t inBytes = 0;
    std::size_t outBytes = 0;
    bool allowNullIn = false;
    bool allowNullOut = false;
};

BufferExtents ComputeExtents(const LibreSslEvpCipherCtx& context, const LibreSslEvpCipher& cipher, int length, bool decrypt) {
    const int blockSize = cipher.blockSize;
    const int blockMask = Mask(blockSize);
    if ((blockSize & blockMask) != 0 || length < 0) return {};
    if (length == 0 && (cipher.flags & CipherModeMask) != CipherModeCcm) return {};
    if (cipher.flags & CipherFlagCustomCipher) {
        if (length == 0) return {};
        return {static_cast<std::size_t>(length), static_cast<std::size_t>(length), true, true};
    }
    const bool paddedDecrypt = decrypt && !(context.flags & CipherFlagNoPadding);
    std::size_t outBytes = 0;
    if (paddedDecrypt) {
        if (static_cast<std::size_t>(blockSize) > sizeof(context.final)) return {};
        if (context.finalUsed) {
            if ((length & ~blockMask) > INT_MAX - blockSize) return {};
            outBytes = static_cast<std::size_t>(blockSize);
        }
    }
    if (length == 0) return {0, outBytes, false, false};
    const int partial = context.bufLen;
    if (partial == 0 && (length & blockMask) == 0) {
        return {static_cast<std::size_t>(length), outBytes + static_cast<std::size_t>(length), false, false};
    }
    if (partial < 0 || partial >= blockSize || static_cast<std::size_t>(blockSize) > sizeof(context.buf)) {
        return {0, outBytes, false, false};
    }
    if (partial > 0) {
        const int needed = blockSize - partial;
        if (needed > length) return {static_cast<std::size_t>(length), outBytes, false, false};
        if (((length - needed) & ~blockMask) > INT_MAX - blockSize) return {0, outBytes, false, false};
        const std::size_t encryptOut = static_cast<std::size_t>(blockSize) + static_cast<std::size_t>((length - needed) & ~blockMask);
        return {static_cast<std::size_t>(length), outBytes + encryptOut, false, false};
    }
    const std::size_t encryptOut = static_cast<std::size_t>(length & ~blockMask);
    return {static_cast<std::size_t>(length), outBytes + encryptOut, false, false};
}

bool Validate(LibreSslEvpCipherCtx* context, unsigned char* out, int* written, const unsigned char* in, int length, bool useContextMode, bool decrypt, LibreSslEvpCipher& cipher, const char* function) {
    const ErrnoGuard errnoGuard;
    if (!context) throw std::invalid_argument(std::string(function) + ": null context");
    LibreSslEvpCipherCtx guestContext{};
    if (!GuestCopyIn_nid_no_patch(&guestContext, context, sizeof(guestContext)) ||
        !GuestCopyOut_nid_no_patch(context, &guestContext, sizeof(guestContext))) {
        throw std::invalid_argument(std::string(function) + ": invalid context");
    }
    if (!guestContext.cipher) throw std::invalid_argument(std::string(function) + ": no cipher set");
    if (!GuestCopyIn_nid_no_patch(&cipher, guestContext.cipher, sizeof(cipher))) {
        throw std::invalid_argument(std::string(function) + ": unreadable cipher");
    }
    if (!written) throw std::invalid_argument(std::string(function) + ": null output length");
    const bool effectiveDecrypt = useContextMode ? !guestContext.encrypt : decrypt;
    const auto extents = ComputeExtents(guestContext, cipher, length, effectiveDecrypt);
    if (extents.inBytes > 0) {
        if (!in) {
            if (!extents.allowNullIn) throw std::invalid_argument(std::string(function) + ": null buffer");
        } else if (!(in == out && extents.outBytes >= extents.inBytes)) {
            if (!GuestReadableRange(in, extents.inBytes)) {
                throw std::invalid_argument(std::string(function) + ": unreadable input");
            }
        }
    }
    if (extents.outBytes > 0) {
        if (!out) {
            if (!extents.allowNullOut) throw std::invalid_argument(std::string(function) + ": null buffer");
        } else if (!GuestWritableRange(out, extents.outBytes)) {
            throw std::invalid_argument(std::string(function) + ": unwritable output");
        }
    }
    const int zero = 0;
    if (!GuestCopyOut_nid_no_patch(written, &zero, sizeof(zero))) {
        throw std::invalid_argument(std::string(function) + ": unwritable output length");
    }
    return effectiveDecrypt;
}

void Copy(unsigned char* destination, const unsigned char* source, int length, const char* function) {
    if (length <= 0) return;
    if (!destination || !source) throw std::invalid_argument(std::string(function) + ": null buffer");
    std::memcpy(destination, source, static_cast<std::size_t>(length));
}

int Run(LibreSslEvpCipherCtx* context, const LibreSslEvpCipher& cipher, unsigned char* out, int* written, const unsigned char* in, int length, const char* function) {
    if (!cipher.doCipher) throw std::invalid_argument(std::string(function) + ": cipher without do_cipher");
    if (cipher.flags & CipherFlagCustomCipher) {
        const int produced = cipher.doCipher(context, out, in, static_cast<std::size_t>(length));
        if (produced < 0) return 0;
        *written = produced;
        return 1;
    }
    if (length > 0 && (!out || !in)) throw std::invalid_argument(std::string(function) + ": null buffer");
    if (!cipher.doCipher(context, out, in, static_cast<std::size_t>(length))) return 0;
    *written = length;
    return 1;
}

int Encrypt(LibreSslEvpCipherCtx* context, const LibreSslEvpCipher& cipher, unsigned char* out, int* written, const unsigned char* in, int length, const char* function) {
    const int blockSize = cipher.blockSize;
    const int blockMask = Mask(blockSize);
    int partial = context->bufLen;
    int total = 0;
    *written = 0;
    if ((blockSize & blockMask) != 0) return 0;
    if (length < 0) return 0;
    if (length == 0 && (cipher.flags & CipherModeMask) != CipherModeCcm) return 1;
    if (cipher.flags & CipherFlagCustomCipher) return Run(context, cipher, out, written, in, length, function);
    if (partial == 0 && (length & blockMask) == 0) return Run(context, cipher, out, written, in, length, function);
    if (partial < 0 || partial >= blockSize || static_cast<std::size_t>(blockSize) > sizeof(context->buf)) {
        LibreSslPutError_nid_no_patch(EvpLibrary, AnyFunction, BadBlockLength, __FILE__, __LINE__);
        return 0;
    }
    if (partial > 0) {
        const int needed = blockSize - partial;
        if (needed > length) {
            Copy(context->buf + partial, in, length, function);
            context->bufLen += length;
            return 1;
        }
        if (((length - needed) & ~blockMask) > INT_MAX - blockSize) {
            LibreSslPutError_nid_no_patch(EvpLibrary, AnyFunction, TooLarge, __FILE__, __LINE__);
            return 0;
        }
        Copy(context->buf + partial, in, needed, function);
        int produced = 0;
        if (!Run(context, cipher, out, &produced, context->buf, blockSize, function)) return 0;
        total = produced;
        length -= needed;
        in += needed;
        out += produced;
    }
    partial = length & blockMask;
    length -= partial;
    if (length > 0) {
        if (INT_MAX - length < total) return 0;
        int produced = 0;
        if (!Run(context, cipher, out, &produced, in, length, function)) return 0;
        if (INT_MAX - produced < total) return 0;
        total += produced;
    }
    Copy(context->buf, in + length, partial, function);
    context->bufLen = partial;
    *written = total;
    return 1;
}

int Decrypt(LibreSslEvpCipherCtx* context, const LibreSslEvpCipher& cipher, unsigned char* out, int* written, const unsigned char* in, int length, const char* function) {
    const int blockSize = cipher.blockSize;
    const int blockMask = Mask(blockSize);
    int total = 0;
    *written = 0;
    if ((blockSize & blockMask) != 0) return 0;
    if (length < 0) return 0;
    if (length == 0 && (cipher.flags & CipherModeMask) != CipherModeCcm) return 1;
    if (cipher.flags & CipherFlagCustomCipher) return Run(context, cipher, out, written, in, length, function);
    if (context->flags & CipherFlagNoPadding) return Encrypt(context, cipher, out, written, in, length, function);
    if (static_cast<std::size_t>(blockSize) > sizeof(context->final)) {
        LibreSslPutError_nid_no_patch(EvpLibrary, AnyFunction, BadBlockLength, __FILE__, __LINE__);
        return 0;
    }
    if (context->finalUsed) {
        if ((length & ~blockMask) > INT_MAX - blockSize) {
            LibreSslPutError_nid_no_patch(EvpLibrary, AnyFunction, TooLarge, __FILE__, __LINE__);
            return 0;
        }
        Copy(out, context->final, blockSize, function);
        out += blockSize;
        total = blockSize;
    }
    context->finalUsed = 0;
    int produced = 0;
    if (!Encrypt(context, cipher, out, &produced, in, length, function)) return 0;
    if (blockSize > 1 && context->bufLen == 0) {
        if (produced < blockSize) return 0;
        produced -= blockSize;
        Copy(context->final, out + produced, blockSize, function);
        context->finalUsed = 1;
    }
    if (produced > INT_MAX - total) return 0;
    total += produced;
    *written = total;
    return 1;
}

}

extern "C" {

int APS5_VABI EVP_EncryptUpdate_nid_disambig1_nid_postfix(LibreSslEvpCipherCtx* context, unsigned char* out, int* written, const unsigned char* in, int length) {
    LibreSslEvpCipher cipher{};
    Validate(context, out, written, in, length, false, false, cipher, __func__);
    return Encrypt(context, cipher, out, written, in, length, __func__);
}

int APS5_VABI EVP_DecryptUpdate_nid_disambig1_nid_postfix(LibreSslEvpCipherCtx* context, unsigned char* out, int* written, const unsigned char* in, int length) {
    LibreSslEvpCipher cipher{};
    Validate(context, out, written, in, length, false, true, cipher, __func__);
    return Decrypt(context, cipher, out, written, in, length, __func__);
}

int APS5_VABI EVP_CipherUpdate_nid_disambig1_nid_postfix(LibreSslEvpCipherCtx* context, unsigned char* out, int* written, const unsigned char* in, int length) {
    LibreSslEvpCipher cipher{};
    const bool decrypt = Validate(context, out, written, in, length, true, false, cipher, __func__);
    return decrypt ? Decrypt(context, cipher, out, written, in, length, __func__) : Encrypt(context, cipher, out, written, in, length, __func__);
}

}
