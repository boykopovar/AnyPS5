#include "prx/libSceLibreSsl/include/Api.hpp"
#include "prx/libc/include/Api.hpp"
#include "prx/libc/include/ApplicationHeap.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <source_location>
#include <stdexcept>
#include <string>
#include <vector>

struct TestBignum {
    std::uint64_t* d;
    int top;
    int dmax;
    int neg;
    int flags;
};

struct TestRsaMethod {
    const char* name;
    void* operations[8];
    int flags;
    char* appData;
    void* sign;
    void* verify;
    void* keygen;
};

struct TestRsa {
    int pad;
    std::int64_t version;
    const TestRsaMethod* meth;
    void* engine;
    void* n;
    void* rest[15];
};

static_assert(offsetof(TestRsaMethod, flags) == 72);
static_assert(offsetof(TestRsa, meth) == 16 && offsetof(TestRsa, n) == 32 && sizeof(TestRsa) == 160);

static const LibreSslRsa* Rsa(const TestRsa* rsa) {
    return reinterpret_cast<const LibreSslRsa*>(rsa);
}

static void Require(bool condition, std::source_location location = std::source_location::current()) {
    if (condition) return;
    std::fprintf(stderr, "LibreSSL RSA check failed at line %u\n", location.line());
    std::abort();
}

template<typename TAction> static void RequireThrows(TAction action, std::source_location location = std::source_location::current()) {
    try { action(); } catch (const std::exception&) { return; }
    Require(false, location);
}

static void TestFlags() {
    TestRsaMethod method {};
    TestRsa rsa {};
    rsa.meth = &method;
    Require(RSA_flags_nid_postfix(nullptr) == 0);
    Require(RSA_flags_nid_postfix(Rsa(&rsa)) == 0);
    method.flags = 0x0400;
    Require(RSA_flags_nid_postfix(Rsa(&rsa)) == 0x0400);
    method.flags = -1;
    Require(RSA_flags_nid_postfix(Rsa(&rsa)) == -1);
    rsa.meth = nullptr;
    RequireThrows([&] { RSA_flags_nid_postfix(Rsa(&rsa)); });
}

static int Size(TestBignum* modulus) {
    TestRsa rsa {};
    rsa.n = modulus;
    return RSA_size_nid_postfix(Rsa(&rsa));
}

static void TestSize() {
    std::uint64_t words[32] {};
    TestBignum modulus {words, 0, 32, 0, 0};
    Require(Size(&modulus) == 0);
    modulus.d = nullptr;
    Require(Size(&modulus) == 0);
    modulus.d = words;
    modulus.top = 1;
    words[0] = 1;
    Require(Size(&modulus) == 1);
    words[0] = 0xFF;
    Require(Size(&modulus) == 1);
    words[0] = 0x100;
    Require(Size(&modulus) == 2);
    words[0] = 0x8000000000000000ull;
    Require(Size(&modulus) == 8);
    modulus.top = 2;
    words[1] = 1;
    Require(Size(&modulus) == 9);
    modulus.top = 32;
    words[31] = 0xC000000000000000ull;
    Require(Size(&modulus) == 256);
    modulus.neg = 1;
    Require(Size(&modulus) == 256);
    modulus.neg = 0;
    modulus.top = 16;
    words[15] = 0x00FFFFFFFFFFFFFFull;
    Require(Size(&modulus) == 127);
    words[15] = 0;
    Require(Size(&modulus) == 120);
    modulus.top = -1;
    RequireThrows([&] { Size(&modulus); });
    modulus.top = 1;
    modulus.d = nullptr;
    RequireThrows([&] { Size(&modulus); });
    RequireThrows([] { Size(nullptr); });
    RequireThrows([] { RSA_size_nid_postfix(nullptr); });
}

constexpr int BignumStaticData = 0x02;
constexpr int BignumMalloced = 0x01;

struct Finishes {
    int calls;
    LibreSslRsa* rsa;
};

static Finishes finishes;

static int APS5_VABI RecordFinish(LibreSslRsa* rsa) {
    ++finishes.calls;
    finishes.rsa = rsa;
    return 1;
}

static void InitializeHeap() {
    void* const api[10] {};
    ApplicationHeapRegister_nid_no_patch(api);
}

static bool Zero(const void* data, std::size_t size) {
    const auto* bytes = static_cast<const unsigned char*>(data);
    for (std::size_t index = 0; index < size; ++index)
        if (bytes[index]) return false;
    return true;
}

static TestBignum* AllocatedBignum(std::uint64_t value) {
    auto* number = static_cast<TestBignum*>(malloc_nid_postfix(sizeof(TestBignum)));
    auto* words = static_cast<std::uint64_t*>(malloc_nid_postfix(2 * sizeof(std::uint64_t)));
    Require(number != nullptr && words != nullptr);
    words[0] = value;
    words[1] = 0;
    *number = {words, 1, 2, 0, BignumMalloced};
    return number;
}

static void TestNew() {
    auto* rsa = RSA_new_nid_postfix();
    Require(rsa != nullptr && rsa->meth != nullptr);
    Require(rsa->pad == 0 && rsa->version == 0 && rsa->engine == nullptr && rsa->references == 1 && rsa->flags == 0x0006);
    for (void* field : {rsa->n, rsa->e, rsa->d, rsa->p, rsa->q, rsa->dmp1, rsa->dmq1, rsa->iqmp, rsa->exData}) Require(field == nullptr);
    for (void* field : {rsa->methodModN, rsa->methodModP, rsa->methodModQ, rsa->blinding, rsa->mtBlinding}) Require(field == nullptr);
    const auto* method = rsa->meth;
    Require(std::string(method->name) == "Eric Young's PKCS#1 RSA" && method->flags == 0 && method->appData == nullptr);
    for (void* operation : method->operations) Require(operation != nullptr);
    Require(method->sign == nullptr && method->verify == nullptr && method->keygen == nullptr);
    Require(RSA_flags_nid_postfix(rsa) == 0);
    auto* second = RSA_new_nid_postfix();
    Require(second != nullptr && second != rsa && second->meth == method);
    RSA_free_nid_postfix(second);
    RSA_free_nid_postfix(rsa);
}

