#include "prx/libSceLibreSsl3/libSceLibreSsl3.hpp"
#include "prx/libSceLibreSsl3/include/Objects.hpp"
#include "prx/libc/include/Api.hpp"
#include "prx/libc/include/General.hpp"

#include <cerrno>
#include <charconv>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

constexpr int PositiveIntegerType = 2;
constexpr int NegativeIntegerType = 0x102;
constexpr int DsaKeyType = 116;
constexpr int SignatureIndent = 9;
constexpr std::size_t SignatureWidth = 18;
constexpr int VersionIndent = 8;
constexpr int SerialIndent = 12;

struct CertificateFields {
    const LibreSsl3CertificateInfo* certInfo;
    const LibreSsl3X509Algor* sigAlg;
    const LibreSsl3Asn1String* signature;
};

static_assert(sizeof(CertificateFields) == 24);
static_assert(sizeof(LibreSsl3Asn1String) == sizeof(Asn1String));
static_assert(sizeof(LibreSsl3X509Validity) == 2 * sizeof(void*));

template <typename Value>
Value CopyIn(const Value* source, const char* function) {
    Value value{};
    const int savedErrno = errno;
    const bool copied = GuestCopyIn_nid_no_patch(&value, source, sizeof(value));
    errno = savedErrno;
    if (!copied) NotImplementedAt_nid_no_patch(function, __FILE__, __LINE__);
    return value;
}

void CopyBytesIn(void* destination, const void* source, std::size_t bytes) {
    const int savedErrno = errno;
    const bool copied = GuestCopyIn_nid_no_patch(destination, source, bytes);
    errno = savedErrno;
    if (!copied) NotImplementedAt_nid_no_patch("X509_print", __FILE__, __LINE__);
}

template <std::size_t Size>
int WriteLiteral(LibreSsl::Bio* bio, const char (&text)[Size]) {
    return BIO_write_nid_postfix(bio, text, static_cast<int>(Size - 1));
}

int WriteText(LibreSsl::Bio* bio, const std::string& text) {
    return BIO_write_nid_postfix(bio, text.data(), static_cast<int>(text.size()));
}

std::string Decimal(std::uint64_t value) {
    char buffer[24];
    const auto end = std::to_chars(buffer, buffer + sizeof(buffer), value).ptr;
    return std::string(buffer, end);
}

std::string Hexadecimal(std::uint64_t value) {
    char buffer[24];
    const auto end = std::to_chars(buffer, buffer + sizeof(buffer), value, 16).ptr;
    return std::string(buffer, end);
}

std::string ByteHexadecimal(unsigned char value) {
    constexpr char Digits[] = "0123456789abcdef";
    std::string text;
    text += Digits[value >> 4];
    text += Digits[value & 0x0f];
    return text;
}

std::string GuestString(const char* address) {
    std::string text;
    for (std::size_t offset = 0;; ++offset) {
        char character = 0;
        CopyBytesIn(&character, address + offset, 1);
        if (character == '\0') return text;
        text += character;
    }
}

bool IsNameEntryStart(const std::string& text, std::size_t position) {
    if (text[position] != '/') return false;
    if (position + 1 >= text.size()) return false;
    if (text[position + 1] < 'A' || text[position + 1] > 'Z') return false;
    if (position + 2 >= text.size()) return false;
    if (text[position + 2] == '=') return true;
    if (text[position + 2] < 'A' || text[position + 2] > 'Z') return false;
    return position + 3 < text.size() && text[position + 3] == '=';
}

bool PrintFlatName(LibreSsl::Bio* bio, const std::string& text) {
    if (text.empty()) return true;
    std::size_t cursor = 1;
    for (std::size_t position = 1;; ++position) {
        if (IsNameEntryStart(text, position) || text[position] == '\0') {
            const int length = static_cast<int>(position - cursor);
            if (BIO_write_nid_postfix(bio, text.data() + cursor, length) != length) return false;
            cursor = position + 1;
            if (text[position] != '\0' && BIO_write_nid_postfix(bio, ", ", 2) != 2) return false;
        }
        if (text[position] == '\0') return true;
    }
}

struct GuestFree {
    void operator()(char* pointer) const {
        free_nid_postfix(pointer);
    }
};

