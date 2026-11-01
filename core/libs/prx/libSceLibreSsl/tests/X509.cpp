#include "prx/libSceLibreSsl/libSceLibreSsl.hpp"
#include "prx/libc/include/Api.hpp"
#include "prx/libc/include/ApplicationHeap.hpp"
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <initializer_list>
#include <set>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

using Bytes = std::vector<unsigned char>;

constexpr std::uint64_t Asn1Code(int reason) {
    return (13ull << 24) | (0xfffull << 12) | static_cast<std::uint64_t>(reason);
}

void Require(bool condition, const char* message) {
    if (condition) return;
    std::fprintf(stderr, "failed: %s\n", message);
    std::abort();
}

Bytes Cat(std::initializer_list<Bytes> parts) {
    Bytes joined;
    for (const auto& part : parts) joined.insert(joined.end(), part.begin(), part.end());
    return joined;
}

Bytes Tlv(unsigned char tag, const Bytes& content) {
    Bytes encoded{tag};
    if (content.size() < 128) {
        encoded.push_back(static_cast<unsigned char>(content.size()));
    } else if (content.size() < 256) {
        encoded.push_back(0x81);
        encoded.push_back(static_cast<unsigned char>(content.size()));
    } else {
        encoded.push_back(0x82);
        encoded.push_back(static_cast<unsigned char>(content.size() >> 8));
        encoded.push_back(static_cast<unsigned char>(content.size()));
    }
    encoded.insert(encoded.end(), content.begin(), content.end());
    return encoded;
}

Bytes Text(const std::string& text) {
    return Bytes(text.begin(), text.end());
}

const Bytes CommonName{0x55, 0x04, 0x03};
const Bytes Organization{0x55, 0x04, 0x0a};
const Bytes Country{0x55, 0x04, 0x06};
const Bytes Sha256WithRsa{0x2a, 0x86, 0x48, 0x86, 0xf7, 0x0d, 0x01, 0x01, 0x0b};
const Bytes RsaEncryption{0x2a, 0x86, 0x48, 0x86, 0xf7, 0x0d, 0x01, 0x01, 0x01};

Bytes Entry(const Bytes& object, unsigned char type, const Bytes& value) {
    return Tlv(0x30, Cat({Tlv(0x06, object), Tlv(type, value)}));
}

Bytes Algorithm() {
    return Tlv(0x30, Cat({Tlv(0x06, Sha256WithRsa), Tlv(0x05, {})}));
}

Bytes Issuer() {
    return Tlv(0x30, Cat({Tlv(0x31, Entry(CommonName, 0x13, Text("  Test   CA "))),
        Tlv(0x31, Cat({Entry(Organization, 0x0c, Text("Org")), Entry(Country, 0x13, Text("FR"))}))}));
}

Bytes Subject(const std::string& name) {
    return Tlv(0x30, Tlv(0x31, Entry(CommonName, 0x0c, Text(name))));
}

Bytes Validity() {
    return Tlv(0x30, Cat({Tlv(0x17, Text("200101000000Z")), Tlv(0x18, Text("20500101000000Z"))}));
}

Bytes PublicKey() {
    return Tlv(0x30, Cat({Tlv(0x30, Cat({Tlv(0x06, RsaEncryption), Tlv(0x05, {})})), Tlv(0x03, {0x00, 0x30, 0x03, 0x02, 0x01, 0x05})}));
}

Bytes Extensions() {
    return Tlv(0xa3, Tlv(0x30, Cat({Tlv(0x30, Cat({Tlv(0x06, {0x55, 0x1d, 0x13}), Tlv(0x01, {0xff}), Tlv(0x04, {0x30, 0x03, 0x01, 0x01, 0xff})})),
        Tlv(0x30, Cat({Tlv(0x06, {0x55, 0x1d, 0x0f}), Tlv(0x04, {0x03, 0x02, 0x05, 0xa0})}))})));
}

Bytes Signed(const Bytes& tbs) {
    return Tlv(0x30, Cat({tbs, Algorithm(), Tlv(0x03, {0x03, 0xde, 0xad, 0xbf})}));
}

