#include "prx/libSceLibreSsl/include/Api.hpp"
#include "prx/libSceLibreSsl/include/Bio.hpp"
#include "prx/libSceLibreSsl/include/Error.hpp"
#include "prx/libSceLibreSsl/include/ObjectTable.hpp"
#include "prx/libc/include/General.hpp"

#include <algorithm>
#include <bit>
#include <cerrno>
#include <charconv>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <limits>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace {

constexpr std::uint64_t X509FlagCompat = 0;
constexpr std::uint64_t X509FlagNoHeader = 1ULL << 0;
constexpr std::uint64_t X509FlagNoVersion = 1ULL << 1;
constexpr std::uint64_t X509FlagNoSerial = 1ULL << 2;
constexpr std::uint64_t X509FlagNoSigName = 1ULL << 3;
constexpr std::uint64_t X509FlagNoIssuer = 1ULL << 4;
constexpr std::uint64_t X509FlagNoValidity = 1ULL << 5;
constexpr std::uint64_t X509FlagNoSubject = 1ULL << 6;
constexpr std::uint64_t X509FlagNoPubkey = 1ULL << 7;
constexpr std::uint64_t X509FlagNoExtensions = 1ULL << 8;
constexpr std::uint64_t X509FlagNoSigDump = 1ULL << 9;
constexpr std::uint64_t X509FlagNoAux = 1ULL << 10;

constexpr std::uint64_t XnFlagSepMask = 0xfULL << 16;
constexpr std::uint64_t XnFlagSepMultiline = 4ULL << 16;

constexpr std::int32_t Asn1TagInteger = 2;
constexpr std::int32_t Asn1TagNegativeInteger = 0x102;
constexpr std::int32_t Asn1TagObject = 6;

constexpr int NidRsaEncryption = 6;
constexpr int NidEcPublicKey = 408;
constexpr int NidRsaSsaPss = 912;

struct Asn1Type {
    std::int32_t type;
    std::int32_t padding;
    const void* value;
};

struct X509Algor {
    const LibreSslAsn1Object* algorithm;
    const Asn1Type* parameter;
};

struct X509Validity {
    const Asn1String* notBefore;
    const Asn1String* notAfter;
};

struct X509Pubkey {
    const X509Algor* algor;
    const Asn1String* publicKey;
    const void* pkey;
};

struct EvpPkey {
    std::int32_t type;
    std::int32_t saveType;
    std::int32_t references;
    std::int32_t padding;
    const void* ameth;
    void* engine;
    void* key;
    std::int32_t saveParameters;
    std::int32_t padding2;
    void* attributes;
};

struct X509Extension {
    const LibreSslAsn1Object* object;
    std::int32_t critical;
    std::int32_t padding;
    const Asn1String* value;
};

struct BignumWords {
    std::uint64_t* d;
    std::int32_t top;
    std::int32_t dmax;
    std::int32_t neg;
    std::int32_t flags;
};

static_assert(sizeof(LibreSslX509) == 32);
static_assert(sizeof(LibreSslX509CertInfo) == 104);
static_assert(offsetof(LibreSslX509CertInfo, key) == 48);
static_assert(offsetof(LibreSslX509CertInfo, extensions) == 72);
static_assert(sizeof(Asn1Type) == 16);
static_assert(offsetof(Asn1Type, value) == 8);
static_assert(sizeof(X509Algor) == 16);
static_assert(offsetof(X509Algor, parameter) == 8);
static_assert(sizeof(X509Validity) == 16);
static_assert(sizeof(X509Pubkey) == 24);
static_assert(offsetof(X509Pubkey, publicKey) == 8);
static_assert(offsetof(X509Pubkey, pkey) == 16);
static_assert(sizeof(EvpPkey) == 56);
static_assert(offsetof(EvpPkey, key) == 32);
static_assert(sizeof(X509Extension) == 24);
static_assert(offsetof(X509Extension, critical) == 8);
static_assert(offsetof(X509Extension, value) == 16);
static_assert(sizeof(BignumWords) == 24);
static_assert(offsetof(BignumWords, neg) == 16);

struct EcNamedCurve {
    int nid;
    int orderBits;
    const char* shortName;
    const char* nistName;
};