static void TestFree() {
    RSA_free_nid_postfix(nullptr);
    auto* rsa = RSA_new_nid_postfix();
    std::uint64_t words[4] {0x1234, 0x5678, 0, 0};
    TestBignum stored {words, 2, 4, 0, BignumStaticData};
    rsa->n = &stored;
    rsa->e = AllocatedBignum(65537);
    rsa->d = AllocatedBignum(3);
    rsa->references = 2;
    RSA_free_nid_postfix(rsa);
    Require(rsa->references == 1 && rsa->n == &stored && stored.top == 2);
    RSA_free_nid_postfix(rsa);
    Require(Zero(&stored, sizeof(stored)) && words[0] == 0x1234 && words[1] == 0x5678);

    rsa = RSA_new_nid_postfix();
    LibreSslRsaMethod method = *rsa->meth;
    method.operations[7] = reinterpret_cast<void*>(RecordFinish);
    rsa->meth = &method;
    finishes = {};
    RSA_free_nid_postfix(rsa);
    Require(finishes.calls == 1 && finishes.rsa == rsa);
    method.operations[7] = nullptr;
    rsa = RSA_new_nid_postfix();
    rsa->meth = &method;
    RSA_free_nid_postfix(rsa);
    Require(finishes.calls == 1);

    rsa = RSA_new_nid_postfix();
    rsa->meth = nullptr;
    RequireThrows([&] { RSA_free_nid_postfix(rsa); });
}

using Bytes = std::vector<unsigned char>;

constexpr const char* CavpModulus =
    "cea80475324c1dc8347827818da58bac069d3419c614a6ea1ac6a3b510dcd72cc516954905e9fef908d45e13006adf27"
    "d467a7d83c111d1a5df15ef293771aefb920032a5bb989f8e4f5e1b05093d3f130f984c07a772a3683f4dc6fb28a9681"
    "5b32123ccdd13954f19d5b8b24a103e771a34c328755c65ed64e1924ffd04d30b2142cc262f6e0048fef6dbc652f2147"
    "9ea1c4b1d66d28f4d46ef7185e390cbfa2e02380582f3188bb94ebbf05d31487a09aff01fcbb4cd4bfd1f0a833b38c11"
    "813c84360bb53c7d4481031c40bad8713bb6b835cb08098ed15ba31ee4ba728a8c8e10f7294e1b4163b7aee57277bfd8"
    "81a6f9d43e02c6925aa3a043fb7fb78d";
constexpr const char* CavpSpki =
    "30820122300d06092a864886f70d01010105000382010f003082010a0282010100cea80475324c1dc8347827818da58b"
    "ac069d3419c614a6ea1ac6a3b510dcd72cc516954905e9fef908d45e13006adf27d467a7d83c111d1a5df15ef293771a"
    "efb920032a5bb989f8e4f5e1b05093d3f130f984c07a772a3683f4dc6fb28a96815b32123ccdd13954f19d5b8b24a103"
    "e771a34c328755c65ed64e1924ffd04d30b2142cc262f6e0048fef6dbc652f21479ea1c4b1d66d28f4d46ef7185e390c"
    "bfa2e02380582f3188bb94ebbf05d31487a09aff01fcbb4cd4bfd1f0a833b38c11813c84360bb53c7d4481031c40bad8"
    "713bb6b835cb08098ed15ba31ee4ba728a8c8e10f7294e1b4163b7aee57277bfd881a6f9d43e02c6925aa3a043fb7fb7"
    "8d0203260445";

constexpr const char* CavpPrivateExponent =
    "0997634c477c1a039d44c810b2aaa3c7862b0b88d3708272e1e15f66fc9389709f8a11f3ea6a5af7effa2d01c189c50f"
    "0d5bcbe3fa272e56cfc4a4e1d388a9dcd65df8628902556c8b6bb6a641709b5a35dd2622c73d4640bfa1359d0e76e1f2"
    "19f8e33eb9bd0b59ec198eb2fccaae0346bd8b401e12e3c67cb629569c185a2e0f35a2f741644c1cca5ebb139d77a89a"
    "2953fc5e30048c0e619f07c8d21d1e56b8af07193d0fdf3f49cd49f2ef3138b5138862f1470bd2d16e34a2b9e7777a6c"
    "8c8d4cb94b4e8b5d616cd5393753e7b0f31cc7da559ba8e98d888914e334773baf498ad88d9631eb5fe32e53a4145bf0"
    "ba548bf2b0a50c63f67b14e398a34b0d";
constexpr const char* CavpSignature =
    "6b8be97d9e518a2ede746ff4a7d91a84a1fc665b52f154a927650db6e7348c69f8c8881f7bcf9b1a6d3366eed30c3aed"
    "4e93c203c43f5528a45de791895747ade9c5fa5eee81427edee02082147aa311712a6ad5fb1732e93b3d6cd23ffd46a0"
    "b3caf62a8b69957cc68ae39f9993c1a779599cdda949bdaababb77f248fcfeaa44059be5459fb9b899278e929528ee13"
    "0facd53372ecbc42f3e8de2998425860406440f248d817432de687112e504d734028e6c5620fa282ca07647006cf0a2f"
    "f83e19a916554cc61810c2e855305db4e5cf893a6a96767365794556ff033359084d7e38a8456e68e21155b76151314a"
    "29875feee09557161cbc654541e89e42";
