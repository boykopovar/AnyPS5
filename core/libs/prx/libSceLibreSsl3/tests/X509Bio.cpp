#include "prx/libSceLibreSsl3/libSceLibreSsl3.hpp"
#include "prx/libSceLibreSsl/include/Error.hpp"
#include "prx/libc/include/ApplicationHeap.hpp"

#include <algorithm>
#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <initializer_list>
#include <string>
#include <type_traits>
#include <vector>

namespace {

using Bytes = std::vector<unsigned char>;

constexpr std::uint64_t ErrorCode(int library, int reason) {
    return (static_cast<std::uint64_t>(library) << 24) | (0xfffull << 12) | static_cast<std::uint64_t>(reason);
}

struct Memory {
    Bytes bytes;
    std::size_t offset{};
    int maximum{};
    int reads{};
    int before{};
    int after{};
};

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
const Bytes Sha256WithRsa{0x2a, 0x86, 0x48, 0x86, 0xf7, 0x0d, 0x01, 0x01, 0x0b};
const Bytes RsaEncryption{0x2a, 0x86, 0x48, 0x86, 0xf7, 0x0d, 0x01, 0x01, 0x01};

Bytes Entry(const Bytes& object, const Bytes& value) {
    return Tlv(0x30, Cat({Tlv(0x06, object), Tlv(0x0c, value)}));
}

Bytes Algorithm() {
    return Tlv(0x30, Cat({Tlv(0x06, Sha256WithRsa), Tlv(0x05, {})}));
}

Bytes Name(const std::string& name) {
    return Tlv(0x30, Tlv(0x31, Entry(CommonName, Text(name))));
}

Bytes Validity() {
    return Tlv(0x30, Cat({Tlv(0x17, Text("200101000000Z")), Tlv(0x18, Text("20500101000000Z"))}));
}

Bytes PublicKey() {
    return Tlv(0x30, Cat({Tlv(0x30, Cat({Tlv(0x06, RsaEncryption), Tlv(0x05, {})})),
        Tlv(0x03, {0x00, 0x30, 0x03, 0x02, 0x01, 0x05})}));
}

Bytes Info(const std::string& name) {
    return Tlv(0x30, Cat({Tlv(0x02, {0x01}), Algorithm(), Name("Issuer"), Validity(), Name(name), PublicKey()}));
}

Bytes Certificate(const std::string& name) {
    return Tlv(0x30, Cat({Info(name), Algorithm(), Tlv(0x03, {0x00})}));
}

Bytes IndefiniteCertificate(const std::string& name) {
    const Bytes info = Cat({{0x30, 0x80}, Tlv(0x02, {0x01}), Algorithm(), Name("Issuer"), Validity(),
        Name(name), PublicKey(), {0x00, 0x00}});
    return Cat({{0x30, 0x80}, info, Algorithm(), Tlv(0x03, {0x00}), {0x00, 0x00}});
}

int APS5_VABI Read(LibreSsl::Bio* bio, char* destination, int length) {
    auto& memory = *static_cast<Memory*>(bio->data);
    ++memory.reads;
    if (memory.offset == memory.bytes.size()) return 0;
    std::size_t count = std::min<std::size_t>(memory.bytes.size() - memory.offset, static_cast<std::size_t>(length));
    if (memory.maximum > 0) count = std::min(count, static_cast<std::size_t>(memory.maximum));
    std::memcpy(destination, memory.bytes.data() + memory.offset, count);
    memory.offset += count;
    return static_cast<int>(count);
}

std::int64_t APS5_VABI Callback(LibreSsl::Bio* bio, int operation, const char*, int, std::int64_t, std::int64_t result) {
    auto& memory = *static_cast<Memory*>(bio->data);
    if (operation == 2) ++memory.before;
    if (operation == 0x82) ++memory.after;
    return result;
}

LibreSsl::BioMethod Method() {
    LibreSsl::BioMethod method{};
    method.read = Read;
    return method;
}

LibreSsl::Bio Bio(Memory& memory, const LibreSsl::BioMethod& method, bool callback = false) {
    LibreSsl::Bio bio{};
    bio.method = &method;
    bio.callback = callback ? Callback : nullptr;
    bio.initialized = 1;
    bio.data = &memory;
    return bio;
}

void DefiniteAndCursor() {
    Bytes encoded = Certificate("Leaf");
    Memory memory{Cat({encoded, {0xde, 0xad}})};
    const auto method = Method();
    auto bio = Bio(memory, method, true);
    LibreSslX509* certificate = d2i_X509_bio_nid_postfix(&bio, nullptr);
    Require(certificate && std::strcmp(certificate->name, "/CN=Leaf") == 0, "a definite certificate decodes");
    Require(memory.offset == encoded.size() && bio.bytesRead == encoded.size(), "the BIO stops after the certificate");
    Require(memory.before == memory.after && memory.before == memory.reads, "BIO callbacks surround every read");
    X509_free_nid_postfix(certificate);
}

void Reuse() {
    const auto method = Method();
    Memory first{Certificate("First")};
    auto firstBio = Bio(first, method);
    LibreSslX509* certificate = nullptr;
    Require(d2i_X509_bio_nid_postfix(&firstBio, &certificate) == certificate && certificate, "the output receives a certificate");
    LibreSslX509* original = certificate;
    Memory second{Certificate("Second")};
    auto secondBio = Bio(second, method);
    Require(d2i_X509_bio_nid_postfix(&secondBio, &certificate) == original && certificate == original,
        "an existing certificate is reused");
    Require(std::strcmp(certificate->name, "/CN=Second") == 0, "the reused certificate is replaced");
    X509_free_nid_postfix(certificate);
}

void Indefinite() {
    Bytes encoded = IndefiniteCertificate("Ber");
    Memory memory{Cat({encoded, {0xcc}})};
    const auto method = Method();
    auto bio = Bio(memory, method);
    LibreSslX509* certificate = d2i_X509_bio_nid_postfix(&bio, nullptr);
    Require(certificate && std::strcmp(certificate->name, "/CN=Ber") == 0, "an indefinite certificate decodes");
    Require(memory.offset == encoded.size() + 1, "the upstream header read may buffer trailing data after an EOC");
    X509_free_nid_postfix(certificate);
}

void ReadFailurePreservesOutput() {
    const auto method = Method();
    Memory complete{Certificate("Keep")};
    auto completeBio = Bio(complete, method);
    LibreSslX509* certificate = d2i_X509_bio_nid_postfix(&completeBio, nullptr);
    LibreSslX509* original = certificate;
    Bytes truncated = Certificate("Broken");
    truncated.resize(12);
    Memory incomplete{truncated};
    auto incompleteBio = Bio(incomplete, method);
    Require(!d2i_X509_bio_nid_postfix(&incompleteBio, &certificate), "a truncated BIO fails");
    Require(certificate == original && std::strcmp(certificate->name, "/CN=Keep") == 0,
        "a BIO failure preserves the existing output");
    X509_free_nid_postfix(certificate);
    ERR_clear_error_nid_postfix();
}

void DecodeFailureClearsOutput() {
    const auto method = Method();
    Memory complete{Certificate("Replace")};
    auto completeBio = Bio(complete, method);
    LibreSslX509* certificate = d2i_X509_bio_nid_postfix(&completeBio, nullptr);
    Memory invalid{{0x30, 0x03, 0x02, 0x01, 0x00}};
    auto invalidBio = Bio(invalid, method);
    Require(!d2i_X509_bio_nid_postfix(&invalidBio, &certificate) && !certificate,
        "a complete invalid object clears the reused output through the decoder");
    ERR_clear_error_nid_postfix();
}

void ShortHeaderRead() {
    Memory memory{Certificate("Short"), 0, 1};
    const auto method = Method();
    auto bio = Bio(memory, method);
    Require(!d2i_X509_bio_nid_postfix(&bio, nullptr), "an incomplete first header read follows LibreSSL and fails");
    Require(memory.offset == 1 && bio.bytesRead == 1, "the failed short header read advances by one byte");
    ERR_clear_error_nid_postfix();
}

void BioErrors() {
    Require(!d2i_X509_bio_nid_postfix(nullptr, nullptr), "a null BIO fails");
    Require(ERR_get_error_nid_postfix() == ErrorCode(32, 121) && ERR_get_error_nid_postfix() == ErrorCode(13, 142),
        "a null BIO queues BIO and ASN.1 errors");
    LibreSsl::BioMethod method{};
    Memory memory{Certificate("NoRead")};
    auto bio = Bio(memory, method);
    Require(!d2i_X509_bio_nid_postfix(&bio, nullptr), "a BIO without a read method fails");
    ERR_clear_error_nid_postfix();
    method = Method();
    bio = Bio(memory, method);
    bio.initialized = 0;
    Require(!d2i_X509_bio_nid_postfix(&bio, nullptr), "an uninitialized BIO fails");
    ERR_clear_error_nid_postfix();
}

}

int main() {
    static_assert(std::is_same_v<decltype(&d2i_X509_bio_nid_postfix),
        LibreSslX509* (APS5_VABI *)(LibreSsl::Bio*, LibreSslX509**)>);
    std::array<void*, 10> heap{};
    ApplicationHeapRegister_nid_no_patch(heap.data());
    DefiniteAndCursor();
    Reuse();
    Indefinite();
    ReadFailurePreservesOutput();
    DecodeFailureClearsOutput();
    ShortHeaderRead();
    BioErrors();
}