Bytes Info() {
    return Tlv(0x30, Cat({Tlv(0xa0, Tlv(0x02, {0x02})), Tlv(0x02, {0x00, 0x8f, 0x01}), Algorithm(), Issuer(), Validity(), Subject("Leaf"), PublicKey(), Extensions()}));
}

Bytes VersionOne(const std::string& name) {
    return Tlv(0x30, Cat({Tlv(0x02, {0x01}), Algorithm(), Issuer(), Validity(), Subject(name), PublicKey()}));
}

bool Holds(const Asn1String* string, int type, const Bytes& data, std::int64_t flags) {
    if (!string || string->type != type || string->flags != flags || string->length != static_cast<int>(data.size())) return false;
    return data.empty() || std::memcmp(string->data, data.data(), data.size()) == 0;
}

bool Holds(const LibreSslAsn1Object* object, const Bytes& data) {
    return object && object->flags == 9 && object->nid == 0 && !object->shortName && !object->longName &&
        object->length == static_cast<int>(data.size()) && std::memcmp(object->data, data.data(), data.size()) == 0;
}

LibreSslX509NameEntry* EntryAt(const LibreSslX509Name* name, int index) {
    return reinterpret_cast<LibreSslX509NameEntry*>(name->entries->data[index]);
}

const LibreSslX509Extension* ExtensionAt(const LibreSslStack* extensions, int index) {
    return reinterpret_cast<const LibreSslX509Extension*>(extensions->data[index]);
}

LibreSslX509* Decode(const Bytes& bytes) {
    const unsigned char* in = bytes.data();
    LibreSslX509* certificate = d2i_X509_nid_postfix(nullptr, &in, static_cast<std::int64_t>(bytes.size()));
    Require(certificate && in == bytes.data() + bytes.size(), "a certificate decodes and consumes its encoding");
    return certificate;
}

void Basics() {
    const Bytes info = Info();
    const Bytes encoded = Signed(info);
    const unsigned char* in = encoded.data();
    LibreSslX509* certificate = nullptr;
    LibreSslX509* result = d2i_X509_nid_postfix(&certificate, &in, static_cast<std::int64_t>(encoded.size()));
    Require(result && result == certificate && in == encoded.data() + encoded.size(), "the certificate is stored through the out pointer and the input advances");
    Require(certificate->references == 1 && certificate->valid == 0 && certificate->pathLength == -1 && certificate->extensionFlags == 0, "a new certificate starts with one reference");
    Require(certificate->name && std::strcmp(certificate->name, "/CN=Leaf") == 0, "the cached name is the one-line subject");
    const auto* tbs = certificate->certificateInfo;
    Require(Holds(tbs->version, 2, {0x02}, 0) && Holds(tbs->serialNumber, 2, {0x8f, 0x01}, 0), "the version and the serial drop the sign padding");
    Require(Holds(tbs->signature->algorithm, Sha256WithRsa) && tbs->signature->parameter && tbs->signature->parameter->type == 5 && !tbs->signature->parameter->value,
        "an algorithm keeps its identifier and a NULL parameter");
    const Bytes issuer = Issuer();
    const auto* name = tbs->issuer;
    Require(name->entries->num == 3 && name->modified == 0, "the issuer has three entries");
    Require(EntryAt(name, 0)->set == 0 && EntryAt(name, 1)->set == 1 && EntryAt(name, 2)->set == 1, "entries record their RDN index");
    Require(Holds(static_cast<const Asn1String*>(EntryAt(name, 0)->value), 0x13, Text("  Test   CA "), 0x40) &&
            Holds(static_cast<const LibreSslAsn1Object*>(EntryAt(name, 1)->object), Organization),
        "entry values keep their type, bytes and the multi-string flag");
    const auto* bytes = static_cast<const std::uint64_t*>(name->bytes);
    const auto* cached = reinterpret_cast<const unsigned char* const*>(static_cast<const char*>(name->bytes) + 8);
    Require(bytes[0] == issuer.size() && bytes[2] == (issuer.size() + 3) / 3 * 4 && std::memcmp(*cached, issuer.data(), issuer.size()) == 0, "the issuer keeps its DER encoding");
    const Bytes canonical = Cat({Tlv(0x31, Entry(CommonName, 0x0c, Text("test ca"))), Tlv(0x31, Cat({Entry(Country, 0x0c, Text("fr")), Entry(Organization, 0x0c, Text("org"))}))});
    Require(name->canonEnclen == static_cast<int>(canonical.size()) && std::memcmp(name->canonEnc, canonical.data(), canonical.size()) == 0,
        "the canonical encoding is lower case UTF8String with collapsed spaces and sorted sets");
    Require(Holds(tbs->validity->notBefore, 0x17, Text("200101000000Z"), 0x40) && Holds(tbs->validity->notAfter, 0x18, Text("20500101000000Z"), 0x40), "both times decode");
    Require(Holds(tbs->key->algorithm->algorithm, RsaEncryption) && Holds(tbs->key->publicKey, 3, {0x30, 0x03, 0x02, 0x01, 0x05}, 0x08) && !tbs->key->key,
        "the public key stays encoded");
    Require(!tbs->issuerUid && !tbs->subjectUid, "absent unique identifiers are null");
    Require(tbs->extensions && tbs->extensions->num == 2, "both extensions decode");
    Require(ExtensionAt(tbs->extensions, 0)->critical == 0xff && Holds(ExtensionAt(tbs->extensions, 0)->value, 4, {0x30, 0x03, 0x01, 0x01, 0xff}, 0), "a critical extension");
    Require(ExtensionAt(tbs->extensions, 1)->critical == -1 && Holds(ExtensionAt(tbs->extensions, 1)->object, {0x55, 0x1d, 0x0f}), "an extension without a critical flag");
    Require(tbs->encoding.length == static_cast<std::int64_t>(info.size()) && tbs->encoding.modified == 0 && std::memcmp(tbs->encoding.data, info.data(), info.size()) == 0,
        "the signed part keeps its encoding");
    Require(Holds(certificate->signature, 3, {0xde, 0xad, 0xb8}, 0x0b), "the signature keeps its unused bit count and clears the unused bits");
    X509_free_nid_postfix(certificate);
    X509_free_nid_postfix(nullptr);
}