constexpr const char* CavpDigest = "077877895a428028f60998b985550820025a2a42c0beab27165c0802d3098150";

static Bytes Hex(const char* text) {
    const auto nibble = [](char digit) { return static_cast<unsigned char>(digit <= '9' ? digit - '0' : digit - 'a' + 10); };
    Bytes bytes;
    for (; text[0] && text[1]; text += 2) bytes.push_back(static_cast<unsigned char>(nibble(text[0]) << 4 | nibble(text[1])));
    return bytes;
}

static Bytes Concat(std::initializer_list<Bytes> parts) {
    Bytes bytes;
    for (const auto& part : parts) bytes.insert(bytes.end(), part.begin(), part.end());
    return bytes;
}

static Bytes Encode(unsigned char tag, const Bytes& content) {
    Bytes bytes {tag};
    if (content.size() < 0x80) {
        bytes.push_back(static_cast<unsigned char>(content.size()));
    } else if (content.size() < 0x100) {
        bytes.insert(bytes.end(), {0x81, static_cast<unsigned char>(content.size())});
    } else {
        bytes.insert(bytes.end(), {0x82, static_cast<unsigned char>(content.size() >> 8), static_cast<unsigned char>(content.size())});
    }
    return Concat({bytes, content});
}

static const Bytes RsaEncryption {0x06, 0x09, 0x2A, 0x86, 0x48, 0x86, 0xF7, 0x0D, 0x01, 0x01, 0x01};
static const Bytes Null {0x05, 0x00};
static const Bytes CavpExponent {0x26, 0x04, 0x45};

static Bytes RsaKey(const Bytes& modulus, const Bytes& exponent) {
    return Encode(0x30, Concat({Encode(0x02, modulus), Encode(0x02, exponent)}));
}

static Bytes PublicKeyInfo(const Bytes& algorithm, const Bytes& bitString) {
    return Encode(0x30, Concat({Encode(0x30, algorithm), Encode(0x03, bitString)}));
}

static Bytes StandardKey() {
    return RsaKey(Concat({{0x00}, Hex(CavpModulus)}), CavpExponent);
}

static LibreSslRsa* Decode(const Bytes& der, long length, long* consumed = nullptr) {
    const unsigned char* data = der.data();
    auto* rsa = d2i_RSA_PUBKEY_nid_postfix(nullptr, &data, length);
    if (consumed) *consumed = data - der.data();
    return rsa;
}

static const TestBignum& Number(const LibreSslRsa* rsa, bool modulus) {
    return *static_cast<const TestBignum*>(modulus ? rsa->n : rsa->e);
}

static void RequireAccepted(const Bytes& der, std::uint64_t exponent, std::source_location location = std::source_location::current()) {
    long consumed = 0;
    auto* rsa = Decode(der, static_cast<long>(der.size()), &consumed);
    Require(rsa != nullptr && consumed == static_cast<long>(der.size()), location);
    Require(Number(rsa, false).top == (exponent ? 1 : 0) && (!exponent || Number(rsa, false).d[0] == exponent), location);
    RSA_free_nid_postfix(rsa);
}

static void RequireRefused(const Bytes& der, std::source_location location = std::source_location::current()) {
    long consumed = -1;
    Require(Decode(der, static_cast<long>(der.size()), &consumed) == nullptr && consumed == 0, location);
}

static void TestPublicKeyInfo() {
    const auto published = Hex(CavpSpki);
    const auto rsaKey = StandardKey();
    Require(PublicKeyInfo(Concat({RsaEncryption, Null}), Concat({{0x00}, rsaKey})) == published);
    long consumed = 0;
    auto* rsa = Decode(published, static_cast<long>(published.size()), &consumed);
    Require(rsa != nullptr && consumed == 294 && RSA_size_nid_postfix(rsa) == 256);
    auto* fresh = RSA_new_nid_postfix();
    Require(rsa->meth == fresh->meth && rsa->flags == 0x0006 && rsa->references == 1 && rsa->engine == nullptr);
    RSA_free_nid_postfix(fresh);
    const auto& modulus = Number(rsa, true);
    const auto& exponent = Number(rsa, false);
    Require(modulus.dmax == 33 && modulus.top == 32 && modulus.neg == 0 && modulus.flags == BignumMalloced);
    Require(modulus.d[0] == 0x5aa3a043fb7fb78dULL && modulus.d[31] == 0xcea80475324c1dc8ULL);
    Require(exponent.dmax == 1 && exponent.top == 1 && exponent.d[0] == 0x260445 && exponent.flags == BignumMalloced);
    for (void* field : {rsa->d, rsa->p, rsa->q, rsa->dmp1, rsa->dmq1, rsa->iqmp}) Require(field == nullptr);

    LibreSslRsa* previous = RSA_new_nid_postfix();
    LibreSslRsa* target = previous;
    const unsigned char* data = published.data();
    auto* replaced = d2i_RSA_PUBKEY_nid_postfix(&target, &data, 294);
    Require(replaced != nullptr && target == replaced && data == published.data() + 294);
    RSA_free_nid_postfix(replaced);
    RSA_free_nid_postfix(rsa);

    auto padded = Concat({published, Null});
    Require((rsa = Decode(padded, static_cast<long>(padded.size()), &consumed)) != nullptr && consumed == 294);
    RSA_free_nid_postfix(rsa);
    Require(Decode(published, 293, &consumed) == nullptr && consumed == 0);
    RequireRefused(Bytes(published.begin(), published.end() - 1));
    Require(Decode(published, 0) == nullptr && Decode(published, -1) == nullptr);
    RequireThrows([&] { d2i_RSA_PUBKEY_nid_postfix(nullptr, nullptr, 294); });
    RequireThrows([&] {
        const unsigned char* none = nullptr;
        d2i_RSA_PUBKEY_nid_postfix(nullptr, &none, 294);
    });
}

