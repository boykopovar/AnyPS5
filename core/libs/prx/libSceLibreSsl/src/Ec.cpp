#include "prx/libSceLibreSsl/include/Api.hpp"
#include <atomic>
#include <climits>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <memory>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>
#include "prx/libSceLibreSsl/include/Ec.hpp"
#include "prx/libc/include/Api.hpp"
#include "prx/libc/include/General.hpp"
#include "prx/libc/include/HostEntropy.hpp"
#include "mbedtls/bignum.h"
#include "mbedtls/ecdh.h"
#include "mbedtls/ecdsa.h"
#include "mbedtls/ecp.h"

EcGroup::EcGroup(const NamedCurve& named) : curve(named) {
    mbedtls_ecp_group_init(&group);
    if (mbedtls_ecp_group_load(&group, named.id) != 0) {
        mbedtls_ecp_group_free(&group);
        throw std::runtime_error("EC_GROUP: mbedtls failed to load curve " + std::to_string(named.nid));
    }
}

EcGroup::~EcGroup() {
    mbedtls_ecp_group_free(&group);
}

struct EcBignum {
    std::uint64_t* d;
    int top;
    int dmax;
    int neg;
    int flags;
};

static_assert(sizeof(EcBignum) == 24);
static_assert(offsetof(EcBignum, top) == 8);

EcPoint::EcPoint(const NamedCurve& named) : curve(named) {
    mbedtls_ecp_point_init(&point);
}

EcPoint::~EcPoint() {
    mbedtls_ecp_point_free(&point);
}

namespace {
constexpr NamedCurve NamedCurves[] = {
    {409, MBEDTLS_ECP_DP_SECP192R1},
    {415, MBEDTLS_ECP_DP_SECP256R1},
    {711, MBEDTLS_ECP_DP_SECP192K1},
    {712, MBEDTLS_ECP_DP_SECP224K1},
    {713, MBEDTLS_ECP_DP_SECP224R1},
    {714, MBEDTLS_ECP_DP_SECP256K1},
    {715, MBEDTLS_ECP_DP_SECP384R1},
    {716, MBEDTLS_ECP_DP_SECP521R1},
    {928, MBEDTLS_ECP_DP_BP256R1},
    {932, MBEDTLS_ECP_DP_BP384R1},
    {934, MBEDTLS_ECP_DP_BP512R1},
};

constexpr std::pair<int, int> CurvesMissingFromMbedtls[] = {
    {410, 414}, {684, 690}, {693, 696}, {699, 710}, {717, 745}, {749, 750},
    {839, 844}, {922, 927}, {929, 931}, {933, 933}, {935, 936}, {943, 944},
};

constexpr int PointConversionCompressed = 2;
constexpr int PointConversionUncompressed = 4;
constexpr int PointConversionHybrid = 6;

void Check(int result, const char* function) {
    if (result != 0) throw std::runtime_error(std::string(function) + ": mbedtls error " + std::to_string(result));
}

int HostRandom(void*, unsigned char* buffer, std::size_t size) {
    HostEntropy_nid_no_patch(buffer, size);
    return 0;
}

struct Mpi {
    Mpi() {
        mbedtls_mpi_init(&value);
    }

    ~Mpi() {
        mbedtls_mpi_free(&value);
    }

    Mpi(const Mpi&) = delete;
    Mpi& operator=(const Mpi&) = delete;

    mbedtls_mpi value;
};

struct Point {
    Point() {
        mbedtls_ecp_point_init(&value);
    }

    ~Point() {
        mbedtls_ecp_point_free(&value);
    }

    Point(const Point&) = delete;
    Point& operator=(const Point&) = delete;