constexpr EcNamedCurve NamedCurves[] = {
    {409, 192, "prime192v1", "P-192"},
    {415, 256, "prime256v1", "P-256"},
    {711, 192, "secp192k1", nullptr},
    {712, 224, "secp224k1", nullptr},
    {713, 224, "secp224r1", "P-224"},
    {714, 256, "secp256k1", nullptr},
    {715, 384, "secp384r1", "P-384"},
    {716, 521, "secp521r1", "P-521"},
    {928, 256, "brainpoolP256r1", nullptr},
    {932, 384, "brainpoolP384r1", nullptr},
    {934, 512, "brainpoolP512r1", nullptr},
};

const EcNamedCurve* FindNamedCurve(int nid) {
    for (const auto& curve : NamedCurves)
        if (curve.nid == nid) return &curve;
    return nullptr;
}

bool IsDsaSignatureNid(int nid) {
    switch (nid) {
    case 66:
    case 70:
    case 113:
    case 671:
    case 672:
        return true;
    default:
        return false;
    }
}

class PreservedErrno {
public:
    PreservedErrno() : saved(errno) {}
    ~PreservedErrno() { errno = saved; }

private:
    int saved;
};

template <typename T>
T CopyIn(const T* source, const char* function) {
    PreservedErrno guard;
    T value{};
    if (!GuestCopyIn_nid_no_patch(&value, source, sizeof(value)))
        NotImplementedAt_nid_no_patch(function, __FILE__, __LINE__);
    return value;
}

std::vector<unsigned char> CopyBytes(const unsigned char* source, std::size_t count, const char* function) {
    std::vector<unsigned char> bytes(count);
    if (count == 0) return bytes;
    PreservedErrno guard;
    if (!GuestCopyIn_nid_no_patch(bytes.data(), source, count))
        NotImplementedAt_nid_no_patch(function, __FILE__, __LINE__);
    return bytes;
}

std::string CopyGuestCString(const char* source, const char* function) {
    constexpr std::uintptr_t Page = 4096;
    std::string text;
    auto address = reinterpret_cast<std::uintptr_t>(source);
    char chunk[Page];
    for (;;) {
        const std::size_t size = Page - address % Page;
        {
            PreservedErrno guard;
            if (!GuestCopyIn_nid_no_patch(chunk, reinterpret_cast<const void*>(address), size))
                NotImplementedAt_nid_no_patch(function, __FILE__, __LINE__);
        }
        const auto* end = static_cast<const char*>(std::memchr(chunk, '\0', size));
        text.append(chunk, end ? static_cast<std::size_t>(end - chunk) : size);
        if (end) return text;
        address += size;
    }
}

struct HostAsn1Object {
    LibreSslAsn1Object object{};
    std::vector<unsigned char> data;
    std::string shortName;
    std::string longName;

    const LibreSslAsn1Object* CopyFrom(const LibreSslAsn1Object* guest, const char* function) {
        if (!guest) return nullptr;
        object = CopyIn(guest, function);
        if (object.shortName) {
            shortName = CopyGuestCString(object.shortName, function);
            object.shortName = shortName.c_str();
        }
        if (object.longName) {
            longName = CopyGuestCString(object.longName, function);
            object.longName = longName.c_str();
        }
        if (object.length > 0 && object.data) {
            data = CopyBytes(object.data, static_cast<std::size_t>(object.length), function);
            object.data = data.data();
        }
        return &object;
    }
};

struct HostX509Name {
    LibreSslX509Name name{};
    LibreSslStack stack{};
    std::vector<char*> pointers;
    std::vector<LibreSslX509NameEntry> entries;
    std::vector<HostAsn1Object> objects;
    std::vector<Asn1String> values;
    std::vector<std::vector<unsigned char>> valueBytes;

    const LibreSslX509Name* CopyFrom(const LibreSslX509Name* guest, const char* function) {
        if (!guest) return nullptr;
        name = CopyIn(guest, function);
        if (!name.entries) return &name;
        stack = CopyIn(name.entries, function);
        const std::size_t count = stack.num > 0 ? static_cast<std::size_t>(stack.num) : 0;
        if (count > 0) {
            if (!stack.data) throw std::invalid_argument(std::string(function) + ": null name entry array");
            pointers.resize(count);
            {
                PreservedErrno guard;
                if (!GuestCopyIn_nid_no_patch(pointers.data(), stack.data, count * sizeof(char*)))
                    NotImplementedAt_nid_no_patch(function, __FILE__, __LINE__);
            }
            entries.resize(count);
            objects.resize(count);
            values.resize(count);
            valueBytes.resize(count);
            for (std::size_t i = 0; i < count; ++i) {
                if (!pointers[i]) throw std::invalid_argument(std::string(function) + ": null name entry");
                entries[i] = CopyIn(reinterpret_cast<const LibreSslX509NameEntry*>(pointers[i]), function);
                if (entries[i].object) {
                    entries[i].object = const_cast<LibreSslAsn1Object*>(
                        objects[i].CopyFrom(static_cast<const LibreSslAsn1Object*>(entries[i].object), function));
                }
                if (entries[i].value) {
                    values[i] = CopyIn(static_cast<const Asn1String*>(entries[i].value), function);
                    if (values[i].length > 0 && values[i].data) {
                        valueBytes[i] = CopyBytes(values[i].data, static_cast<std::size_t>(values[i].length), function);
                        values[i].data = valueBytes[i].data();
                    }
                    entries[i].value = &values[i];
                }
                pointers[i] = reinterpret_cast<char*>(&entries[i]);
            }
            stack.data = pointers.data();
        }
        name.entries = &stack;
        return &name;
    }
};