bool PrintName(LibreSsl::Bio* bio, const void* guestName, const char* title) {
    if (WriteText(bio, title) <= 0) return false;
    const std::unique_ptr<char, GuestFree> flat(X509_NAME_oneline_nid_postfix(guestName, nullptr, 0));
    if (!flat) return false;
    if (!PrintFlatName(bio, GuestString(flat.get()))) return false;
    return WriteLiteral(bio, "\n") > 0;
}

bool PrintVersion(LibreSsl::Bio* bio, const void* guestVersion) {
    const std::int64_t version = ASN1_INTEGER_get_nid_postfix(reinterpret_cast<const Asn1String*>(guestVersion));
    std::string text(VersionIndent, ' ');
    text += "Version: ";
    text += Decimal(static_cast<std::uint64_t>(version) + 1);
    text += " (0x";
    text += Hexadecimal(static_cast<std::uint64_t>(version));
    text += ")\n";
    return WriteText(bio, text) > 0;
}

int WriteAlgorithmName(LibreSsl::Bio* bio, const void* guestObject) {
    if (!guestObject) return WriteLiteral(bio, "NULL");
    const LibreSslAsn1Object object = CopyIn(static_cast<const LibreSslAsn1Object*>(guestObject), "X509_print");
    if (!object.data) return WriteLiteral(bio, "NULL");
    const int nid = LibreSsl3::ObjectNid(&object);
    if (nid != 0) {
        const LibreSslAsn1Object* entry = LibreSsl3::ObjectByNid(nid);
        if (entry) {
            const char* name = entry->longName ? entry->longName : entry->shortName;
            if (name) return WriteText(bio, name);
        }
    }
    return i2a_ASN1_OBJECT_nid_postfix(bio, &object);
}

bool PrintSignatureDump(LibreSsl::Bio* bio, const LibreSsl3Asn1String* guestSignature) {
    const LibreSsl3Asn1String signature = CopyIn(guestSignature, "X509_print");
    if (signature.length < 0) throw std::invalid_argument("X509_print: negative signature length");
    if (signature.length > 0 && !signature.data) throw std::invalid_argument("X509_print: null signature data");
    std::vector<unsigned char> bytes(static_cast<std::size_t>(signature.length));
    if (!bytes.empty()) CopyBytesIn(bytes.data(), signature.data, bytes.size());
    for (std::size_t index = 0; index < bytes.size(); ++index) {
        if (index % SignatureWidth == 0) {
            if (WriteLiteral(bio, "\n") <= 0) return false;
            for (int step = 0; step < SignatureIndent; ++step)
                if (WriteLiteral(bio, " ") != 1) return false;
        }
        std::string pair = ByteHexadecimal(bytes[index]);
        if (index + 1 != bytes.size()) pair += ':';
        if (WriteText(bio, pair) <= 0) return false;
    }
    return WriteLiteral(bio, "\n") == 1;
}

bool PrintSignatureAlgorithm(LibreSsl::Bio* bio, const LibreSsl3X509Algor* guestAlgorithm,
    const LibreSsl3Asn1String* guestSignature) {
    if (!guestAlgorithm) throw std::invalid_argument("X509_print: null signature algorithm");
    const LibreSsl3X509Algor algorithm = CopyIn(guestAlgorithm, "X509_print");
    if (WriteLiteral(bio, "    Signature Algorithm: ") <= 0) return false;
    if (WriteAlgorithmName(bio, algorithm.algorithm) <= 0) return false;
    int signatureNid = 0;
    if (algorithm.algorithm)
        signatureNid = LibreSsl3::ObjectNid(static_cast<const LibreSslAsn1Object*>(algorithm.algorithm));
    if (signatureNid != 0) {
        int keyNid = 0;
        if (OBJ_find_sigid_algs_nid_postfix(signatureNid, nullptr, &keyNid) != 0) {
            if (keyNid == LibreSsl3PkeyRsaPss) NotImplementedAt_nid_no_patch("X509_print", __FILE__, __LINE__);
            if (keyNid == DsaKeyType && guestSignature)
                NotImplementedAt_nid_no_patch("X509_print", __FILE__, __LINE__);
        }
    }
    if (!guestSignature) return WriteLiteral(bio, "\n") > 0;
    return PrintSignatureDump(bio, guestSignature);
}