    mbedtls_ecp_point value;
};

std::unique_ptr<EcGroup> NewGroup(int nid) {
    for (const NamedCurve& curve : NamedCurves) {
        if (curve.nid == nid) return std::make_unique<EcGroup>(curve);
    }
    for (const auto& [first, last] : CurvesMissingFromMbedtls) {
        if (nid >= first && nid <= last) NotImplemented_nid_no_patch("EC_GROUP_new_by_curve_name: a LibreSSL curve that mbedtls does not provide");
    }
    return nullptr;
}

constexpr int BignumFlagMalloced = 0x01;
constexpr int BignumFlagStaticData = 0x02;
constexpr int BignumMaxWords = INT_MAX / (4 * 64);

void Wipe(void* data, std::size_t size) {
    auto* bytes = static_cast<volatile unsigned char*>(data);
    for (std::size_t index = 0; index < size; ++index) bytes[index] = 0;
}

bool SharesWords(const EcBignum& source, const EcBignum* stored) {
    if (!stored || source.top == 0) return false;
    const auto sourceStart = reinterpret_cast<std::uintptr_t>(source.d);
    const auto storedStart = reinterpret_cast<std::uintptr_t>(stored->d);
    const std::uintptr_t sourceEnd = sourceStart + static_cast<std::uintptr_t>(source.top) * sizeof(std::uint64_t);
    const std::uintptr_t storedEnd = storedStart + static_cast<std::uintptr_t>(stored->dmax) * sizeof(std::uint64_t);
    return sourceStart < storedEnd && storedStart < sourceEnd;
}

std::unique_ptr<EcBignum, EcBignumDeleter> NewBignum() {
    std::unique_ptr<EcBignum, EcBignumDeleter> number(static_cast<EcBignum*>(malloc_nid_postfix(sizeof(EcBignum))));
    if (number) *number = {nullptr, 0, 0, 0, BignumFlagMalloced};
    return number;
}

std::unique_ptr<EcBignum, EcBignumDeleter> DuplicateBignum(const EcBignum& source) {
    std::unique_ptr<EcBignum, EcBignumDeleter> number = NewBignum();
    if (!number) return nullptr;
    if (source.top > 0) {
        const std::size_t words = static_cast<std::size_t>(source.top);
        number->d = static_cast<std::uint64_t*>(malloc_nid_postfix(words * sizeof(std::uint64_t)));
        if (!number->d) return nullptr;
        number->dmax = source.top;
        std::memcpy(number->d, source.d, words * sizeof(std::uint64_t));
    }
    number->top = source.top;
    number->neg = source.neg;
    return number;
}

int RandomRangeWords(const mbedtls_mpi& order) {
    std::size_t bits = mbedtls_mpi_bitlen(&order);
    if (mbedtls_mpi_get_bit(&order, bits - 2) == 0 && mbedtls_mpi_get_bit(&order, bits - 3) == 0) ++bits;
    return static_cast<int>(((bits + 7) / 8 + sizeof(std::uint64_t) - 1) / sizeof(std::uint64_t));
}

void ReadPrivateKey(const EcGroup& group, const EcBignum& stored, mbedtls_mpi& out, const char* function) {
    const mbedtls_mpi& order = group.group.N;
    const std::size_t orderWords = (mbedtls_mpi_bitlen(&order) + 63) / 64;
    const bool normalized = stored.top > 0 && !stored.neg && static_cast<std::size_t>(stored.top) <= orderWords && stored.d[stored.top - 1] != 0;
    if (normalized) Check(mbedtls_mpi_read_binary_le(&out, reinterpret_cast<const unsigned char*>(stored.d), static_cast<std::size_t>(stored.top) * sizeof(stored.d[0])), function);
    if (!normalized || mbedtls_mpi_cmp_mpi(&out, &order) >= 0) {
        NotImplemented_nid_no_patch((std::string(function) + ": a private key that is zero, negative, at least the group order or has zero high words").c_str());
    }
}

int DerLengthSize(int length) {
    int size = 1;
    if (length > 127) {
        for (; length > 0; length >>= 8) ++size;
    }
    return size;
}

void RequireSameCurve(const EcGroup& group, const NamedCurve& curve, const char* what) {
    if (group.curve.nid != curve.nid) NotImplemented_nid_no_patch(what);
}

bool AtInfinity(const mbedtls_ecp_point& point) {
    return mbedtls_ecp_is_zero(const_cast<mbedtls_ecp_point*>(&point)) == 1;
}

std::size_t FieldSize(const EcGroup& group) {
    return mbedtls_mpi_size(&group.group.P);
}

std::vector<unsigned char> EncodeUncompressed(const EcGroup& group, const mbedtls_ecp_point& point) {
    std::vector<unsigned char> out(1 + 2 * FieldSize(group));
    std::size_t size = 0;
    Check(mbedtls_ecp_point_write_binary(&group.group, &point, MBEDTLS_ECP_PF_UNCOMPRESSED, &size, out.data(), out.size()), "EC point encoding");
    out.resize(size);
    return out;
}

void AppendDerLength(std::vector<unsigned char>& out, std::size_t length) {
    if (length < 128) {
        out.push_back(static_cast<unsigned char>(length));
        return;
    }
    unsigned char bytes[sizeof(length)];
    std::size_t count = 0;
    for (; length > 0; length >>= 8) bytes[count++] = static_cast<unsigned char>(length);
    out.push_back(static_cast<unsigned char>(0x80 | count));
    while (count > 0) out.push_back(bytes[--count]);
}

void AppendDerInteger(std::vector<unsigned char>& out, const mbedtls_mpi& value) {
    const std::size_t size = mbedtls_mpi_bitlen(&value) / 8 + 1;
    out.push_back(0x02);
    AppendDerLength(out, size);
    out.resize(out.size() + size);
    Check(mbedtls_mpi_write_binary(&value, out.data() + out.size() - size, size), "ECDSA signature encoding");
}

std::vector<unsigned char> EncodeSignature(const mbedtls_mpi& r, const mbedtls_mpi& s) {
    std::vector<unsigned char> integers;
    AppendDerInteger(integers, r);
    AppendDerInteger(integers, s);
    std::vector<unsigned char> out{0x30};
    AppendDerLength(out, integers.size());
    out.insert(out.end(), integers.begin(), integers.end());
    return out;
}

struct DerBytes {
    const unsigned char* data = nullptr;
    std::size_t size = 0;
};

bool ReadDerElement(const unsigned char*& cursor, const unsigned char* end, unsigned char tag, DerBytes& content) {
    if (end - cursor < 2 || cursor[0] != tag) return false;
    std::size_t length = cursor[1];
    cursor += 2;
    if (length & 0x80) {
        const std::size_t count = length & 0x7f;
        if (count == 0 || count > sizeof(int) || static_cast<std::size_t>(end - cursor) < count || cursor[0] == 0) return false;
        length = 0;
        for (std::size_t i = 0; i < count; ++i) length = (length << 8) | *cursor++;
        if (length < 128) return false;
    }
    if (static_cast<std::size_t>(end - cursor) < length) return false;
    content = {cursor, length};
    cursor += length;
    return true;
}

bool ReadDerInteger(const unsigned char*& cursor, const unsigned char* end, DerBytes& magnitude) {
    if (!ReadDerElement(cursor, end, 0x02, magnitude) || magnitude.size == 0 || (magnitude.data[0] & 0x80)) return false;
    if (magnitude.data[0] == 0) {
        if (magnitude.size > 1 && !(magnitude.data[1] & 0x80)) return false;
        ++magnitude.data;
        --magnitude.size;
    }
    return true;
}

bool DecodeSignature(const unsigned char* signature, int length, DerBytes& r, DerBytes& s) {
    const unsigned char* cursor = signature;
    DerBytes sequence;
    if (!ReadDerElement(cursor, signature + length, 0x30, sequence) || cursor != signature + length) return false;
    const unsigned char* const sequenceEnd = sequence.data + sequence.size;
    return ReadDerInteger(sequence.data, sequenceEnd, r) && ReadDerInteger(sequence.data, sequenceEnd, s) && sequence.data == sequenceEnd;
}

bool ReadInRange(const DerBytes& magnitude, const mbedtls_mpi& order, mbedtls_mpi& out) {
    if (magnitude.size > mbedtls_mpi_size(&order)) return false;
    Check(mbedtls_mpi_read_binary(&out, magnitude.data, magnitude.size), "ECDSA_verify");
    return mbedtls_mpi_cmp_int(&out, 0) > 0 && mbedtls_mpi_cmp_mpi(&out, &order) < 0;
}

void DigestToInteger(const EcGroup& group, const unsigned char* digest, int length, mbedtls_mpi& out) {
    const std::size_t bits = group.group.nbits;
    std::size_t used = static_cast<std::size_t>(length);
    if (8 * used > bits) used = (bits + 7) / 8;
    Check(mbedtls_mpi_read_binary(&out, digest, used), "ECDSA_verify");
    if (8 * used > bits) Check(mbedtls_mpi_shift_r(&out, 8 * used - bits), "ECDSA_verify");
}

void RequireUsablePublicKey(const EcGroup& group, const EcPoint& point, const char* what) {
    if (AtInfinity(point.point) || mbedtls_ecp_check_pubkey(&group.group, &point.point) != 0) NotImplemented_nid_no_patch(what);
}

}