bool WriteBytes(LibreSsl::Bio* bio, std::string_view text) {
    if (!bio) return false;
    if (text.empty()) return true;
    return LibreSslBioWrite_nid_no_patch(bio, text.data(), static_cast<int>(text.size())) > 0;
}

bool WriteIndent(LibreSsl::Bio* bio, int indent) {
    if (indent <= 0) return true;
    const int clamped = std::min(indent, 128);
    const std::string spaces(static_cast<std::size_t>(clamped), ' ');
    return WriteBytes(bio, spaces);
}

std::string Decimal(std::uint64_t value) {
    char buffer[24];
    const auto end = std::to_chars(buffer, buffer + sizeof(buffer), value).ptr;
    return std::string(buffer, end);
}

std::string HexLower(std::uint64_t value) {
    char buffer[24];
    const auto end = std::to_chars(buffer, buffer + sizeof(buffer), value, 16).ptr;
    return std::string(buffer, end);
}

std::string ByteHexLower(unsigned char value) {
    constexpr char Digits[] = "0123456789abcdef";
    std::string text;
    text.push_back(Digits[value >> 4]);
    text.push_back(Digits[value & 0x0f]);
    return text;
}

bool PrintVersion(LibreSsl::Bio* bio, const Asn1String* guestVersion) {
    const std::int64_t version = ASN1_INTEGER_get_nid_postfix(guestVersion);
    std::string line("        Version: ");
    line += Decimal(static_cast<std::uint64_t>(version) + 1);
    line += " (0x";
    line += HexLower(static_cast<std::uint64_t>(version));
    line += ")\n";
    return WriteBytes(bio, line);
}

bool PrintSerial(LibreSsl::Bio* bio, const void* guestSerial) {
    if (!WriteBytes(bio, "        Serial Number:")) return false;
    if (!guestSerial) throw std::invalid_argument("X509_print_ex: null serial number");
    const Asn1String serial = CopyIn(static_cast<const Asn1String*>(guestSerial), __func__);
    if (serial.length < 0) throw std::invalid_argument("X509_print_ex: negative serial number length");
    if (serial.length > 0 && !serial.data) throw std::invalid_argument("X509_print_ex: null serial number data");
    const std::vector<unsigned char> bytes = CopyBytes(serial.data, static_cast<std::size_t>(serial.length), __func__);
    const bool validIntType = serial.type == Asn1TagInteger || serial.type == Asn1TagNegativeInteger;
    const bool negative = serial.type == Asn1TagNegativeInteger;
    bool printScalar = false;
    std::uint64_t magnitude = 0;
    if (validIntType && bytes.size() <= sizeof(std::int64_t)) {
        for (const unsigned char byte : bytes) magnitude = (magnitude << 8) | byte;
        const std::uint64_t limit = negative
            ? (std::uint64_t{1} << 63)
            : static_cast<std::uint64_t>(std::numeric_limits<std::int64_t>::max());
        if (magnitude <= limit && !(negative && magnitude == 1)) printScalar = true;
    }
    if (printScalar) {
        std::string text(" ");
        if (negative) text += '-';
        text += Decimal(magnitude);
        text += " (";
        if (negative) text += '-';
        text += "0x";
        text += HexLower(magnitude);
        text += ")\n";
        return WriteBytes(bio, text);
    }
    std::string prefix("\n            ");
    if (negative) prefix += " (Negative)";
    if (!WriteBytes(bio, prefix)) return false;
    for (std::size_t i = 0; i < bytes.size(); ++i) {
        std::string piece = ByteHexLower(bytes[i]);
        piece.push_back(i + 1 == bytes.size() ? '\n' : ':');
        if (!WriteBytes(bio, piece)) return false;
    }
    return true;
}

