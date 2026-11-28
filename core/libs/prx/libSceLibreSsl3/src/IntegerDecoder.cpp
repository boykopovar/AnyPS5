/* Derived from LibreSSL 4.0.0 src/lib/libcrypto/asn1/asn1_old_lib.c, asn1_lib.c and a_int.c
 * https://github.com/libressl/openbsd/tree/ce488b78e0bd994780569df718596b9776ff681a
 * The licence text below is that of asn1_lib.c in the bundled 3rdparty/libressl-openbsd, pinned at
 * https://github.com/libressl/openbsd/tree/078b3d1df8ca2011c1605cd40418c58fa0f5b143 */
/* $OpenBSD: asn1_lib.c,v 1.45 2020/12/08 15:06:42 tb Exp $ */
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

#include "prx/libSceLibreSsl3/libSceLibreSsl3.hpp"
#include "prx/libSceLibreSsl/include/Error.hpp"
#include "prx/libc/include/General.hpp"
#include "prx/libc/libc.hpp"
#include <algorithm>
#include <cerrno>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <limits>
#include <new>
#include <optional>
#include <span>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

constexpr int Asn1Library = 13;
constexpr int AnyFunction = 0xfff;
constexpr int NestedAsn1Error = 58;
constexpr int LengthMismatch = 62;
constexpr int MallocFailure = 65;
constexpr int BadObjectHeader = 102;
constexpr int HeaderTooLong = 123;
constexpr int TooLong = 155;
constexpr int WrongTag = 168;
constexpr int TypeNotPrimitive = 231;

static_assert(sizeof(Asn1String) == 24);
static_assert(offsetof(Asn1String, data) == 8);
static_assert(offsetof(Asn1String, flags) == 16);

template <typename T>
T GuestLoad(const T* source, const char* function) {
    T value;
    const int savedErrno = errno;
    const bool copied = GuestCopyIn_nid_no_patch(&value, source, sizeof(T));
    errno = savedErrno;
    if (!copied) throw std::invalid_argument(std::string(function) + ": unreadable guest pointer");
    return value;
}

void RequireWritable(void* destination, std::size_t bytes, const char* function) {
    const int savedErrno = errno;
    const bool writable = GuestWritable_nid_no_patch(destination, bytes);
    errno = savedErrno;
    if (!writable) throw std::invalid_argument(std::string(function) + ": unwritable guest pointer");
}

template <typename T>
void GuestStore(T* destination, const T& value, const char* function) {
    const int savedErrno = errno;
    const bool copied = GuestCopyOut_nid_no_patch(destination, &value, sizeof(T));
    errno = savedErrno;
    if (!copied) throw std::invalid_argument(std::string(function) + ": unwritable guest pointer");
}

void PutError(int reason) {
    LibreSslPutError_nid_no_patch(Asn1Library, AnyFunction, reason);
}

void PutHeaderError(int reason) {
    PutError(reason);
    PutError(NestedAsn1Error);
}

class GuestInput {
public:
    GuestInput(const unsigned char* start, std::size_t size, const char* function)
        : start(start), size(size), function(function) {
        if (reinterpret_cast<std::uintptr_t>(start) > std::numeric_limits<std::uintptr_t>::max() - size)
            throw std::invalid_argument(std::string(function) + ": guest range wraps the address space");
    }

    std::size_t Remaining() const {
        return size - offset;
    }

    const unsigned char* Position() const {
        return start + offset;
    }

    bool ReadByte(unsigned char& byte) {
        if (offset == size) return false;
        copyIn(&byte, 1);
        ++offset;
        return true;
    }

    std::span<const unsigned char> Take(std::size_t count) {
        if (count > Remaining()) throw std::invalid_argument(std::string(function) + ": content exceeds the input");
        taken.clear();
        while (count != 0) {
            const std::size_t step = std::min<std::size_t>(count, chunkSize);
            taken.resize(taken.size() + step);
            copyIn(taken.data() + taken.size() - step, step);
            offset += step;
            count -= step;
        }
        return taken;
    }

private:
    void copyIn(unsigned char* destination, std::size_t count) {
        const int savedErrno = errno;
        const bool copied = GuestCopyIn_nid_no_patch(destination, start + offset, count);
        errno = savedErrno;
        if (!copied) throw std::invalid_argument(std::string(function) + ": unreadable guest input");
    }

    const unsigned char* start;
    std::size_t size;
    static constexpr std::size_t chunkSize = 65536;

    const char* function;
    std::size_t offset = 0;
    std::vector<unsigned char> taken;
};