void EcBignumDeleter::operator()(EcBignum* number) const {
    if (number->d && !(number->flags & BignumFlagStaticData)) {
        Wipe(number->d, static_cast<std::size_t>(number->dmax) * sizeof(number->d[0]));
        free_nid_postfix(number->d);
    }
    const bool allocated = number->flags & BignumFlagMalloced;
    Wipe(number, sizeof(EcBignum));
    if (allocated) free_nid_postfix(number);
}

extern "C" {

EcGroup* APS5_VABI EC_GROUP_new_by_curve_name_nid_postfix(int nid) {
    return NewGroup(nid).release();
}

void APS5_VABI EC_GROUP_free_nid_postfix(EcGroup* group) {
    delete group;
}

int APS5_VABI EC_GROUP_get_degree_nid_postfix(const EcGroup* group) {
    if (group == nullptr) APS5_INVALID_ARG_EX;
    return static_cast<int>(group->group.pbits);
}

EcPoint* APS5_VABI EC_POINT_new_nid_postfix(const EcGroup* group) {
    if (group == nullptr) return nullptr;
    return new EcPoint(group->curve);
}

void APS5_VABI EC_POINT_free_nid_postfix(EcPoint* point) {
    delete point;
}

std::size_t APS5_VABI EC_POINT_point2oct_nid_postfix(const EcGroup* group, const EcPoint* point, int form, unsigned char* buffer, std::size_t length, void*) {
    if (group == nullptr || point == nullptr) APS5_INVALID_ARG_EX;
    RequireSameCurve(*group, point->curve, "EC_POINT_point2oct: a point of another curve");
    if (form != PointConversionCompressed && form != PointConversionUncompressed && form != PointConversionHybrid) return 0;
    const mbedtls_ecp_point& value = point->point;
    if (AtInfinity(value)) {
        if (buffer != nullptr) {
            if (length < 1) return 0;
            buffer[0] = 0;
        }
        return 1;
    }
    const std::size_t fieldSize = FieldSize(*group);
    const std::size_t size = form == PointConversionCompressed ? 1 + fieldSize : 1 + 2 * fieldSize;
    if (buffer == nullptr) return size;
    if (length < size) return 0;
    const std::vector<unsigned char> uncompressed = EncodeUncompressed(*group, value);
    buffer[0] = static_cast<unsigned char>(form != PointConversionUncompressed && (uncompressed.back() & 1) ? form + 1 : form);
    std::memcpy(buffer + 1, uncompressed.data() + 1, size - 1);
    return size;
}

int APS5_VABI EC_POINT_oct2point_nid_postfix(const EcGroup* group, EcPoint* point, const unsigned char* buffer, std::size_t length, void*) {
    if (group == nullptr || point == nullptr) APS5_INVALID_ARG_EX;
    RequireSameCurve(*group, point->curve, "EC_POINT_oct2point: a point of another curve");
    if (length == 0) return 0;
    if (buffer == nullptr) APS5_INVALID_ARG_EX;
    const int yBit = buffer[0] & 1;
    const int form = buffer[0] & ~1;
    if (form != 0 && form != PointConversionCompressed && form != PointConversionUncompressed && form != PointConversionHybrid) return 0;
    if ((form == 0 || form == PointConversionUncompressed) && yBit) return 0;
    if (form == 0) {
        if (length != 1) return 0;
        Check(mbedtls_ecp_set_zero(&point->point), __func__);
        return 1;
    }
    const std::size_t fieldSize = FieldSize(*group);
    if (length != (form == PointConversionCompressed ? 1 + fieldSize : 1 + 2 * fieldSize)) return 0;
    const mbedtls_ecp_group& curve = group->group;
    Mpi coordinate;
    Check(mbedtls_mpi_read_binary(&coordinate.value, buffer + 1, fieldSize), __func__);
    if (mbedtls_mpi_cmp_mpi(&coordinate.value, &curve.P) >= 0) return 0;
    if (form == PointConversionCompressed) {
        if (mbedtls_mpi_get_bit(&curve.P, 1) == 0) NotImplemented_nid_no_patch("EC_POINT_oct2point: a compressed point on a curve whose prime is not 3 mod 4");
        Point decoded;
        Check(mbedtls_ecp_point_read_binary(&curve, &decoded.value, buffer, length), __func__);
        if (mbedtls_ecp_check_pubkey(&curve, &decoded.value) != 0) return 0;
        Check(mbedtls_ecp_copy(&point->point, &decoded.value), __func__);
        return 1;
    }
    Check(mbedtls_mpi_read_binary(&coordinate.value, buffer + 1 + fieldSize, fieldSize), __func__);
    if (mbedtls_mpi_cmp_mpi(&coordinate.value, &curve.P) >= 0) return 0;
    if (form == PointConversionHybrid && yBit != (buffer[length - 1] & 1)) return 0;
    std::vector<unsigned char> uncompressed(buffer, buffer + length);
    uncompressed[0] = PointConversionUncompressed;
    Check(mbedtls_ecp_point_read_binary(&curve, &point->point, uncompressed.data(), uncompressed.size()), __func__);
    return mbedtls_ecp_check_pubkey(&curve, &point->point) == 0 ? 1 : 0;
}

EcKey* APS5_VABI EC_KEY_new_nid_postfix(void) {
    return new EcKey;
}

EcKey* APS5_VABI EC_KEY_new_by_curve_name_nid_postfix(int nid) {
    std::unique_ptr<EcGroup> group = NewGroup(nid);
    if (!group) return nullptr;
    return new EcKey{std::move(group)};
}

void APS5_VABI EC_KEY_free_nid_postfix(EcKey* key) {
    static_assert(std::atomic_ref<std::int32_t>::required_alignment <= alignof(std::int32_t));
    if (key && std::atomic_ref<std::int32_t>(key->references).fetch_sub(1) == 1) delete key;
}

const EcGroup* APS5_VABI EC_KEY_get0_group_nid_postfix(const EcKey* key) {
    if (key == nullptr) APS5_INVALID_ARG_EX;
    return key->group.get();
}

int APS5_VABI EC_KEY_set_group_nid_postfix(EcKey* key, const EcGroup* group) {
    if (key == nullptr) APS5_INVALID_ARG_EX;
    if (group) {
        if (key->publicKey) RequireSameCurve(*group, key->publicKey->curve, "EC_KEY_set_group: a curve other than the one of the key's public key");
    }
    key->group = group ? std::make_unique<EcGroup>(group->curve) : nullptr;
    return key->group ? 1 : 0;
}

const void* APS5_VABI EC_KEY_get0_private_key_nid_postfix(const EcKey* key) {
    if (key == nullptr) APS5_INVALID_ARG_EX;
    return key->privateKey.get();
}

int APS5_VABI EC_KEY_set_private_key_nid_postfix(EcKey* key, const void* privateKey) {
    if (key == nullptr) APS5_INVALID_ARG_EX;
    const auto* source = static_cast<const EcBignum*>(privateKey);
    if (source != nullptr && source == key->privateKey.get()) throw std::invalid_argument("EC_KEY_set_private_key: the key's own private key, which LibreSSL frees before copying");
    if (source != nullptr && source->top <= BignumMaxWords) {
        if (source->top < 0) throw std::invalid_argument("EC_KEY_set_private_key: private key word count " + std::to_string(source->top));
        if (source->top > 0 && !source->d) throw std::invalid_argument("EC_KEY_set_private_key: null private key words");
        if (SharesWords(*source, key->privateKey.get())) throw std::invalid_argument("EC_KEY_set_private_key: the words of the key's own private key, which LibreSSL frees before copying");
    }
    key->privateKey.reset();
    if (source == nullptr || source->top > BignumMaxWords) return 0;
    key->privateKey = DuplicateBignum(*source);
    return key->privateKey ? 1 : 0;
}

const EcPoint* APS5_VABI EC_KEY_get0_public_key_nid_postfix(const EcKey* key) {
    if (key == nullptr) APS5_INVALID_ARG_EX;
    return key->publicKey.get();
}

int APS5_VABI EC_KEY_set_public_key_nid_postfix(EcKey* key, const EcPoint* point) {
    if (key == nullptr) APS5_INVALID_ARG_EX;
    if (point == nullptr || !key->group) {
        key->publicKey.reset();
        return 0;
    }
    RequireSameCurve(*key->group, point->curve, "EC_KEY_set_public_key: a point of another curve");
    auto copy = std::make_unique<EcPoint>(point->curve);
    Check(mbedtls_ecp_copy(&copy->point, &point->point), __func__);
    key->publicKey = std::move(copy);
    return 1;
}

int APS5_VABI EC_KEY_generate_key_nid_postfix(EcKey* key) {
    if (key == nullptr || !key->group) return 0;
    EcGroup& group = *key->group;
    Mpi secret;
    Point publicKey;
    Check(mbedtls_ecp_gen_keypair(&group.group, &secret.value, &publicKey.value, HostRandom, nullptr), __func__);
    std::unique_ptr<EcBignum, EcBignumDeleter> created;
    EcBignum* number = key->privateKey.get();
    if (!number) {
        created = NewBignum();
        if (!created) return 0;
        number = created.get();
    }
    const int words = RandomRangeWords(group.group.N);
    if (words > number->dmax) {
        if (number->flags & BignumFlagStaticData) return 0;
        auto* expanded = static_cast<std::uint64_t*>(malloc_nid_postfix(static_cast<std::size_t>(words) * sizeof(std::uint64_t)));
        if (!expanded) return 0;
        if (number->d) {
            Wipe(number->d, static_cast<std::size_t>(number->dmax) * sizeof(number->d[0]));
            free_nid_postfix(number->d);
        }
        number->d = expanded;
        number->dmax = words;
    }
    Check(mbedtls_mpi_write_binary_le(&secret.value, reinterpret_cast<unsigned char*>(number->d), static_cast<std::size_t>(words) * sizeof(number->d[0])), __func__);
    number->top = static_cast<int>((mbedtls_mpi_bitlen(&secret.value) + 63) / 64);
    number->neg = 0;
    if (!key->publicKey) key->publicKey = std::make_unique<EcPoint>(group.curve);
    Check(mbedtls_ecp_copy(&key->publicKey->point, &publicKey.value), __func__);
    if (created) key->privateKey = std::move(created);
    return 1;
}

int APS5_VABI EC_KEY_check_key_nid_postfix(const EcKey* key) {
    if (key == nullptr || !key->group || !key->publicKey) return 0;
    EcGroup& group = *key->group;
    const mbedtls_ecp_point& publicKey = key->publicKey->point;
    if (AtInfinity(publicKey) || mbedtls_ecp_check_pubkey(&group.group, &publicKey) != 0) return 0;
    if (key->privateKey) {
        Mpi secret;
        ReadPrivateKey(group, *key->privateKey, secret.value, "EC_KEY_check_key");
        Point product;
        Check(mbedtls_ecp_mul(&group.group, &product.value, &secret.value, &group.group.G, HostRandom, nullptr), __func__);
        if (mbedtls_ecp_point_cmp(&product.value, &publicKey) != 0) return 0;
    }
    return 1;
}

int APS5_VABI ECDSA_size_nid_postfix(const EcKey* key) {
    if (key == nullptr || !key->group) return 0;
    const int orderBytes = static_cast<int>((mbedtls_mpi_bitlen(&key->group->group.N) + 7) / 8);
    const int integerSize = 1 + DerLengthSize(orderBytes + 1) + orderBytes + 1;
    return 1 + DerLengthSize(2 * integerSize) + 2 * integerSize;
}

int APS5_VABI ECDSA_sign_nid_postfix(int, const unsigned char* digest, int digestLength, unsigned char* signature, unsigned int* signatureLength, EcKey* key) {
    if (key == nullptr || signatureLength == nullptr) APS5_INVALID_ARG_EX;
    if (!key->group || !key->privateKey) {
        *signatureLength = 0;
        return 0;
    }
    if (digestLength < 0 || (digest == nullptr && digestLength > 0)) APS5_INVALID_ARG_EX;
    Mpi secret;
    ReadPrivateKey(*key->group, *key->privateKey, secret.value, "ECDSA_sign");
    Mpi r;
    Mpi s;
    Check(mbedtls_ecdsa_sign(&key->group->group, &r.value, &s.value, &secret.value, digest, static_cast<std::size_t>(digestLength), HostRandom, nullptr), __func__);
    const std::vector<unsigned char> encoded = EncodeSignature(r.value, s.value);
    if (signature != nullptr) std::memcpy(signature, encoded.data(), encoded.size());
    *signatureLength = static_cast<unsigned int>(encoded.size());
    return 1;
}

int APS5_VABI ECDSA_verify_nid_postfix(int, const unsigned char* digest, int digestLength, const unsigned char* signature, int signatureLength, EcKey* key) {
    if (signatureLength <= 0) return -1;
    if (signature == nullptr) APS5_INVALID_ARG_EX;
    DerBytes encodedR;
    DerBytes encodedS;
    if (!DecodeSignature(signature, signatureLength, encodedR, encodedS)) return -1;
    if (key == nullptr) APS5_INVALID_ARG_EX;
    if (!key->group || !key->publicKey) return -1;
    EcGroup& group = *key->group;
    const mbedtls_mpi& order = group.group.N;
    Mpi r;
    Mpi s;
    if (!ReadInRange(encodedR, order, r.value) || !ReadInRange(encodedS, order, s.value)) return 0;
    if (digestLength < 0 || (digest == nullptr && digestLength > 0)) APS5_INVALID_ARG_EX;
    RequireUsablePublicKey(group, *key->publicKey, "ECDSA_verify: a public key at infinity or off the curve");
    Mpi inverse;
    Mpi message;
    Mpi u1;
    Mpi u2;
    Check(mbedtls_mpi_inv_mod(&inverse.value, &s.value, &order), __func__);
    DigestToInteger(group, digest, digestLength, message.value);
    Check(mbedtls_mpi_mul_mpi(&u1.value, &message.value, &inverse.value), __func__);
    Check(mbedtls_mpi_mod_mpi(&u1.value, &u1.value, &order), __func__);
    Check(mbedtls_mpi_mul_mpi(&u2.value, &r.value, &inverse.value), __func__);
    Check(mbedtls_mpi_mod_mpi(&u2.value, &u2.value, &order), __func__);
    Point sum;
    Check(mbedtls_ecp_muladd(&group.group, &sum.value, &u1.value, &group.group.G, &u2.value, &key->publicKey->point), __func__);
    if (AtInfinity(sum.value)) return -1;
    const std::vector<unsigned char> encoded = EncodeUncompressed(group, sum.value);
    Check(mbedtls_mpi_read_binary(&u1.value, encoded.data() + 1, FieldSize(group)), __func__);
    Check(mbedtls_mpi_mod_mpi(&u1.value, &u1.value, &order), __func__);
    return mbedtls_mpi_cmp_mpi(&u1.value, &r.value) == 0 ? 1 : 0;
}

int APS5_VABI ECDH_compute_key_nid_postfix(void* out, std::size_t outLength, const EcPoint* publicKey, EcKey* key, void* (APS5_VABI* kdf)(const void*, std::size_t, void*, std::size_t*)) {
    if (key == nullptr) APS5_INVALID_ARG_EX;
    if (outLength > INT_MAX || !key->privateKey || !key->group || publicKey == nullptr || AtInfinity(publicKey->point)) return -1;
    EcGroup& group = *key->group;
    RequireSameCurve(group, publicKey->curve, "ECDH_compute_key: a public key of another curve");
    RequireUsablePublicKey(group, *publicKey, "ECDH_compute_key: a public key off the curve");
    Mpi privateKey;
    ReadPrivateKey(group, *key->privateKey, privateKey.value, "ECDH_compute_key");
    Mpi shared;
    Check(mbedtls_ecdh_compute_shared(&group.group, &shared.value, &publicKey->point, &privateKey.value, HostRandom, nullptr), __func__);
    const std::size_t size = FieldSize(group);
    if (kdf == nullptr && outLength < size) return -1;
    std::vector<unsigned char> secret(size);
    Check(mbedtls_mpi_write_binary(&shared.value, secret.data(), size), __func__);
    if (kdf != nullptr) {
        if (kdf(secret.data(), size, out, &outLength) == nullptr) return -1;
        return static_cast<int>(outLength);
    }
    if (out == nullptr) APS5_INVALID_ARG_EX;
    std::memset(static_cast<unsigned char*>(out) + size, 0, outLength - size);
    std::memcpy(out, secret.data(), size);
    return static_cast<int>(size);
}

}