bool WriteAsn1Bn(LibreSsl::Bio* bio, std::string_view label, const std::vector<unsigned char>& rawBytes,
    bool negative, int indent) {
    std::size_t firstNonZero = 0;
    while (firstNonZero < rawBytes.size() && rawBytes[firstNonZero] == 0) ++firstNonZero;
    const std::size_t byteCount = rawBytes.size() - firstNonZero;
    if (!WriteIndent(bio, indent)) return false;
    if (byteCount == 0) {
        std::string line(label);
        line += " 0\n";
        return WriteBytes(bio, line);
    }
    if (byteCount <= sizeof(std::uint64_t)) {
        std::uint64_t value = 0;
        for (std::size_t i = firstNonZero; i < rawBytes.size(); ++i) value = (value << 8) | rawBytes[i];
        std::string line(label);
        line += ' ';
        if (negative) line += '-';
        line += Decimal(value);
        line += " (";
        if (negative) line += '-';
        line += "0x";
        line += HexLower(value);
        line += ")\n";
        return WriteBytes(bio, line);
    }
    std::string header(label);
    if (negative) header += " (Negative)";
    if (!WriteBytes(bio, header)) return false;
    std::vector<unsigned char> dumped;
    if (rawBytes[firstNonZero] & 0x80) {
        dumped.reserve(byteCount + 1);
        dumped.push_back(0);
    } else {
        dumped.reserve(byteCount);
    }
    dumped.insert(dumped.end(), rawBytes.begin() + static_cast<std::ptrdiff_t>(firstNonZero), rawBytes.end());
    for (std::size_t i = 0; i < dumped.size(); ++i) {
        if (i % 15 == 0) {
            if (!WriteBytes(bio, "\n") || !WriteIndent(bio, indent + 4)) return false;
        }
        std::string piece = ByteHexLower(dumped[i]);
        if (i + 1 != dumped.size()) piece.push_back(':');
        if (!WriteBytes(bio, piece)) return false;
    }
    return WriteBytes(bio, "\n");
}

bool DecodeDerTlv(std::span<const unsigned char>& input, unsigned char expectedTag, std::span<const unsigned char>& content) {
    if (input.size() < 2 || input[0] != expectedTag) return false;
    std::size_t cursor = 1;
    const unsigned char firstLen = input[cursor++];
    std::size_t length = 0;
    if ((firstLen & 0x80) == 0) {
        length = firstLen;
    } else {
        const std::size_t lenBytes = firstLen & 0x7f;
        if (lenBytes == 0 || lenBytes > sizeof(std::size_t) || input.size() - cursor < lenBytes) return false;
        if (input[cursor] == 0) return false;
        for (std::size_t i = 0; i < lenBytes; ++i) length = (length << 8) | input[cursor++];
        if (length < 0x80) return false;
    }
    if (input.size() - cursor < length) return false;
    content = input.subspan(cursor, length);
    input = input.subspan(cursor + length);
    return true;
}

bool DecodeDerInteger(std::span<const unsigned char>& input, std::vector<unsigned char>& bytes, bool& negative) {
    std::span<const unsigned char> content;
    if (!DecodeDerTlv(input, 0x02, content) || content.empty()) return false;
    if (content.size() > 1) {
        if (content[0] == 0x00 && (content[1] & 0x80) == 0) return false;
        if (content[0] == 0xff && (content[1] & 0x80) != 0) return false;
    }
    negative = (content[0] & 0x80) != 0;
    if (!negative) {
        std::size_t start = 0;
        while (start < content.size() && content[start] == 0) ++start;
        bytes.assign(content.begin() + static_cast<std::ptrdiff_t>(start), content.end());
        return true;
    }
    bytes.assign(content.begin(), content.end());
    std::uint32_t carry = 1;
    for (std::size_t i = bytes.size(); i-- > 0;) {
        const std::uint32_t inverted = static_cast<unsigned char>(~bytes[i]) + carry;
        bytes[i] = static_cast<unsigned char>(inverted);
        carry = inverted >> 8;
    }
    if (carry != 0) bytes.insert(bytes.begin(), static_cast<unsigned char>(carry));
    std::size_t start = 0;
    while (start < bytes.size() && bytes[start] == 0) ++start;
    bytes.erase(bytes.begin(), bytes.begin() + static_cast<std::ptrdiff_t>(start));
    return true;
}

