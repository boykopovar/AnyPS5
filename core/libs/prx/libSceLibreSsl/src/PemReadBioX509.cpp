/* Derived from LibreSSL 2.5.4 (openbsd 2e2c612066aa993717528a0249c15c51128d61cb) src/lib/libcrypto/pem/pem_lib.c
 * (pem_lib.c,v 1.44) and src/lib/libcrypto/evp/encode.c (encode.c,v 1.24) */
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
#include "prx/libSceLibreSsl/include/Error.hpp"
#include "prx/libc/include/Api.hpp"
#include "prx/libc/include/General.hpp"
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <stdexcept>
#include <string>
#include <utility>
#include "prx/libSceLibreSsl/include/Bio.hpp"
#include "PemReadBio.hpp"

namespace LibreSsl {

namespace {

constexpr int BufferLibrary = 7;
constexpr int BioLibrary = 32;
constexpr int PemLibrary = 9;
constexpr int ErrorFunctionUnknown = 0xfff;
constexpr int ErrorMallocFailure = 65;
constexpr int BioUninitialized = 120;
constexpr int BioUnsupportedMethod = 121;
constexpr int PemBadBase64Decode = 100;
constexpr int PemBadEndLine = 102;
constexpr int PemNotDekInfo = 105;
constexpr int PemNotEncrypted = 106;
constexpr int PemNotProcType = 107;
constexpr int PemNoStartLine = 108;
constexpr int PemShortHeader = 112;
constexpr int BioCallbackGets = 0x05;
constexpr int BioCallbackReturn = 0x80;
constexpr int KeyRsa = 6;
constexpr int KeyDsa = 116;
constexpr int KeyEc = 408;
constexpr int KeyHmac = 855;
constexpr unsigned char Base64Newline = 0xF0;
constexpr unsigned char Base64CarriageReturn = 0xF1;
constexpr unsigned char Base64End = 0xF2;
constexpr unsigned char Base64Whitespace = 0xE0;

constexpr unsigned char Base64Values[128] = {
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xE0, 0xF0, 0xFF, 0xFF, 0xF1, 0xFF, 0xFF,
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
    0xE0, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x3E, 0xFF, 0xF2, 0xFF, 0x3F,
    0x34, 0x35, 0x36, 0x37, 0x38, 0x39, 0x3A, 0x3B, 0x3C, 0x3D, 0xFF, 0xFF, 0xFF, 0x00, 0xFF, 0xFF,
    0xFF, 0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E,
    0x0F, 0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17, 0x18, 0x19, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
    0xFF, 0x1A, 0x1B, 0x1C, 0x1D, 0x1E, 0x1F, 0x20, 0x21, 0x22, 0x23, 0x24, 0x25, 0x26, 0x27, 0x28,
    0x29, 0x2A, 0x2B, 0x2C, 0x2D, 0x2E, 0x2F, 0x30, 0x31, 0x32, 0x33, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
};

struct DecodeContext {
    int num;
    int length;
    unsigned char data[80];
    int lineNumber;
    int expectNewline;
};

unsigned char Base64Value(unsigned char character) {
    return Base64Values[character & 0x7f];
}

bool NotBase64(int value) {
    return (value | 0x13) == 0xF3;
}

int DecodeBlock(unsigned char* to, const unsigned char* from, int count) {
    while (Base64Value(*from) == Base64Whitespace && count > 0) {
        ++from;
        --count;
    }
    while (count > 3 && NotBase64(Base64Value(from[count - 1]))) --count;
    if (count % 4 != 0) return -1;
    int result = 0;
    for (int index = 0; index < count; index += 4) {
        const int a = Base64Value(*from++);
        const int b = Base64Value(*from++);
        const int c = Base64Value(*from++);
        const int d = Base64Value(*from++);
        if ((a & 0x80) || (b & 0x80) || (c & 0x80) || (d & 0x80)) return -1;
        const unsigned long value = static_cast<unsigned long>(a) << 18 | static_cast<unsigned long>(b) << 12 |
                                    static_cast<unsigned long>(c) << 6 | static_cast<unsigned long>(d);
        *to++ = static_cast<unsigned char>(value >> 16);
        *to++ = static_cast<unsigned char>(value >> 8);
        *to++ = static_cast<unsigned char>(value);
        result += 3;
    }
    return result;
}

int DecodeUpdate(DecodeContext& context, unsigned char* out, int* outLength, const unsigned char* in, int inLength) {
    int paddingStart = -1;
    int padding = 0;
    int status = -1;
    int total = 0;
    int count = context.num;
    unsigned char* const data = context.data;
    int line = context.lineNumber;
    int expectNewline = context.expectNewline;

    if (inLength == 0 || (count == 0 && Base64Value(in[0]) == Base64End)) {
        status = 0;
    } else {
        int index = 0;
        for (; index < inLength; ++index) {
            if (line >= 80) break;
            const unsigned char character = *in++;
            int value = Base64Value(character);
            if (!NotBase64(value)) {
                if (count >= static_cast<int>(sizeof(context.data)))
                    throw std::logic_error("EVP_DecodeUpdate saved more than 80 characters");
                data[count++] = character;
                ++line;
            }
            if (padding && character != '=' && character != '\r' && character != '\n' && value != Base64End) break;
            if (character == '=') {
                if (paddingStart == -1) paddingStart = count;
                ++padding;
            }
            if (padding > 2) break;
            if (value == Base64CarriageReturn) {
                line = 0;
                if (expectNewline) continue;
            }
            if (value == Base64Newline) {
                line = 0;
                if (expectNewline) {
                    expectNewline = 0;
                    continue;
                }
            }
            expectNewline = 0;
            if (index + 1 == inLength && ((count & 3) == 0 || padding)) {
                value = Base64End;
                padding = 0;
                if (count >= 1 && data[count - 1] == '=') ++padding;
                if (count >= 2 && data[count - 2] == '=') ++padding;
            }
            if ((value == Base64End && (count & 3) == 0) || count >= 64) {
                if (value != Base64End && count >= 64) expectNewline = 1;
                if (count > 0) {
                    value = DecodeBlock(out, data, count);
                    count = 0;
                    if (value < 0) {
                        status = 0;
                        break;
                    }
                    total += value - padding;
                } else {
                    padding = 1;
                    value = 0;
                }
                if (value < context.length && padding) {
                    status = 0;
                    break;
                }
                context.length = value;
                if (paddingStart >= 0) {
                    status = 0;
                    break;
                }
                out += value;
            }
        }
        if (index == inLength) status = 1;
    }
    *outLength = total;
    context.num = count;
    context.lineNumber = line;
    context.expectNewline = expectNewline;
    return status;
}

int DecodeFinal(DecodeContext& context, unsigned char* out, int* outLength) {
    *outLength = 0;
    if (context.num == 0) return 1;
    const int count = DecodeBlock(out, context.data, context.num);
    if (count < 0) return -1;
    context.num = 0;
    *outLength = count;
    return 1;
}

void PutBufferError() {
    LibreSslPutError_nid_no_patch(BufferLibrary, ErrorFunctionUnknown, ErrorMallocFailure, __FILE__, __LINE__);
}

PemBuffer* NewPemBuffer() {
    auto* const buffer = static_cast<PemBuffer*>(calloc_nid_postfix(1, sizeof(PemBuffer)));
    if (buffer == nullptr) PutBufferError();
    return buffer;
}

bool GrowPemBuffer(PemBuffer* buffer, std::size_t length, bool clean = false) {
    if (buffer->length >= length) {
        if (clean) std::memset(buffer->data + length, 0, buffer->length - length);
        buffer->length = length;
        return true;
    }
    if (buffer->capacity >= length) {
        buffer->length = length;
        return true;
    }
    if (length > 0x5ffffffc) {
        PutBufferError();
        return false;
    }
    const std::size_t capacity = ((length + 3) / 3) * 4;
    auto* const grown = static_cast<char*>(recallocarray_nid_postfix(buffer->data, buffer->capacity, capacity, 1));
    if (grown == nullptr) {
        PutBufferError();
        return false;
    }
    buffer->data = grown;
    buffer->capacity = capacity;
    buffer->length = length;
    return true;
}

int BioGets(Bio* bio, char* buffer, int size) {
    if (bio == nullptr || bio->method == nullptr || bio->method->gets == nullptr) {
        LibreSslPutError_nid_no_patch(BioLibrary, ErrorFunctionUnknown, BioUnsupportedMethod, __FILE__, __LINE__);
        return -2;
    }
    const auto callback = bio->callback;
    int result = 0;
    if (callback && (result = static_cast<int>(callback(bio, BioCallbackGets, buffer, size, 0, 1))) <= 0) return result;
    if (!bio->initialized) {
        LibreSslPutError_nid_no_patch(BioLibrary, ErrorFunctionUnknown, BioUninitialized, __FILE__, __LINE__);
        return -2;
    }
    result = bio->method->gets(bio, buffer, size);
    if (callback) result = static_cast<int>(callback(bio, BioCallbackGets | BioCallbackReturn, buffer, size, 0, result));
    return result;
}

int ReadLine(Bio* bio, std::array<char, 256>& line) {
    line[254] = '\0';
    int index = BioGets(bio, line.data(), 254);
    if (index <= 0) return index;
    if (index > 254) throw std::invalid_argument("PEM_read_bio BIO_gets returned an oversized line");
    while (index >= 0 && line[static_cast<std::size_t>(index)] <= ' ') --index;
    line[static_cast<std::size_t>(++index)] = '\n';
    line[static_cast<std::size_t>(++index)] = '\0';
    return index;
}

bool ReadPem(Bio* bio, PemBuffers& buffers, long& length) {
    buffers.name = NewPemBuffer();
    buffers.header = NewPemBuffer();
    buffers.data = NewPemBuffer();
    if (buffers.name == nullptr || buffers.header == nullptr || buffers.data == nullptr) {
        PutPemError(ErrorMallocFailure);
        return false;
    }

    std::array<char, 256> line{};
    int count = 0;
    for (;;) {
        if (ReadLine(bio, line) <= 0) {
            PutPemError(PemNoStartLine);
            return false;
        }
        if (std::strncmp(line.data(), "-----BEGIN ", 11) != 0) continue;
        count = static_cast<int>(std::strlen(line.data() + 11));
        if (std::strncmp(line.data() + 11 + count - 6, "-----\n", 6) != 0) continue;
        if (!GrowPemBuffer(buffers.name, static_cast<std::size_t>(count + 9))) {
            PutPemError(ErrorMallocFailure);
            return false;
        }
        std::memcpy(buffers.name->data, line.data() + 11, static_cast<std::size_t>(count - 6));
        buffers.name->data[count - 6] = '\0';
        break;
    }

    int headerLength = 0;
    bool noHeader = false;
    if (!GrowPemBuffer(buffers.header, 256)) {
        PutPemError(ErrorMallocFailure);
        return false;
    }
    buffers.header->data[0] = '\0';
    for (;;) {
        count = ReadLine(bio, line);
        if (count <= 0 || line[0] == '\n') break;
        if (!GrowPemBuffer(buffers.header, static_cast<std::size_t>(headerLength + count + 9))) {
            PutPemError(ErrorMallocFailure);
            return false;
        }
        if (std::strncmp(line.data(), "-----END ", 9) == 0) {
            noHeader = true;
            break;
        }
        std::memcpy(buffers.header->data + headerLength, line.data(), static_cast<std::size_t>(count));
        buffers.header->data[headerLength + count] = '\0';
        headerLength += count;
    }

    int bodyLength = 0;
    if (!GrowPemBuffer(buffers.data, 1024)) {
        PutPemError(ErrorMallocFailure);
        return false;
    }
    buffers.data->data[0] = '\0';
    if (!noHeader) {
        bool end = false;
        for (;;) {
            count = ReadLine(bio, line);
            if (count <= 0) break;
            if (count != 65) end = true;
            if (std::strncmp(line.data(), "-----END ", 9) == 0 || count > 65) break;
            if (!GrowPemBuffer(buffers.data, static_cast<std::size_t>(count + bodyLength + 9), true)) {
                PutPemError(ErrorMallocFailure);
                return false;
            }
            std::memcpy(buffers.data->data + bodyLength, line.data(), static_cast<std::size_t>(count));
            buffers.data->data[bodyLength + count] = '\0';
            bodyLength += count;
            if (end) {
                line[0] = '\0';
                ReadLine(bio, line);
                break;
            }
        }
    } else {
        std::swap(buffers.header, buffers.data);
        bodyLength = headerLength;
    }

    const std::size_t nameLength = std::strlen(buffers.name->data);
    if (std::strncmp(line.data(), "-----END ", 9) != 0 || std::strncmp(buffers.name->data, line.data() + 9, nameLength) != 0 ||
        std::strncmp(line.data() + 9 + nameLength, "-----\n", 6) != 0) {
        PutPemError(PemBadEndLine);
        return false;
    }

    DecodeContext context{0, 30, {}, 0, 0};
    auto* const bytes = reinterpret_cast<unsigned char*>(buffers.data->data);
    if (DecodeUpdate(context, bytes, &bodyLength, bytes, bodyLength) < 0) {
        PutPemError(PemBadBase64Decode);
        return false;
    }
    int finalLength = 0;
    if (DecodeFinal(context, bytes + bodyLength, &finalLength) < 0) {
        PutPemError(PemBadBase64Decode);
        return false;
    }
    bodyLength += finalLength;
    if (bodyLength == 0) return false;
    length = bodyLength;
    return true;
}

bool CheckPem(const char* found, const char* wanted) {
    if (!std::strcmp(found, wanted)) return true;
    if (!std::strcmp(wanted, "ANY PRIVATE KEY")) {
        if (!std::strcmp(found, "ENCRYPTED PRIVATE KEY") || !std::strcmp(found, "PRIVATE KEY")) return true;
        const int prefix = PemCheckSuffix(found, "PRIVATE KEY");
        return prefix > 0 && OldPrivateKeyType(found, prefix) != 0;
    }
    constexpr std::pair<const char*, const char*> aliases[] {
        {"X509 CERTIFICATE", "CERTIFICATE"},
        {"NEW CERTIFICATE REQUEST", "CERTIFICATE REQUEST"},
        {"CERTIFICATE", "TRUSTED CERTIFICATE"},
        {"X509 CERTIFICATE", "TRUSTED CERTIFICATE"},
        {"CERTIFICATE", "PKCS7"},
        {"PKCS #7 SIGNED DATA", "PKCS7"},
    };
    for (const auto& [name, accepted] : aliases)
        if (!std::strcmp(found, name) && !std::strcmp(wanted, accepted)) return true;
    return false;
}

bool ReadCipherInfo(const char* header, const char* function) {
    if (header == nullptr || *header == '\0' || *header == '\n') return true;
    if (std::strncmp(header, "Proc-Type: ", 11) != 0) {
        PutPemError(PemNotProcType);
        return false;
    }
    header += 11;
    if (*header++ != '4') return false;
    if (*header++ != ',') return false;
    if (std::strncmp(header, "ENCRYPTED", 9) != 0) {
        PutPemError(PemNotEncrypted);
        return false;
    }
    while (*header != '\n' && *header != '\0') ++header;
    if (*header == '\0') {
        PutPemError(PemShortHeader);
        return false;
    }
    ++header;
    if (std::strncmp(header, "DEK-Info: ", 10) != 0) {
        PutPemError(PemNotDekInfo);
        return false;
    }
    NotImplemented_nid_no_patch((std::string(function) + ": DEK-Info cipher lookup and decryption of an encrypted PEM record").c_str());
}

}

void PutPemError(int reason) {
    LibreSslPutError_nid_no_patch(PemLibrary, ErrorFunctionUnknown, reason, __FILE__, __LINE__);
}

void FreePemBuffer(PemBuffer* buffer) {
    if (buffer == nullptr) return;
    if (buffer->data != nullptr) {
        auto* const bytes = reinterpret_cast<volatile unsigned char*>(buffer->data);
        for (std::size_t index = 0; index < buffer->capacity; ++index) bytes[index] = 0;
        free_nid_postfix(buffer->data);
    }
    free_nid_postfix(buffer);
}

int PemCheckSuffix(const char* name, const char* suffix) {
    const int nameLength = static_cast<int>(std::strlen(name));
    const int suffixLength = static_cast<int>(std::strlen(suffix));
    if (suffixLength + 1 >= nameLength) return 0;
    const char* position = name + nameLength - suffixLength;
    if (std::strcmp(position, suffix) != 0) return 0;
    if (*--position != ' ') return 0;
    return static_cast<int>(position - name);
}

int OldPrivateKeyType(const char* name, int length) {
    constexpr std::pair<const char*, int> methods[] {{"RSA", KeyRsa}, {"DSA", KeyDsa}, {"EC", KeyEc}, {"HMAC", KeyHmac}};
    for (const auto& [text, type] : methods) {
        if (static_cast<int>(std::strlen(text)) != length) continue;
        int index = 0;
        while (index < length && (text[index] | 0x20) == (name[index] | 0x20)) ++index;
        if (index == length) return type;
    }
    return 0;
}

bool ReadPemBytes(Bio* bio, const char* name, PemBuffers& record, long& length, const char* function) {
    for (;;) {
        record.Clear();
        if (!ReadPem(bio, record, length)) return false;
        if (CheckPem(record.name->data, name)) break;
    }
    return ReadCipherInfo(record.header->data, function);
}

LibreSslX509* ReadPemRecord(Bio* bio, LibreSslX509** certificate, LibreSslPemPasswordCallback callback,
                            void* userData, const char* name, const char* function) {
    static_cast<void>(certificate);
    static_cast<void>(callback);
    static_cast<void>(userData);
    PemBuffers record;
    long length = 0;
    if (!ReadPemBytes(bio, name, record, length, function)) return nullptr;
    NotImplemented_nid_no_patch(function);
}

}

extern "C" {

LibreSslX509* APS5_VABI PEM_read_bio_X509_nid_postfix(LibreSsl::Bio* bio, LibreSslX509** certificate,
                                                      LibreSslPemPasswordCallback callback, void* userData) {
    return LibreSsl::ReadPemRecord(bio, certificate, callback, userData, "CERTIFICATE", __func__);
}

}
