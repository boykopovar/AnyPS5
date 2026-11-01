#include "prx/libSceLibreSsl/include/Api.hpp"
#include "prx/libSceLibreSsl/include/ObjectTable.hpp"
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
using RsaSign = int (APS5_VABI*)(int type, const unsigned char* digest, unsigned int digestLength, unsigned char* signature, unsigned int* signatureLength, const LibreSslRsa* rsa);
using Words = std::vector<std::uint64_t>;

constexpr int OperationPublicEncrypt = 0;
constexpr int OperationPublicDecrypt = 1;
constexpr int OperationPrivateEncrypt = 2;
constexpr int OperationPrivateDecrypt = 3;
constexpr int OperationModExp = 4;
constexpr int OperationBignumModExp = 5;
constexpr int OperationInit = 6;
constexpr int OperationFinish = 7;
constexpr int RsaFlagCachePublic = 0x0002;
constexpr int RsaFlagCachePrivate = 0x0004;
constexpr int RsaFlagExternalPrivateKey = 0x0020;
constexpr int RsaFlagSignVerify = 0x0040;
constexpr int RsaFlagNoBlinding = 0x0080;
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
constexpr int BlindingDraws = 33;

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

void RejectAddedObject(const Algorithm& algorithm, const char* function) {
    const LibreSslAsn1Object object {nullptr, nullptr, 0, static_cast<int>(algorithm.objectLength), algorithm.object, 0};
    if (LibreSslAddedObjectNid_nid_no_patch(&object)) NotImplemented_nid_no_patch((std::string(function) + ": an object registered by OBJ_create").c_str());
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

int APS5_VABI DefaultModExp(void*, const void*, LibreSslRsa*, void*);
int APS5_VABI DefaultBignumModExp(void*, const void*, const void*, const void*, void*, void*);
int ByteCount(const LibreSslBignum* number, const char* function);

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
    RejectAddedObject(algorithm, function);
    if (type < 0 || type >= ObjectTypeCount) return false;
    for (const auto& digest : DigestObjects) {
        const bool same = algorithm.objectLength == digest.length && !std::memcmp(algorithm.object, digest.object, static_cast<std::size_t>(digest.length));
        if (digest.type == type) return same;
        if (same) return false;
    }
    NotImplemented_nid_no_patch((std::string(function) + ": digest type " + std::to_string(type) + " with an object outside the digest table").c_str());
}

const DigestObject& DigestObjectFor(int type, const char* function) {
    for (const auto& digest : DigestObjects)
        if (digest.type == type) return digest;
    NotImplemented_nid_no_patch((std::string(function) + ": digest type " + std::to_string(type) + " outside the digest table").c_str());
}

std::uint64_t DerLengthBytes(std::uint64_t length) {
    std::uint64_t bytes = 1;
    if (length > 127)
        for (; length; length >>= 8) ++bytes;
    return bytes;
}