void Reuse() {
    LibreSslX509* certificate = Decode(Signed(Info()));
    LibreSslX509* original = certificate;
    const Bytes second = Signed(VersionOne("Other"));
    const unsigned char* in = second.data();
    Require(d2i_X509_nid_postfix(&certificate, &in, static_cast<std::int64_t>(second.size())) == original && certificate == original,
        "decoding into a certificate reuses it");
    Require(!certificate->certificateInfo->version && !certificate->certificateInfo->extensions && std::strcmp(certificate->name, "/CN=Other") == 0,
        "absent optional fields are cleared and the name follows the new subject");
    const Bytes broken{0x30, 0x03, 0x02, 0x01};
    in = broken.data();
    Require(!d2i_X509_nid_postfix(&certificate, &in, static_cast<std::int64_t>(broken.size())) && !certificate && in == broken.data(),
        "a failed decode frees the reused certificate");
    ERR_clear_error_nid_postfix();
}

void References() {
    LibreSslX509* certificate = Decode(Signed(Info()));
    certificate->references = 2;
    X509_free_nid_postfix(certificate);
    Require(certificate->references == 1 && std::strcmp(certificate->name, "/CN=Leaf") == 0, "freeing a shared certificate only drops a reference");
    X509_free_nid_postfix(certificate);
}

