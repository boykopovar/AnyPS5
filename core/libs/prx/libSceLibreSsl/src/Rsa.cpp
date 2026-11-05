#include "prx/libSceLibreSsl/include/Api.hpp"
#include <atomic>
#include <bit>
#include <climits>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <stdexcept>
#include <string>
#include <vector>
#include "mbedtls/bignum.h"
#include "mbedtls/sha1.h"
#include "prx/libc/include/Api.hpp"
#include "prx/libc/include/General.hpp"
#include "prx/libc/include/HostEntropy.hpp"

struct LibreSslBignum {
    std::uint64_t* d;
    int top;
    int dmax;
    int neg;
    int flags;
};

static_assert(sizeof(LibreSslBignum) == 24);
static_assert(offsetof(LibreSslBignum, top) == 8);
static_assert(sizeof(LibreSslRsaMethod) == 112);
static_assert(offsetof(LibreSslRsaMethod, flags) == 72);
static_assert(sizeof(LibreSslRsa) == 152);
static_assert(offsetof(LibreSslRsa, meth) == 16);
static_assert(offsetof(LibreSslRsa, n) == 32);
static_assert(offsetof(LibreSslRsa, flags) == 108);

namespace {

using RsaCallback = int (APS5_VABI*)(LibreSslRsa* rsa);
using RsaCrypt = int (APS5_VABI*)(int length, const unsigned char* from, unsigned char* to, LibreSslRsa* rsa, int padding);
using RsaVerify = int (APS5_VABI*)(int type, const unsigned char* digest, unsigned int digestLength, const unsigned char* signature, unsigned int signatureLength, const LibreSslRsa* rsa);
using Words = std::vector<std::uint64_t>;

constexpr int OperationPublicEncrypt = 0;
constexpr int OperationPublicDecrypt = 1;
constexpr int OperationBignumModExp = 5;
constexpr int OperationInit = 6;
constexpr int OperationFinish = 7;
constexpr int RsaFlagCachePublic = 0x0002;
constexpr int RsaFlagCachePrivate = 0x0004;
constexpr int RsaFlagSignVerify = 0x0040;
constexpr int RsaFlagNonFipsAllow = 0x0400;
constexpr int BignumFlagStaticData = 0x02;
constexpr int BignumFlagMalloced = 0x01;
constexpr int PaddingPkcs1 = 1;
constexpr int PaddingSslv23 = 2;
constexpr int PaddingNone = 3;
constexpr int PaddingOaep = 4;
constexpr int PaddingX931 = 5;
constexpr int MaxModulusBits = 16384;
constexpr int SmallModulusBits = 3072;
constexpr int MaxPublicExponentBits = 64;
constexpr int Sha1Bytes = 20;
constexpr int DigestMd5Sha1 = 114;
constexpr int ObjectTypeCount = 956;
constexpr unsigned int Md5Sha1Bytes = 36;

template<typename TFunction> TFunction Operation(const LibreSslRsaMethod& method, int index) {
    return reinterpret_cast<TFunction>(method.operations[index]);
}

void Wipe(void* data, std::size_t size) {
    auto* bytes = static_cast<volatile unsigned char*>(data);
    for (std::size_t index = 0; index < size; ++index) bytes[index] = 0;
}

void ClearFreeBignum(LibreSslBignum* number) {
    if (!number) return;
    if (number->d && !(number->flags & BignumFlagStaticData)) {
        if (number->dmax < 0) throw std::invalid_argument("BN_clear_free: negative word capacity");
        Wipe(number->d, static_cast<std::size_t>(number->dmax) * sizeof(number->d[0]));
        free_nid_postfix(number->d);
    }
    const bool allocated = number->flags & BignumFlagMalloced;
    Wipe(number, sizeof(LibreSslBignum));
    if (allocated) free_nid_postfix(number);
}

LibreSslBignum* NewBignum(const unsigned char* bytes, std::size_t length) {
    constexpr std::size_t maxWords = INT_MAX / (4 * 64);
    auto* number = static_cast<LibreSslBignum*>(malloc_nid_postfix(sizeof(LibreSslBignum)));
    if (!number) return nullptr;
    *number = {nullptr, 0, 0, 0, BignumFlagMalloced};
    if (!length) return number;
    const std::size_t words = (length - 1) / 8 + 1;
    if (words <= maxWords) number->d = static_cast<std::uint64_t*>(malloc_nid_postfix(words * sizeof(std::uint64_t)));
    if (!number->d) {
        ClearFreeBignum(number);
        return nullptr;
    }
    number->dmax = static_cast<int>(words);
    for (std::size_t word = 0; word < words; ++word) {
        std::uint64_t value = 0;
        for (std::size_t byte = 0; byte < 8; ++byte) {
            const std::size_t position = word * 8 + byte;
            if (position < length) value |= std::uint64_t(bytes[length - 1 - position]) << (8 * byte);
        }
        number->d[word] = value;
    }
    number->top = static_cast<int>(words);
    while (number->top > 0 && !number->d[number->top - 1]) --number->top;
    return number;
}

struct Element {
    int tag;
    int tagClass;
    bool constructed;
    bool indefinite;
    const unsigned char* content;
    long length;
    const unsigned char* end;
};

struct Cursor {
    const unsigned char* data;
    long length;
};

[[noreturn]] void UnsupportedEncoding(const char* function, const char* what) {
    NotImplemented_nid_no_patch((std::string(function) + ": BER " + what).c_str());
}

bool ReadHeader(const unsigned char* start, long available, Element& element) {
    if (available <= 0) return false;
    const unsigned char* data = start;
    long remaining = available;
    const bool constructed = *data & 0x20;
    const int tagClass = *data & 0xC0;
    int tag = *data & 0x1F;
    if (tag == 0x1F) {
        ++data;
        if (--remaining == 0) return false;
        long value = 0;
        while (*data & 0x80) {
            value = value << 7 | (*data++ & 0x7F);
            if (--remaining == 0) return false;
            if (value > (INT_MAX >> 7)) return false;
        }
        value = value << 7 | (*data++ & 0x7F);
        tag = static_cast<int>(value);
        if (--remaining == 0) return false;
    } else {
        ++data;
        if (--remaining == 0) return false;
    }
    int lengthBytes = static_cast<int>(remaining);
    if (lengthBytes-- < 1) return false;
    bool indefinite = false;
    unsigned long length = 0;
    if (*data == 0x80) {
        indefinite = true;
        ++data;
    } else {
        unsigned count = *data & 0x7F;
        if (*data++ & 0x80) {
            if (lengthBytes < static_cast<int>(count)) return false;
            while (count && !*data) {
                ++data;
                --count;
            }
            if (count > sizeof(long)) return false;
            while (count-- > 0) length = length << 8 | *data++;
        } else {
            length = count;
        }
    }
    if (length > static_cast<unsigned long>(LONG_MAX)) return false;
    if (indefinite && !constructed) return false;
    const long header = data - start;
    if (static_cast<long>(length) > available - header) return false;
    element = {tag, tagClass, constructed, indefinite, data, static_cast<long>(length), data + length};
    return true;
}

bool ReadTop(const unsigned char* data, long length, int tag, Element& element) {
    return ReadHeader(data, length, element) && element.tag == tag && element.tagClass == 0;
}

enum class Field { Present, Absent, Failed };

Field ReadField(Cursor& cursor, int tag, Element& element) {
    if (!cursor.length) return Field::Absent;
    if (cursor.length >= 2 && !cursor.data[0] && !cursor.data[1]) return Field::Failed;
    if (!ReadHeader(cursor.data, cursor.length, element)) return Field::Failed;
    if (tag >= 0 && (element.tag != tag || element.tagClass != 0)) return Field::Failed;
    return Field::Present;
}

void Advance(Cursor& cursor, const Element& element) {
    cursor.length -= element.end - cursor.data;
    cursor.data = element.end;
}

bool ValidObject(const Element& element) {
    if (element.length <= 0 || element.length > INT_MAX || (element.content[element.length - 1] & 0x80)) return false;
    for (long index = 0; index < element.length; ++index)
        if (element.content[index] == 0x80 && (!index || !(element.content[index - 1] & 0x80))) return false;
    return true;
}

bool ValidAny(const Element& element, const char* function) {
    if (element.tagClass != 0) {
        if (element.indefinite) UnsupportedEncoding(function, "indefinite-length parameter");
        return true;
    }
    if (element.tag == 16 || element.tag == 17) {
        if (!element.constructed) return false;
        if (element.indefinite) UnsupportedEncoding(function, "indefinite-length parameter");
        return true;
    }
    if (element.constructed) UnsupportedEncoding(function, "constructed parameter");
    switch (element.tag) {
    case 1: return element.length == 1;
    case 3: return element.length >= 1;
    case 5: return element.length == 0;
    case 6: return ValidObject(element);
    case 28: return element.length % 4 == 0;
    case 30: return element.length % 2 == 0;
    default: return true;
    }
}

struct Algorithm {
    const unsigned char* object;
    long objectLength;
    bool parameter;
    bool nullParameter;
};

bool ReadSequence(Cursor& cursor, Element& element, const char* function) {
    if (ReadField(cursor, 16, element) != Field::Present || !element.constructed) return false;
    if (element.indefinite) UnsupportedEncoding(function, "indefinite-length SEQUENCE");
    return true;
}

bool ReadPrimitive(Cursor& cursor, int tag, Element& element, const char* function) {
    if (ReadField(cursor, tag, element) != Field::Present) return false;
    if (element.constructed) UnsupportedEncoding(function, "constructed string");
    Advance(cursor, element);
    return true;
}

bool ReadAlgorithm(Cursor& cursor, Algorithm& algorithm, const char* function) {
    Element sequence;
    if (!ReadSequence(cursor, sequence, function)) return false;
    Advance(cursor, sequence);
    Cursor fields {sequence.content, sequence.length};
    Element object;
    if (!ReadPrimitive(fields, 6, object, function) || !ValidObject(object)) return false;
    algorithm = {object.content, object.length, false, false};
    if (fields.length) {
        Element parameter;
        if (ReadField(fields, -1, parameter) != Field::Present || !ValidAny(parameter, function)) return false;
        Advance(fields, parameter);
        algorithm.parameter = true;
        algorithm.nullParameter = parameter.tagClass == 0 && parameter.tag == 5;
    }
    return !fields.length;
}

template<std::size_t Size> bool SameObject(const Algorithm& algorithm, const unsigned char (&object)[Size]) {
    return algorithm.objectLength == static_cast<long>(Size) && !std::memcmp(algorithm.object, object, Size);
}

constexpr unsigned char RsaEncryptionObject[] {0x2A, 0x86, 0x48, 0x86, 0xF7, 0x0D, 0x01, 0x01, 0x01};
constexpr unsigned char RsaObject[] {0x55, 0x08, 0x01, 0x01};

LibreSslRsa* DecodeRsaPublicKey(const unsigned char* data, long length, const char* function) {
    Element sequence;
    if (!ReadTop(data, length, 16, sequence) || !sequence.constructed) return nullptr;
    if (sequence.indefinite) UnsupportedEncoding(function, "indefinite-length SEQUENCE");
    Cursor fields {sequence.content, sequence.length};
    Element modulus;
    Element exponent;
    if (!ReadPrimitive(fields, 2, modulus, function) || !ReadPrimitive(fields, 2, exponent, function) || fields.length) return nullptr;
    auto* rsa = RSA_new_nid_postfix();
    if (!rsa) return nullptr;
    rsa->n = NewBignum(modulus.content, static_cast<std::size_t>(modulus.length));
    rsa->e = rsa->n ? NewBignum(exponent.content, static_cast<std::size_t>(exponent.length)) : nullptr;
    if (!rsa->e) {
        RSA_free_nid_postfix(rsa);
        return nullptr;
    }
    return rsa;
}

const LibreSslBignum& RequireNumber(const void* number, const char* function, const char* name) {
    if (!number) throw std::invalid_argument(std::string(function) + ": null " + name);
    const auto& value = *static_cast<const LibreSslBignum*>(number);
    if (value.top < 0 || value.top > INT_MAX / 64) throw std::invalid_argument(std::string(function) + ": " + name + " word count " + std::to_string(value.top));
    if (value.top && !value.d) throw std::invalid_argument(std::string(function) + ": null " + name + " words");
    if (value.neg) NotImplemented_nid_no_patch((std::string(function) + ": negative " + name).c_str());
    return value;
}

int Bits(const LibreSslBignum& value) {
    if (!value.top) return 0;
    return (value.top - 1) * 64 + static_cast<int>(std::bit_width(value.d[value.top - 1]));
}

int Compare(const std::uint64_t* left, int leftTop, const std::uint64_t* right, int rightTop) {
    if (leftTop != rightTop) return leftTop - rightTop;
    for (int index = leftTop - 1; index >= 0; --index)
        if (left[index] != right[index]) return left[index] > right[index] ? 1 : -1;
    return 0;
}

Words FromBytes(const unsigned char* bytes, std::size_t length) {
    Words words((length + 7) / 8);
    for (std::size_t position = 0; position < length; ++position) words[position / 8] |= std::uint64_t(bytes[length - 1 - position]) << (8 * (position % 8));
    while (!words.empty() && !words.back()) words.pop_back();
    return words;
}

std::vector<unsigned char> ToBytes(const Words& words) {
    std::size_t length = words.size() * 8;
    if (!words.empty()) length -= static_cast<std::size_t>(std::countl_zero(words.back()) / 8);
    std::vector<unsigned char> bytes(length);
    for (std::size_t position = 0; position < length; ++position) bytes[length - 1 - position] = static_cast<unsigned char>(words[position / 8] >> (8 * (position % 8)));
    return bytes;
}

bool ModExp(const Words& base, const LibreSslBignum& exponent, const LibreSslBignum& modulus, Words& result) {
    if (!modulus.top || !(modulus.d[0] & 1)) return false;
    if (!Bits(exponent)) {
        result = modulus.top == 1 && modulus.d[0] == 1 ? Words {} : Words {1};
        return true;
    }
    if (Bits(modulus) > MBEDTLS_MPI_MAX_BITS) NotImplemented_nid_no_patch("BN_mod_exp_mont_ct: a modulus over 8192 bits");
    mbedtls_mpi value, power, mod, output;
    for (auto* number : {&value, &power, &mod, &output}) mbedtls_mpi_init(number);
    int status = mbedtls_mpi_read_binary_le(&value, reinterpret_cast<const unsigned char*>(base.data()), base.size() * 8);
    if (!status) status = mbedtls_mpi_read_binary_le(&power, reinterpret_cast<const unsigned char*>(exponent.d), static_cast<std::size_t>(exponent.top) * 8);
    if (!status) status = mbedtls_mpi_read_binary_le(&mod, reinterpret_cast<const unsigned char*>(modulus.d), static_cast<std::size_t>(modulus.top) * 8);
    if (!status) status = mbedtls_mpi_exp_mod(&output, &value, &power, &mod, nullptr);
    if (!status) {
        result.assign((mbedtls_mpi_size(&output) + 7) / 8, 0);
        status = mbedtls_mpi_write_binary_le(&output, reinterpret_cast<unsigned char*>(result.data()), result.size() * 8);
        while (!result.empty() && !result.back()) result.pop_back();
    }
    for (auto* number : {&value, &power, &mod, &output}) mbedtls_mpi_free(number);
    return !status;
}

void RequireBuffer(const void* buffer, long length, const char* function, const char* name) {
    if (length > 0 && !buffer) throw std::invalid_argument(std::string(function) + ": null " + name);
}

bool Mgf1Sha1(unsigned char* mask, long length, const unsigned char* seed, long seedLength) {
    mbedtls_sha1_context context;
    mbedtls_sha1_init(&context);
    bool done = true;
    for (std::uint32_t counter = 0; done && length > 0; ++counter) {
        const unsigned char count[4] {static_cast<unsigned char>(counter >> 24), static_cast<unsigned char>(counter >> 16), static_cast<unsigned char>(counter >> 8), static_cast<unsigned char>(counter)};
        unsigned char digest[Sha1Bytes];
        done = !mbedtls_sha1_starts(&context) && !mbedtls_sha1_update(&context, seed, static_cast<std::size_t>(seedLength)) &&
            !mbedtls_sha1_update(&context, count, sizeof(count)) && !mbedtls_sha1_finish(&context, digest);
        const long part = length < Sha1Bytes ? length : Sha1Bytes;
        std::memcpy(mask, digest, static_cast<std::size_t>(part));
        mask += part;
        length -= part;
    }
    mbedtls_sha1_free(&context);
    return done;
}

int AddPkcs1Type2(unsigned char* to, int size, const unsigned char* from, int length, const char* function) {
    if (length > size - 11) return 0;
    if (length < 0) throw std::invalid_argument(std::string(function) + ": negative input length");
    RequireBuffer(from, length, function, "input");
    to[0] = 0;
    to[1] = 2;
    unsigned char* padding = to + 2;
    const int random = size - 3 - length;
    HostEntropy_nid_no_patch(padding, static_cast<std::size_t>(random));
    for (int index = 0; index < random; ++index, ++padding)
        while (!*padding) HostEntropy_nid_no_patch(padding, 1);
    *padding++ = 0;
    if (length) std::memcpy(padding, from, static_cast<std::size_t>(length));
    return 1;
}

int AddOaep(unsigned char* to, int size, const unsigned char* from, int length, const char* function) {
    const int encoded = size - 1;
    if (length > encoded - 2 * Sha1Bytes - 1) return 0;
    if (encoded < 2 * Sha1Bytes + 1) return 0;
    if (length < 0) throw std::invalid_argument(std::string(function) + ": negative input length");
    RequireBuffer(from, length, function, "input");
    to[0] = 0;
    unsigned char* seed = to + 1;
    unsigned char* block = to + Sha1Bytes + 1;
    static constexpr unsigned char empty[1] {};
    if (mbedtls_sha1(empty, 0, block)) return 0;
    std::memset(block + Sha1Bytes, 0, static_cast<std::size_t>(encoded - length - 2 * Sha1Bytes - 1));
    block[encoded - length - Sha1Bytes - 1] = 0x01;
    if (length) std::memcpy(block + encoded - length - Sha1Bytes, from, static_cast<std::size_t>(length));
    HostEntropy_nid_no_patch(seed, Sha1Bytes);
    std::vector<unsigned char> blockMask(static_cast<std::size_t>(encoded - Sha1Bytes));
    if (!Mgf1Sha1(blockMask.data(), encoded - Sha1Bytes, seed, Sha1Bytes)) return 0;
    for (int index = 0; index < encoded - Sha1Bytes; ++index) block[index] ^= blockMask[static_cast<std::size_t>(index)];
    unsigned char seedMask[Sha1Bytes];
    if (!Mgf1Sha1(seedMask, Sha1Bytes, block, encoded - Sha1Bytes)) return 0;
    for (int index = 0; index < Sha1Bytes; ++index) seed[index] ^= seedMask[index];
    return 1;
}

int AddNone(unsigned char* to, int size, const unsigned char* from, int length, const char* function) {
    if (length > size || length < size) return 0;
    RequireBuffer(from, length, function, "input");
    std::memcpy(to, from, static_cast<std::size_t>(length));
    return 1;
}

int CheckPkcs1Type1(unsigned char* to, int size, const unsigned char* from, int length, int modulusLength, const char* function) {
    if (modulusLength != length + 1 || !length || from[0] != 0x01) return -1;
    const unsigned char* data = from + 1;
    int remaining = length - 1;
    int index = 0;
    for (; index < remaining; ++index, ++data) {
        if (*data == 0xFF) continue;
        if (*data) return -1;
        ++data;
        break;
    }
    if (index == remaining || index < 8) return -1;
    remaining -= index + 1;
    if (remaining > size) return -1;
    RequireBuffer(to, remaining, function, "output");
    if (remaining) std::memcpy(to, data, static_cast<std::size_t>(remaining));
    return remaining;
}

int CheckNone(unsigned char* to, int size, const unsigned char* from, int length, const char* function) {
    if (length > size) return -1;
    RequireBuffer(to, size, function, "output");
    std::memset(to, 0, static_cast<std::size_t>(size - length));
    if (length) std::memcpy(to + size - length, from, static_cast<std::size_t>(length));
    return size;
}

int APS5_VABI DefaultBignumModExp(void*, const void*, const void*, const void*, void*, void*);

bool UsablePublicKey(const LibreSslRsa* rsa, const char* function, const LibreSslBignum*& modulus, const LibreSslBignum*& exponent) {
    if (!rsa) throw std::invalid_argument(std::string(function) + ": null key");
    modulus = &RequireNumber(rsa->n, function, "modulus");
    const int bits = Bits(*modulus);
    if (bits > MaxModulusBits) return false;
    exponent = &RequireNumber(rsa->e, function, "exponent");
    if (Compare(modulus->d, modulus->top, exponent->d, exponent->top) <= 0) return false;
    return bits <= SmallModulusBits || Bits(*exponent) <= MaxPublicExponentBits;
}

bool PublicOperation(const LibreSslRsa& rsa, const Words& value, const LibreSslBignum& exponent, const LibreSslBignum& modulus, Words& result, const char* function) {
    if (!rsa.meth) throw std::invalid_argument(std::string(function) + ": null method");
    if (rsa.meth->operations[OperationBignumModExp] != reinterpret_cast<void*>(DefaultBignumModExp))
        NotImplemented_nid_no_patch((std::string(function) + ": a method with its own bn_mod_exp").c_str());
    return ModExp(value, exponent, modulus, result);
}

int APS5_VABI DefaultPublicEncrypt(int length, const unsigned char* from, unsigned char* to, LibreSslRsa* rsa, int padding) {
    constexpr const char* function = "RSA_eay_public_encrypt";
    const LibreSslBignum* modulus;
    const LibreSslBignum* exponent;
    if (!UsablePublicKey(rsa, function, modulus, exponent)) return -1;
    const int size = (Bits(*modulus) + 7) / 8;
    std::vector<unsigned char> block(static_cast<std::size_t>(size));
    int padded = 0;
    switch (padding) {
    case PaddingPkcs1:
        padded = AddPkcs1Type2(block.data(), size, from, length, function);
        break;
    case PaddingOaep:
        padded = AddOaep(block.data(), size, from, length, function);
        break;
    case PaddingSslv23:
        NotImplemented_nid_no_patch("RSA_padding_add_SSLv23");
    case PaddingNone:
        padded = AddNone(block.data(), size, from, length, function);
        break;
    default:
        return -1;
    }
    if (padded <= 0) return -1;
    const auto value = FromBytes(block.data(), block.size());
    if (Compare(value.data(), static_cast<int>(value.size()), modulus->d, modulus->top) >= 0) return -1;
    Words result;
    if (!PublicOperation(*rsa, value, *exponent, *modulus, result, function)) return -1;
    const auto bytes = ToBytes(result);
    RequireBuffer(to, size, function, "output");
    std::memset(to, 0, static_cast<std::size_t>(size) - bytes.size());
    std::memcpy(to + size - bytes.size(), bytes.data(), bytes.size());
    return size;
}

int APS5_VABI DefaultPublicDecrypt(int length, const unsigned char* from, unsigned char* to, LibreSslRsa* rsa, int padding) {
    constexpr const char* function = "RSA_eay_public_decrypt";
    const LibreSslBignum* modulus;
    const LibreSslBignum* exponent;
    if (!UsablePublicKey(rsa, function, modulus, exponent)) return -1;
    const int size = (Bits(*modulus) + 7) / 8;
    if (length > size || length < 0) return -1;
    RequireBuffer(from, length, function, "input");
    const auto value = FromBytes(from, static_cast<std::size_t>(length));
    if (Compare(value.data(), static_cast<int>(value.size()), modulus->d, modulus->top) >= 0) return -1;
    Words result;
    if (!PublicOperation(*rsa, value, *exponent, *modulus, result, function)) return -1;
    if (padding == PaddingX931) NotImplemented_nid_no_patch("RSA_padding_check_X931");
    const auto bytes = ToBytes(result);
    switch (padding) {
    case PaddingPkcs1:
        return CheckPkcs1Type1(to, size, bytes.data(), static_cast<int>(bytes.size()), size, function);
    case PaddingNone:
        return CheckNone(to, size, bytes.data(), static_cast<int>(bytes.size()), function);
    default:
        return -1;
    }
}

struct DigestObject {
    int type;
    long length;
    unsigned char object[9];
};

constexpr DigestObject DigestObjects[] {
    {3, 8, {0x2A, 0x86, 0x48, 0x86, 0xF7, 0x0D, 0x02, 0x02}},
    {4, 8, {0x2A, 0x86, 0x48, 0x86, 0xF7, 0x0D, 0x02, 0x05}},
    {41, 5, {0x2B, 0x0E, 0x03, 0x02, 0x12}},
    {64, 5, {0x2B, 0x0E, 0x03, 0x02, 0x1A}},
    {95, 4, {0x55, 0x08, 0x03, 0x65}},
    {117, 5, {0x2B, 0x24, 0x03, 0x02, 0x01}},
    {257, 8, {0x2A, 0x86, 0x48, 0x86, 0xF7, 0x0D, 0x02, 0x04}},
    {672, 9, {0x60, 0x86, 0x48, 0x01, 0x65, 0x03, 0x04, 0x02, 0x01}},
    {673, 9, {0x60, 0x86, 0x48, 0x01, 0x65, 0x03, 0x04, 0x02, 0x02}},
    {674, 9, {0x60, 0x86, 0x48, 0x01, 0x65, 0x03, 0x04, 0x02, 0x03}},
    {675, 9, {0x60, 0x86, 0x48, 0x01, 0x65, 0x03, 0x04, 0x02, 0x04}},
    {804, 6, {0x28, 0xCF, 0x06, 0x03, 0x00, 0x37}},
    {809, 6, {0x2A, 0x85, 0x03, 0x02, 0x02, 0x09}},
    {941, 8, {0x2A, 0x85, 0x03, 0x07, 0x01, 0x01, 0x02, 0x02}},
    {942, 8, {0x2A, 0x85, 0x03, 0x07, 0x01, 0x01, 0x02, 0x03}},
};

bool SameDigestType(const Algorithm& algorithm, int type, const char* function) {
    if (type < 0 || type >= ObjectTypeCount) return false;
    for (const auto& digest : DigestObjects) {
        const bool same = algorithm.objectLength == digest.length && !std::memcmp(algorithm.object, digest.object, static_cast<std::size_t>(digest.length));
        if (digest.type == type) return same;
        if (same) return false;
    }
    NotImplemented_nid_no_patch((std::string(function) + ": digest type " + std::to_string(type) + " with an object outside the digest table").c_str());
}

int VerifyDigestInfo(int type, const unsigned char* digest, unsigned int digestLength, const unsigned char* data, long length, const char* function) {
    Element sequence;
    if (!ReadTop(data, length, 16, sequence) || !sequence.constructed) return 0;
    if (sequence.indefinite) UnsupportedEncoding(function, "indefinite-length SEQUENCE");
    Cursor fields {sequence.content, sequence.length};
    Algorithm algorithm;
    Element hash;
    if (!ReadAlgorithm(fields, algorithm, function) || !ReadPrimitive(fields, 4, hash, function) || fields.length) return 0;
    if (sequence.end != data + length) return 0;
    if (algorithm.parameter && !algorithm.nullParameter) return 0;
    if (!SameDigestType(algorithm, type, function)) return 0;
    if (static_cast<unsigned long>(hash.length) != digestLength) return 0;
    RequireBuffer(digest, digestLength, function, "digest");
    return !digestLength || !std::memcmp(digest, hash.content, digestLength);
}

int APS5_VABI DefaultPrivateEncrypt(int, const unsigned char*, unsigned char*, LibreSslRsa*, int) {
    NotImplemented_nid_no_patch("RSA_eay_private_encrypt");
}

int APS5_VABI DefaultPrivateDecrypt(int, const unsigned char*, unsigned char*, LibreSslRsa*, int) {
    NotImplemented_nid_no_patch("RSA_eay_private_decrypt");
}

int APS5_VABI DefaultModExp(void*, const void*, LibreSslRsa*, void*) {
    NotImplemented_nid_no_patch("RSA_eay_mod_exp");
}

int APS5_VABI DefaultBignumModExp(void*, const void*, const void*, const void*, void*, void*) {
    NotImplemented_nid_no_patch("BN_mod_exp_mont_ct");
}

int APS5_VABI DefaultInit(LibreSslRsa* rsa) {
    rsa->flags |= RsaFlagCachePublic | RsaFlagCachePrivate;
    return 1;
}

int APS5_VABI DefaultFinish(LibreSslRsa* rsa) {
    if (rsa->methodModN || rsa->methodModP || rsa->methodModQ) NotImplemented_nid_no_patch("RSA_eay_finish: freeing a Montgomery context");
    return 1;
}

const LibreSslRsaMethod DefaultMethod {
    "Eric Young's PKCS#1 RSA",
    {
        reinterpret_cast<void*>(DefaultPublicEncrypt),
        reinterpret_cast<void*>(DefaultPublicDecrypt),
        reinterpret_cast<void*>(DefaultPrivateEncrypt),
        reinterpret_cast<void*>(DefaultPrivateDecrypt),
        reinterpret_cast<void*>(DefaultModExp),
        reinterpret_cast<void*>(DefaultBignumModExp),
        reinterpret_cast<void*>(DefaultInit),
        reinterpret_cast<void*>(DefaultFinish),
    },
    0,
    nullptr,
    nullptr,
    nullptr,
    nullptr,
};

int ByteCount(const LibreSslBignum* number, const char* function) {
    if (!number) throw std::invalid_argument(std::string(function) + ": null modulus");
    if (number->top == 0) return 0;
    if (number->top < 0) throw std::invalid_argument(std::string(function) + ": negative modulus word count");
    if (!number->d) throw std::invalid_argument(std::string(function) + ": null modulus words");
    const std::int64_t bits = static_cast<std::int64_t>(number->top - 1) * 64 + std::bit_width(number->d[number->top - 1]);
    if (bits > INT_MAX - 7) throw std::invalid_argument(std::string(function) + ": modulus bit count overflows int");
    return static_cast<int>((bits + 7) / 8);
}

}