static void TestPublicKeyInfoVariants() {
    const auto rsaKey = StandardKey();
    const auto bits = Concat({{0x00}, rsaKey});
    RequireAccepted(PublicKeyInfo(Concat({Encode(0x06, {0x55, 0x08, 0x01, 0x01}), Null}), bits), 0x260445);
    RequireAccepted(PublicKeyInfo(RsaEncryption, bits), 0x260445);
    for (const Bytes& parameter : {Bytes {0x04, 0x00}, Bytes {0x30, 0x00}, Bytes {0xA0, 0x00}, Bytes {0x02, 0x00}, Bytes {0x01, 0x01, 0xFF}, Bytes {0x05, 0x81, 0x00}})
        RequireAccepted(PublicKeyInfo(Concat({RsaEncryption, parameter}), bits), 0x260445);
    for (const Bytes& parameter : {Bytes {0x05, 0x01, 0x00}, Bytes {0x10, 0x00}, Bytes {0x01, 0x02, 0x00, 0x00}, Bytes {0x06, 0x01, 0x80}, Bytes {0x03, 0x00}, Bytes {0x00, 0x00}})
        RequireRefused(PublicKeyInfo(Concat({RsaEncryption, parameter}), bits));
    RequireRefused(PublicKeyInfo(Concat({RsaEncryption, Null, Null}), bits));
    RequireRefused(PublicKeyInfo(Concat({Encode(0x06, {0x2A, 0x86, 0x48, 0xCE, 0x3D, 0x02, 0x01}), Null}), bits));
    RequireRefused(PublicKeyInfo(Concat({Encode(0x06, {0x2A, 0x86, 0x48, 0x86, 0xF7, 0x0D, 0x01, 0x01, 0x0A}), Null}), bits));
    RequireRefused(PublicKeyInfo(Concat({Encode(0x06, {0x2A, 0x80, 0x86, 0x48, 0x86, 0xF7, 0x0D, 0x01, 0x01, 0x01}), Null}), bits));
    RequireRefused(Encode(0x30, Concat({Encode(0x30, Concat({RsaEncryption, Null})), Encode(0x03, bits), Null})));
    RequireRefused(Encode(0x30, Concat({Encode(0x30, Concat({RsaEncryption, Null})), Bytes {0x00, 0x00}, Encode(0x03, bits)})));
    RequireRefused(Encode(0x30, Encode(0x30, Concat({RsaEncryption, Null}))));
    RequireRefused(Encode(0x10, Concat({Encode(0x30, Concat({RsaEncryption, Null})), Encode(0x03, bits)})));
    RequireRefused(Encode(0x31, Concat({Encode(0x30, Concat({RsaEncryption, Null})), Encode(0x03, bits)})));
    const auto algorithm = Concat({RsaEncryption, Null});
    RequireAccepted(PublicKeyInfo(algorithm, Concat({{0x01}, rsaKey})), 0x260444);
    RequireAccepted(PublicKeyInfo(algorithm, Concat({{0x08}, rsaKey})), 0x260400);
    RequireAccepted(PublicKeyInfo(algorithm, Concat({{0x00}, rsaKey, Null})), 0x260445);
    RequireAccepted(PublicKeyInfo(algorithm, Concat({{0x00}, RsaKey(Hex(CavpModulus), CavpExponent)})), 0x260445);
    RequireAccepted(PublicKeyInfo(algorithm, Concat({{0x00}, RsaKey(Concat({{0x00}, Hex(CavpModulus)}), {})})), 0);
    RequireAccepted(PublicKeyInfo(algorithm, Concat({{0x00}, RsaKey(Concat({{0x00}, Hex(CavpModulus)}), {0x00, 0x00, 0x01, 0x00, 0x01})})), 0x10001);
    RequireRefused(PublicKeyInfo(algorithm, {}));
    RequireRefused(PublicKeyInfo(algorithm, {0x00}));
    RequireRefused(PublicKeyInfo(algorithm, Concat({{0x00}, Encode(0x30, Encode(0x02, Hex(CavpModulus)))})));
    RequireRefused(PublicKeyInfo(algorithm, Concat({{0x00}, Encode(0x30, Concat({Encode(0x02, Hex(CavpModulus)), Encode(0x02, CavpExponent), Encode(0x02, CavpExponent)}))})));
    RequireRefused(PublicKeyInfo(algorithm, Concat({{0x00}, Encode(0x31, Concat({Encode(0x02, Hex(CavpModulus)), Encode(0x02, CavpExponent)}))})));
    auto spki = Hex(CavpSpki);
    auto longForm = Concat({{0x30, 0x83, 0x00, 0x01, 0x22}, Bytes(spki.begin() + 4, spki.end())});
    RequireAccepted(longForm, 0x260445);
    auto highTag = Concat({{0x3F, 0x10}, Bytes(spki.begin() + 1, spki.end())});
    RequireAccepted(highTag, 0x260445);
    RequireRefused(Concat({{0x30, 0x89, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x22}, Bytes(spki.begin() + 4, spki.end())}));
}

constexpr int PaddingPkcs1 = 1;
constexpr int PaddingSslv23 = 2;
constexpr int PaddingNone = 3;
constexpr int PaddingOaep = 4;
constexpr int PaddingX931 = 5;

struct StaticNumber {
    std::vector<std::uint64_t> words;
    TestBignum number;
};