void Ber() {
    const Bytes subject = Tlv(0x30, Tlv(0x31, Tlv(0x30, Cat({Tlv(0x06, CommonName), {0x2c, 0x80, 0x0c, 0x02, 'L', 'e', 0x0c, 0x02, 'a', 'f', 0x00, 0x00}}))));
    const Bytes extensions = Tlv(0xa3, Tlv(0x30, Tlv(0x30, Cat({Tlv(0x06, {0x55, 0x1d, 0x0e}), {0x24, 0x08, 0x04, 0x01, 0xaa, 0x24, 0x03, 0x04, 0x01, 0xbb}}))));
    const Bytes tbs = Cat({{0x30, 0x80}, Tlv(0x02, {0x05}), Algorithm(), Issuer(), Validity(), subject, PublicKey(), extensions, {0x00, 0x00}});
    const Bytes encoded = Cat({{0x30, 0x80}, tbs, Algorithm(), Tlv(0x03, {0x00}), {0x00, 0x00}});
    LibreSslX509* certificate = Decode(encoded);
    const auto* info = certificate->certificateInfo;
    Require(std::strcmp(certificate->name, "/CN=Leaf") == 0 && Holds(static_cast<const Asn1String*>(EntryAt(info->subject, 0)->value), 0x0c, Text("Leaf"), 0x40),
        "a constructed string is joined");
    Require(info->extensions->num == 1 && Holds(ExtensionAt(info->extensions, 0)->value, 4, {0xaa, 0xbb}, 0), "nested constructed octet strings are joined");
    Require(info->encoding.length == static_cast<std::int64_t>(tbs.size()) && std::memcmp(info->encoding.data, tbs.data(), tbs.size()) == 0,
        "an indefinite length part keeps its end of contents octets");
    Require(Holds(certificate->signature, 3, {}, 0x08), "an empty signature has no data");
    X509_free_nid_postfix(certificate);
}

void Fails(const Bytes& encoded, std::initializer_list<int> reasons, const char* message) {
    ERR_clear_error_nid_postfix();
    const unsigned char* in = encoded.data();
    Require(!d2i_X509_nid_postfix(nullptr, &in, static_cast<std::int64_t>(encoded.size())) && in == encoded.data(), message);
    for (const int reason : reasons) Require(ERR_get_error_nid_postfix() == Asn1Code(reason), message);
    Require(ERR_get_error_nid_postfix() == 0, message);
}

void Errors() {
    Fails({}, {123, 102, 58}, "empty input");
    const Bytes encoded = Signed(Info());
    Fails(Bytes(encoded.begin(), encoded.end() - 1), {155, 102, 58}, "a truncated certificate");
    Fails(Signed(Tlv(0x30, Cat({Tlv(0x02, {0x01}), Algorithm(), Issuer(), Validity(), Subject("Leaf")}))), {121, 58}, "a missing public key");
    Fails(Signed(Tlv(0x30, Cat({Tlv(0x02, {0x01}), Algorithm(), Issuer(), Validity(), Subject("Leaf"), PublicKey(), Tlv(0x05, {})}))), {168, 58, 58}, "trailing data in the signed part");
    Fails(Signed(Tlv(0x30, Cat({Tlv(0x02, {0x01}), Algorithm(), Tlv(0x30, Tlv(0x31, Entry(CommonName, 0x02, {0x01}))), Validity(), Subject("Leaf"), PublicKey()}))),
        {140, 58, 58, 58, 58, 58}, "a name value with a tag outside the printable types");
    Fails(Signed(Tlv(0x30, Cat({Tlv(0x02, {0x01}), Algorithm(), Tlv(0x30, Tlv(0x31, Entry(CommonName, 0x1e, {0x00}))), Validity(), Subject("Leaf"), PublicKey()}))),
        {214, 58, 58, 58, 58, 58}, "an odd length BMPString");
    Fails(Signed(Tlv(0x30, Cat({Tlv(0x02, {0x01}), Algorithm(), Tlv(0x30, Tlv(0x31, Entry(CommonName, 0x0c, {0xff}))), Validity(), Subject("Leaf"), PublicKey()}))),
        {134, 58, 58, 58}, "an invalid UTF8String name value");
}

std::set<void*> live;
int failAt = 0;
int calls = 0;