bool DecodeDerTwoIntegers(std::span<const unsigned char> der, std::vector<unsigned char>& first, bool& firstNeg,
    std::vector<unsigned char>& second, bool& secondNeg) {
    std::span<const unsigned char> seq;
    if (!DecodeDerTlv(der, 0x30, seq) || !der.empty()) return false;
    if (!DecodeDerInteger(seq, first, firstNeg)) return false;
    if (!DecodeDerInteger(seq, second, secondNeg)) return false;
    return seq.empty();
}

bool PrintSignatureDump(LibreSsl::Bio* bio, const Asn1String* guestSignature) {
    const Asn1String signature = CopyIn(guestSignature, __func__);
    if (signature.length < 0) throw std::invalid_argument("X509_print_ex: negative signature length");
    if (signature.length > 0 && !signature.data) throw std::invalid_argument("X509_print_ex: null signature data");
    const std::vector<unsigned char> bytes = CopyBytes(signature.data, static_cast<std::size_t>(signature.length), __func__);
    for (std::size_t i = 0; i < bytes.size(); ++i) {
        if (i % 18 == 0) {
            if (!WriteBytes(bio, "\n") || !WriteIndent(bio, 9)) return false;
        }
        std::string piece = ByteHexLower(bytes[i]);
        if (i + 1 != bytes.size()) piece.push_back(':');
        if (!WriteBytes(bio, piece)) return false;
    }
    return WriteBytes(bio, "\n");
}

bool PrintSignature(LibreSsl::Bio* bio, const void* guestAlg, const void* guestSig) {
    if (!guestAlg) throw std::invalid_argument("X509_print_ex: null signature algorithm");
    const X509Algor algor = CopyIn(static_cast<const X509Algor*>(guestAlg), __func__);
    if (!WriteBytes(bio, "    Signature Algorithm: ")) return false;
    HostAsn1Object algObj;
    const LibreSslAsn1Object* hostAlgObj = algObj.CopyFrom(algor.algorithm, __func__);
    if (!bio || i2a_ASN1_OBJECT_nid_postfix(bio, hostAlgObj) <= 0) return false;
    const int sigNid = hostAlgObj ? LibreSslObjectNid_nid_no_patch(hostAlgObj) : 0;
    if (sigNid == NidRsaSsaPss) NotImplemented_nid_no_patch("X509_print_ex: RSA-PSS signature parameters");
    if (IsDsaSignatureNid(sigNid)) {
        if (!guestSig) return WriteBytes(bio, "\n");
        const Asn1String sig = CopyIn(static_cast<const Asn1String*>(guestSig), __func__);
        if (sig.length < 0) throw std::invalid_argument("X509_print_ex: negative signature length");
        if (sig.length > 0 && !sig.data) throw std::invalid_argument("X509_print_ex: null signature data");
        const std::vector<unsigned char> der = CopyBytes(sig.data, static_cast<std::size_t>(sig.length), __func__);
        std::vector<unsigned char> rBytes;
        std::vector<unsigned char> sBytes;
        bool rNeg = false;
        bool sNeg = false;
        if (DecodeDerTwoIntegers(der, rBytes, rNeg, sBytes, sNeg)) {
            if (!WriteBytes(bio, "\n")) return false;
            if (!WriteAsn1Bn(bio, "r:   ", rBytes, rNeg, 9)) return false;
            return WriteAsn1Bn(bio, "s:   ", sBytes, sNeg, 9);
        }
        return PrintSignatureDump(bio, static_cast<const Asn1String*>(guestSig));
    }
    if (!guestSig) return WriteBytes(bio, "\n");
    return PrintSignatureDump(bio, static_cast<const Asn1String*>(guestSig));
}

bool ExtractBignumBytes(const void* guestBn, std::vector<unsigned char>& bytes, bool& negative, int& bits, const char* function) {
    bytes.clear();
    negative = false;
    bits = 0;
    if (!guestBn) return false;
    const BignumWords bn = CopyIn(static_cast<const BignumWords*>(guestBn), function);
    negative = bn.neg != 0;
    if (bn.top <= 0) return true;
    if (!bn.d) throw std::invalid_argument(std::string(function) + ": null bignum limb array");
    const std::size_t wordCount = static_cast<std::size_t>(bn.top);
    std::vector<std::uint64_t> words(wordCount);
    {
        PreservedErrno guard;
        if (!GuestCopyIn_nid_no_patch(words.data(), bn.d, wordCount * sizeof(std::uint64_t)))
            NotImplementedAt_nid_no_patch(function, __FILE__, __LINE__);
    }
    std::size_t top = wordCount;
    while (top > 0 && words[top - 1] == 0) --top;
    if (top == 0) return true;
    bits = static_cast<int>((top - 1) * 64 + static_cast<std::size_t>(std::bit_width(words[top - 1])));
    for (std::size_t idx = top; idx-- > 0;) {
        const std::uint64_t w = words[idx];
        for (int shift = 56; shift >= 0; shift -= 8) {
            const unsigned char byte = static_cast<unsigned char>((w >> shift) & 0xffU);
            if (!bytes.empty() || byte != 0) bytes.push_back(byte);
        }
    }
    return true;
}

