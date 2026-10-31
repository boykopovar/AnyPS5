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
}