const Bytes SubjectAltName{0x55, 0x1d, 0x11};
const Bytes BasicConstraintsObject{0x55, 0x1d, 0x13};
const Bytes KeyUsageObject{0x55, 0x1d, 0x0f};
const Bytes ExtendedKeyUsageObject{0x55, 0x1d, 0x25};
const Bytes AuthorityKeyIdObject{0x55, 0x1d, 0x23};
const Bytes CrlDistributionPoints{0x55, 0x1d, 0x1f};
const Bytes CertificatePolicies{0x55, 0x1d, 0x20};
const Bytes DirectoryAttributes{0x55, 0x1d, 0x09};
const Bytes AuthorityInfoAccess{0x2b, 0x06, 0x01, 0x05, 0x05, 0x07, 0x01, 0x01};
const Bytes OcspNonce{0x2b, 0x06, 0x01, 0x05, 0x05, 0x07, 0x30, 0x01, 0x02};
const Bytes OcspMethod{0x2b, 0x06, 0x01, 0x05, 0x05, 0x07, 0x30, 0x01};
const Bytes ServerAuth{0x2b, 0x06, 0x01, 0x05, 0x05, 0x07, 0x03, 0x01};
const Bytes ClientAuth{0x2b, 0x06, 0x01, 0x05, 0x05, 0x07, 0x03, 0x02};

Bytes Extension(const Bytes& object, int critical, const Bytes& value) {
    Bytes fields = Tlv(0x06, object);
    if (critical >= 0) fields = Cat({fields, Tlv(0x01, {static_cast<unsigned char>(critical ? 0xff : 0x00)})});
    return Tlv(0x30, Cat({fields, Tlv(0x04, value)}));
}

LibreSslX509* WithExtensions(const Bytes& extensions) {
    return Decode(Signed(Tlv(0x30, Cat({Tlv(0xa0, Tlv(0x02, {0x02})), Tlv(0x02, {0x01}), Algorithm(), Issuer(), Validity(), Subject("Ext"), PublicKey(),
        Tlv(0xa3, Tlv(0x30, extensions))}))));
}

const LibreSslGeneralName* NameAt(const LibreSslStack* names, int index) {
    return reinterpret_cast<const LibreSslGeneralName*>(names->data[index]);
}

void* Lookup(LibreSslX509* certificate, int nid, int* critical, int* index) {
    return X509_get_ext_d2i_nid_postfix(certificate, nid, critical, index);
}

void GeneralNames() {
    const Bytes otherName = Tlv(0xa0, Cat({Tlv(0x06, {0x2b, 0x06, 0x01, 0x04, 0x01, 0x82, 0x37, 0x14, 0x02, 0x03}), Tlv(0xa0, Tlv(0x0c, Text("upn")))}));
    const Bytes names = Tlv(0x30, Cat({otherName, Tlv(0x81, Text("a@b.c")), Tlv(0x82, Text("example.com")), Tlv(0xa3, Tlv(0x02, {0x01})), Tlv(0xa4, Subject("Dir")),
        Tlv(0xa5, {}), Tlv(0x86, Text("https://x/")), Tlv(0x87, {127, 0, 0, 1}), Tlv(0x88, {0x2a, 0x03})}));
    LibreSslX509* certificate = WithExtensions(Extension(SubjectAltName, 1, names));
    int critical = 7;
    const auto* decoded = static_cast<const LibreSslStack*>(Lookup(certificate, 85, &critical, nullptr));
    Require(decoded && decoded->num == 9 && critical == 1, "subjectAltName decodes every general name and reports the critical flag");
    for (int index = 0; index < 9; ++index) Require(NameAt(decoded, index)->type == index, "each general name records its CHOICE selector");
    const auto* other = static_cast<const LibreSslOtherName*>(NameAt(decoded, 0)->value);
    Require(Holds(other->typeId, {0x2b, 0x06, 0x01, 0x04, 0x01, 0x82, 0x37, 0x14, 0x02, 0x03}) && other->value->type == 12 &&
        Holds(static_cast<const Asn1String*>(other->value->value), 12, Text("upn"), 0), "an otherName keeps its type and explicitly tagged value");
    Require(Holds(static_cast<const Asn1String*>(NameAt(decoded, 1)->value), 22, Text("a@b.c"), 0), "an rfc822Name is an IA5String");
    Require(Holds(static_cast<const Asn1String*>(NameAt(decoded, 2)->value), 22, Text("example.com"), 0), "a dNSName is an IA5String");
    Require(Holds(static_cast<const Asn1String*>(NameAt(decoded, 3)->value), 16, Tlv(0xa3, Tlv(0x02, {0x01})), 0), "an x400Address keeps its whole encoding");
    const auto* directory = static_cast<const LibreSslX509Name*>(NameAt(decoded, 4)->value);
    Require(directory->entries->num == 1 && directory->modified == 0, "a directoryName is a decoded X509_NAME");
    const auto* party = static_cast<const LibreSslEdiPartyName*>(NameAt(decoded, 5)->value);
    Require(!party->nameAssigner && !party->partyName, "an empty ediPartyName has no fields");
    Require(Holds(static_cast<const Asn1String*>(NameAt(decoded, 6)->value), 22, Text("https://x/"), 0), "a URI is an IA5String");
    Require(Holds(static_cast<const Asn1String*>(NameAt(decoded, 7)->value), 4, {127, 0, 0, 1}, 0), "an iPAddress is an OCTET STRING");
    Require(Holds(static_cast<const LibreSslAsn1Object*>(NameAt(decoded, 8)->value), {0x2a, 0x03}), "a registeredID is an object");
    X509_free_nid_postfix(certificate);
}