bool ReadIdentifier(GuestInput& input, std::uint8_t& tagClass, bool& constructed, std::uint32_t& tag) {
    unsigned char identifier;
    if (!input.ReadByte(identifier)) return false;
    tagClass = identifier >> 6;
    constructed = (identifier & 0x20) != 0;
    tag = identifier & 0x1f;
    if (tag != 0x1f) return true;
    tag = 0;
    unsigned char byte;
    do {
        if (!input.ReadByte(byte)) return false;
        if (tag > (std::numeric_limits<std::uint32_t>::max() >> 7)) return false;
        tag = (tag << 7) | (byte & 0x7f);
    } while (byte & 0x80);
    return true;
}

bool ReadLength(GuestInput& input, bool& indefinite, std::size_t& length) {
    unsigned char lengthByte;
    if (!input.ReadByte(lengthByte)) return false;
    indefinite = lengthByte == 0x80;
    length = lengthByte < 0x80 ? lengthByte : 0;
    if (lengthByte <= 0x80) return true;
    const auto bytes = lengthByte & 0x7f;
    if (bytes == 0x7f) return false;
    for (int index = 0; index < bytes; ++index) {
        unsigned char byte;
        if (!input.ReadByte(byte)) return false;
        if (length > (std::numeric_limits<std::size_t>::max() >> 8)) return false;
        length = (length << 8) | byte;
    }
    return true;
}

std::optional<std::span<const unsigned char>> ReadInteger(GuestInput& input) {
    std::uint8_t tagClass;
    bool constructed;
    std::uint32_t tag;
    if (!ReadIdentifier(input, tagClass, constructed, tag)) {
        PutHeaderError(BadObjectHeader);
        return std::nullopt;
    }
    if (tag != 2 || tagClass != 0) {
        PutHeaderError(WrongTag);
        return std::nullopt;
    }
    bool indefinite;
    std::size_t length;
    if (!ReadLength(input, indefinite, length) || (indefinite && !constructed)) {
        PutHeaderError(BadObjectHeader);
        return std::nullopt;
    }
    if (!indefinite && length > input.Remaining()) {
        PutHeaderError(TooLong);
        return std::nullopt;
    }
    if (constructed) {
        PutError(TypeNotPrimitive);
        return std::nullopt;
    }
    if (length > static_cast<std::size_t>(std::numeric_limits<int>::max())) return std::nullopt;
    return input.Take(length);
}

void ClearAndFree(void* data, std::size_t size) {
    auto* bytes = static_cast<volatile unsigned char*>(data);
    for (std::size_t index = 0; index < size; ++index) bytes[index] = 0;
    free_nid_postfix(data);
}

Asn1String ValidateInteger(void* value) {
    Asn1String integer{};
    if (!value) return integer;
    integer = GuestLoad(static_cast<const Asn1String*>(value), "ASN1_STRING");
    if (!GuestWritable_nid_no_patch(value, sizeof(Asn1String))) throw std::invalid_argument("ASN1_STRING: unwritable object");
    if ((integer.flags & 0x10) || !integer.data) return integer;
    if (integer.length < 0) throw std::invalid_argument("ASN1_STRING: negative length");
    if (!GuestWritable_nid_no_patch(integer.data, static_cast<std::size_t>(integer.length))) throw std::invalid_argument("ASN1_STRING: unwritable data");
    return integer;
}

void ReleaseInteger(void* value, const Asn1String& snapshot) {
    if (!value) return;
    if (!(snapshot.flags & 0x10) && snapshot.data) ClearAndFree(snapshot.data, static_cast<std::size_t>(snapshot.length));
    auto& integer = *static_cast<Asn1String*>(value);
    integer.flags = snapshot.flags & ~std::int64_t{0x10};
    integer.data = nullptr;
    integer.length = 0;
    free_nid_postfix(value);
}

void FreeInteger(void* value) {
    ReleaseInteger(value, ValidateInteger(value));
}

}

