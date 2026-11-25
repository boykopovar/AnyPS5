/* Derived from LibreSSL 4.0.0 (openbsd ce488b78e0bd994780569df718596b9776ff681a) src/lib/libcrypto/pem/pem_lib.c
 * (pem_lib.c,v 1.56), src/lib/libcrypto/pem/pem_pkey.c (pem_pkey.c,v 1.28),
 * src/lib/libcrypto/pem/pem_all.c (pem_all.c,v 1.21), src/lib/libcrypto/pem/pem_oth.c (pem_oth.c,v 1.9),
 * src/lib/libcrypto/pem/pem_pk8.c (pem_pk8.c,v 1.14) and
 * src/lib/libcrypto/evp/evp_encode.c (evp_encode.c,v 1.3) */
/* $OpenBSD: pem_lib.c,v 1.56 2024-02-18 15:44:10 tb Exp $ */
/* $OpenBSD: pem_pkey.c,v 1.28 2023-11-19 15:46:10 tb Exp $ */
/* $OpenBSD: pem_all.c,v 1.21 2023-07-07 13:40:44 beck Exp $ */
/* $OpenBSD: pem_oth.c,v 1.9 2023-07-07 13:40:44 beck Exp $ */
/* $OpenBSD: pem_pk8.c,v 1.14 2023-07-07 13:40:44 beck Exp $ */
/* $OpenBSD: evp_encode.c,v 1.3 2024-04-09 13:52:41 beck Exp $ */
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
 * OR SERVICES; LOSS OF DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
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
 * Copyright (c) 1998-2002 The OpenSSL Project.  All rights reserved.
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
 *    for use in the OpenSSL Toolkit. (http://www.openssl.org/)"
 *
 * 4. The names "OpenSSL Toolkit" and "OpenSSL Project" must not be used to
 *    endorse or promote products derived from this software without
 *    prior written permission. For written permission, please contact
 *    openssl-core@openssl.org.
 *
 * 5. Products derived from this software may not be called "OpenSSL"
 *    nor may "OpenSSL" appear in their names without prior written
 *    permission of the OpenSSL Project.
 *
 * 6. Redistributions of any form whatsoever must retain the following
 *    acknowledgment:
 *    "This product includes software developed by the OpenSSL Project
 *    for use in the OpenSSL Toolkit (http://www.openssl.org/)"
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

#include "prx/libSceLibreSsl3/libSceLibreSsl3.hpp"
#include "prx/libSceLibreSsl3/include/PkeyState.hpp"
#include "prx/libSceLibreSsl/include/Bio.hpp"
#include "prx/libSceLibreSsl/include/Error.hpp"
#include "prx/libc/include/Api.hpp"
#include "prx/libc/include/General.hpp"
#include "prx/libc/include/GuestPtr.hpp"
#include <array>
#include <cerrno>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <limits>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

constexpr int BufferLibrary = 7;
constexpr int PemLibrary = 9;
constexpr int BioLibrary = 32;
constexpr int AnyFunction = 0xfff;

constexpr int ErrRBufLib = 7;
constexpr int ErrRAsn1Lib = 13;
constexpr int ErrRMallocFailure = 65;

constexpr int PemBadBase64Decode = 100;
constexpr int PemBadEndLine = 102;
constexpr int PemNotDekInfo = 105;
constexpr int PemNotEncrypted = 106;
constexpr int PemNotProcType = 107;
constexpr int PemNoStartLine = 108;
constexpr int PemShortHeader = 112;
constexpr int PemErrorConvertingPrivateKey = 115;

constexpr int BioUninitialized = 120;
constexpr int BioUnsupportedMethod = 121;
constexpr int BioCallbackGets = 0x05;
constexpr int BioCallbackReturn = 0x80;

constexpr int RsaType = 6;
constexpr int DsaType = 116;
constexpr int EcType = 408;

constexpr int PemBufferSize = 1024;
constexpr int PemEncodeChunk = PemBufferSize * 5;
constexpr int PemWriteBufferLength = PemBufferSize * 8;
constexpr int EncodeLineBytes = 48;
constexpr int LineCharacterLimit = 254;
constexpr int PemRecordOverhead = 9;

constexpr const char* EncryptedPrivateKeyLabel = "ENCRYPTED PRIVATE KEY";
constexpr const char* Pkcs8InfLabel = "PRIVATE KEY";
constexpr const char* PublicKeyLabel = "PUBLIC KEY";
constexpr const char* AnyPrivateKeyLabel = "ANY PRIVATE KEY";
constexpr const char* PrivateKeySuffix = "PRIVATE KEY";

struct PkeyAmeth {
    const char* pemString;
    int type;
    bool oldPrivDecode;
};

constexpr std::array<PkeyAmeth, 9> PkeyAmeths{{
    {"CMAC", 0, false},
    {"DH", 0, false},
    {"DSA", DsaType, true},
    {"EC", EcType, true},
    {"ED25519", 0, false},
    {"HMAC", 0, false},
    {"RSA", RsaType, true},
    {"RSA-PSS", 0, false},
    {"X25519", 0, false},
}};

constexpr unsigned char Base64Newline = 0xF0;
constexpr unsigned char Base64CarriageReturn = 0xF1;
constexpr unsigned char Base64End = 0xF2;
constexpr unsigned char Base64Whitespace = 0xE0;
constexpr unsigned char Base64Error = 0xFF;

constexpr std::array<unsigned char, 128> AsciiToBinary{
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xE0, 0xF0, 0xFF, 0xFF, 0xF1, 0xFF, 0xFF,
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
    0xE0, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x3E, 0xFF, 0xF2, 0xFF, 0x3F,
    0x34, 0x35, 0x36, 0x37, 0x38, 0x39, 0x3A, 0x3B, 0x3C, 0x3D, 0xFF, 0xFF, 0xFF, 0x00, 0xFF, 0xFF,
    0xFF, 0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E,
    0x0F, 0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17, 0x18, 0x19, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
    0xFF, 0x1A, 0x1B, 0x1C, 0x1D, 0x1E, 0x1F, 0x20, 0x21, 0x22, 0x23, 0x24, 0x25, 0x26, 0x27, 0x28,
    0x29, 0x2A, 0x2B, 0x2C, 0x2D, 0x2E, 0x2F, 0x30, 0x31, 0x32, 0x33, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
};

constexpr const char* BinaryToAscii = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

void PutPemError(int reason) {
    LibreSslPutError_nid_no_patch(PemLibrary, AnyFunction, reason);
}

[[noreturn]] void EncryptedPrivateKeyUnsupported() {
    NotImplemented_nid_no_patch(__func__);
}

[[noreturn]] void OldPrivateKeyDecodeUnsupported() {
    NotImplemented_nid_no_patch(__func__);
}

void ClearBytes(void* data, std::size_t length) {
    auto* const bytes = static_cast<volatile unsigned char*>(data);
    for (std::size_t index = 0; index < length; ++index) bytes[index] = 0;
}

struct ClearOnExit {
    std::vector<unsigned char>& bytes;
    ~ClearOnExit() { ClearBytes(bytes.data(), bytes.size()); }
};

unsigned char BinaryValue(unsigned char character) {
    if (character & 0x80) return Base64Error;
    return AsciiToBinary[character];
}

bool NotBase64(int value) {
    return (value | 0x13) == 0xF3;
}

int DecodeBlock(unsigned char* out, const unsigned char* in, int count) {
    while (BinaryValue(*in) == Base64Whitespace && count > 0) {
        ++in;
        --count;
    }
    while (count > 3 && NotBase64(BinaryValue(in[count - 1])))
        --count;
    if (count % 4 != 0) return -1;
    int total = 0;
    for (int index = 0; index < count; index += 4) {
        const int a = BinaryValue(*in++);
        const int b = BinaryValue(*in++);
        const int c = BinaryValue(*in++);
        const int d = BinaryValue(*in++);
        if ((a & 0x80) || (b & 0x80) || (c & 0x80) || (d & 0x80)) return -1;
        const unsigned long value = static_cast<unsigned long>(a) << 18L
                                    | static_cast<unsigned long>(b) << 12L
                                    | static_cast<unsigned long>(c) << 6L
                                    | static_cast<unsigned long>(d);
        *out++ = static_cast<unsigned char>((value >> 16L) & 0xff);
        *out++ = static_cast<unsigned char>((value >> 8L) & 0xff);
        *out++ = static_cast<unsigned char>(value & 0xff);
        total += 3;
    }
    return total;
}

struct DecodeContext {
    std::array<unsigned char, 80> encodedData{};
    int num = 0;
};

int DecodeUpdate(DecodeContext& context, unsigned char* out, int* outLength, const unsigned char* in, int inLength) {
    int seof = 0;
    int eof = 0;
    int rv = -1;
    int ret = 0;
    int v = 0;
    int n = context.num;
    int decodedLength = 0;
    unsigned char* d = context.encodedData.data();

    if (n > 0 && d[n - 1] == '=') {
        ++eof;
        if (n > 1 && d[n - 2] == '=')
            ++eof;
    }

    if (inLength == 0) {
        rv = 0;
    } else {
        for (int index = 0; index < inLength; ++index) {
            const unsigned char temporary = *in++;
            v = BinaryValue(temporary);
            if (v == Base64Error) {
                rv = -1;
                *outLength = ret;
                context.num = n;
                return rv;
            }

            if (temporary == '=') {
                ++eof;
            } else if (eof > 0 && !NotBase64(v)) {
                rv = -1;
                *outLength = ret;
                context.num = n;
                return rv;
            }

            if (eof > 2) {
                rv = -1;
                *outLength = ret;
                context.num = n;
                return rv;
            }

            if (v == Base64End) {
                seof = 1;
                break;
            }

            if (!NotBase64(v)) {
                if (n >= 64) {
                    rv = -1;
                    *outLength = ret;
                    context.num = n;
                    return rv;
                }
                d[n++] = temporary;
            }

            if (n == 64) {
                decodedLength = DecodeBlock(out, d, n);
                n = 0;
                if (decodedLength < 0 || eof > decodedLength) {
                    rv = -1;
                    *outLength = ret;
                    context.num = n;
                    return rv;
                }
                ret += decodedLength - eof;
                out += decodedLength - eof;
            }
        }

        if (n > 0) {
            if ((n & 3) == 0) {
                decodedLength = DecodeBlock(out, d, n);
                n = 0;
                if (decodedLength < 0 || eof > decodedLength) {
                    rv = -1;
                    *outLength = ret;
                    context.num = n;
                    return rv;
                }
                ret += decodedLength - eof;
            } else if (seof) {
                rv = -1;
                *outLength = ret;
                context.num = n;
                return rv;
            }
        }

        rv = seof || (n == 0 && eof) ? 0 : 1;
    }

    *outLength = ret;
    context.num = n;
    return rv;
}

int DecodeFinal(DecodeContext& context, unsigned char* out, int* outLength) {
    *outLength = 0;
    if (context.num == 0) return 1;
    const int count = DecodeBlock(out, context.encodedData.data(), context.num);
    if (count < 0) return -1;
    context.num = 0;
    *outLength = count;
    return 1;
}

int EncodeBlock(unsigned char* out, const unsigned char* in, int length) {
    int total = 0;
    for (int remaining = length; remaining > 0; remaining -= 3) {
        unsigned long value = 0;
        if (remaining >= 3) {
            value = static_cast<unsigned long>(in[0]) << 16L
                    | (static_cast<unsigned long>(in[1]) << 8L)
                    | static_cast<unsigned long>(in[2]);
            *out++ = BinaryToAscii[(value >> 18L) & 0x3f];
            *out++ = BinaryToAscii[(value >> 12L) & 0x3f];
            *out++ = BinaryToAscii[(value >> 6L) & 0x3f];
            *out++ = BinaryToAscii[value & 0x3f];
        } else {
            value = static_cast<unsigned long>(in[0]) << 16L;
            if (remaining == 2)
                value |= static_cast<unsigned long>(in[1]) << 8L;
            *out++ = BinaryToAscii[(value >> 18L) & 0x3f];
            *out++ = BinaryToAscii[(value >> 12L) & 0x3f];
            *out++ = (remaining == 1) ? '=' : BinaryToAscii[(value >> 6L) & 0x3f];
            *out++ = '=';
        }
        total += 4;
        in += 3;
    }
    *out = '\0';
    return total;
}

struct EncodeContext {
    std::array<unsigned char, EncodeLineBytes> encodedData{};
    int num = 0;
};

int EncodeUpdate(EncodeContext& context, unsigned char* out, int* outLength, const unsigned char* in, int inLength) {
    std::size_t total = 0;

    *outLength = 0;
    if (inLength <= 0) return 0;
    if (EncodeLineBytes - context.num > inLength) {
        std::memcpy(context.encodedData.data() + context.num, in, static_cast<std::size_t>(inLength));
        context.num += inLength;
        return 1;
    }
    int pending = inLength;
    if (context.num != 0) {
        const int fill = EncodeLineBytes - context.num;
        std::memcpy(context.encodedData.data() + context.num, in, static_cast<std::size_t>(fill));
        in += fill;
        pending -= fill;
        const int written = EncodeBlock(out, context.encodedData.data(), EncodeLineBytes);
        context.num = 0;
        out += written;
        *out++ = '\n';
        *out = '\0';
        total = static_cast<std::size_t>(written) + 1;
    }
    while (pending >= EncodeLineBytes && total <= static_cast<std::size_t>(std::numeric_limits<int>::max())) {
        const int written = EncodeBlock(out, in, EncodeLineBytes);
        in += EncodeLineBytes;
        pending -= EncodeLineBytes;
        out += written;
        *out++ = '\n';
        *out = '\0';
        total += static_cast<std::size_t>(written) + 1;
    }
    if (total > static_cast<std::size_t>(std::numeric_limits<int>::max())) {
        *outLength = 0;
        return 0;
    }
    if (pending != 0)
        std::memcpy(context.encodedData.data(), in, static_cast<std::size_t>(pending));
    context.num = pending;
    *outLength = static_cast<int>(total);
    return 1;
}

void EncodeFinal(EncodeContext& context, unsigned char* out, int* outLength) {
    std::size_t written = 0;
    if (context.num != 0) {
        written = static_cast<std::size_t>(EncodeBlock(out, context.encodedData.data(), context.num));
        out[written++] = '\n';
        out[written] = '\0';
        context.num = 0;
    }
    *outLength = static_cast<int>(written);
}

int BioGets(LibreSsl::Bio* bio, char* buffer, int size) {
    if (bio == nullptr || bio->method == nullptr || bio->method->gets == nullptr) {
        LibreSslPutError_nid_no_patch(BioLibrary, AnyFunction, BioUnsupportedMethod);
        return -2;
    }
    const auto callback = bio->callback;
    int result = 0;
    if (callback && (result = static_cast<int>(callback(bio, BioCallbackGets, buffer, size, 0, 1))) <= 0)
        return result;
    if (!bio->initialized) {
        LibreSslPutError_nid_no_patch(BioLibrary, AnyFunction, BioUninitialized);
        return -2;
    }
    result = bio->method->gets(bio, buffer, size);
    if (callback)
        result = static_cast<int>(callback(bio, BioCallbackGets | BioCallbackReturn, buffer, size, 0, result));
    return result;
}

int ReadPemLine(LibreSsl::Bio* bio, std::array<char, 256>& line) {
    line[LineCharacterLimit] = '\0';
    int count = BioGets(bio, line.data(), LineCharacterLimit);
    if (count <= 0) return count;
    if (count > LineCharacterLimit)
        throw std::invalid_argument("PEM_read_bio: BIO_gets returned an oversized line");
    while (count >= 0 && line[static_cast<std::size_t>(count)] <= ' ')
        --count;
    line[static_cast<std::size_t>(++count)] = '\n';
    line[static_cast<std::size_t>(++count)] = '\0';
    return count;
}

bool ReadPem(LibreSsl::Bio* bio, std::string& name, std::string& header, std::vector<unsigned char>& data,
    int& length) {
    std::array<char, 256> line{};
    int count = 0;
    for (;;) {
        count = ReadPemLine(bio, line);
        if (count <= 0) {
            PutPemError(PemNoStartLine);
            return false;
        }
        if (std::strncmp(line.data(), "-----BEGIN ", 11) != 0) continue;
        count = static_cast<int>(std::strlen(line.data() + 11));
        if (std::strncmp(line.data() + 11 + count - 6, "-----\n", 6) != 0) continue;
        name.assign(line.data() + 11, static_cast<std::size_t>(count - 6));
        break;
    }

    header.clear();
    bool noHeader = false;
    for (;;) {
        count = ReadPemLine(bio, line);
        if (count <= 0 || line[0] == '\n') break;
        if (std::strncmp(line.data(), "-----END ", 9) == 0) {
            noHeader = true;
            break;
        }
        header.append(line.data(), static_cast<std::size_t>(count));
    }

    ClearBytes(data.data(), data.size());
    data.clear();
    bool end = false;
    if (!noHeader) {
        for (;;) {
            count = ReadPemLine(bio, line);
            if (count <= 0) break;
            if (count != 65) end = true;
            if (std::strncmp(line.data(), "-----END ", 9) == 0 || count > 65) break;
            data.insert(data.end(), line.begin(), line.begin() + count);
            if (end) {
                line[0] = '\0';
                ReadPemLine(bio, line);
                break;
            }
        }
    } else {
        data.assign(header.begin(), header.end());
        ClearBytes(header.data(), header.size());
        header.clear();
    }

    const std::size_t nameLength = name.size();
    if (std::strncmp(line.data(), "-----END ", 9) != 0
        || std::strncmp(name.c_str(), line.data() + 9, nameLength) != 0
        || std::strncmp(line.data() + 9 + nameLength, "-----\n", 6) != 0) {
        PutPemError(PemBadEndLine);
        return false;
    }

    DecodeContext context{};
    if (data.empty()) return false;
    int decoded = static_cast<int>(data.size());
    if (DecodeUpdate(context, data.data(), &decoded, data.data(), decoded) < 0) {
        PutPemError(PemBadBase64Decode);
        return false;
    }
    int finalLength = 0;
    if (DecodeFinal(context, data.data() + decoded, &finalLength) < 0) {
        PutPemError(PemBadBase64Decode);
        return false;
    }
    decoded += finalLength;
    if (decoded == 0) return false;
    length = decoded;
    return true;
}

int PemCheckSuffix(const char* pemString, const char* suffix) {
    const int stringLength = static_cast<int>(std::strlen(pemString));
    const int suffixLength = static_cast<int>(std::strlen(suffix));
    if (suffixLength + 1 >= stringLength) return 0;
    const char* position = pemString + stringLength - suffixLength;
    if (std::strcmp(position, suffix) != 0) return 0;
    --position;
    if (*position != ' ') return 0;
    return static_cast<int>(position - pemString);
}

char AsciiLower(char character) {
    return character >= 'A' && character <= 'Z' ? static_cast<char>(character + ('a' - 'A')) : character;
}

const PkeyAmeth* FindAmethByString(const char* text, int length) {
    if (length < 0) return nullptr;
    const std::size_t count = static_cast<std::size_t>(length);
    for (const PkeyAmeth& ameth : PkeyAmeths) {
        if (std::strlen(ameth.pemString) != count) continue;
        bool equal = true;
        for (std::size_t index = 0; index < count; ++index) {
            if (AsciiLower(ameth.pemString[index]) != AsciiLower(text[index])) {
                equal = false;
                break;
            }
        }
        if (equal) return &ameth;
    }
    return nullptr;
}

bool CheckPem(const std::string& found, const char* wanted) {
    if (found == wanted) return true;
    if (std::strcmp(wanted, AnyPrivateKeyLabel) != 0) return false;
    if (found == EncryptedPrivateKeyLabel || found == Pkcs8InfLabel) return true;
    const int suffixLength = PemCheckSuffix(found.c_str(), PrivateKeySuffix);
    if (suffixLength <= 0) return false;
    const PkeyAmeth* ameth = FindAmethByString(found.c_str(), suffixLength);
    return ameth != nullptr && ameth->oldPrivDecode;
}

bool GetCipherInfo(const std::string& headerText) {
    const char* header = headerText.c_str();
    if (*header == '\0' || *header == '\n') return true;
    if (std::strncmp(header, "Proc-Type: ", 11) != 0) {
        PutPemError(PemNotProcType);
        return false;
    }
    header += 11;
    if (*header != '4') return false;
    ++header;
    if (*header != ',') return false;
    ++header;
    if (std::strncmp(header, "ENCRYPTED", 9) != 0) {
        PutPemError(PemNotEncrypted);
        return false;
    }
    while (*header != '\n' && *header != '\0')
        ++header;
    if (*header == '\0') {
        PutPemError(PemShortHeader);
        return false;
    }
    ++header;
    if (std::strncmp(header, "DEK-Info: ", 10) != 0) {
        PutPemError(PemNotDekInfo);
        return false;
    }
    EncryptedPrivateKeyUnsupported();
}

bool ReadKeyBytes(LibreSsl::Bio* bio, const char* label, std::string& name, std::vector<unsigned char>& data,
    int& length) {
    std::string header;
    for (;;) {
        if (!ReadPem(bio, name, header, data, length)) return false;
        if (CheckPem(name, label)) break;
        ClearBytes(data.data(), data.size());
    }
    return GetCipherInfo(header);
}

int WriteBytes(LibreSsl::Bio* bio, const char* text, int count) {
    return LibreSslBioWrite_nid_no_patch(bio, text, count);
}

int WritePem(LibreSsl::Bio* bio, const char* name, const char* header, const unsigned char* data, int length) {
    EncodeContext context{};
    const int nameLength = static_cast<int>(std::strlen(name));
    const int headerLength = header == nullptr ? 0 : static_cast<int>(std::strlen(header));
    int reason = ErrRBufLib;
    int total = 0;
    int finalLength = 0;
    bool failed = WriteBytes(bio, "-----BEGIN ", 11) != 11
        || WriteBytes(bio, name, nameLength) != nameLength
        || WriteBytes(bio, "-----\n", 6) != 6;
    if (!failed && headerLength > 0)
        failed = WriteBytes(bio, header, headerLength) != headerLength || WriteBytes(bio, "\n", 1) != 1;

    unsigned char* buffer = nullptr;
    if (!failed) {
        buffer = static_cast<unsigned char*>(malloc_nid_postfix(PemWriteBufferLength));
        if (buffer == nullptr) {
            reason = ErrRMallocFailure;
            failed = true;
        }
    }
    int read = 0;
    int remaining = length;
    while (!failed && remaining > 0) {
        const int chunk = remaining > PemEncodeChunk ? PemEncodeChunk : remaining;
        int written = 0;
        if (!EncodeUpdate(context, buffer, &written, data + read, chunk)) {
            failed = true;
            break;
        }
        if (written != 0 && WriteBytes(bio, reinterpret_cast<const char*>(buffer), written) != written) {
            failed = true;
            break;
        }
        total += written;
        remaining -= chunk;
        read += chunk;
    }
    if (!failed) {
        EncodeFinal(context, buffer, &finalLength);
        if (finalLength > 0
            && WriteBytes(bio, reinterpret_cast<const char*>(buffer), finalLength) != finalLength)
            failed = true;
    }
    if (buffer != nullptr) ClearBytes(buffer, PemWriteBufferLength);
    free_nid_postfix(buffer);
    if (!failed)
        failed = WriteBytes(bio, "-----END ", 9) != 9 || WriteBytes(bio, name, nameLength) != nameLength
            || WriteBytes(bio, "-----\n", 6) != 6;
    if (failed) {
        PutPemError(reason);
        return 0;
    }
    return total + finalLength;
}

int WritePkcs8PrivateKey(LibreSsl::Bio* bio, const LibreSsl3EvpPkey* key) {
    auto* p8inf = EVP_PKEY2PKCS8_nid_postfix(key);
    if (p8inf == nullptr) {
        PutPemError(PemErrorConvertingPrivateKey);
        return 0;
    }
    const int estimated = i2d_PKCS8_PRIV_KEY_INFO_nid_postfix(p8inf, nullptr);
    if (estimated < 0 || static_cast<std::size_t>(estimated) + 20
            > static_cast<std::size_t>(std::numeric_limits<std::int64_t>::max())) {
        PutPemError(ErrRAsn1Lib);
        PKCS8_PRIV_KEY_INFO_free_nid_postfix(p8inf);
        return 0;
    }
    auto* data = static_cast<unsigned char*>(malloc_nid_postfix(static_cast<std::size_t>(estimated) + 20));
    if (data == nullptr) {
        PutPemError(ErrRMallocFailure);
        PKCS8_PRIV_KEY_INFO_free_nid_postfix(p8inf);
        return 0;
    }
    unsigned char* cursor = data;
    const int encoded = i2d_PKCS8_PRIV_KEY_INFO_nid_postfix(p8inf, &cursor);
    if (encoded <= 0) {
        PutPemError(ErrRAsn1Lib);
        ClearBytes(data, static_cast<std::size_t>(estimated) + 20);
        free_nid_postfix(data);
        PKCS8_PRIV_KEY_INFO_free_nid_postfix(p8inf);
        return 0;
    }
    const int written = WritePem(bio, Pkcs8InfLabel, "", data, encoded);
    ClearBytes(data, static_cast<std::size_t>(estimated) + 20);
    free_nid_postfix(data);
    PKCS8_PRIV_KEY_INFO_free_nid_postfix(p8inf);
    return written > 0 ? 1 : 0;
}

template<typename T>
bool CopyGuestInPreservingErrno(T& destination, const T* source) {
    return GuestPtr<const T>(source).CopyIn(destination);
}

template<typename T>
bool CopyGuestOutPreservingErrno(T* destination, const T& value) {
    return GuestPtr<T>(destination).CopyOut(value);
}

}

extern "C" {

LibreSsl3EvpPkey* APS5_VABI PEM_read_bio_PrivateKey_nid_disambig1_nid_postfix(LibreSsl::Bio* bio,
    LibreSsl3EvpPkey** key, LibreSslPemPasswordCallback passwordCallback, void* userData) {
    static_cast<void>(passwordCallback);
    static_cast<void>(userData);
    std::string name;
    std::vector<unsigned char> data;
    const ClearOnExit clearData{data};
    int length = 0;
    if (!ReadKeyBytes(bio, AnyPrivateKeyLabel, name, data, length)) return nullptr;

    const unsigned char* cursor = data.data();
    LibreSsl3EvpPkey* ret = nullptr;
    int suffixLength = 0;
    if (name == Pkcs8InfLabel) {
        auto* p8inf = d2i_PKCS8_PRIV_KEY_INFO_nid_postfix(nullptr, &cursor, length);
        if (p8inf != nullptr) {
            ret = EVP_PKCS82PKEY_nid_postfix(p8inf);
            if (key != nullptr) {
                EVP_PKEY_free_nid_disambig1_nid_postfix(*key);
                *key = ret;
            }
            PKCS8_PRIV_KEY_INFO_free_nid_postfix(p8inf);
        }
    } else if (name == EncryptedPrivateKeyLabel) {
        EncryptedPrivateKeyUnsupported();
    } else if ((suffixLength = PemCheckSuffix(name.c_str(), PrivateKeySuffix)) > 0) {
        const PkeyAmeth* ameth = FindAmethByString(name.c_str(), suffixLength);
        if (ameth != nullptr && ameth->oldPrivDecode) {
            if (ameth->type != RsaType && ameth->type != EcType) OldPrivateKeyDecodeUnsupported();
            ret = DecodeTraditionalPrivateKey_nid_no_patch(ameth->type, key, &cursor, length);
        }
    }
    if (ret == nullptr) PutPemError(ErrRAsn1Lib);
    return ret;
}

LibreSsl3Rsa* APS5_VABI PEM_read_RSAPrivateKey_nid_disambig1_nid_postfix(
    FileStream* stream, LibreSsl3Rsa** rsa, LibreSslPemPasswordCallback passwordCallback, void* userData) {
    if (stream == nullptr) throw std::invalid_argument("PEM_read_RSAPrivateKey: null stream");
    std::unique_ptr<LibreSsl::Bio, decltype(&BIO_free_nid_postfix)> bio(
        BIO_new_fp_nid_postfix(stream, 0), BIO_free_nid_postfix);
    if (bio.get() == nullptr) {
        PutPemError(ErrRBufLib);
        return nullptr;
    }
    std::unique_ptr<LibreSsl3EvpPkey, decltype(&EVP_PKEY_free_nid_disambig1_nid_postfix)> key(
        PEM_read_bio_PrivateKey_nid_disambig1_nid_postfix(bio.get(), nullptr, passwordCallback, userData),
        EVP_PKEY_free_nid_disambig1_nid_postfix);
    bio.reset();
    if (key.get() == nullptr) return nullptr;
    std::unique_ptr<LibreSsl3Rsa, decltype(&RSA_free_nid_disambig1_nid_postfix)> decoded(
        EVP_PKEY_get1_RSA_nid_disambig1_nid_postfix(key.get()), RSA_free_nid_disambig1_nid_postfix);
    key.reset();
    if (decoded.get() == nullptr) return nullptr;
    if (rsa != nullptr) {
        LibreSsl3Rsa* previous = nullptr;
        if (!CopyGuestInPreservingErrno(previous, rsa) || !GuestWritable_nid_no_patch(rsa, sizeof(previous))) {
            NotImplemented_nid_no_patch(__func__);
        }
        if (previous != nullptr) RSA_free_nid_disambig1_nid_postfix(previous);
        LibreSsl3Rsa* replacement = decoded.get();
        if (!CopyGuestOutPreservingErrno(rsa, replacement)) NotImplemented_nid_no_patch(__func__);
    }
    return decoded.release();
}

LibreSsl3EvpPkey* APS5_VABI PEM_read_bio_PUBKEY_nid_postfix(LibreSsl::Bio* bio, LibreSsl3EvpPkey** key,
    LibreSslPemPasswordCallback passwordCallback, void* userData) {
    static_cast<void>(passwordCallback);
    static_cast<void>(userData);
    std::string name;
    std::vector<unsigned char> data;
    const ClearOnExit clearData{data};
    int length = 0;
    if (!ReadKeyBytes(bio, PublicKeyLabel, name, data, length)) return nullptr;

    const unsigned char* cursor = data.data();
    LibreSsl3EvpPkey* ret = d2i_PUBKEY_nid_postfix(key, &cursor, length);
    if (ret == nullptr) PutPemError(ErrRAsn1Lib);
    return ret;
}

int APS5_VABI PEM_write_bio_PrivateKey_nid_postfix(LibreSsl::Bio* bio, LibreSsl3EvpPkey* key,
    const LibreSsl3EvpCipher* cipher, unsigned char* keyString, int keyLength,
    LibreSslPemPasswordCallback passwordCallback, void* userData) {
    static_cast<void>(keyString);
    static_cast<void>(keyLength);
    static_cast<void>(passwordCallback);
    static_cast<void>(userData);
    if (cipher != nullptr) EncryptedPrivateKeyUnsupported();
    if (key == nullptr) throw std::invalid_argument("PEM_write_bio_PrivateKey: null key");
    return WritePkcs8PrivateKey(bio, key);
}

}