void Constraints() {
    const Bytes usages = Tlv(0x30, Cat({Tlv(0x06, ServerAuth), Tlv(0x06, ClientAuth)}));
    const Bytes keyId = Tlv(0x30, Cat({Tlv(0x80, {1, 2, 3}), Tlv(0xa1, Tlv(0xa4, Issuer())), Tlv(0x82, {0x05})}));
    const Bytes points = Tlv(0x30, Tlv(0x30, Tlv(0xa0, Tlv(0xa0, Tlv(0x86, Text("http://crl/"))))));
    const Bytes access = Tlv(0x30, Tlv(0x30, Cat({Tlv(0x06, OcspMethod), Tlv(0x86, Text("http://ocsp/"))})));
    LibreSslX509* certificate = WithExtensions(Cat({Extension(BasicConstraintsObject, 0, Tlv(0x30, Cat({Tlv(0x01, {0xff}), Tlv(0x02, {0x03})}))),
        Extension(KeyUsageObject, -1, {0x03, 0x02, 0x05, 0xa0}), Extension(ExtendedKeyUsageObject, -1, usages), Extension(AuthorityKeyIdObject, -1, keyId),
        Extension(CrlDistributionPoints, -1, points), Extension(AuthorityInfoAccess, -1, access), Extension(OcspNonce, -1, {0x04, 0x02, 0xaa, 0xbb})}));
    int critical = 7;
    const auto* basic = static_cast<const LibreSslBasicConstraints*>(Lookup(certificate, 87, &critical, nullptr));
    Require(basic && critical == 0 && basic->ca == 0xff && Holds(basic->pathLength, 2, {0x03}, 0), "basicConstraints keeps the boolean byte and the path length");
    Require(Holds(static_cast<const Asn1String*>(Lookup(certificate, 83, &critical, nullptr)), 3, {0xa0}, 0x08 | 5) && critical == 0, "keyUsage is a BIT STRING");
    const auto* extended = static_cast<const LibreSslStack*>(Lookup(certificate, 126, nullptr, nullptr));
    Require(extended && extended->num == 2 && Holds(reinterpret_cast<const LibreSslAsn1Object*>(extended->data[0]), ServerAuth) &&
        Holds(reinterpret_cast<const LibreSslAsn1Object*>(extended->data[1]), ClientAuth), "extendedKeyUsage is a stack of objects");
    const auto* authority = static_cast<const LibreSslAuthorityKeyId*>(Lookup(certificate, 90, nullptr, nullptr));
    Require(authority && Holds(authority->keyId, 4, {1, 2, 3}, 0) && authority->issuer->num == 1 && NameAt(authority->issuer, 0)->type == 4 &&
        Holds(authority->serial, 2, {0x05}, 0), "authorityKeyIdentifier decodes its implicitly tagged fields");
    const auto* distribution = static_cast<const LibreSslStack*>(Lookup(certificate, 103, nullptr, nullptr));
    const auto* point = distribution ? reinterpret_cast<const LibreSslDistPoint*>(distribution->data[0]) : nullptr;
    Require(point && distribution->num == 1 && point->distPoint->type == 0 && !point->distPoint->dpName && point->distPoint->name->num == 1 &&
        Holds(static_cast<const Asn1String*>(NameAt(point->distPoint->name, 0)->value), 22, Text("http://crl/"), 0) && !point->reasons && !point->crlIssuer && point->dpReasons == 0,
        "a distribution point holds its full name");
    const auto* information = static_cast<const LibreSslStack*>(Lookup(certificate, 177, nullptr, nullptr));
    const auto* description = information ? reinterpret_cast<const LibreSslAccessDescription*>(information->data[0]) : nullptr;
    Require(description && Holds(description->method, OcspMethod) && description->location->type == 6, "authorityInfoAccess pairs a method with a location");
    Require(Holds(static_cast<const Asn1String*>(Lookup(certificate, 366, nullptr, nullptr)), 4, {0x04, 0x02, 0xaa, 0xbb}, 0), "an OCSP nonce copies the raw value");
    X509_free_nid_postfix(certificate);

    certificate = WithExtensions(Extension(BasicConstraintsObject, -1, Tlv(0x30, {})));
    basic = static_cast<const LibreSslBasicConstraints*>(Lookup(certificate, 87, nullptr, nullptr));
    Require(basic && basic->ca == 0 && !basic->pathLength, "an empty basicConstraints defaults ca to false");
    X509_free_nid_postfix(certificate);
}