extern "C" {

int APS5_VABI ASN1_get_object_nid_postfix(const unsigned char** input, std::int64_t* length,
                                        int* tag, int* tagClass, std::int64_t maximum) {
    if (!input || !length || !tag || !tagClass)
        throw std::invalid_argument("ASN1_get_object: invalid buffer or output");
    const unsigned char* const start = GuestLoad(input, "ASN1_get_object");
    if (!start && maximum != 0) throw std::invalid_argument("ASN1_get_object: invalid buffer or output");
    const std::size_t bound = maximum < 0 ? std::numeric_limits<std::uintptr_t>::max() - reinterpret_cast<std::uintptr_t>(start)
                                          : static_cast<std::size_t>(maximum);
    GuestInput remaining(start, bound, "ASN1_get_object");
    RequireWritable(input, sizeof(*input), "ASN1_get_object");
    RequireWritable(tagClass, sizeof(*tagClass), "ASN1_get_object");
    RequireWritable(tag, sizeof(*tag), "ASN1_get_object");
    RequireWritable(length, sizeof(*length), "ASN1_get_object");
    std::uint8_t parsedClass;
    std::uint32_t parsedTag;
    bool constructed;
    bool indefinite;
    std::size_t parsedLength;
    if (!ReadIdentifier(remaining, parsedClass, constructed, parsedTag)
        || !ReadLength(remaining, indefinite, parsedLength) || (indefinite && !constructed)
        || parsedTag > static_cast<std::uint32_t>(std::numeric_limits<int>::max())) {
        GuestStore(tagClass, 0, "ASN1_get_object");
        GuestStore(tag, 0, "ASN1_get_object");
        GuestStore(length, std::int64_t{0}, "ASN1_get_object");
        PutError(HeaderTooLong);
        return 0x80;
    }
    int result = (constructed ? 0x20 : 0) | (indefinite ? 1 : 0);
    if (parsedLength > remaining.Remaining()
        || parsedLength > static_cast<std::uint64_t>(std::numeric_limits<std::int64_t>::max())) {
        PutError(TooLong);
        result |= 0x80;
    }
    GuestStore(tagClass, static_cast<int>(parsedClass << 6), "ASN1_get_object");
    GuestStore(tag, static_cast<int>(parsedTag), "ASN1_get_object");
    GuestStore(length, static_cast<std::int64_t>(parsedLength), "ASN1_get_object");
    GuestStore(input, remaining.Position(), "ASN1_get_object");
    return result;
}

void* APS5_VABI d2i_ASN1_INTEGER_nid_postfix(void** output, const unsigned char** input, std::int64_t length) {
    if (length < 0) return nullptr;
    if (!input) throw std::invalid_argument("d2i_ASN1_INTEGER: null input");
    const unsigned char* const start = GuestLoad(input, "d2i_ASN1_INTEGER");
    if (!start) throw std::invalid_argument("d2i_ASN1_INTEGER: null input");
    GuestStore(input, start, "d2i_ASN1_INTEGER");
    void* previous = nullptr;
    Asn1String previousState{};
    if (output) {
        previous = GuestLoad(output, "d2i_ASN1_INTEGER");
        GuestStore(output, previous, "d2i_ASN1_INTEGER");
        previousState = ValidateInteger(previous);
    }
    GuestInput remaining(start, static_cast<std::size_t>(length), "d2i_ASN1_INTEGER");
    std::optional<std::span<const unsigned char>> content;
    try {
        content = ReadInteger(remaining);
    } catch (const std::bad_alloc&) {
        PutError(MallocFailure);
        return nullptr;
    }
    if (!content) return nullptr;
    if (content->empty()) {
        PutError(LengthMismatch);
        return nullptr;
    }
    const bool negative = (content->front() & 0x80) != 0;
    const unsigned char padding = negative ? 0xff : 0;
    if (content->size() > 1 && content->front() == padding) {
        bool keepPadding = negative;
        for (std::size_t index = 1; index < content->size(); ++index)
            keepPadding = keepPadding && (*content)[index] == 0;
        if (!keepPadding) {
            content = content->subspan(1);
            if (((content->front() & 0x80) != 0) == negative) {
                PutError(LengthMismatch);
                return nullptr;
            }
        }
    }
    auto* data = static_cast<unsigned char*>(malloc_nid_postfix(content->size()));
    if (!data) return nullptr;
    std::memcpy(data, content->data(), content->size());
    if (negative) {
        unsigned int carry = 1;
        for (std::size_t index = content->size(); index != 0; --index) {
            const unsigned int digit = (data[index - 1] ^ 0xff) + carry;
            data[index - 1] = static_cast<unsigned char>(digit);
            carry = digit >> 8;
        }
    }
    auto* integer = static_cast<Asn1String*>(malloc_nid_postfix(sizeof(Asn1String)));
    if (!integer) {
        ClearAndFree(data, content->size());
        PutError(MallocFailure);
        PutError(MallocFailure);
        return nullptr;
    }
    *integer = {static_cast<std::int32_t>(content->size()), negative ? 0x102 : 2, data, 0};
    try {
        if (output) GuestStore(output, static_cast<void*>(integer), "d2i_ASN1_INTEGER");
        GuestStore(input, remaining.Position(), "d2i_ASN1_INTEGER");
    } catch (...) {
        if (output) {
            const int savedErrno = errno;
            const bool restored = GuestCopyOut_nid_no_patch(output, &previous, sizeof(previous));
            errno = savedErrno;
            if (!restored) throw std::runtime_error("d2i_ASN1_INTEGER: the output slot could not be restored");
        }
        FreeInteger(integer);
        throw;
    }
    ReleaseInteger(previous, previousState);
    return integer;
}

}