int BitLengthOfBigEndian(const std::vector<unsigned char>& bytes) {
    std::size_t firstNonZero = 0;
    while (firstNonZero < bytes.size() && bytes[firstNonZero] == 0) ++firstNonZero;
    if (firstNonZero == bytes.size()) return 0;
    return static_cast<int>((bytes.size() - firstNonZero - 1) * 8 + static_cast<std::size_t>(std::bit_width(bytes[firstNonZero])));
}

bool PrintPublicKey(LibreSsl::Bio* bio, const void* guestKey) {
    if (!WriteBytes(bio, "        Subject Public Key Info:\n")) return false;
    if (!WriteBytes(bio, "            Public Key Algorithm: ")) return false;
    if (!guestKey) throw std::invalid_argument("X509_print_ex: null public key");
    const X509Pubkey pubkey = CopyIn(static_cast<const X509Pubkey*>(guestKey), __func__);
    if (!pubkey.algor) throw std::invalid_argument("X509_print_ex: null public key algorithm");
    const X509Algor algor = CopyIn(pubkey.algor, __func__);
    HostAsn1Object algObj;
    const LibreSslAsn1Object* hostAlgObj = algObj.CopyFrom(algor.algorithm, __func__);
    if (!bio || i2a_ASN1_OBJECT_nid_postfix(bio, hostAlgObj) <= 0) return false;
    if (!WriteBytes(bio, "\n")) return false;

    if (pubkey.pkey) {
        const EvpPkey evp = CopyIn(static_cast<const EvpPkey*>(pubkey.pkey), __func__);
        if (evp.type != NidRsaEncryption)
            NotImplemented_nid_no_patch("X509_print_ex: non-RSA EVP_PKEY public key printing");
        if (!evp.key) return WriteBytes(bio, "            Unable to load Public Key\n");
        const LibreSslRsa rsa = CopyIn(static_cast<const LibreSslRsa*>(evp.key), __func__);
        std::vector<unsigned char> nBytes;
        std::vector<unsigned char> eBytes;
        bool nNeg = false;
        bool eNeg = false;
        int nBits = 0;
        int eBits = 0;
        const bool hasN = ExtractBignumBytes(rsa.n, nBytes, nNeg, nBits, __func__);
        const bool hasE = ExtractBignumBytes(rsa.e, eBytes, eNeg, eBits, __func__);
        if (!WriteIndent(bio, 16)) return false;
        const std::string header = "Public-Key: (" + Decimal(static_cast<std::uint64_t>(nBits)) + " bit)\n";
        if (!WriteBytes(bio, header)) return false;
        if (hasN && !WriteAsn1Bn(bio, "Modulus:", nBytes, nNeg, 16)) return false;
        if (hasE && !WriteAsn1Bn(bio, "Exponent:", eBytes, eNeg, 16)) return false;
        return true;
    }

    const int algNid = hostAlgObj ? LibreSslObjectNid_nid_no_patch(hostAlgObj) : 0;
    if (!pubkey.publicKey) {
        if (algNid != 0 && algNid != NidRsaEncryption && algNid != NidEcPublicKey)
            NotImplemented_nid_no_patch("X509_print_ex: unsupported public key algorithm");
        return WriteBytes(bio, "            Unable to load Public Key\n");
    }
    const Asn1String spkBits = CopyIn(pubkey.publicKey, __func__);
    if (spkBits.length < 0) throw std::invalid_argument("X509_print_ex: negative public key length");
    if (spkBits.length > 0 && !spkBits.data) throw std::invalid_argument("X509_print_ex: null public key data");
    const std::vector<unsigned char> rawKey = CopyBytes(spkBits.data, static_cast<std::size_t>(spkBits.length), __func__);

    if (algNid == 0) return WriteBytes(bio, "            Unable to load Public Key\n");
    if (algNid == NidRsaEncryption) {
        std::vector<unsigned char> nBytes;
        std::vector<unsigned char> eBytes;
        bool nNeg = false;
        bool eNeg = false;
        if (!DecodeDerTwoIntegers(rawKey, nBytes, nNeg, eBytes, eNeg))
            return WriteBytes(bio, "            Unable to load Public Key\n");
        const int modBits = BitLengthOfBigEndian(nBytes);
        if (!WriteIndent(bio, 16)) return false;
        if (!WriteBytes(bio, "Public-Key: (" + Decimal(static_cast<std::uint64_t>(modBits)) + " bit)\n")) return false;
        if (!WriteAsn1Bn(bio, "Modulus:", nBytes, nNeg, 16)) return false;
        return WriteAsn1Bn(bio, "Exponent:", eBytes, eNeg, 16);
    }
    if (algNid == NidEcPublicKey) {
        if (rawKey.empty() || !algor.parameter)
            return WriteBytes(bio, "            Unable to load Public Key\n");
        const Asn1Type param = CopyIn(algor.parameter, __func__);
        if (param.type != Asn1TagObject || !param.value)
            return WriteBytes(bio, "            Unable to load Public Key\n");
        HostAsn1Object curveObj;
        const LibreSslAsn1Object* hostCurveObj = curveObj.CopyFrom(static_cast<const LibreSslAsn1Object*>(param.value), __func__);
        const int curveNid = LibreSslObjectNid_nid_no_patch(hostCurveObj);
        if (curveNid == 0) return WriteBytes(bio, "            Unable to load Public Key\n");
        const EcNamedCurve* curve = FindNamedCurve(curveNid);
        if (!curve) NotImplemented_nid_no_patch("X509_print_ex: unsupported EC named curve");
        if (!WriteIndent(bio, 16)) return false;
        if (!WriteBytes(bio, "Public-Key: (" + Decimal(static_cast<std::uint64_t>(curve->orderBits)) + " bit)\n")) return false;
        if (!WriteAsn1Bn(bio, "pub: ", rawKey, false, 16)) return false;
        if (!WriteIndent(bio, 16)) return false;
        if (!WriteBytes(bio, std::string("ASN1 OID: ") + curve->shortName + "\n")) return false;
        if (curve->nistName) {
            if (!WriteIndent(bio, 16)) return false;
            if (!WriteBytes(bio, std::string("NIST CURVE: ") + curve->nistName + "\n")) return false;
        }
        return true;
    }
    NotImplemented_nid_no_patch("X509_print_ex: unsupported public key algorithm");
}