static void SetNumber(StaticNumber& storage, const Bytes& bigEndian) {
    storage.words.assign(bigEndian.size() / 8 + 1, 0);
    for (std::size_t position = 0; position < bigEndian.size(); ++position)
        storage.words[position / 8] |= std::uint64_t(bigEndian[bigEndian.size() - 1 - position]) << (8 * (position % 8));
    int top = static_cast<int>(storage.words.size());
    while (top > 0 && !storage.words[static_cast<std::size_t>(top) - 1]) --top;
    storage.number = {storage.words.data(), top, static_cast<int>(storage.words.size()), 0, BignumStaticData};
}

struct TestKey {
    StaticNumber modulus;
    StaticNumber exponent;
    LibreSslRsa* rsa;

    TestKey(const Bytes& n, const Bytes& e) : rsa(RSA_new_nid_postfix()) {
        SetNumber(modulus, n);
        SetNumber(exponent, e);
        rsa->n = &modulus.number;
        rsa->e = &exponent.number;
    }
    TestKey(const TestKey&) = delete;
    ~TestKey() { RSA_free_nid_postfix(rsa); }
};

static Bytes Pattern(std::size_t length, unsigned seed) {
    Bytes bytes(length);
    for (std::size_t index = 0; index < length; ++index) bytes[index] = static_cast<unsigned char>(seed + 29 * index);
    return bytes;
}

static Bytes Sha1(const Bytes& data) {
    Sha1Context context {};
    unsigned char digest[20];
    Require(SHA1_Init_nid_postfix(&context) == 1 && SHA1_Update_nid_postfix(&context, data.data(), data.size()) == 1);
    Require(SHA1_Final_nid_postfix(digest, &context) == 1);
    return Bytes(digest, digest + 20);
}

static Bytes Mgf1(const Bytes& seed, std::size_t length) {
    Bytes mask;
    for (std::uint32_t counter = 0; mask.size() < length; ++counter) {
        auto input = seed;
        for (int shift = 24; shift >= 0; shift -= 8) input.push_back(static_cast<unsigned char>(counter >> shift));
        const auto digest = Sha1(input);
        mask.insert(mask.end(), digest.begin(), digest.end());
    }
    mask.resize(length);
    return mask;
}

static Bytes Recover(const Bytes& encrypted) {
    TestKey privateKey(Hex(CavpModulus), Hex(CavpPrivateExponent));
    Bytes block(256);
    Require(RSA_public_decrypt_nid_postfix(256, encrypted.data(), block.data(), privateKey.rsa, PaddingNone) == 256);
    return block;
}

static bool Pkcs1Block(const Bytes& block, const Bytes& message) {
    if (block[0] != 0 || block[1] != 2) return false;
    const std::size_t separator = block.size() - message.size() - 1;
    for (std::size_t index = 2; index < separator; ++index)
        if (!block[index]) return false;
    return block[separator] == 0 && Bytes(block.begin() + static_cast<long>(separator) + 1, block.end()) == message;
}

static bool OaepBlock(const Bytes& block, const Bytes& message) {
    if (block[0]) return false;
    const Bytes maskedSeed(block.begin() + 1, block.begin() + 21);
    const Bytes maskedData(block.begin() + 21, block.end());
    auto seed = maskedSeed;
    const auto seedMask = Mgf1(maskedData, 20);
    for (std::size_t index = 0; index < 20; ++index) seed[index] ^= seedMask[index];
    auto data = maskedData;
    const auto dataMask = Mgf1(seed, data.size());
    for (std::size_t index = 0; index < data.size(); ++index) data[index] ^= dataMask[index];
    if (Bytes(data.begin(), data.begin() + 20) != Sha1({})) return false;
    const std::size_t one = data.size() - message.size() - 1;
    for (std::size_t index = 20; index < one; ++index)
        if (data[index]) return false;
    return data[one] == 1 && Bytes(data.begin() + static_cast<long>(one) + 1, data.end()) == message;
}

static void TestPublicDecrypt() {
    const auto spki = Hex(CavpSpki);
    auto* rsa = Decode(spki, static_cast<long>(spki.size()));
    const auto signature = Hex(CavpSignature);
    const auto digestInfo = Concat({Hex("3031300d060960864801650304020105000420"), Hex(CavpDigest)});
    Bytes out(300, 0xEE);
    Require(RSA_public_decrypt_nid_postfix(256, signature.data(), out.data(), rsa, PaddingPkcs1) == 51);
    Require(Bytes(out.begin(), out.begin() + 51) == digestInfo && out[51] == 0xEE);
    Require(RSA_public_decrypt_nid_postfix(256, signature.data(), out.data(), rsa, PaddingNone) == 256);
    Require(out[0] == 0 && out[1] == 1 && out[2] == 0xFF && out[204] == 0 && Bytes(out.begin() + 205, out.begin() + 256) == digestInfo);
    auto tampered = signature;
    tampered[255] ^= 1;
    Require(RSA_public_decrypt_nid_postfix(256, tampered.data(), out.data(), rsa, PaddingPkcs1) == -1);
    Require(RSA_public_decrypt_nid_postfix(256, tampered.data(), out.data(), rsa, PaddingNone) == 256);
    Require(RSA_public_decrypt_nid_postfix(257, Concat({signature, {0}}).data(), out.data(), rsa, PaddingPkcs1) == -1);
    Require(RSA_public_decrypt_nid_postfix(-1, signature.data(), out.data(), rsa, PaddingPkcs1) == -1);
    const Bytes large(256, 0xFF);
    Require(RSA_public_decrypt_nid_postfix(256, large.data(), out.data(), rsa, PaddingNone) == -1);
    Require(RSA_public_decrypt_nid_postfix(256, Hex(CavpModulus).data(), out.data(), rsa, PaddingNone) == -1);
    std::memset(out.data(), 0xEE, out.size());
    Require(RSA_public_decrypt_nid_postfix(0, nullptr, out.data(), rsa, PaddingNone) == 256);
    Require(Zero(out.data(), 256) && out[256] == 0xEE);
    Require(RSA_public_decrypt_nid_postfix(0, nullptr, out.data(), rsa, PaddingPkcs1) == -1);
    for (int padding : {0, PaddingSslv23, PaddingOaep, 6, 7}) Require(RSA_public_decrypt_nid_postfix(256, signature.data(), out.data(), rsa, padding) == -1);
    RequireThrows([&] { RSA_public_decrypt_nid_postfix(256, nullptr, out.data(), rsa, PaddingPkcs1); });
    RequireThrows([&] { RSA_public_decrypt_nid_postfix(256, signature.data(), nullptr, rsa, PaddingPkcs1); });
    RSA_free_nid_postfix(rsa);
}