bool PrintSerial(LibreSsl::Bio* bio, const void* guestSerial) {
    if (WriteLiteral(bio, "        Serial Number:") <= 0) return false;
    if (!guestSerial) throw std::invalid_argument("X509_print: null serial number");
    const LibreSsl3Asn1String serial = CopyIn(static_cast<const LibreSsl3Asn1String*>(guestSerial), "X509_print");
    const bool negative = serial.type == NegativeIntegerType;
    bool representable = false;
    std::uint64_t magnitude = 0;
    if ((serial.type == PositiveIntegerType || negative) && serial.length >= 0 &&
        serial.length <= static_cast<int>(sizeof(std::int64_t))) {
        if (serial.length == 0) {
            representable = true;
        } else if (serial.data) {
            for (int index = 0; index < serial.length; ++index) {
                unsigned char byte = 0;
                CopyBytesIn(&byte, serial.data + index, 1);
                magnitude = (magnitude << 8) | byte;
            }
            const std::uint64_t limit = negative
                ? std::uint64_t{1} << 63
                : static_cast<std::uint64_t>(std::numeric_limits<std::int64_t>::max());
            representable = magnitude <= limit;
        }
    }
    if (representable) {
        std::string text(" ");
        if (negative) text += "-";
        text += Decimal(magnitude);
        text += " (";
        if (negative) text += "-";
        text += "0x";
        text += Hexadecimal(magnitude);
        text += ")\n";
        return WriteText(bio, text) > 0;
    }
    if (serial.length < 0) throw std::invalid_argument("X509_print: negative serial number length");
    if (serial.length > 0 && !serial.data) throw std::invalid_argument("X509_print: null serial number data");
    std::string prefix("\n");
    prefix.append(SerialIndent, ' ');
    if (serial.type == NegativeIntegerType) prefix += " (Negative)";
    if (WriteText(bio, prefix) <= 0) return false;
    for (int index = 0; index < serial.length; ++index) {
        unsigned char byte = 0;
        CopyBytesIn(&byte, serial.data + index, 1);
        std::string pair = ByteHexadecimal(byte);
        pair += index + 1 == serial.length ? '\n' : ':';
        if (WriteText(bio, pair) <= 0) return false;
    }
    return true;
}

bool PrintValidity(LibreSsl::Bio* bio, const void* guestValidity) {
    if (WriteLiteral(bio, "        Validity\n") <= 0) return false;
    if (WriteLiteral(bio, "            Not Before: ") <= 0) return false;
    if (!guestValidity) throw std::invalid_argument("X509_print: null validity");
    const LibreSsl3X509Validity validity = CopyIn(static_cast<const LibreSsl3X509Validity*>(guestValidity), "X509_print");
    if (!ASN1_TIME_print_nid_postfix(bio, static_cast<const Asn1String*>(validity.notBefore))) return false;
    if (WriteLiteral(bio, "\n            Not After : ") <= 0) return false;
    if (!ASN1_TIME_print_nid_postfix(bio, static_cast<const Asn1String*>(validity.notAfter))) return false;
    return WriteLiteral(bio, "\n") > 0;
}

}

extern "C" int APS5_VABI X509_print_nid_postfix(LibreSsl::Bio* bio, const LibreSsl3Certificate* certificate) {
    if (!bio || !certificate) return 0;
    const CertificateFields fields = CopyIn(reinterpret_cast<const CertificateFields*>(certificate), "X509_print");
    if (!fields.certInfo) throw std::invalid_argument("X509_print: null certificate info");
    const LibreSsl3CertificateInfo info = CopyIn(fields.certInfo, "X509_print");
    if (WriteLiteral(bio, "Certificate:\n") <= 0) return 0;
    if (WriteLiteral(bio, "    Data:\n") <= 0) return 0;
    if (!PrintVersion(bio, info.version)) return 0;
    if (!PrintSerial(bio, info.serialNumber)) return 0;
    if (!PrintSignatureAlgorithm(bio, fields.sigAlg, nullptr)) return 0;
    if (!PrintName(bio, info.issuer, "        Issuer: ")) return 0;
    if (!PrintValidity(bio, info.validity)) return 0;
    if (!PrintName(bio, info.subject, "        Subject: ")) return 0;
    if (!PrintSignatureAlgorithm(bio, fields.sigAlg, fields.signature)) return 0;
    return 1;
}