bool PrintExtensions(LibreSsl::Bio* bio, const void* guestExtensions, std::uint64_t certFlags) {
    if (!guestExtensions) return true;
    const LibreSslStack stack = CopyIn(static_cast<const LibreSslStack*>(guestExtensions), __func__);
    if (stack.num <= 0) return true;
    if (!stack.data) throw std::invalid_argument("X509_print_ex: null extension array");
    const std::size_t count = static_cast<std::size_t>(stack.num);
    std::vector<char*> pointers(count);
    {
        PreservedErrno guard;
        if (!GuestCopyIn_nid_no_patch(pointers.data(), stack.data, count * sizeof(char*)))
            NotImplementedAt_nid_no_patch(__func__, __FILE__, __LINE__);
    }
    if (!WriteBytes(bio, "        X509v3 extensions:\n")) return false;
    for (std::size_t i = 0; i < count; ++i) {
        if (!pointers[i]) throw std::invalid_argument("X509_print_ex: null extension");
        const X509Extension ext = CopyIn(reinterpret_cast<const X509Extension*>(pointers[i]), __func__);
        HostAsn1Object hostObj;
        const LibreSslAsn1Object* objPtr = hostObj.CopyFrom(ext.object, __func__);
        Asn1String hostVal{};
        std::vector<unsigned char> valBytes;
        const Asn1String* valPtr = nullptr;
        if (ext.value) {
            hostVal = CopyIn(ext.value, __func__);
            if (hostVal.length > 0 && hostVal.data) {
                valBytes = CopyBytes(hostVal.data, static_cast<std::size_t>(hostVal.length), __func__);
                hostVal.data = valBytes.data();
            }
            valPtr = &hostVal;
        }
        X509Extension hostExt{objPtr, ext.critical, 0, valPtr};
        if (!WriteIndent(bio, 12)) return false;
        if (!bio || i2a_ASN1_OBJECT_nid_postfix(bio, objPtr) <= 0) return false;
        if (!WriteBytes(bio, ext.critical > 0 ? ": critical\n" : ": \n")) return false;
        if (!X509V3_EXT_print_nid_postfix(bio, &hostExt, certFlags, 16)) {
            if (!WriteIndent(bio, 16)) return false;
            ASN1_STRING_print_nid_postfix(bio, valPtr);
        }
        if (!WriteBytes(bio, "\n")) return false;
    }
    return true;
}