void Positions() {
    const Bytes first = Extension(SubjectAltName, 0, Tlv(0x30, Tlv(0x82, Text("one"))));
    const Bytes second = Extension(SubjectAltName, 1, Tlv(0x30, Tlv(0x82, Text("two"))));
    LibreSslX509* certificate = WithExtensions(Cat({first, Extension(DirectoryAttributes, 1, Tlv(0x30, {})), second}));
    int critical = 7;
    Require(!Lookup(certificate, 85, &critical, nullptr) && critical == -2, "a repeated extension without an index pointer reports -2");
    int index = -1;
    const auto* names = static_cast<const LibreSslStack*>(Lookup(certificate, 85, &critical, &index));
    Require(names && index == 0 && critical == 0 && Holds(static_cast<const Asn1String*>(NameAt(names, 0)->value), 22, Text("one"), 0), "the index finds the first occurrence");
    names = static_cast<const LibreSslStack*>(Lookup(certificate, 85, &critical, &index));
    Require(names && index == 2 && critical == 1 && Holds(static_cast<const Asn1String*>(NameAt(names, 0)->value), 22, Text("two"), 0), "the index resumes after the last occurrence");
    Require(!Lookup(certificate, 85, &critical, &index) && index == -1 && critical == -1, "past the last occurrence the index and flag are -1");
    index = -40;
    Require(Lookup(certificate, 85, nullptr, &index) && index == 0, "a negative index starts at the beginning");
    index = 7;
    Require(!Lookup(certificate, 769, &critical, &index) && index == -1 && critical == -1, "an index past the end finds nothing");
    index = -1;
    ERR_clear_error_nid_postfix();
    Require(!Lookup(certificate, 769, &critical, &index) && index == 1 && critical == 1 && ERR_get_error_nid_postfix() == 0,
        "an extension without a decoder returns NULL with its position and flag and queues no error");
    Require(!Lookup(certificate, 13, &critical, &index) && index == -1 && critical == -1, "a NID that is absent gives -1");
    X509_free_nid_postfix(certificate);

    certificate = Decode(Signed(VersionOne("One")));
    critical = 7;
    index = 3;
    Require(!Lookup(certificate, 85, &critical, &index) && index == -1 && critical == -1, "a certificate without extensions gives -1");
    X509_free_nid_postfix(certificate);
}

