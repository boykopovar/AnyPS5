#include "prx/libSceLibreSsl/libSceLibreSsl.hpp"
#include "prx/libSceLibreSsl/include/Api.hpp"
#include "prx/libc/include/Api.hpp"
#include "prx/libc/include/ApplicationHeap.hpp"
#include <algorithm>
#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <exception>
#include <initializer_list>
#include <stdexcept>
#include <vector>

namespace {

struct PkeyHeader {
    std::int32_t type;
    std::int32_t saveType;
    std::int32_t references;
    const void* ameth;
    void* engine;
    void* key;
    std::int32_t saveParameters;
    void* attributes;
};

struct ForeignKeyContext {
    const void* method;
};

static_assert(sizeof(PkeyHeader) == 56);
static_assert(offsetof(PkeyHeader, references) == 8);
static_assert(offsetof(PkeyHeader, key) == 32);
static_assert(sizeof(EvpMdContext) == 48);
static_assert(offsetof(EvpMdContext, pkeyContext) == 32);

constexpr int NidSha1 = 64;
constexpr std::size_t KeyBytes = 128;
constexpr const char Message[] = "AnyPS5 digest verify message";
constexpr const char OtherMessage[] = "AnyPS5 digest verify other message";
constexpr std::uint64_t EvpQueue = (6ull << 24) | (0xfffull << 12);
constexpr std::uint64_t RsaQueue = (4ull << 24) | (0xfffull << 12);
constexpr std::uint64_t MallocFailure = EvpQueue | 65;
constexpr std::uint64_t InputNotInitialized = EvpQueue | 111;
constexpr std::uint64_t AlgorithmMismatch = RsaQueue | 100;
constexpr std::uint64_t BadSignature = RsaQueue | 104;
constexpr std::uint64_t BlockTypeIsNot01 = RsaQueue | 106;
constexpr std::uint64_t PaddingCheckFailed = RsaQueue | 114;
constexpr std::uint64_t WrongSignatureLength = RsaQueue | 119;
constexpr std::uint64_t DataTooLargeForModulus = RsaQueue | 132;

constexpr const char* PublicKeyDer =
    "30819f300d06092a864886f70d010101050003818d0030818902818100cfb9ade97bdfe0f53bbc2e7fc3548450fcf1f8"
    "74c35aeef29b6fa3e2c59aeb4f3731cf6f5335d806665722251264c3744cc2baef0c5b8975ba580d2674df0f1eb96516"
    "d6450c949ed6a3e4fdd66b173a2fa880b6a7dbf60cea94d33c182d2a9175b1db439d10adcb5e2e9756d27ba6e87291ae"
    "4b96b13592bd9fa570d06b4ebb0203010001";
constexpr const char* Md5Signature =
    "6da10f919c2a6859c396e9fc40a99ba491ac6e8b39ea3cc07dc904c65557ca9f704a6e2b749450dad1320233e481979b"
    "ceac3688f431984efbaab3199fcb10236d5075519eb0e0d45464269aa59a5427c39a2789d575b1b23b68a5ceee83d433"
    "00900d026bf6ed3adafcd9800562d96ff66dc5725c2a0334225110a979f0b7b2";
constexpr const char* Sha1Signature =
    "6118883be4f3da14c8de5243ac47acc8fe64ba35c36e6d05e59e46a56574b64526599c39d54306f3efe66e864973d1b9"
    "7d0b441a06b040996972525dc0c149443d8868070324a58e50a12e9ba0cf2df68c5dd05446ce31dabec473348322b8a1"
    "3ef08764c3603040f05c3a3a11f9f36b4478ba601d6ad72bffcdd27f4db7b9f8";
constexpr const char* Sha224Signature =
    "109bb60c7b72d7079fd5cd8772d3464ac8ffa1677273ac5f8b373e6d67d0f03e427420e017d1962784786e376d1f9899"
    "97873fb6bc7305c754169a42e0f5e995dc1fe0d28d0f9e5574ef3a5dd2cd869f390b8dba6ecd6a9eb71e6fb301727cd3"
    "a51f0a3e86083b78db0411fc4b4fbd1cff36725402b6c7173bb7474b92f6139c";
constexpr const char* Sha256Signature =
    "795db6306fb0c69d3d79ff43253bd65cf18a49dfc9be6fd435fa53f78f1cc02a7b7a840b526ebafa1366e1d851efc38e"
    "ef9634de408bfbc22deb5a6aab43d8c23a21a83feae77d295dbd282480ad69aa663dc2242c41b69503eb0d7f72be25be"
    "65256c263015da9b82cfb575cdd54fe5bf71a50584530f03ccf965479c79e281";
constexpr const char* Sha384Signature =
    "b949f553a6cd9dd73cd285e072f4f15c1f5dc98fb2b6f476151f191978f63e30ed32783b222a44a983869762d583d06a"
    "8c21257b76f67c5f6c42d9089a6fdc98e993a602c53f181ba7cf48a2ea60e0c0a4a16e1b8d52444ee84e7c724af31da4"
    "39c268d9937f857d3b6d6422b547e34fe2a17e669c5361d8612a3fac16ed672f";
constexpr const char* Sha512Signature =
    "8a534bb327ff05724690afb218b91c1ee476cfa6cee65eb98936d991007e2a7127687bdb1e99b5de3ece6c77ecd0a827"
    "c964f2ca4dad9710107595c794ffee62421a3e57350a7cec42baa1e4dc103b7110e33111cd7e003f346d39c7f84b9312"
    "a106a3e140e36305880775bfd5fe93cfdaf8712c28d1b471a7509fc71d1a6bb2";
constexpr const char* OtherSha256Signature =
    "613e57c1b9021d8b04cb8e90d5d56b58c61932c36b5c2d583caee13a43f2d9f9c5a1a672a78d7dd81f2b228231951ba2"
    "0e0ed8c59d04e0b0a926f6c5fd8e76d7f3d8e25eb299edf1e70239f5419f948020dd72f0985bc33c5df826551b37d031"
    "36760feece2bee3255c733c7c98c0dd23f3be2c17b8c37e7829efcffc1fc7d05";

void Require(bool condition, const char* message) {
    if (condition) return;
    std::fprintf(stderr, "failed: %s\n", message);
    std::abort();
}

template<typename TAction>
void RequireThrows(TAction action, const char* message) {
    try {
        action();
    } catch (const std::exception&) {
        return;
    }
    Require(false, message);
}

std::vector<unsigned char> Bytes(const char* hex) {
    const auto digit = [](char c) { return c >= 'a' ? c - 'a' + 10 : c - '0'; };
    std::vector<unsigned char> bytes;
    for (; hex[0] && hex[1]; hex += 2) bytes.push_back(static_cast<unsigned char>(digit(hex[0]) * 16 + digit(hex[1])));
    return bytes;
}

void RequireErrors(std::initializer_list<std::uint64_t> codes, const char* message) {
    for (const auto code : codes) Require(ERR_get_error_nid_postfix() == code, message);
    Require(ERR_get_error_nid_postfix() == 0, message);
}

struct Key {
    void* pkey;
    std::vector<unsigned char> der;
};

Key LoadKey() {
    Key key {{}, Bytes(PublicKeyDer)};
    const unsigned char* cursor = key.der.data();
    auto* rsa = d2i_RSA_PUBKEY_nid_postfix(nullptr, &cursor, static_cast<long>(key.der.size()));
    Require(rsa, "the RSA public key is read from its encoding");
    Require(cursor == key.der.data() + key.der.size(), "the whole public key encoding is read");
    key.pkey = EVP_PKEY_new_nid_postfix();
    Require(key.pkey, "a key object is allocated");
    Require(EVP_PKEY_set1_RSA_nid_postfix(key.pkey, rsa) == 1, "the key object takes the RSA key");
    Require(static_cast<const PkeyHeader*>(key.pkey)->type == 6, "the key object is an RSA key");
    RSA_free_nid_postfix(rsa);
    return key;
}

int References(const Key& key) {
    return static_cast<const PkeyHeader*>(key.pkey)->references;
}

struct Attempt {
    int initialized = 0;
    int updated = 0;
    int verified = 0;
    int cleaned = 0;
    void* pctx = nullptr;
};

Attempt Run(const Key& key, const EvpMd* digest, const std::vector<unsigned char>& signature, const char* message = Message,
    std::size_t signatureLength = KeyBytes) {
    Attempt attempt;
    EvpMdContext context {};
    EVP_MD_CTX_init_nid_postfix(&context);
    attempt.initialized = EVP_DigestVerifyInit_nid_postfix(&context, &attempt.pctx, digest, nullptr, key.pkey);
    if (attempt.initialized == 1) {
        attempt.updated = EVP_DigestUpdate_nid_postfix(&context, message, std::strlen(message)) &&
            EVP_DigestUpdate_nid_postfix(&context, message, 0);
        if (attempt.updated == 1)
            attempt.verified = EVP_DigestVerifyFinal_nid_postfix(&context, const_cast<unsigned char*>(signature.data()), signatureLength);
    }
    attempt.cleaned = EVP_MD_CTX_cleanup_nid_postfix(&context);
    return attempt;
}

void ValidSignatures(const Key& key) {
    struct Case {
        const EvpMd* digest;
        const char* signature;
    };
    const Case cases[] {
        {EVP_md5_nid_postfix(), Md5Signature},
        {EVP_sha256_nid_postfix(), Sha256Signature},
        {EVP_sha384_nid_postfix(), Sha384Signature},
        {EVP_sha512_nid_postfix(), Sha512Signature},
        {nullptr, Sha1Signature},
    };
    for (const auto& testCase : cases) {
        ERR_clear_error_nid_postfix();
        const auto signature = Bytes(testCase.signature);
        Require(signature.size() == KeyBytes, "the fixed signature is as long as the key");
        const auto attempt = Run(key, testCase.digest, signature);
        Require(attempt.initialized == 1 && attempt.updated == 1 && attempt.verified == 1 && attempt.cleaned == 1,
            "a signature made for the digest is accepted");
        Require(attempt.pctx != nullptr, "the caller is given the key context of the digest");
        RequireErrors({}, "an accepted signature queues no error");
        Require(References(key) == 1, "the key returns to one reference");
    }
}

void DefaultDigest(const Key& key) {
    ERR_clear_error_nid_postfix();
    EvpMdContext context {};
    EVP_MD_CTX_init_nid_postfix(&context);
    void* pctx = nullptr;
    Require(EVP_DigestVerifyInit_nid_postfix(&context, &pctx, nullptr, nullptr, key.pkey) == 1, "an initialization without a digest succeeds");
    Require(context.digest && context.digest->type == NidSha1 && context.digest->mdSize == 20, "the RSA key asks for the SHA1 digest");
    Require(pctx && pctx == context.pkeyContext, "the digest context and the caller get the same key context");
    Require(References(key) == 2, "the key context holds one reference to the key");
    RequireErrors({}, "a successful initialization queues no error");
    Require(EVP_DigestUpdate_nid_postfix(&context, Message, 10) == 1 && EVP_DigestUpdate_nid_postfix(&context, Message + 10,
        std::strlen(Message) - 10) == 1, "the message is added in two parts");
    auto signature = Bytes(Sha1Signature);
    Require(EVP_DigestVerifyFinal_nid_postfix(&context, signature.data(), signature.size()) == 1,
        "the SHA1 signature verifies through the default digest");
    RequireErrors({}, "the default digest verification queues no error");
    Require(EVP_MD_CTX_cleanup_nid_postfix(&context) == 1, "the digest context is cleaned");
    Require(context.pkeyContext == nullptr && context.digest == nullptr && context.mdData == nullptr, "the cleanup clears the digest context");
    Require(References(key) == 1, "the cleanup releases the key context reference");
}

void CorruptSignatures(const Key& key) {
    const auto valid = Bytes(Sha256Signature);
    ERR_clear_error_nid_postfix();
    Require(Run(key, EVP_sha256_nid_postfix(), valid, OtherMessage).verified == 0, "a signature of another message is refused");
    RequireErrors({BadSignature}, "a signature of another message queues the bad signature error");
    auto changed = valid;
    changed[63] ^= 0x02;
    const auto before = changed;
    Require(Run(key, EVP_sha256_nid_postfix(), changed).verified == 0, "a changed signature bit is refused");
    RequireErrors({BlockTypeIsNot01, PaddingCheckFailed}, "a changed signature bit queues the padding errors");
    Require(changed == before, "the signature bytes are left alone");
    Require(Run(key, EVP_sha384_nid_postfix(), valid).verified == 0, "a signature of another digest is refused");
    RequireErrors({AlgorithmMismatch}, "a signature of another digest queues the algorithm mismatch error");
    Require(Run(key, EVP_sha256_nid_postfix(), valid, Message, KeyBytes - 1).verified == 0, "a short signature length is refused");
    RequireErrors({WrongSignatureLength}, "a short signature length queues its error");
    RequireErrors({}, "no second error follows the signature length");
    Require(Run(key, EVP_sha256_nid_postfix(), valid, Message, KeyBytes + 8).verified == 0, "a long signature length is refused");
    RequireErrors({WrongSignatureLength}, "a long signature length queues its error");
    std::vector<unsigned char> empty(KeyBytes, 0);
    Require(Run(key, EVP_sha256_nid_postfix(), empty).verified == 0, "an empty signature is refused");
    RequireErrors({BlockTypeIsNot01, PaddingCheckFailed}, "an empty signature queues the padding errors");
    std::vector<unsigned char> above(KeyBytes, 0xff);
    Require(Run(key, EVP_sha256_nid_postfix(), above).verified == 0, "a signature above the modulus is refused");
    RequireErrors({DataTooLargeForModulus}, "a signature above the modulus queues its error");
    Require(Run(key, EVP_sha256_nid_postfix(), valid, Message, 0).verified == 0, "a zero signature length is refused");
    RequireErrors({WrongSignatureLength}, "a zero signature length queues its error");
    auto truncated = valid;
    Require(Run(key, EVP_sha256_nid_postfix(), truncated, Message, 90).verified == 0, "a truncated signature length is refused");
    RequireErrors({WrongSignatureLength}, "a truncated signature length queues its error");
    Require(Run(key, EVP_sha256_nid_postfix(), Bytes(Sha224Signature)).verified == 0, "a SHA224 signature verified as SHA256 is refused");
    RequireErrors({AlgorithmMismatch}, "a SHA224 signature verified as SHA256 queues the algorithm mismatch error");
    Require(References(key) == 1, "the refused signatures leave one key reference");
}

void UninitializedDigest(const Key& key) {
    ERR_clear_error_nid_postfix();
    EvpMdContext context {};
    EVP_MD_CTX_init_nid_postfix(&context);
    void* pctx = nullptr;
    Require(EVP_DigestVerifyInit_nid_postfix(&context, &pctx, EVP_sha256_nid_postfix(), nullptr, key.pkey) == 1, "the context is initialized");
    context.digest = nullptr;
    auto signature = Bytes(Sha256Signature);
    const auto before = signature;
    Require(EVP_DigestVerifyFinal_nid_postfix(&context, signature.data(), signature.size()) == -1,
        "a context without a digest returns the copy failure");
    RequireErrors({InputNotInitialized}, "a context without a digest queues the input not initialized error");
    Require(signature == before, "the copy failure does not read the signature");
    Require(EVP_MD_CTX_cleanup_nid_postfix(&context) == 1, "the context that keeps a key context is cleaned");
    Require(References(key) == 1, "the cleanup of the uninitialized context releases the key reference");
}

void MissingOrForeignState(const Key& key) {
    ERR_clear_error_nid_postfix();
    RequireThrows([] { EVP_DigestVerifyFinal_nid_postfix(nullptr, nullptr, 0); }, "a null digest context follows the source's unchecked read");
    EvpMdContext empty {};
    RequireThrows([&] { EVP_DigestVerifyFinal_nid_postfix(&empty, nullptr, 0); },
        "a digest context without a key context follows the source's unchecked read");
    Require(References(key) == 1, "the missing state touches no key reference");
    int marker = 0;
    const ForeignKeyContext foreign {&marker};
    EvpMdContext context {};
    EVP_MD_CTX_init_nid_postfix(&context);
    context.pkeyContext = const_cast<void*>(static_cast<const void*>(&foreign));
    RequireThrows([&] { EVP_DigestVerifyFinal_nid_postfix(&context, nullptr, 0); }, "a key context this library did not create throws");
    RequireThrows([&] { EVP_DigestVerifyInit_nid_postfix(&context, nullptr, EVP_sha256_nid_postfix(), nullptr, key.pkey); },
        "an existing foreign key context throws during the initialization");
    RequireThrows([&] { EVP_MD_CTX_cleanup_nid_postfix(&context); }, "freeing a foreign key context throws");
    Require(References(key) == 1, "a foreign key context takes no key reference");
    context.pkeyContext = nullptr;
    Require(EVP_DigestVerifyInit_nid_postfix(&context, nullptr, nullptr, nullptr, nullptr) == 0, "an initialization without a key fails");
    Require(context.pkeyContext == nullptr, "an initialization without a key leaves no key context");
    RequireErrors({}, "an initialization without a key queues no error");
    void* untouched = reinterpret_cast<void*>(std::uintptr_t{0x9abc});
    Require(EVP_DigestVerifyInit_nid_postfix(&context, &untouched, EVP_sha256_nid_postfix(),
        reinterpret_cast<void*>(std::uintptr_t{7}), nullptr) == 0, "an initialization with an ENGINE and no key fails without reading them");
    Require(untouched == reinterpret_cast<void*>(std::uintptr_t{0x9abc}), "the missing key leaves the output context alone");
    RequireErrors({}, "an initialization with an ENGINE and no key queues no error");
    Require(EVP_DigestVerifyInit_nid_postfix(&context, nullptr, EVP_sha256_nid_postfix(), nullptr, key.pkey) == 1,
        "the same context is initialized again after the failures");
    Require(EVP_MD_CTX_cleanup_nid_postfix(&context) == 1, "the context is cleaned after the failures");
    Require(References(key) == 1, "the failures leave one key reference");
}

void CopyAndCleanup(const Key& key) {
    ERR_clear_error_nid_postfix();
    auto signature = Bytes(Sha384Signature);
    for (int part = 0; part < 3; ++part) {
        EvpMdContext context {};
        EVP_MD_CTX_init_nid_postfix(&context);
        void* pctx = nullptr;
        Require(EVP_DigestVerifyInit_nid_postfix(&context, &pctx, EVP_sha384_nid_postfix(), nullptr, key.pkey) == 1,
            "the context is initialized for the copy check");
        std::size_t done = 0;
        for (const std::size_t chunk : {std::size_t{7}, std::size_t{11}, std::size_t{10}}) {
            const std::size_t length = std::min(chunk, std::strlen(Message) - done);
            Require(EVP_DigestUpdate_nid_postfix(&context, Message + done, length) == 1, "each part of the message is added");
            done += length;
            if (done == std::strlen(Message)) break;
        }
        Require(done == std::strlen(Message), "every message byte reaches the digest");
        Require(EVP_DigestVerifyFinal_nid_postfix(&context, signature.data(), signature.size()) == 1, "the copied digest context verifies");
        Require(EVP_MD_CTX_cleanup_nid_postfix(&context) == 1, "the copied digest context is cleaned");
    }
    RequireErrors({}, "the copies of the digest context queue no error");
    Require(References(key) == 1, "the copies and cleanups leave one key reference");
}

void ReleaseKey(const Key& key) {
    EVP_PKEY_free_nid_postfix(key.pkey);
}

int allocations = 0;
int callocs = 0;
int frees = 0;
int failAllocationAt = 0;

void* APS5_VABI Allocate_nid_no_patch(std::size_t size) {
    if (++allocations == failAllocationAt) return nullptr;
    return std::malloc(size);
}

void APS5_VABI Free_nid_no_patch(void* pointer) {
    if (pointer) ++frees;
    std::free(pointer);
}

void* APS5_VABI Calloc_nid_no_patch(std::size_t count, std::size_t size) {
    ++callocs;
    return std::calloc(count, size);
}

void* APS5_VABI Reallocate_nid_no_patch(void*, std::size_t) { throw std::runtime_error("unexpected realloc"); }
void* APS5_VABI Align_nid_no_patch(std::size_t, std::size_t) { throw std::runtime_error("unexpected aligned allocation"); }
void* APS5_VABI Realign_nid_no_patch(void*, std::size_t, std::size_t) { throw std::runtime_error("unexpected aligned reallocation"); }
int APS5_VABI PosixAlign_nid_no_patch(void**, std::size_t, std::size_t) { throw std::runtime_error("unexpected posix_memalign"); }

void RegisterHeap() {
    const std::array<void*, 10> api {
        reinterpret_cast<void*>(Allocate_nid_no_patch), reinterpret_cast<void*>(Free_nid_no_patch), reinterpret_cast<void*>(Calloc_nid_no_patch),
        reinterpret_cast<void*>(Reallocate_nid_no_patch), reinterpret_cast<void*>(Align_nid_no_patch), reinterpret_cast<void*>(Realign_nid_no_patch),
        reinterpret_cast<void*>(PosixAlign_nid_no_patch)};
    ApplicationHeapRegister_nid_no_patch(api.data());
}

void Allocator() {
    RegisterHeap();
    allocations = callocs = frees = 0;
    const Key key = LoadKey();
    const int keyBlocks = allocations - frees;
    const auto signature = Bytes(Sha256Signature);
    allocations = callocs = frees = 0;
    const auto full = Run(key, EVP_sha256_nid_postfix(), signature);
    Require(full.initialized == 1 && full.updated == 1 && full.verified == 1 && full.cleaned == 1, "the allocation check verifies its signature");
    Require(allocations == 6 && callocs == 0 && frees == 6,
        "the key context, its method data, the digest data and their copy are all released");
    for (int failing = 1; failing <= 6; ++failing) {
        ERR_clear_error_nid_postfix();
        allocations = callocs = frees = 0;
        failAllocationAt = failing;
        const auto attempt = Run(key, EVP_sha256_nid_postfix(), signature);
        failAllocationAt = 0;
        switch (failing) {
        case 1:
            Require(attempt.initialized == 0 && attempt.verified == 0 && attempt.pctx == nullptr,
                "a failed key context allocation fails the initialization");
            RequireErrors({MallocFailure}, "a failed key context allocation queues the malloc failure");
            break;
        case 2:
            Require(attempt.initialized == 0 && attempt.verified == 0 && attempt.pctx == nullptr,
                "a failed method data allocation fails the initialization");
            RequireErrors({}, "a failed method data allocation queues no error");
            break;
        case 3:
            Require(attempt.initialized == 0 && attempt.verified == 0, "a failed digest data allocation fails the initialization");
            RequireErrors({MallocFailure}, "a failed digest data allocation queues the malloc failure");
            break;
        case 4:
            Require(attempt.initialized == 1 && attempt.updated == 1 && attempt.verified == -1,
                "a failed copy of the digest data fails the verification");
            RequireErrors({MallocFailure}, "a failed copy of the digest data queues the malloc failure");
            break;
        case 5:
        case 6:
            Require(attempt.initialized == 1 && attempt.updated == 1 && attempt.verified == -1,
                "a failed copy of the key context fails the verification");
            RequireErrors({}, "a failed copy of the key context queues no error");
            break;
        default:
            Require(false, "the failing allocation index is one to six");
            break;
        }
        Require(attempt.cleaned == 1 && frees + 1 == allocations, "a failed allocation releases everything the attempt allocated");
    }
    allocations = callocs = frees = 0;
    ReleaseKey(key);
    Require(frees == keyBlocks, "the key release frees every block that is still allocated");
}

} // namespace

int main(int argc, char** argv) {
    if (argc > 1 && std::strcmp(argv[1], "allocator") == 0) {
        Allocator();
        return 0;
    }
    const std::array<void*, 10> api {};
    ApplicationHeapRegister_nid_no_patch(api.data());
    const Key key = LoadKey();
    ValidSignatures(key);
    DefaultDigest(key);
    CorruptSignatures(key);
    UninitializedDigest(key);
    MissingOrForeignState(key);
    CopyAndCleanup(key);
    ReleaseKey(key);
    return 0;
}
