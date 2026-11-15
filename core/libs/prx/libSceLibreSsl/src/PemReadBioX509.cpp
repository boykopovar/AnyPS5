/* Derived from LibreSSL 2.5.4 (openbsd 2e2c612066aa993717528a0249c15c51128d61cb): src/lib/libcrypto/pem/pem_lib.c (v 1.44) L187-269, L271-314, L480-539 and L664-835;
 * src/lib/libcrypto/pem/pem_oth.c (v 1.15) L70-87; src/lib/libcrypto/pem/pem_x509.c (v 1.16) L82-87; src/lib/libcrypto/pem/pem_xaux.c (v 1.14) L82-87;
 * src/lib/libcrypto/evp/encode.c (v 1.24) L89-113 and L217-423 */
#include "prx/libSceLibreSsl/libSceLibreSsl.hpp"
#include "prx/libSceLibreSsl/include/Error.hpp"
#include "prx/libc/include/Api.hpp"
#include "prx/libc/include/General.hpp"
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <stdexcept>
#include "prx/libSceLibreSsl/include/Bio.hpp"
#include "PemReadBio.hpp"
#include "X509Aux.hpp"

namespace LibreSsl {

constexpr int BufferLibrary = 7;
constexpr int BioLibrary = 32;
constexpr int PemLibrary = 9;
constexpr int ErrorFunctionUnknown = 0xfff;
constexpr int ErrorMallocFailure = 65;
constexpr int ErrorAsn1Library = 13;
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
constexpr std::size_t LineSize = 256;
constexpr std::size_t BodyLineSize = 65;
constexpr int EncodedChunkSize = 80;
constexpr int MaximumEncodedLine = 80;

static_assert(sizeof(PemBuffer) == 24);

void PutBufferError() {
    LibreSslPutError_nid_no_patch(BufferLibrary, ErrorFunctionUnknown, ErrorMallocFailure, __FILE__, __LINE__);
}

void PutPemError(int reason) {
    LibreSslPutError_nid_no_patch(PemLibrary, ErrorFunctionUnknown, reason, __FILE__, __LINE__);
}

PemBuffer* NewPemBuffer() {
    auto* const buffer = static_cast<PemBuffer*>(calloc_nid_postfix(1, sizeof(PemBuffer)));
    if (buffer == nullptr) PutBufferError();
    return buffer;
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

bool PemBuffers::Allocate() {
    name = NewPemBuffer();
    header = NewPemBuffer();
    data = NewPemBuffer();
    return name != nullptr && header != nullptr && data != nullptr;
}

void PemBuffers::Clear() {
    FreePemBuffer(name);
    FreePemBuffer(header);
    FreePemBuffer(data);
    name = header = data = nullptr;
}

PemBuffers::~PemBuffers() {
    Clear();
}

bool GrowPemBuffer(PemBuffer* buffer, std::size_t length) {
    if (buffer->length >= length || buffer->capacity >= length) {
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

bool ReadLine(Bio* bio, std::array<char, LineSize>& line, std::size_t& length) {
    line[254] = '\0';
    const int count = BioGets(bio, line.data(), 254);
    if (count <= 0) return false;
    if (count > 254) throw std::invalid_argument("PEM_read_bio returned an oversized line");
    int index = count;
    while (index >= 0 && line[static_cast<std::size_t>(index)] <= ' ') --index;
    line[static_cast<std::size_t>(++index)] = '\n';
    line[static_cast<std::size_t>(++index)] = '\0';
    length = static_cast<std::size_t>(index);
    return true;
}

bool IsBeginLine(const std::array<char, LineSize>& line, std::size_t length, std::size_t& nameLength) {
    static constexpr char prefix[] = "-----BEGIN ";
    static constexpr char suffix[] = "-----\n";
    if (length < sizeof(prefix) - 1 || std::memcmp(line.data(), prefix, sizeof(prefix) - 1) != 0) return false;
    const std::size_t content = length - (sizeof(prefix) - 1);
    if (content < sizeof(suffix) - 1) return false;
    if (std::memcmp(line.data() + length - (sizeof(suffix) - 1), suffix, sizeof(suffix) - 1) != 0) return false;
    nameLength = content - (sizeof(suffix) - 1);
    return true;
}

struct EncodeContext {
    std::array<unsigned char, EncodedChunkSize> encoded{};
    int number = 0;
    int length = 0;
    int lineNumber = 0;
    int expectNewline = 0;
};

constexpr int Base64Whitespace = 0xE0;
constexpr int Base64EndOfLine = 0xF0;
constexpr int Base64CarriageReturn = 0xF1;
constexpr int Base64End = 0xF2;
constexpr int Base64Error = 0xFF;

constexpr std::array<unsigned char, 128> AsciiToBin = {
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xE0, 0xF0, 0xFF, 0xFF, 0xF1, 0xFF, 0xFF,
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
    0xE0, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x3E, 0xFF, 0xF2, 0xFF, 0x3F,
    0x34, 0x35, 0x36, 0x37, 0x38, 0x39, 0x3A, 0x3B, 0x3C, 0x3D, 0xFF, 0xFF, 0xFF, 0x00, 0xFF, 0xFF,
    0xFF, 0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E,
    0x0F, 0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17, 0x18, 0x19, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
    0xFF, 0x1A, 0x1B, 0x1C, 0x1D, 0x1E, 0x1F, 0x20, 0x21, 0x22, 0x23, 0x24, 0x25, 0x26, 0x27, 0x28,
    0x29, 0x2A, 0x2B, 0x2C, 0x2D, 0x2E, 0x2F, 0x30, 0x31, 0x32, 0x33, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
};

int AsciiToBinValue(unsigned char value) {
    return AsciiToBin.at(value & 0x7f);
}

bool IsNotBase64(int value) {
    return (value | 0x13) == 0xF3;
}

void DecodeInit(EncodeContext* context) {
    context->length = 30;
    context->number = 0;
    context->lineNumber = 0;
    context->expectNewline = 0;
}

int DecodeBlock(unsigned char* target, const unsigned char* source, int length) {
    int result = 0;
    while (length > 0 && AsciiToBinValue(*source) == Base64Whitespace) {
        ++source;
        --length;
    }
    while (length > 3 && IsNotBase64(AsciiToBinValue(source[length - 1]))) --length;
    if (length % 4 != 0) return -1;
    for (int index = 0; index < length; index += 4) {
        const int first = AsciiToBinValue(*(source++));
        const int second = AsciiToBinValue(*(source++));
        const int third = AsciiToBinValue(*(source++));
        const int fourth = AsciiToBinValue(*(source++));
        if ((first & 0x80) || (second & 0x80) || (third & 0x80) || (fourth & 0x80)) return -1;
        const auto group = (static_cast<unsigned long>(first) << 18L) | (static_cast<unsigned long>(second) << 12L) |
            (static_cast<unsigned long>(third) << 6L) | static_cast<unsigned long>(fourth);
        *(target++) = static_cast<unsigned char>(group >> 16L) & 0xff;
        *(target++) = static_cast<unsigned char>(group >> 8L) & 0xff;
        *(target++) = static_cast<unsigned char>(group) & 0xff;
        result += 3;
    }
    return result;
}

int DecodeUpdate(EncodeContext* context, unsigned char* out, int* outLength, const unsigned char* in, int inLength) {
    int seof = -1;
    int eof = 0;
    int written = 0;
    int number = context->number;
    int lineNumber = context->lineNumber;
    int expectNewline = context->expectNewline;
    unsigned char* stored = context->encoded.data();
    const auto finish = [&](int result) {
        *outLength = written;
        context->number = number;
        context->lineNumber = lineNumber;
        context->expectNewline = expectNewline;
        return result;
    };
    if (inLength == 0 || (number == 0 && AsciiToBinValue(in[0]) == Base64End)) return finish(0);
    for (int index = 0; index < inLength; ++index) {
        if (lineNumber >= MaximumEncodedLine) return finish(-1);
        const unsigned char character = *(in++);
        int value = AsciiToBinValue(character);
        if (!IsNotBase64(value)) {
            if (number >= EncodedChunkSize) NotImplemented_nid_no_patch("EVP_DecodeUpdate with more than 80 stored characters");
            stored[number++] = character;
            ++lineNumber;
        } else if (value == Base64Error) {
            return finish(-1);
        }
        if (eof && character != '=' && character != '\r' && character != '\n' && value != Base64End) return finish(-1);
        if (character == '=') {
            if (seof == -1) seof = number;
            ++eof;
        }
        if (eof > 2) return finish(-1);
        if (value == Base64CarriageReturn) {
            lineNumber = 0;
            if (expectNewline) continue;
        }
        if (value == Base64EndOfLine) {
            lineNumber = 0;
            if (expectNewline) {
                expectNewline = 0;
                continue;
            }
        }
        expectNewline = 0;
        if ((index + 1) == inLength && ((number & 3) == 0 || eof != 0)) {
            value = Base64End;
            eof = 0;
            if (stored[number - 1] == '=') ++eof;
            if (stored[number - 2] == '=') ++eof;
        }
        if ((value == Base64End && (number & 3) == 0) || number >= 64) {
            if (value != Base64End && number >= 64) expectNewline = 1;
            int chunk = 0;
            if (number > 0) {
                chunk = DecodeBlock(out, stored, number);
                number = 0;
                if (chunk < 0) return finish(0);
                written += chunk - eof;
            } else {
                eof = 1;
            }
            if (chunk < context->length && eof != 0) return finish(0);
            context->length = chunk;
            if (seof >= 0) return finish(0);
            out += chunk;
        }
    }
    return finish(1);
}

int DecodeFinal(EncodeContext* context, unsigned char* out, int* outLength) {
    *outLength = 0;
    if (context->number == 0) return 1;
    const int decoded = DecodeBlock(out, context->encoded.data(), context->number);
    if (decoded < 0) return -1;
    context->number = 0;
    *outLength = decoded;
    return 1;
}

bool ReadRecord(Bio* bio, PemBuffers& buffers, std::size_t& decodedLength) {
    std::array<char, LineSize> line{};
    std::size_t lineLength = 0;
    for (;;) {
        if (!ReadLine(bio, line, lineLength)) {
            PutPemError(PemNoStartLine);
            return false;
        }
        std::size_t nameLength = 0;
        if (!IsBeginLine(line, lineLength, nameLength)) continue;
        if (!GrowPemBuffer(buffers.name, nameLength + 1)) {
            PutPemError(ErrorMallocFailure);
            return false;
        }
        std::memcpy(buffers.name->data, line.data() + 11, nameLength);
        buffers.name->data[nameLength] = '\0';
        break;
    }

    std::size_t headerLength = 0;
    if (!GrowPemBuffer(buffers.header, 256)) {
        PutPemError(ErrorMallocFailure);
        return false;
    }
    buffers.header->data[0] = '\0';
    bool withoutHeader = false;
    for (;;) {
        if (!ReadLine(bio, line, lineLength)) break;
        if (line[0] == '\n') break;
        if (!GrowPemBuffer(buffers.header, headerLength + lineLength + 9)) {
            PutPemError(ErrorMallocFailure);
            return false;
        }
        if (std::strncmp(line.data(), "-----END ", 9) == 0) {
            withoutHeader = true;
            break;
        }
        std::memcpy(buffers.header->data + headerLength, line.data(), lineLength);
        buffers.header->data[headerLength + lineLength] = '\0';
        headerLength += lineLength;
    }

    std::size_t length = 0;
    if (!GrowPemBuffer(buffers.data, 1024)) {
        PutPemError(ErrorMallocFailure);
        return false;
    }
    buffers.data->data[0] = '\0';
    bool lastLine = false;
    if (!withoutHeader) {
        for (;;) {
            if (!ReadLine(bio, line, lineLength)) break;
            if (lineLength != BodyLineSize) lastLine = true;
            if (std::strncmp(line.data(), "-----END ", 9) == 0) break;
            if (lineLength > BodyLineSize) break;
            if (!GrowPemBuffer(buffers.data, length + lineLength + 9)) {
                PutPemError(ErrorMallocFailure);
                return false;
            }
            std::memcpy(buffers.data->data + length, line.data(), lineLength);
            buffers.data->data[length + lineLength] = '\0';
            length += lineLength;
            if (lastLine) {
                line[0] = '\0';
                lineLength = 1;
                ReadLine(bio, line, lineLength);
                break;
            }
        }
    } else {
        std::swap(buffers.header, buffers.data);
        length = headerLength;
    }

    const std::size_t nameLength = std::strlen(buffers.name->data);
    if (std::strncmp(line.data(), "-----END ", 9) != 0 || std::strncmp(buffers.name->data, line.data() + 9, nameLength) != 0 ||
        std::strncmp(line.data() + 9 + nameLength, "-----\n", 6) != 0) {
        PutPemError(PemBadEndLine);
        return false;
    }

    EncodeContext context{};
    DecodeInit(&context);
    int decoded = static_cast<int>(length);
    if (DecodeUpdate(&context, reinterpret_cast<unsigned char*>(buffers.data->data), &decoded,
            reinterpret_cast<const unsigned char*>(buffers.data->data), static_cast<int>(length)) < 0) {
        PutPemError(PemBadBase64Decode);
        return false;
    }
    int final = 0;
    if (DecodeFinal(&context, reinterpret_cast<unsigned char*>(buffers.data->data) + decoded, &final) < 0) {
        PutPemError(PemBadBase64Decode);
        return false;
    }
    decoded += final;
    if (decoded == 0) return false;
    decodedLength = static_cast<std::size_t>(decoded);
    return true;
}

bool CheckPem(const char* name, const char* expected) {
    if (std::strcmp(name, expected) == 0) return true;
    if (std::strcmp(expected, "CERTIFICATE") == 0) return std::strcmp(name, "X509 CERTIFICATE") == 0;
    return std::strcmp(name, "CERTIFICATE") == 0 || std::strcmp(name, "X509 CERTIFICATE") == 0;
}

bool CipherInfo(const char* header) {
    if (header == nullptr || *header == '\0' || *header == '\n') return true;
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
    NotImplemented_nid_no_patch("PEM_read_bio of an encrypted record");
}

LibreSslX509* ReadPemRecord(Bio* bio, LibreSslX509** certificate, LibreSslPemPasswordCallback callback, void* userData,
    const char* function, const char* expectedLabel) {
    static_cast<void>(callback);
    static_cast<void>(userData);
    static_cast<void>(function);
    const bool auxiliary = std::strcmp(expectedLabel, "TRUSTED CERTIFICATE") == 0;
    PemBuffers buffers;
    std::size_t decodedLength = 0;
    for (;;) {
        if (!buffers.Allocate()) {
            PutPemError(ErrorMallocFailure);
            return nullptr;
        }
        if (!ReadRecord(bio, buffers, decodedLength)) return nullptr;
        if (CheckPem(buffers.name->data, expectedLabel)) break;
        buffers.Clear();
    }
    if (!CipherInfo(buffers.header->data)) return nullptr;
    const unsigned char* input = reinterpret_cast<const unsigned char*>(buffers.data->data);
    LibreSslX509* result = auxiliary ? X509AuxRead_nid_no_patch(certificate, &input, static_cast<std::int64_t>(decodedLength))
                                     : d2i_X509_nid_postfix(certificate, &input, static_cast<std::int64_t>(decodedLength));
    if (result == nullptr) PutPemError(ErrorAsn1Library);
    return result;
}

}

extern "C" {

LibreSslX509* APS5_VABI PEM_read_bio_X509_nid_postfix(LibreSsl::Bio* bio, LibreSslX509** certificate,
    LibreSslPemPasswordCallback callback, void* userData) {
    return LibreSsl::ReadPemRecord(bio, certificate, callback, userData, __func__, "CERTIFICATE");
}

}