void ExtensionFailures() {
    LibreSslX509* certificate = WithExtensions(Cat({Extension(SubjectAltName, -1, Tlv(0x30, Tlv(0x89, {0x01}))), Extension(CertificatePolicies, -1, Tlv(0x30, {}))}));
    ERR_clear_error_nid_postfix();
    int critical = 7;
    Require(!Lookup(certificate, 85, &critical, nullptr) && critical == 0, "a general name with an unknown tag fails");
    Require(ERR_get_error_nid_postfix() == Asn1Code(143), "an unknown general name reports ASN1_R_NO_MATCHING_CHOICE_TYPE first");
    ERR_clear_error_nid_postfix();
    bool threw = false;
    try {
        Lookup(certificate, 89, nullptr, nullptr);
    } catch (...) {
        threw = true;
    }
    Require(threw, "certificatePolicies throws instead of decoding qualifiers");
    threw = false;
    try {
        Lookup(nullptr, 85, nullptr, nullptr);
    } catch (const std::invalid_argument&) {
        threw = true;
    }
    Require(threw, "a null certificate throws");
    X509_free_nid_postfix(certificate);
}

void* Track(void* pointer) {
    if (pointer) live.insert(pointer);
    return pointer;
}

void* APS5_VABI Allocate_nid_no_patch(std::size_t size) {
    if (++calls == failAt) return nullptr;
    return Track(std::malloc(size));
}

void* APS5_VABI Calloc_nid_no_patch(std::size_t count, std::size_t size) {
    if (++calls == failAt) return nullptr;
    return Track(std::calloc(count, size));
}

void APS5_VABI Free_nid_no_patch(void* pointer) {
    if (pointer) Require(live.erase(pointer) == 1, "only live allocations are freed");
    std::free(pointer);
}

void* APS5_VABI Reallocate_nid_no_patch(void* pointer, std::size_t size) {
    if (++calls == failAt) return nullptr;
    if (pointer) Require(live.erase(pointer) == 1, "only live allocations are reallocated");
    return Track(std::realloc(pointer, size));
}

void* APS5_VABI Align_nid_no_patch(std::size_t, std::size_t) {
    std::abort();
}

void* APS5_VABI Realign_nid_no_patch(void*, std::size_t, std::size_t) {
    std::abort();
}

int APS5_VABI PosixAlign_nid_no_patch(void**, std::size_t, std::size_t) {
    std::abort();
}

void Allocator() {
    std::array<void*, 10> api{
        reinterpret_cast<void*>(Allocate_nid_no_patch), reinterpret_cast<void*>(Free_nid_no_patch), reinterpret_cast<void*>(Calloc_nid_no_patch),
        reinterpret_cast<void*>(Reallocate_nid_no_patch), reinterpret_cast<void*>(Align_nid_no_patch), reinterpret_cast<void*>(Realign_nid_no_patch),
        reinterpret_cast<void*>(PosixAlign_nid_no_patch)};
    ApplicationHeapRegister_nid_no_patch(api.data());
    const Bytes encoded = Signed(Info());
    int failures = 0;
    for (int failing = 1;; ++failing) {
        ERR_clear_error_nid_postfix();
        calls = 0;
        failAt = failing;
        const unsigned char* in = encoded.data();
        LibreSslX509* certificate = d2i_X509_nid_postfix(nullptr, &in, static_cast<std::int64_t>(encoded.size()));
        const bool exhausted = calls < failing;
        const bool decoded = certificate != nullptr;
        if (certificate) {
            Require(in == encoded.data() + encoded.size(), "a successful decode consumes the input");
            X509_free_nid_postfix(certificate);
        } else {
            ++failures;
            Require(in == encoded.data() && ERR_peek_error_nid_postfix() != 0, "a failed allocation fails the decode with an error");
        }
        Require(live.empty(), "nothing leaks");
        if (exhausted) {
            Require(decoded, "the run without a failure succeeds");
            break;
        }
    }
    failAt = 0;
    Require(failures > 50, "most allocations are checked");
}

} // namespace

int main(int argc, char**) {
    if (argc > 1) {
        Allocator();
        return 0;
    }
    std::array<void*, 10> api{};
    ApplicationHeapRegister_nid_no_patch(api.data());
    Basics();
    Reuse();
    References();
    Ber();
    Errors();
    GeneralNames();
    Constraints();
    Positions();
    ExtensionFailures();
}