static void TestPublicEncrypt() {
    const auto spki = Hex(CavpSpki);
    auto* rsa = Decode(spki, static_cast<long>(spki.size()));
    const auto message = Pattern(32, 5);
    Bytes first(256);
    Bytes second(256);
    for (int padding : {PaddingPkcs1, PaddingOaep}) {
        Require(RSA_public_encrypt_nid_postfix(32, message.data(), first.data(), rsa, padding) == 256);
        Require(RSA_public_encrypt_nid_postfix(32, message.data(), second.data(), rsa, padding) == 256);
        Require(first != second);
        for (const auto* encrypted : {&first, &second}) {
            const auto block = Recover(*encrypted);
            Require(padding == PaddingPkcs1 ? Pkcs1Block(block, message) : OaepBlock(block, message));
        }
    }
    for (auto [padding, longest] : {std::pair {PaddingPkcs1, 245}, std::pair {PaddingOaep, 214}}) {
        const auto longMessage = Pattern(static_cast<std::size_t>(longest) + 1, 9);
        Require(RSA_public_encrypt_nid_postfix(longest, longMessage.data(), first.data(), rsa, padding) == 256);
        const Bytes accepted(longMessage.begin(), longMessage.begin() + longest);
        Require(padding == PaddingPkcs1 ? Pkcs1Block(Recover(first), accepted) : OaepBlock(Recover(first), accepted));
        Require(RSA_public_encrypt_nid_postfix(longest + 1, longMessage.data(), first.data(), rsa, padding) == -1);
        Require(RSA_public_encrypt_nid_postfix(0, nullptr, first.data(), rsa, padding) == 256);
        Require(padding == PaddingPkcs1 ? Pkcs1Block(Recover(first), {}) : OaepBlock(Recover(first), {}));
        RequireThrows([&] { RSA_public_encrypt_nid_postfix(-1, message.data(), first.data(), rsa, padding); });
    }
    auto raw = Concat({{0x00}, Pattern(255, 3)});
    Require(RSA_public_encrypt_nid_postfix(256, raw.data(), first.data(), rsa, PaddingNone) == 256 && Recover(first) == raw);
    Require(RSA_public_encrypt_nid_postfix(255, raw.data(), first.data(), rsa, PaddingNone) == -1);
    Require(RSA_public_encrypt_nid_postfix(257, Concat({raw, {0}}).data(), first.data(), rsa, PaddingNone) == -1);
    Require(RSA_public_encrypt_nid_postfix(-1, raw.data(), first.data(), rsa, PaddingNone) == -1);
    Require(RSA_public_encrypt_nid_postfix(256, Hex(CavpModulus).data(), first.data(), rsa, PaddingNone) == -1);
    for (int padding : {0, PaddingX931, 6, 7}) Require(RSA_public_encrypt_nid_postfix(32, message.data(), first.data(), rsa, padding) == -1);
    RequireThrows([&] { RSA_public_encrypt_nid_postfix(32, message.data(), nullptr, rsa, PaddingPkcs1); });
    RequireThrows([&] { RSA_public_encrypt_nid_postfix(32, nullptr, first.data(), rsa, PaddingPkcs1); });
    RSA_free_nid_postfix(rsa);
}

struct Crypts {
    int calls;
    int length;
    const unsigned char* from;
    unsigned char* to;
    LibreSslRsa* rsa;
    int padding;
};

static Crypts crypts;

static int APS5_VABI RecordCrypt(int length, const unsigned char* from, unsigned char* to, LibreSslRsa* rsa, int padding) {
    crypts = {crypts.calls + 1, length, from, to, rsa, padding};
    return 77;
}

