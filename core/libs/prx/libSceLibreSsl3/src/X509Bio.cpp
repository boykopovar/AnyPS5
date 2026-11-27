/* $OpenBSD: a_d2i_fp.c,v 1.16 2017/01/29 17:49:22 beck Exp $ */
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
 *
 * Adapted from https://github.com/libressl/openbsd/blob/078b3d1df8ca2011c1605cd40418c58fa0f5b143/src/lib/libcrypto/asn1/a_d2i_fp.c
 */

#include "prx/libSceLibreSsl3/include/Api.hpp"
#include "prx/libSceLibreSsl/include/Error.hpp"
#include "prx/libc/include/Api.hpp"
#include "prx/libc/include/General.hpp"

#include <algorithm>
#include <cerrno>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <new>
#include <stdexcept>
#include <vector>

namespace {

constexpr int Asn1Library = 13;
constexpr int BioLibrary = 32;
constexpr int AnyFunction = 0xfff;
constexpr int MallocFailure = 65;
constexpr int HeaderTooLong = 123;
constexpr int NotEnoughData = 142;
constexpr int TooLong = 155;
constexpr int BioUninitialized = 120;
constexpr int BioUnsupportedMethod = 121;
constexpr std::size_t HeaderSize = 8;
constexpr std::size_t InitialChunkSize = 16 * 1024;

struct Header {
    bool constructed;
    bool indefinite;
    int tag;
    int tagClass;
    std::size_t length;
    std::size_t size;
};

struct Frame {
    bool indefinite;
    std::size_t end;
    std::size_t boundary;
};

template <typename T>
bool CopyGuestIn(T& destination, const T* source) {
    const int savedErrno = errno;
    const bool copied = GuestCopyIn_nid_no_patch(&destination, source, sizeof(T));
    errno = savedErrno;
    return copied;
}

template <typename T>
bool CopyGuestOut(T* destination, const T& value) {
    const int savedErrno = errno;
    const bool copied = GuestCopyOut_nid_no_patch(destination, &value, sizeof(T));
    errno = savedErrno;
    return copied;
}

void PutError(int library, int reason) {
    LibreSslPutError_nid_no_patch(library, AnyFunction, reason, __FILE__, __LINE__);
}

int BioRead(LibreSsl::Bio* bio, char* destination, int length) {
    LibreSsl::Bio state{};
    if (!bio || !CopyGuestIn(state, bio)) {
        PutError(BioLibrary, BioUnsupportedMethod);
        return -2;
    }
    LibreSsl::BioMethod method{};
    if (!state.method || !CopyGuestIn(method, state.method) || !method.read) {
        PutError(BioLibrary, BioUnsupportedMethod);
        return -2;
    }
    const auto callback = state.callback;
    int result = callback ? static_cast<int>(callback(bio, 2, destination, length, 0, 1)) : 1;
    if (result <= 0) return result;
    if (!CopyGuestIn(state, bio)) NotImplemented_nid_no_patch(__func__);
    if (!state.initialized) {
        PutError(BioLibrary, BioUninitialized);
        return -2;
    }
    if (!state.method || !CopyGuestIn(method, state.method) || !method.read)
        NotImplemented_nid_no_patch(__func__);
    result = method.read(bio, destination, length);
    if (result > length) NotImplemented_nid_no_patch(__func__);
    if (result > 0) {
        if (!CopyGuestIn(state, bio)) NotImplemented_nid_no_patch(__func__);
        state.bytesRead += static_cast<std::uint64_t>(result);
        if (!CopyGuestOut(&bio->bytesRead, state.bytesRead)) NotImplemented_nid_no_patch(__func__);
    }
    if (callback) result = static_cast<int>(callback(bio, 0x82, destination, length, 0, result));
    if (result > length) NotImplemented_nid_no_patch(__func__);
    return result;
}

bool ParseHeader(const unsigned char* data, std::size_t available, Header& header) {
    if (available == 0) return false;
    std::size_t offset = 0;
    const unsigned char identifier = data[offset++];
    header.constructed = (identifier & 0x20) != 0;
    header.tagClass = identifier & 0xc0;
    header.tag = identifier & 0x1f;
    if (header.tag == 0x1f) {
        std::uint64_t tag = 0;
        unsigned char byte = 0;
        do {
            if (offset == available || tag > (static_cast<std::uint64_t>(std::numeric_limits<int>::max()) >> 7))
                return false;
            byte = data[offset++];
            tag = (tag << 7) | (byte & 0x7f);
        } while (byte & 0x80);
        header.tag = static_cast<int>(tag);
    }
    if (offset == available) return false;
    const unsigned char firstLength = data[offset++];
    header.indefinite = firstLength == 0x80;
    header.length = firstLength < 0x80 ? firstLength : 0;
    if (firstLength > 0x80) {
        std::size_t count = firstLength & 0x7f;
        if (available - offset < count) return false;
        while (count && data[offset] == 0) {
            ++offset;
            --count;
        }
        if (count > sizeof(std::int64_t)) return false;
        while (count--) {
            if (header.length > (static_cast<std::size_t>(std::numeric_limits<std::int64_t>::max()) >> 8))
                return false;
            header.length = (header.length << 8) | data[offset++];
        }
    }
    if (header.indefinite && !header.constructed) return false;
    header.size = offset;
    return true;
}

std::size_t DefiniteBoundary(const std::vector<Frame>& frames) {
    return frames.empty() ? std::numeric_limits<std::size_t>::max() : frames.back().boundary;
}

bool PopCompletedFrames(std::vector<Frame>& frames, std::size_t offset) {
    while (!frames.empty() && !frames.back().indefinite) {
        if (offset > frames.back().end) return false;
        if (offset < frames.back().end) break;
        frames.pop_back();
    }
    return true;
}

bool ReadObject(LibreSsl::Bio* bio, std::vector<unsigned char>& bytes) {
    ERR_clear_error_nid_postfix();
    bytes.clear();
    std::size_t offset = 0;
    std::size_t length = 0;
    std::vector<Frame> frames;
    const auto readBody = [&](std::size_t bodyLength) {
        const std::size_t bodyAvailable = length - offset;
        std::size_t want = bodyLength;
        if (want > bodyAvailable) {
            want -= bodyAvailable;
            if (want > static_cast<std::size_t>(std::numeric_limits<int>::max()) ||
                length > static_cast<std::size_t>(std::numeric_limits<int>::max()) - want) {
                PutError(Asn1Library, TooLong);
                return false;
            }
            std::size_t chunkMaximum = InitialChunkSize;
            while (want > 0) {
                const std::size_t chunkSize = std::min(want, chunkMaximum);
                bytes.resize(length + chunkSize);
                std::size_t chunk = chunkSize;
                want -= chunk;
                while (chunk > 0) {
                    const int read = BioRead(bio, reinterpret_cast<char*>(bytes.data() + length), static_cast<int>(chunk));
                    if (read <= 0) {
                        PutError(Asn1Library, NotEnoughData);
                        return false;
                    }
                    length += static_cast<std::size_t>(read);
                    chunk -= static_cast<std::size_t>(read);
                }
                if (chunkMaximum < static_cast<std::size_t>(std::numeric_limits<int>::max() / 2)) chunkMaximum *= 2;
            }
        }
        if (offset > static_cast<std::size_t>(std::numeric_limits<int>::max()) - bodyLength) {
            PutError(Asn1Library, TooLong);
            return false;
        }
        offset += bodyLength;
        return true;
    };
    for (;;) {
        const std::size_t boundary = DefiniteBoundary(frames);
        if (offset > boundary || (offset == boundary && !frames.empty() && frames.back().indefinite)) {
            PutError(Asn1Library, HeaderTooLong);
            return false;
        }
        const std::size_t available = length - offset;
        if (HeaderSize >= available) {
            const std::size_t boundaryRemaining = length < boundary ? boundary - length : 0;
            const std::size_t request = std::min(HeaderSize - available, boundaryRemaining);
            if (request > static_cast<std::size_t>(std::numeric_limits<int>::max()) || length > static_cast<std::size_t>(std::numeric_limits<int>::max()) - request) {
                PutError(Asn1Library, TooLong);
                return false;
            }
            if (request > 0) {
                bytes.resize(length + request);
                const int read = BioRead(bio, reinterpret_cast<char*>(bytes.data() + length), static_cast<int>(request));
                if (read < 0 && available == 0) {
                    PutError(Asn1Library, NotEnoughData);
                    return false;
                }
                if (read > 0) length += static_cast<std::size_t>(read);
            }
        }
        Header header{};
        const std::size_t headerAvailable = std::min(length - offset, boundary - offset);
        if (!ParseHeader(bytes.data() + offset, headerAvailable, header)) {
            PutError(Asn1Library, HeaderTooLong);
            return false;
        }
        offset += header.size;
        if (frames.empty()) {
            if (header.indefinite) {
                frames.push_back({true, 0, boundary});
                continue;
            }
            if (!readBody(header.length)) return false;
            break;
        }
        if (header.indefinite) {
            frames.push_back({true, 0, boundary});
            continue;
        }
        const bool eoc = !header.constructed && header.length == 0 && header.tag == 0 && header.tagClass == 0;
        if (eoc && frames.back().indefinite) {
            frames.pop_back();
            if (!PopCompletedFrames(frames, offset)) {
                PutError(Asn1Library, HeaderTooLong);
                return false;
            }
            if (frames.empty()) break;
            continue;
        }
        if (offset > std::numeric_limits<std::size_t>::max() - header.length ||
            offset + header.length > DefiniteBoundary(frames)) {
            PutError(Asn1Library, HeaderTooLong);
            return false;
        }
        if (header.constructed) {
            const std::size_t end = offset + header.length;
            frames.push_back({false, end, std::min(boundary, end)});
        } else if (!readBody(header.length)) {
            return false;
        }
        if (!PopCompletedFrames(frames, offset)) {
            PutError(Asn1Library, HeaderTooLong);
            return false;
        }
        if (frames.empty()) break;
    }
    bytes.resize(offset);
    return true;
}

}

extern "C" LibreSslX509* APS5_VABI d2i_X509_bio_nid_postfix(
    LibreSsl::Bio* bio, LibreSslX509** certificate) {
    LibreSslX509* output = nullptr;
    if (certificate) {
        if (!CopyGuestIn(output, certificate) || !CopyGuestOut(certificate, output))
            NotImplemented_nid_no_patch(__func__);
    }
    std::vector<unsigned char> bytes;
    try {
        if (!ReadObject(bio, bytes)) return nullptr;
    } catch (const std::bad_alloc&) {
        PutError(Asn1Library, MallocFailure);
        return nullptr;
    } catch (const std::length_error&) {
        PutError(Asn1Library, MallocFailure);
        return nullptr;
    }
    const unsigned char* input = bytes.data();
    LibreSslX509* result = d2i_X509_nid_postfix(certificate ? &output : nullptr, &input,
        static_cast<std::int64_t>(bytes.size()));
    if (certificate && !CopyGuestOut(certificate, output)) NotImplemented_nid_no_patch(__func__);
    return result;
}