extern "C" {

int APS5_VABI RSA_flags_nid_postfix(const LibreSslRsa* rsa) {
    if (!rsa) return 0;
    if (!rsa->meth) throw std::invalid_argument(std::string(__func__) + ": null method");
    return rsa->meth->flags;
}

int APS5_VABI RSA_size_nid_postfix(const LibreSslRsa* rsa) {
    if (!rsa) throw std::invalid_argument(std::string(__func__) + ": null key");
    return ByteCount(static_cast<const LibreSslBignum*>(rsa->n), __func__);
}

LibreSslRsa* APS5_VABI RSA_new_nid_postfix(void) {
    auto* rsa = static_cast<LibreSslRsa*>(malloc_nid_postfix(sizeof(LibreSslRsa)));
    if (!rsa) return nullptr;
    rsa->meth = &DefaultMethod;
    rsa->engine = nullptr;
    rsa->pad = 0;
    rsa->version = 0;
    rsa->n = rsa->e = rsa->d = rsa->p = rsa->q = rsa->dmp1 = rsa->dmq1 = rsa->iqmp = nullptr;
    rsa->references = 1;
    rsa->methodModN = rsa->methodModP = rsa->methodModQ = nullptr;
    rsa->blinding = rsa->mtBlinding = nullptr;
    rsa->flags = rsa->meth->flags & ~RsaFlagNonFipsAllow;
    rsa->exData = nullptr;
    const auto init = Operation<RsaCallback>(*rsa->meth, OperationInit);
    if (init && !init(rsa)) {
        free_nid_postfix(rsa);
        return nullptr;
    }
    return rsa;
}

void APS5_VABI RSA_free_nid_postfix(LibreSslRsa* rsa) {
    if (!rsa) return;
    if (std::atomic_ref<int>(rsa->references).fetch_sub(1) - 1 > 0) return;
    if (!rsa->meth) throw std::invalid_argument(std::string(__func__) + ": null method");
    if (rsa->engine) NotImplemented_nid_no_patch("RSA_free: releasing an ENGINE");
    if (rsa->exData) NotImplemented_nid_no_patch("RSA_free: freeing ex_data");
    if (rsa->blinding || rsa->mtBlinding) NotImplemented_nid_no_patch("RSA_free: freeing blinding");
    if (const auto finish = Operation<RsaCallback>(*rsa->meth, OperationFinish)) finish(rsa);
    for (void* number : {rsa->n, rsa->e, rsa->d, rsa->p, rsa->q, rsa->dmp1, rsa->dmq1, rsa->iqmp})
        ClearFreeBignum(static_cast<LibreSslBignum*>(number));
    free_nid_postfix(rsa);
}

int APS5_VABI RSA_public_encrypt_nid_postfix(int length, const unsigned char* from, unsigned char* to, LibreSslRsa* rsa, int padding) {
    if (!rsa || !rsa->meth) throw std::invalid_argument(std::string(__func__) + ": null key or method");
    const auto encrypt = Operation<RsaCrypt>(*rsa->meth, OperationPublicEncrypt);
    if (!encrypt) throw std::invalid_argument(std::string(__func__) + ": method without rsa_pub_enc");
    return encrypt(length, from, to, rsa, padding);
}

int APS5_VABI RSA_public_decrypt_nid_postfix(int length, const unsigned char* from, unsigned char* to, LibreSslRsa* rsa, int padding) {
    if (!rsa || !rsa->meth) throw std::invalid_argument(std::string(__func__) + ": null key or method");
    const auto decrypt = Operation<RsaCrypt>(*rsa->meth, OperationPublicDecrypt);
    if (!decrypt) throw std::invalid_argument(std::string(__func__) + ": method without rsa_pub_dec");
    return decrypt(length, from, to, rsa, padding);
}

int APS5_VABI RSA_verify_nid_postfix(int type, const unsigned char* digest, unsigned int digestLength, const unsigned char* signature, unsigned int signatureLength, LibreSslRsa* rsa) {
    if (!rsa) throw std::invalid_argument(std::string(__func__) + ": null key");
    if (rsa->flags & RsaFlagSignVerify) {
        if (!rsa->meth) throw std::invalid_argument(std::string(__func__) + ": null method");
        if (rsa->meth->verify) return reinterpret_cast<RsaVerify>(rsa->meth->verify)(type, digest, digestLength, signature, signatureLength, rsa);
    }
    if (signatureLength != static_cast<unsigned int>(RSA_size_nid_postfix(rsa))) return 0;
    if (type == DigestMd5Sha1 && digestLength != Md5Sha1Bytes) return 0;
    std::vector<unsigned char> block(signatureLength);
    const int length = RSA_public_decrypt_nid_postfix(static_cast<int>(signatureLength), signature, block.data(), rsa, PaddingPkcs1);
    if (length <= 0) return 0;
    if (type != DigestMd5Sha1) return VerifyDigestInfo(type, digest, digestLength, block.data(), length, __func__);
    if (length != static_cast<int>(Md5Sha1Bytes)) return 0;
    RequireBuffer(digest, Md5Sha1Bytes, __func__, "digest");
    return !std::memcmp(block.data(), digest, Md5Sha1Bytes);
}

LibreSslRsa* APS5_VABI d2i_RSA_PUBKEY_nid_postfix(LibreSslRsa** out, const unsigned char** input, long length) {
    if (!input) throw std::invalid_argument(std::string(__func__) + ": null input pointer");
    const unsigned char* data = *input;
    if (length <= 0) return nullptr;
    if (!data) throw std::invalid_argument(std::string(__func__) + ": null input");
    Element publicKey;
    if (!ReadTop(data, length, 16, publicKey) || !publicKey.constructed) return nullptr;
    if (publicKey.indefinite) UnsupportedEncoding(__func__, "indefinite-length SEQUENCE");
    Cursor fields {publicKey.content, publicKey.length};
    Algorithm algorithm;
    Element bits;
    if (!ReadAlgorithm(fields, algorithm, __func__) || !ReadPrimitive(fields, 3, bits, __func__) || bits.length < 1 || fields.length) return nullptr;
    if (!SameObject(algorithm, RsaEncryptionObject) && !SameObject(algorithm, RsaObject)) return nullptr;
    const unsigned unused = bits.content[0];
    std::vector<unsigned char> key(bits.content + 1, bits.content + bits.length);
    if (!key.empty()) {
        if (unused >= 32) UnsupportedEncoding(__func__, "BIT STRING with 32 or more unused bits");
        key.back() &= static_cast<unsigned char>(0xFF << unused);
    }
    auto* rsa = DecodeRsaPublicKey(key.data(), static_cast<long>(key.size()), __func__);
    if (!rsa) return nullptr;
    *input = publicKey.end;
    if (out) {
        RSA_free_nid_postfix(*out);
        *out = rsa;
    }
    return rsa;
}

}