static void TestPublicKeyChecks() {
    const auto modulus = Hex(CavpModulus);
    const auto message = Pattern(256, 1);
    Bytes out(4096);
    {
        TestKey key(modulus, {});
        Require(RSA_public_encrypt_nid_postfix(256, Concat({{0}, Pattern(255, 1)}).data(), out.data(), key.rsa, PaddingNone) == 256);
        Require(Zero(out.data(), 255) && out[255] == 1);
    }
    {
        TestKey key(modulus, modulus);
        Require(RSA_public_encrypt_nid_postfix(32, message.data(), out.data(), key.rsa, PaddingPkcs1) == -1);
        Require(RSA_public_decrypt_nid_postfix(256, message.data(), out.data(), key.rsa, PaddingNone) == -1);
    }
    {
        auto even = modulus;
        even.back() &= 0xFE;
        TestKey key(even, CavpExponent);
        Require(RSA_public_encrypt_nid_postfix(32, message.data(), out.data(), key.rsa, PaddingPkcs1) == -1);
        Require(RSA_public_decrypt_nid_postfix(255, message.data(), out.data(), key.rsa, PaddingNone) == -1);
    }
    Bytes large(385, 0);
    large[0] = 0x01;
    large.back() = 0x03;
    Bytes wideExponent(9, 0);
    wideExponent[0] = 0x01;
    wideExponent.back() = 0x01;
    {
        TestKey key(large, wideExponent);
        Require(RSA_public_encrypt_nid_postfix(385, Bytes(385, 0).data(), out.data(), key.rsa, PaddingNone) == -1);
        Require(RSA_public_decrypt_nid_postfix(1, message.data(), out.data(), key.rsa, PaddingNone) == -1);
    }
    {
        TestKey key(large, Bytes(wideExponent.begin() + 1, wideExponent.end()));
        Require(RSA_public_decrypt_nid_postfix(1, message.data(), out.data(), key.rsa, PaddingNone) == 385);
    }
    Bytes huge(2049, 0);
    huge[0] = 0x01;
    huge.back() = 0x01;
    {
        TestKey key(huge, {0x03});
        Require(RSA_public_decrypt_nid_postfix(1, message.data(), out.data(), key.rsa, PaddingNone) == -1);
    }
    {
        TestKey key(modulus, CavpExponent);
        const auto* defaultMethod = key.rsa->meth;
        LibreSslRsaMethod method = *defaultMethod;
        method.operations[0] = reinterpret_cast<void*>(RecordCrypt);
        method.operations[1] = reinterpret_cast<void*>(RecordCrypt);
        key.rsa->meth = &method;
        crypts = {};
        Require(RSA_public_encrypt_nid_postfix(5, message.data(), out.data(), key.rsa, 9) == 77);
        Require(crypts.calls == 1 && crypts.length == 5 && crypts.from == message.data() && crypts.to == out.data() && crypts.rsa == key.rsa && crypts.padding == 9);
        Require(RSA_public_decrypt_nid_postfix(6, message.data(), out.data(), key.rsa, 8) == 77 && crypts.calls == 2 && crypts.length == 6);
        method = *defaultMethod;
        method.operations[0] = nullptr;
        RequireThrows([&] { RSA_public_encrypt_nid_postfix(32, message.data(), out.data(), key.rsa, PaddingPkcs1); });
        key.rsa->meth = nullptr;
        RequireThrows([&] { RSA_public_decrypt_nid_postfix(32, message.data(), out.data(), key.rsa, PaddingPkcs1); });
        key.rsa->meth = defaultMethod;
        key.rsa->e = nullptr;
        RequireThrows([&] { RSA_public_decrypt_nid_postfix(32, message.data(), out.data(), key.rsa, PaddingPkcs1); });
    }
    RequireThrows([&] { RSA_public_encrypt_nid_postfix(32, message.data(), out.data(), nullptr, PaddingPkcs1); });
}

constexpr int DigestSha1 = 64;
constexpr int DigestMd5Sha1 = 114;
constexpr int DigestSha256 = 672;
constexpr int RsaFlagSignVerify = 0x0040;

static Bytes Sign(const Bytes& encoded) {
    return Recover(Concat({{0x00, 0x01}, Bytes(253 - encoded.size(), 0xFF), {0x00}, encoded}));
}

static Bytes DigestInfo(const Bytes& object, const Bytes& parameter, const Bytes& digest) {
    return Encode(0x30, Concat({Encode(0x30, Concat({Encode(0x06, object), parameter})), Encode(0x04, digest)}));
}

static int Verify(int type, const Bytes& digest, const Bytes& signature, LibreSslRsa* rsa) {
    return RSA_verify_nid_postfix(type, digest.data(), static_cast<unsigned>(digest.size()), signature.data(), static_cast<unsigned>(signature.size()), rsa);
}

struct Verifies {
    int calls;
    int type;
    const unsigned char* digest;
    unsigned digestLength;
    const unsigned char* signature;
    unsigned signatureLength;
    const LibreSslRsa* rsa;
};

static Verifies verifies;

static int APS5_VABI RecordVerify(int type, const unsigned char* digest, unsigned int digestLength, const unsigned char* signature, unsigned int signatureLength, const LibreSslRsa* rsa) {
    verifies = {verifies.calls + 1, type, digest, digestLength, signature, signatureLength, rsa};
    return 55;
}