bool PrintValidity(LibreSsl::Bio* bio, const void* guestValidity) {
    if (!WriteBytes(bio, "        Validity\n")) return false;
    if (!WriteBytes(bio, "            Not Before: ")) return false;
    if (!guestValidity) throw std::invalid_argument("X509_print_ex: null validity");
    const X509Validity validity = CopyIn(static_cast<const X509Validity*>(guestValidity), __func__);
    if (!bio || !ASN1_TIME_print_nid_postfix(bio, validity.notBefore)) return false;
    if (!WriteBytes(bio, "\n            Not After : ")) return false;
    if (!ASN1_TIME_print_nid_postfix(bio, validity.notAfter)) return false;
    return WriteBytes(bio, "\n");
}

bool PrintNameSection(LibreSsl::Bio* bio, const void* guestName, std::string_view label, char mlch,
    int nmindent, std::uint64_t nameFlags) {
    std::string prefix(label);
    prefix.push_back(mlch);
    if (!WriteBytes(bio, prefix)) return false;
    HostX509Name hostName;
    const LibreSslX509Name* namePtr = hostName.CopyFrom(static_cast<const LibreSslX509Name*>(guestName), __func__);
    const int minSuccess = nameFlags == X509FlagCompat ? 1 : 0;
    if (!bio || X509_NAME_print_ex_nid_postfix(bio, namePtr, nmindent, nameFlags) < minSuccess) return false;
    return WriteBytes(bio, "\n");
}

} // namespace

extern "C" {

int APS5_VABI X509_print_ex_nid_postfix(LibreSsl::Bio* bio, const LibreSslX509* certificate,
    std::uint64_t nameFlags, std::uint64_t certFlags) {
    if (!certificate) throw std::invalid_argument("X509_print_ex: null certificate");
    const LibreSslX509 cert = CopyIn(certificate, __func__);
    if (!cert.certificateInfo) throw std::invalid_argument("X509_print_ex: null certificate info");
    const LibreSslX509CertInfo info = CopyIn(static_cast<const LibreSslX509CertInfo*>(cert.certificateInfo), __func__);

    char mlch = ' ';
    int nmindent = 0;
    if ((nameFlags & XnFlagSepMask) == XnFlagSepMultiline) {
        mlch = '\n';
        nmindent = 12;
    }
    if (nameFlags == X509FlagCompat) nmindent = 16;

    if (!(certFlags & X509FlagNoHeader)) {
        if (!WriteBytes(bio, "Certificate:\n")) return 0;
        if (!WriteBytes(bio, "    Data:\n")) return 0;
    }
    if (!(certFlags & X509FlagNoVersion)) {
        if (!PrintVersion(bio, static_cast<const Asn1String*>(info.version))) return 0;
    }
    if (!(certFlags & X509FlagNoSerial)) {
        if (!PrintSerial(bio, info.serialNumber)) return 0;
    }
    if (!(certFlags & X509FlagNoSigName)) {
        if (!PrintSignature(bio, cert.signatureAlgorithm, nullptr)) return 0;
    }
    if (!(certFlags & X509FlagNoIssuer)) {
        if (!PrintNameSection(bio, info.issuer, "        Issuer:", mlch, nmindent, nameFlags)) return 0;
    }
    if (!(certFlags & X509FlagNoValidity)) {
        if (!PrintValidity(bio, info.validity)) return 0;
    }
    if (!(certFlags & X509FlagNoSubject)) {
        if (!PrintNameSection(bio, info.subject, "        Subject:", mlch, nmindent, nameFlags)) return 0;
    }
    if (!(certFlags & X509FlagNoPubkey)) {
        if (!PrintPublicKey(bio, info.key)) return 0;
    }
    if (!(certFlags & X509FlagNoExtensions)) {
        if (!PrintExtensions(bio, info.extensions, certFlags)) return 0;
    }
    if (!(certFlags & X509FlagNoSigDump)) {
        if (!PrintSignature(bio, cert.signatureAlgorithm, cert.signature)) return 0;
    }
    static_cast<void>(X509FlagNoAux);
    return 1;
}

}