void AppendDerHeader(std::vector<unsigned char>& output, unsigned char tag, std::uint64_t length) {
    output.push_back(tag);
    if (length <= 127) {
        output.push_back(static_cast<unsigned char>(length));
        return;
    }
    const auto count = DerLengthBytes(length) - 1;
    output.push_back(static_cast<unsigned char>(0x80 | count));
    for (auto index = count; index-- > 0;) output.push_back(static_cast<unsigned char>(length >> (8 * index)));
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

const LibreSslBignum& RequireNormal(const void* number, const char* function, const char* name) {
    const auto& value = RequireNumber(number, function, name);
    if (value.top && !value.d[value.top - 1]) NotImplemented_nid_no_patch((std::string(function) + ": a non-normalised " + name).c_str());
    return value;
}

bool CheckedModExp(const Words& base, const LibreSslBignum& exponent, const LibreSslBignum& modulus, Words& result) {
    if (modulus.top && (modulus.d[0] & 1) && Bits(modulus) <= MBEDTLS_MPI_MAX_BITS && Bits(exponent) > MBEDTLS_MPI_MAX_BITS)
        NotImplemented_nid_no_patch("BN_mod_exp_mont_ct: an exponent over 8192 bits");
    return ModExp(base, exponent, modulus, result);
}

bool MethodModExp(const LibreSslRsa& rsa, const Words& value, const LibreSslBignum& exponent, const LibreSslBignum& modulus, const void* montgomery, Words& result, const char* function) {
    if (!rsa.meth) throw std::invalid_argument(std::string(function) + ": null method");
    if (rsa.meth->operations[OperationBignumModExp] != reinterpret_cast<void*>(DefaultBignumModExp))
        NotImplemented_nid_no_patch((std::string(function) + ": a method with its own bn_mod_exp").c_str());
    if (montgomery) NotImplemented_nid_no_patch((std::string(function) + ": a Montgomery context set by the caller").c_str());
    return CheckedModExp(value, exponent, modulus, result);
}

Words Value(const LibreSslBignum& number) {
    return Words(number.d, number.d + number.top);
}

void CheckSize(int status, const char* function) {
    if (status) NotImplemented_nid_no_patch((std::string(function) + ": a number over the mbedtls size limits").c_str());
}

struct Mpi {
    mbedtls_mpi value;

    Mpi() {
        mbedtls_mpi_init(&value);
    }

    Mpi(const std::uint64_t* words, std::size_t count, const char* function) : Mpi() {
        CheckSize(mbedtls_mpi_read_binary_le(&value, reinterpret_cast<const unsigned char*>(words), count * 8), function);
    }

    Mpi(const Words& words, const char* function) : Mpi(words.data(), words.size(), function) {}

    Mpi(const LibreSslBignum& number, const char* function) : Mpi(number.d, static_cast<std::size_t>(number.top), function) {}

    Mpi(const Mpi&) = delete;
    Mpi& operator=(const Mpi&) = delete;

    ~Mpi() {
        mbedtls_mpi_free(&value);
    }

    Words Get(const char* function) const {
        Words words((mbedtls_mpi_size(&value) + 7) / 8);
        CheckSize(mbedtls_mpi_write_binary_le(&value, reinterpret_cast<unsigned char*>(words.data()), words.size() * 8), function);
        while (!words.empty() && !words.back()) words.pop_back();
        return words;
    }
};

Words Remainder(const Words& value, const LibreSslBignum& modulus, const char* function) {
    Mpi number(value, function), divisor(modulus, function), result;
    CheckSize(mbedtls_mpi_mod_mpi(&result.value, &number.value, &divisor.value), function);
    return result.Get(function);
}

Words MultiplyRemainder(const Words& left, const Words& right, const LibreSslBignum& modulus, const char* function) {
    Mpi first(left, function), second(right, function), divisor(modulus, function), product, result;
    CheckSize(mbedtls_mpi_mul_mpi(&product.value, &first.value, &second.value), function);
    CheckSize(mbedtls_mpi_mod_mpi(&result.value, &product.value, &divisor.value), function);
    return result.Get(function);
}

Words Difference(const Words& larger, const Words& smaller, const char* function) {
    Mpi first(larger, function), second(smaller, function), result;
    CheckSize(mbedtls_mpi_sub_abs(&result.value, &first.value, &second.value), function);
    return result.Get(function);
}

bool Inverse(const Mpi& value, const Mpi& modulus, Words& result, const char* function) {
    Mpi inverse;
    const int status = mbedtls_mpi_inv_mod(&inverse.value, &value.value, &modulus.value);
    if (status == MBEDTLS_ERR_MPI_NOT_ACCEPTABLE) return false;
    CheckSize(status, function);
    result = inverse.Get(function);
    return true;
}

bool MontgomeryContext(const void* cached, const LibreSslBignum& modulus, const char* function) {
    if (cached) NotImplemented_nid_no_patch((std::string(function) + ": a Montgomery context set by the caller").c_str());
    if (!modulus.top) NotImplemented_nid_no_patch((std::string(function) + ": a Montgomery context for a zero modulus").c_str());
    return modulus.d[0] & 1;
}

bool PublicExponent(const LibreSslRsa& rsa, Words& exponent, const char* function) {
    if (!rsa.d || !rsa.p || !rsa.q) return false;
    const auto& privateExponent = RequireNormal(rsa.d, function, "private exponent");
    const auto& p = RequireNormal(rsa.p, function, "p");
    const auto& q = RequireNormal(rsa.q, function, "q");
    Mpi first(p, function), second(q, function), order, number(privateExponent, function);
    CheckSize(p.top ? mbedtls_mpi_sub_int(&first.value, &first.value, 1) : mbedtls_mpi_lset(&first.value, 1), function);
    CheckSize(q.top ? mbedtls_mpi_sub_int(&second.value, &second.value, 1) : mbedtls_mpi_lset(&second.value, 1), function);
    CheckSize(mbedtls_mpi_mul_mpi(&order.value, &first.value, &second.value), function);
    if (!mbedtls_mpi_cmp_int(&order.value, 0)) return false;
    if (!mbedtls_mpi_cmp_int(&order.value, 1)) {
        exponent.clear();
        return true;
    }
    return Inverse(number, order, exponent, function);
}

Words RandomBelow(const LibreSslBignum& modulus) {
    const int bits = Bits(modulus);
    std::vector<unsigned char> bytes(static_cast<std::size_t>((bits + 7) / 8));
    for (;;) {
        HostEntropy_nid_no_patch(bytes.data(), bytes.size());
        if (bits % 8) bytes[0] &= static_cast<unsigned char>((1u << (bits % 8)) - 1);
        auto value = FromBytes(bytes.data(), bytes.size());
        if (Compare(value.data(), static_cast<int>(value.size()), modulus.d, modulus.top) < 0) return value;
    }
}

struct Blinding {
    Words factor;
    Words unblind;
    bool known;
};

bool SetupBlinding(const LibreSslRsa& rsa, const LibreSslBignum& modulus, Blinding& blinding, const char* function) {
    if (rsa.blinding) NotImplemented_nid_no_patch((std::string(function) + ": a blinding set by the caller").c_str());
    Words derived;
    LibreSslBignum derivedNumber {};
    const LibreSslBignum* exponent = &derivedNumber;
    if (rsa.e) {
        exponent = &RequireNormal(rsa.e, function, "exponent");
    } else {
        if (!PublicExponent(rsa, derived, function)) return false;
        derivedNumber = {derived.data(), static_cast<int>(derived.size()), static_cast<int>(derived.size()), 0, 0};
    }
    if (!rsa.meth) throw std::invalid_argument(std::string(function) + ": null method");
    if (modulus.top == 1 && modulus.d[0] == 1) NotImplemented_nid_no_patch((std::string(function) + ": blinding with a modulus of 1").c_str());
    const Mpi divisor(modulus, function);
    Words random;
    bool inverted = false;
    for (int draw = 0; draw < BlindingDraws && !inverted; ++draw) {
        random = RandomBelow(modulus);
        inverted = Inverse(Mpi(random, function), divisor, blinding.unblind, function);
    }
    if (!inverted) return false;
    if (rsa.methodModN) NotImplemented_nid_no_patch((std::string(function) + ": a Montgomery context set by the caller").c_str());
    blinding.known = modulus.d[0] & 1;
    return !blinding.known || CheckedModExp(random, *exponent, modulus, blinding.factor);
}

Words CombineCrt(const Words& high, const Words& low, const LibreSslBignum& coefficient, const LibreSslBignum& p, const LibreSslBignum& q, const char* function) {
    Mpi first(high, function), second(low, function), factor(coefficient, function), primeP(p, function), primeQ(q, function), work, result;
    CheckSize(mbedtls_mpi_sub_mpi(&work.value, &first.value, &second.value), function);
    CheckSize(mbedtls_mpi_mul_mpi(&result.value, &work.value, &factor.value), function);
    CheckSize(mbedtls_mpi_mod_mpi(&work.value, &result.value, &primeP.value), function);
    CheckSize(mbedtls_mpi_mul_mpi(&result.value, &work.value, &primeQ.value), function);
    CheckSize(mbedtls_mpi_add_mpi(&work.value, &result.value, &second.value), function);
    return work.Get(function);
}

bool CrtModExp(const LibreSslRsa& rsa, const LibreSslBignum& modulus, const Words& input, Words& result, const char* function) {
    const auto& p = RequireNormal(rsa.p, function, "p");
    const auto& q = RequireNormal(rsa.q, function, "q");
    if (rsa.flags & RsaFlagCachePrivate) {
        if (!MontgomeryContext(rsa.methodModP, p, function)) return false;
        if (!MontgomeryContext(rsa.methodModQ, q, function)) return false;
    }
    if ((rsa.flags & RsaFlagCachePublic) && !MontgomeryContext(rsa.methodModN, modulus, function)) return false;
    if (!q.top) return false;
    const auto& dmq1 = RequireNormal(rsa.dmq1, function, "dmq1");
    Words low;
    if (!MethodModExp(rsa, Remainder(input, q, function), dmq1, q, rsa.methodModQ, low, function)) return false;
    if (!p.top) return false;
    const auto& dmp1 = RequireNormal(rsa.dmp1, function, "dmp1");
    Words high;
    if (!MethodModExp(rsa, Remainder(input, p, function), dmp1, p, rsa.methodModP, high, function)) return false;
    result = CombineCrt(high, low, RequireNormal(rsa.iqmp, function, "iqmp"), p, q, function);
    if (!rsa.e) return true;
    Words check;
    if (!MethodModExp(rsa, result, RequireNormal(rsa.e, function, "exponent"), modulus, rsa.methodModN, check, function)) return false;
    if (check == input) return true;
    return MethodModExp(rsa, input, RequireNormal(rsa.d, function, "private exponent"), modulus, rsa.methodModN, result, function);
}

bool PrivateOperation(const LibreSslRsa& rsa, const LibreSslBignum& modulus, const Words& input, Words& result, const char* function) {
    const bool blind = !(rsa.flags & RsaFlagNoBlinding);
    Blinding blinding {};
    Words value = input;
    if (blind) {
        if (!SetupBlinding(rsa, modulus, blinding, function)) return false;
        if (blinding.known) value = MultiplyRemainder(value, blinding.factor, modulus, function);
    }
    if ((rsa.flags & RsaFlagExternalPrivateKey) || (rsa.p && rsa.q && rsa.dmp1 && rsa.dmq1 && rsa.iqmp)) {
        if (!rsa.meth) throw std::invalid_argument(std::string(function) + ": null method");
        if (rsa.meth->operations[OperationModExp] != reinterpret_cast<void*>(DefaultModExp))
            NotImplemented_nid_no_patch((std::string(function) + ": a method with its own rsa_mod_exp").c_str());
        if (!CrtModExp(rsa, modulus, value, result, function)) return false;
    } else {
        const auto& exponent = RequireNormal(rsa.d, function, "private exponent");
        if ((rsa.flags & RsaFlagCachePublic) && !MontgomeryContext(rsa.methodModN, modulus, function)) return false;
        if (!MethodModExp(rsa, value, exponent, modulus, rsa.methodModN, result, function)) return false;
    }
    if (!blind) return true;
    if (!blinding.known) NotImplemented_nid_no_patch((std::string(function) + ": blinding with an even modulus").c_str());
    result = MultiplyRemainder(result, blinding.unblind, modulus, function);
    return true;
}

int AddPkcs1Type1(unsigned char* to, int size, const unsigned char* from, int length, const char* function) {
    if (length > size - 11) return 0;
    if (length < 0) throw std::invalid_argument(std::string(function) + ": negative input length");
    RequireBuffer(from, length, function, "input");
    to[0] = 0;
    to[1] = 1;
    std::memset(to + 2, 0xFF, static_cast<std::size_t>(size - 3 - length));
    to[size - length - 1] = 0;
    if (length) std::memcpy(to + size - length, from, static_cast<std::size_t>(length));
    return 1;
}

int AddX931(unsigned char* to, int size, const unsigned char* from, int length, const char* function) {
    const std::int64_t padding = std::int64_t(size) - length - 2;
    if (padding < 0) return -1;
    if (length < 0) throw std::invalid_argument(std::string(function) + ": negative input length");
    RequireBuffer(from, length, function, "input");
    unsigned char* data = to;
    if (!padding) {
        *data++ = 0x6A;
    } else {
        *data++ = 0x6B;
        std::memset(data, 0xBB, static_cast<std::size_t>(padding - 1));
        data += padding - 1;
        *data++ = 0xBA;
    }
    if (length) std::memcpy(data, from, static_cast<std::size_t>(length));
    data[length] = 0xCC;
    return 1;
}

int CopyMessage(unsigned char* to, int size, const unsigned char* from, int length, const char* function) {
    if (length > size) return -1;
    RequireBuffer(to, length, function, "output");
    if (length) std::memcpy(to, from, static_cast<std::size_t>(length));
    return length;
}

int CheckPkcs1Type2(unsigned char* to, int size, const unsigned char* from, int length, int modulusLength, const char* function) {
    if (modulusLength != length + 1 || !length || from[0] != 0x02) return -1;
    const int remaining = length - 1;
    int index = 0;
    while (index < remaining && from[1 + index]) ++index;
    if (index == remaining || index < 8) return -1;
    return CopyMessage(to, size, from + 2 + index, remaining - index - 1, function);
}

int CheckSslv23(unsigned char* to, int size, const unsigned char* from, int length, int modulusLength, const char* function) {
    if (length < 10) return -1;
    if (modulusLength != length + 1 || from[0] != 0x02) return -1;
    const int remaining = length - 1;
    int index = 0;
    while (index < remaining && from[1 + index]) ++index;
    if (index == remaining || index < 8) return -1;
    const unsigned char* message = from + 2 + index;
    int marker = -9;
    while (marker < -1 && message[marker] == 0x03) ++marker;
    if (marker == -1) return -1;
    return CopyMessage(to, size, message, remaining - index - 1, function);
}

int CheckOaep(unsigned char* to, int size, const unsigned char* from, int length, int modulusLength, const char* function) {
    const int encoded = modulusLength - 1;
    if (encoded < 2 * Sha1Bytes + 1) return -1;
    int leadingZeros = encoded - length;
    const bool bad = leadingZeros < 0;
    if (bad) {
        leadingZeros = 0;
        length = encoded;
    }
    const int blockLength = encoded - Sha1Bytes;
    std::vector<unsigned char> padded(static_cast<std::size_t>(encoded));
    if (length) std::memcpy(padded.data() + leadingZeros, from, static_cast<std::size_t>(length));
    const unsigned char* maskedBlock = padded.data() + Sha1Bytes;
    unsigned char seed[Sha1Bytes];
    if (!Mgf1Sha1(seed, Sha1Bytes, maskedBlock, blockLength)) return -1;
    for (int index = 0; index < Sha1Bytes; ++index) seed[index] ^= padded[static_cast<std::size_t>(index)];
    std::vector<unsigned char> block(static_cast<std::size_t>(blockLength));
    if (!Mgf1Sha1(block.data(), blockLength, seed, Sha1Bytes)) return -1;
    for (int index = 0; index < blockLength; ++index) block[static_cast<std::size_t>(index)] ^= maskedBlock[index];
    static constexpr unsigned char empty[1] {};
    unsigned char labelHash[Sha1Bytes];
    if (mbedtls_sha1(empty, 0, labelHash)) return -1;
    if (std::memcmp(block.data(), labelHash, Sha1Bytes) || bad) return -1;
    int index = Sha1Bytes;
    while (index < blockLength && !block[static_cast<std::size_t>(index)]) ++index;
    if (index == blockLength || block[static_cast<std::size_t>(index)] != 0x01) return -1;
    return CopyMessage(to, size, block.data() + index + 1, blockLength - index - 1, function);
}

void WriteResult(unsigned char* to, int size, const Words& result, const char* function) {
    const auto bytes = ToBytes(result);
    if (bytes.size() > static_cast<std::size_t>(size)) throw std::invalid_argument(std::string(function) + ": a CRT result longer than the modulus");
    RequireBuffer(to, size, function, "output");
    std::memset(to, 0, static_cast<std::size_t>(size) - bytes.size());
    if (!bytes.empty()) std::memcpy(to + size - bytes.size(), bytes.data(), bytes.size());
}

int APS5_VABI DefaultPrivateEncrypt(int length, const unsigned char* from, unsigned char* to, LibreSslRsa* rsa, int padding) {
    constexpr const char* function = "RSA_eay_private_encrypt";
    if (!rsa) throw std::invalid_argument(std::string(function) + ": null key");
    const int size = ByteCount(static_cast<const LibreSslBignum*>(rsa->n), function);
    const auto& modulus = RequireNormal(rsa->n, function, "modulus");
    std::vector<unsigned char> block(static_cast<std::size_t>(size));
    int padded = 0;
    switch (padding) {
    case PaddingPkcs1:
        padded = AddPkcs1Type1(block.data(), size, from, length, function);
        break;
    case PaddingX931:
        padded = AddX931(block.data(), size, from, length, function);
        break;
    case PaddingNone:
        padded = AddNone(block.data(), size, from, length, function);
        break;
    default:
        return -1;
    }
    if (padded <= 0) return -1;
    const auto value = FromBytes(block.data(), block.size());
    if (Compare(value.data(), static_cast<int>(value.size()), modulus.d, modulus.top) >= 0) return -1;
    Words result;
    if (!PrivateOperation(*rsa, modulus, value, result, function)) return -1;
    if (padding == PaddingX931) {
        const auto number = Value(modulus);
        if (Compare(result.data(), static_cast<int>(result.size()), modulus.d, modulus.top) > 0) {
            result = Difference(result, number, function);
        } else {
            auto complement = Difference(number, result, function);
            if (Compare(result.data(), static_cast<int>(result.size()), complement.data(), static_cast<int>(complement.size())) > 0) result = std::move(complement);
        }
    }
    WriteResult(to, size, result, function);
    return size;
}

int APS5_VABI DefaultPrivateDecrypt(int length, const unsigned char* from, unsigned char* to, LibreSslRsa* rsa, int padding) {
    constexpr const char* function = "RSA_eay_private_decrypt";
    if (!rsa) throw std::invalid_argument(std::string(function) + ": null key");
    const int size = ByteCount(static_cast<const LibreSslBignum*>(rsa->n), function);
    const auto& modulus = RequireNormal(rsa->n, function, "modulus");
    if (length > size || length < 0) return -1;
    RequireBuffer(from, length, function, "input");
    const auto value = FromBytes(from, static_cast<std::size_t>(length));
    if (Compare(value.data(), static_cast<int>(value.size()), modulus.d, modulus.top) >= 0) return -1;
    Words result;
    if (!PrivateOperation(*rsa, modulus, value, result, function)) return -1;
    const auto bytes = ToBytes(result);
    if (bytes.size() > static_cast<std::size_t>(size)) throw std::invalid_argument(std::string(function) + ": a CRT result longer than the modulus");
    const int count = static_cast<int>(bytes.size());
    switch (padding) {
    case PaddingPkcs1:
        return CheckPkcs1Type2(to, size, bytes.data(), count, size, function);
    case PaddingOaep:
        return CheckOaep(to, size, bytes.data(), count, size, function);
    case PaddingSslv23:
        return CheckSslv23(to, size, bytes.data(), count, size, function);
    case PaddingNone:
        return CheckNone(to, size, bytes.data(), count, function);
    default:
        return -1;
    }
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

int APS5_VABI RSA_private_encrypt_nid_postfix(int length, const unsigned char* from, unsigned char* to, LibreSslRsa* rsa, int padding) {
    if (!rsa || !rsa->meth) throw std::invalid_argument(std::string(__func__) + ": null key or method");
    const auto encrypt = Operation<RsaCrypt>(*rsa->meth, OperationPrivateEncrypt);
    if (!encrypt) throw std::invalid_argument(std::string(__func__) + ": method without rsa_priv_enc");
    return encrypt(length, from, to, rsa, padding);
}

int APS5_VABI RSA_private_decrypt_nid_postfix(int length, const unsigned char* from, unsigned char* to, LibreSslRsa* rsa, int padding) {
    if (!rsa || !rsa->meth) throw std::invalid_argument(std::string(__func__) + ": null key or method");
    const auto decrypt = Operation<RsaCrypt>(*rsa->meth, OperationPrivateDecrypt);
    if (!decrypt) throw std::invalid_argument(std::string(__func__) + ": method without rsa_priv_dec");
    return decrypt(length, from, to, rsa, padding);
}

int APS5_VABI RSA_sign_nid_postfix(int type, const unsigned char* digest, unsigned int digestLength, unsigned char* signature, unsigned int* signatureLength, LibreSslRsa* rsa) {
    if (!rsa) throw std::invalid_argument(std::string(__func__) + ": null key");
    if (rsa->flags & RsaFlagSignVerify) {
        if (!rsa->meth) throw std::invalid_argument(std::string(__func__) + ": null method");
        if (rsa->meth->sign) return reinterpret_cast<RsaSign>(rsa->meth->sign)(type, digest, digestLength, signature, signatureLength, rsa);
    }
    const DigestObject* object = nullptr;
    std::uint64_t algorithmLength = 0;
    std::uint64_t contentLength = 0;
    std::uint64_t length = Md5Sha1Bytes;
    if (type == DigestMd5Sha1) {
        if (digestLength != Md5Sha1Bytes) return 0;
    } else {
        if (type <= 0) return 0;
        object = &DigestObjectFor(type, __func__);
        if (digestLength > INT_MAX) NotImplemented_nid_no_patch((std::string(__func__) + ": a digest length over INT_MAX").c_str());
        algorithmLength = 2 + static_cast<std::uint64_t>(object->length) + 2;
        contentLength = 1 + DerLengthBytes(algorithmLength) + algorithmLength + 1 + DerLengthBytes(digestLength) + digestLength;
        length = 1 + DerLengthBytes(contentLength) + contentLength;
        if (length > INT_MAX) NotImplemented_nid_no_patch((std::string(__func__) + ": a DigestInfo over INT_MAX bytes").c_str());
    }
    if (static_cast<std::int64_t>(length) > std::int64_t(RSA_size_nid_postfix(rsa)) - 11) return 0;
    std::vector<unsigned char> encoded;
    const unsigned char* data = digest;
    if (object) {
        encoded.reserve(static_cast<std::size_t>(length));
        AppendDerHeader(encoded, 0x30, contentLength);
        AppendDerHeader(encoded, 0x30, algorithmLength);
        AppendDerHeader(encoded, 0x06, static_cast<std::uint64_t>(object->length));
        encoded.insert(encoded.end(), object->object, object->object + object->length);
        encoded.push_back(0x05);
        encoded.push_back(0x00);
        AppendDerHeader(encoded, 0x04, digestLength);
        RequireBuffer(digest, digestLength, __func__, "digest");
        if (digestLength) encoded.insert(encoded.end(), digest, digest + digestLength);
        data = encoded.data();
    }
    const int result = RSA_private_encrypt_nid_postfix(static_cast<int>(length), data, signature, rsa, PaddingPkcs1);
    if (result <= 0) return 0;
    if (!signatureLength) throw std::invalid_argument(std::string(__func__) + ": null signature length");
    *signatureLength = static_cast<unsigned int>(result);
    return 1;
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
    RejectAddedObject(algorithm, __func__);
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