static void TestVerify() {
    const auto spki = Hex(CavpSpki);
    auto* rsa = Decode(spki, static_cast<long>(spki.size()));
    const auto digest = Hex(CavpDigest);
    const auto signature = Hex(CavpSignature);
    Require(Verify(DigestSha256, digest, signature, rsa) == 1);
    auto tampered = signature;
    tampered[100] ^= 0x10;
    Require(Verify(DigestSha256, digest, tampered, rsa) == 0);
    auto wrong = digest;
    wrong[31] ^= 1;
    Require(Verify(DigestSha256, wrong, signature, rsa) == 0);
    Require(Verify(DigestSha256, Bytes(digest.begin(), digest.end() - 1), signature, rsa) == 0);
    Require(Verify(DigestSha256, Concat({digest, {0}}), signature, rsa) == 0);
    for (int type : {DigestSha1, 0, 1, 673, 675, -1, 956, DigestMd5Sha1}) Require(Verify(type, digest, signature, rsa) == 0);
    Require(Verify(DigestSha256, digest, Bytes(signature.begin() + 1, signature.end()), rsa) == 0);
    Require(Verify(DigestSha256, digest, Concat({{0}, signature}), rsa) == 0);
    const auto ssl = Pattern(36, 7);
    const auto sslSignature = Sign(ssl);
    Require(Verify(DigestMd5Sha1, ssl, sslSignature, rsa) == 1);
    Require(Verify(DigestMd5Sha1, Bytes(ssl.begin(), ssl.end() - 1), sslSignature, rsa) == 0);
    auto otherSsl = ssl;
    otherSsl[35] ^= 1;
    Require(Verify(DigestMd5Sha1, otherSsl, sslSignature, rsa) == 0);
    Require(Verify(DigestMd5Sha1, ssl, Sign(Bytes(ssl.begin(), ssl.end() - 1)), rsa) == 0);
    Require(Verify(DigestSha256, ssl, sslSignature, rsa) == 0);
    RequireThrows([&] { RSA_verify_nid_postfix(DigestMd5Sha1, nullptr, 36, sslSignature.data(), 256, rsa); });
    const auto sha256 = Hex("608648016503040201");
    Require(Verify(DigestSha256, digest, Sign(DigestInfo(sha256, Null, digest)), rsa) == 1);
    Require(Verify(DigestSha256, digest, Sign(DigestInfo(sha256, {}, digest)), rsa) == 1);
    Require(Verify(DigestSha256, digest, Sign(DigestInfo(sha256, {0x05, 0x81, 0x00}, digest)), rsa) == 1);
    for (const auto& parameter : {Bytes {0x04, 0x00}, Bytes {0x05, 0x01, 0x00}, Bytes {0x30, 0x00}, Bytes {0x80, 0x00}, Bytes {0x01, 0x01, 0x00}, Concat({Null, Null})})
        Require(Verify(DigestSha256, digest, Sign(DigestInfo(sha256, parameter, digest)), rsa) == 0);
    const auto algorithm = Encode(0x30, Concat({Encode(0x06, sha256), Null}));
    Require(Verify(DigestSha256, digest, Sign(Concat({DigestInfo(sha256, Null, digest), {0x00}})), rsa) == 0);
    Require(Verify(DigestSha256, digest, Sign(Encode(0x30, Concat({algorithm, Encode(0x04, digest), Null}))), rsa) == 0);
    Require(Verify(DigestSha256, digest, Sign(Encode(0x30, algorithm)), rsa) == 0);
    Require(Verify(DigestSha256, digest, Sign(Encode(0x30, Concat({algorithm, Encode(0x03, digest)}))), rsa) == 0);
    Require(Verify(DigestSha256, digest, Sign(Encode(0x31, Concat({algorithm, Encode(0x04, digest)}))), rsa) == 0);
    Require(Verify(DigestSha256, digest, Sign(Encode(0x04, digest)), rsa) == 0);
    Require(Verify(0, digest, Sign(DigestInfo({}, Null, digest)), rsa) == 0);
    Require(Verify(0, digest, Sign(DigestInfo({0x2A, 0x80}, Null, digest)), rsa) == 0);
    const std::pair<int, const char*> objects[] {{3, "2a864886f70d0202"}, {4, "2a864886f70d0205"}, {41, "2b0e030212"}, {64, "2b0e03021a"}, {95, "55080365"},
        {117, "2b24030201"}, {257, "2a864886f70d0204"}, {672, "608648016503040201"}, {673, "608648016503040202"}, {674, "608648016503040203"},
        {675, "608648016503040204"}, {804, "28cf06030037"}, {809, "2a8503020209"}, {941, "2a85030701010202"}, {942, "2a85030701010203"}};
    const auto hash = Pattern(20, 11);
    for (std::size_t index = 0; index < std::size(objects); ++index) {
        const auto encoded = Sign(DigestInfo(Hex(objects[index].second), Null, hash));
        Require(Verify(objects[index].first, hash, encoded, rsa) == 1);
        Require(Verify(objects[(index + 1) % std::size(objects)].first, hash, encoded, rsa) == 0);
        Require(Verify(1, hash, encoded, rsa) == 0);
    }
    const auto unknown = Sign(DigestInfo({0x2A, 0x03, 0x04}, Null, hash));
    for (int type : {-1, 956, 2000, DigestSha1, DigestSha256}) Require(Verify(type, hash, unknown, rsa) == 0);
    const auto* defaultMethod = rsa->meth;
    LibreSslRsaMethod method = *defaultMethod;
    method.verify = reinterpret_cast<void*>(RecordVerify);
    rsa->meth = &method;
    verifies = {};
    Require(Verify(DigestSha256, digest, signature, rsa) == 1 && verifies.calls == 0);
    rsa->flags |= RsaFlagSignVerify;
    Require(Verify(5, wrong, tampered, rsa) == 55);
    Require(verifies.calls == 1 && verifies.type == 5 && verifies.digest == wrong.data() && verifies.digestLength == 32);
    Require(verifies.signature == tampered.data() && verifies.signatureLength == 256 && verifies.rsa == rsa);
    method.verify = nullptr;
    Require(Verify(DigestSha256, digest, signature, rsa) == 1 && verifies.calls == 1);
    rsa->meth = nullptr;
    RequireThrows([&] { Verify(DigestSha256, digest, signature, rsa); });
    rsa->flags &= ~RsaFlagSignVerify;
    rsa->meth = defaultMethod;
    RequireThrows([&] { Verify(DigestSha256, digest, signature, nullptr); });
    RequireThrows([&] { RSA_verify_nid_postfix(DigestSha256, nullptr, 32, signature.data(), 256, rsa); });
    Require(RSA_verify_nid_postfix(DigestSha256, nullptr, 32, tampered.data(), 256, rsa) == 0);
    Require(RSA_verify_nid_postfix(DigestSha256, nullptr, 0, signature.data(), 256, rsa) == 0);
    RequireThrows([&] { RSA_verify_nid_postfix(DigestSha256, digest.data(), 32, nullptr, 256, rsa); });
    Require(RSA_verify_nid_postfix(DigestSha256, digest.data(), 32, nullptr, 255, rsa) == 0);
    RSA_free_nid_postfix(rsa);
}

int main() {
    InitializeHeap();
    TestFlags();
    TestSize();
    TestNew();
    TestFree();
    TestPublicKeyInfo();
    TestPublicKeyInfoVariants();
    TestPublicDecrypt();
    TestPublicEncrypt();
    TestPublicKeyChecks();
    TestVerify();
    std::puts("libSceLibreSsl RSA tests passed");
}
