#include "prx/libSceLibreSsl/include/Api.hpp"
#include "prx/libSceLibreSsl/include/CipherList.hpp"
#include "prx/libc/include/Api.hpp"
#include "prx/libc/include/ApplicationHeap.hpp"
#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <initializer_list>
#include <iterator>
#include <set>
#include <source_location>
#include <stdexcept>
#include <string>
#include <vector>

struct TestCtxInternal {
    unsigned char beforeCipherListById[256];
    void* cipherListById;
    unsigned char rest[200];
};

struct TestCtx {
    const void* method;
    void* cipherList;
    unsigned char beforeInternal[96];
    TestCtxInternal* internal;
};

struct CipherView {
    std::int32_t valid;
    const char* name;
    std::uint64_t id;
    unsigned char rest[64];
};

static_assert(sizeof(TestCtxInternal) == 464 && offsetof(TestCtxInternal, cipherListById) == 256);
static_assert(sizeof(TestCtxInternal) == sizeof(LibreSslCtxInternal) && offsetof(TestCtxInternal, cipherListById) == offsetof(LibreSslCtxInternal, cipherListById));
static_assert(sizeof(TestCtx) == 120 && offsetof(TestCtx, cipherList) == 8 && offsetof(TestCtx, internal) == 112);
static_assert(sizeof(TestCtx) == sizeof(LibreSslCtx) && offsetof(TestCtx, cipherList) == offsetof(LibreSslCtx, cipherList) && offsetof(TestCtx, internal) == offsetof(LibreSslCtx, internal));
static_assert(sizeof(CipherView) == 88 && offsetof(CipherView, name) == 8 && offsetof(CipherView, id) == 16);

static constexpr std::uint64_t ErrorCode(int reason) {
    return (20ull << 24) | (0xfffull << 12) | static_cast<std::uint64_t>(reason);
}

static constexpr std::uint64_t NoCipherMatch = ErrorCode(185);
static constexpr std::uint64_t InvalidCommand = ErrorCode(280);

static_assert(NoCipherMatch == 0x14FFF0B9 && InvalidCommand == 0x14FFF118);

struct ExpectedCipher {
    std::uint64_t id;
    const char* name;
    int strengthBits;
    int algorithmBits;
};

static const ExpectedCipher table[] = {
    {0x03000001, "NULL-MD5", 0, 0},
    {0x03000002, "NULL-SHA", 0, 0},
    {0x03000004, "RC4-MD5", 128, 128},
    {0x03000005, "RC4-SHA", 128, 128},
    {0x03000009, "DES-CBC-SHA", 56, 56},
    {0x0300000A, "DES-CBC3-SHA", 112, 168},
    {0x03000012, "EDH-DSS-DES-CBC-SHA", 56, 56},
    {0x03000013, "EDH-DSS-DES-CBC3-SHA", 112, 168},
    {0x03000015, "EDH-RSA-DES-CBC-SHA", 56, 56},
    {0x03000016, "EDH-RSA-DES-CBC3-SHA", 112, 168},
    {0x03000018, "ADH-RC4-MD5", 128, 128},
    {0x0300001A, "ADH-DES-CBC-SHA", 56, 56},
    {0x0300001B, "ADH-DES-CBC3-SHA", 112, 168},
    {0x0300002F, "AES128-SHA", 128, 128},
    {0x03000032, "DHE-DSS-AES128-SHA", 128, 128},
    {0x03000033, "DHE-RSA-AES128-SHA", 128, 128},
    {0x03000034, "ADH-AES128-SHA", 128, 128},
    {0x03000035, "AES256-SHA", 256, 256},
    {0x03000038, "DHE-DSS-AES256-SHA", 256, 256},
    {0x03000039, "DHE-RSA-AES256-SHA", 256, 256},
    {0x0300003A, "ADH-AES256-SHA", 256, 256},
    {0x0300003B, "NULL-SHA256", 0, 0},
    {0x0300003C, "AES128-SHA256", 128, 128},
    {0x0300003D, "AES256-SHA256", 256, 256},
    {0x03000040, "DHE-DSS-AES128-SHA256", 128, 128},
    {0x03000041, "CAMELLIA128-SHA", 128, 128},
    {0x03000044, "DHE-DSS-CAMELLIA128-SHA", 128, 128},
    {0x03000045, "DHE-RSA-CAMELLIA128-SHA", 128, 128},
    {0x03000046, "ADH-CAMELLIA128-SHA", 128, 128},
    {0x03000067, "DHE-RSA-AES128-SHA256", 128, 128},
    {0x0300006A, "DHE-DSS-AES256-SHA256", 256, 256},
    {0x0300006B, "DHE-RSA-AES256-SHA256", 256, 256},
    {0x0300006C, "ADH-AES128-SHA256", 128, 128},
    {0x0300006D, "ADH-AES256-SHA256", 256, 256},
    {0x03000081, "GOST2001-GOST89-GOST89", 256, 256},
    {0x03000083, "GOST2001-NULL-GOST94", 0, 0},
    {0x03000084, "CAMELLIA256-SHA", 256, 256},
    {0x03000087, "DHE-DSS-CAMELLIA256-SHA", 256, 256},
    {0x03000088, "DHE-RSA-CAMELLIA256-SHA", 256, 256},
    {0x03000089, "ADH-CAMELLIA256-SHA", 256, 256},
    {0x0300009C, "AES128-GCM-SHA256", 128, 128},
    {0x0300009D, "AES256-GCM-SHA384", 256, 256},
    {0x0300009E, "DHE-RSA-AES128-GCM-SHA256", 128, 128},
    {0x0300009F, "DHE-RSA-AES256-GCM-SHA384", 256, 256},
    {0x030000A2, "DHE-DSS-AES128-GCM-SHA256", 128, 128},
    {0x030000A3, "DHE-DSS-AES256-GCM-SHA384", 256, 256},
    {0x030000A6, "ADH-AES128-GCM-SHA256", 128, 128},
    {0x030000A7, "ADH-AES256-GCM-SHA384", 256, 256},
    {0x030000BA, "CAMELLIA128-SHA256", 128, 128},
    {0x030000BD, "DHE-DSS-CAMELLIA128-SHA256", 128, 128},
    {0x030000BE, "DHE-RSA-CAMELLIA128-SHA256", 128, 128},
    {0x030000BF, "ADH-CAMELLIA128-SHA256", 128, 128},
    {0x030000C0, "CAMELLIA256-SHA256", 256, 256},
    {0x030000C3, "DHE-DSS-CAMELLIA256-SHA256", 256, 256},
    {0x030000C4, "DHE-RSA-CAMELLIA256-SHA256", 256, 256},
    {0x030000C5, "ADH-CAMELLIA256-SHA256", 256, 256},
    {0x0300C006, "ECDHE-ECDSA-NULL-SHA", 0, 0},
    {0x0300C007, "ECDHE-ECDSA-RC4-SHA", 128, 128},
    {0x0300C008, "ECDHE-ECDSA-DES-CBC3-SHA", 112, 168},
    {0x0300C009, "ECDHE-ECDSA-AES128-SHA", 128, 128},
    {0x0300C00A, "ECDHE-ECDSA-AES256-SHA", 256, 256},
    {0x0300C010, "ECDHE-RSA-NULL-SHA", 0, 0},
    {0x0300C011, "ECDHE-RSA-RC4-SHA", 128, 128},
    {0x0300C012, "ECDHE-RSA-DES-CBC3-SHA", 112, 168},
    {0x0300C013, "ECDHE-RSA-AES128-SHA", 128, 128},
    {0x0300C014, "ECDHE-RSA-AES256-SHA", 256, 256},
    {0x0300C015, "AECDH-NULL-SHA", 0, 0},
    {0x0300C016, "AECDH-RC4-SHA", 128, 128},
    {0x0300C017, "AECDH-DES-CBC3-SHA", 112, 168},
    {0x0300C018, "AECDH-AES128-SHA", 128, 128},
    {0x0300C019, "AECDH-AES256-SHA", 256, 256},
    {0x0300C023, "ECDHE-ECDSA-AES128-SHA256", 128, 128},
    {0x0300C024, "ECDHE-ECDSA-AES256-SHA384", 256, 256},
    {0x0300C027, "ECDHE-RSA-AES128-SHA256", 128, 128},
    {0x0300C028, "ECDHE-RSA-AES256-SHA384", 256, 256},
    {0x0300C02B, "ECDHE-ECDSA-AES128-GCM-SHA256", 128, 128},
    {0x0300C02C, "ECDHE-ECDSA-AES256-GCM-SHA384", 256, 256},
    {0x0300C02F, "ECDHE-RSA-AES128-GCM-SHA256", 128, 128},
    {0x0300C030, "ECDHE-RSA-AES256-GCM-SHA384", 256, 256},
    {0x0300CC13, "ECDHE-RSA-CHACHA20-POLY1305-OLD", 256, 256},
    {0x0300CC14, "ECDHE-ECDSA-CHACHA20-POLY1305-OLD", 256, 256},
    {0x0300CC15, "DHE-RSA-CHACHA20-POLY1305-OLD", 256, 256},
    {0x0300CCA8, "ECDHE-RSA-CHACHA20-POLY1305", 256, 256},
    {0x0300CCA9, "ECDHE-ECDSA-CHACHA20-POLY1305", 256, 256},
    {0x0300CCAA, "DHE-RSA-CHACHA20-POLY1305", 256, 256},
    {0x0300FF85, "GOST2012256-GOST89-GOST89", 256, 256},
    {0x0300FF87, "GOST2012256-NULL-STREEBOG256", 0, 0},
};

static constexpr int TableSize = static_cast<int>(std::size(table));

static_assert(TableSize == 87);

static LibreSslCtx* Context(TestCtx* ctx) {
    return reinterpret_cast<LibreSslCtx*>(ctx);
}

static void Require(bool condition, std::source_location location = std::source_location::current()) {
    if (condition) return;
    std::fprintf(stderr, "LibreSSL cipher list check failed at line %u\n", location.line());
    std::abort();
}

template<typename TAction> static void RequireThrows(TAction action, std::source_location location = std::source_location::current()) {
    try { action(); } catch (const std::exception&) { return; }
    Require(false, location);
}

static std::uint64_t IdOf(const void* cipher) {
    return static_cast<const CipherView*>(cipher)->id;
}

static const char* NameOf(const void* cipher) {
    return SSL_CIPHER_get_name_nid_postfix(cipher);
}

static std::vector<std::uint64_t> DrainErrors() {
    std::vector<std::uint64_t> codes;
    while (const std::uint64_t code = ERR_get_error_nid_postfix()) codes.push_back(code);
    return codes;
}

static bool Contains(const std::string& text, const char* part) {
    return text.find(part) != std::string::npos;
}

static bool StartsWith(const std::string& text, const char* prefix) {
    return text.rfind(prefix, 0) == 0;
}

static void FreeStack(void* pointer) {
    auto* stack = static_cast<LibreSslStack*>(pointer);
    if (!stack) return;
    free_nid_postfix(stack->data);
    free_nid_postfix(stack);
}

static std::vector<std::string> Names(const LibreSslStack* stack) {
    std::vector<std::string> names;
    for (int index = 0; index < stack->num; ++index) names.emplace_back(NameOf(stack->data[index]));
    return names;
}

static int ExpectedAllocation(int num) {
    int allocation = 4;
    while (allocation <= num) allocation *= 2;
    return allocation;
}

static int Sign(std::uint64_t left, std::uint64_t right) {
    return left > right ? 1 : left < right ? -1 : 0;
}

struct Fixture {
    TestCtxInternal internal {};
    TestCtx ctx {};

    Fixture() { ctx.internal = &internal; }
    Fixture(const Fixture&) = delete;
    Fixture& operator=(const Fixture&) = delete;
    ~Fixture() {
        FreeStack(ctx.cipherList);
        FreeStack(internal.cipherListById);
    }

    int Set(const std::string& rules) {
        std::string padded = rules;
        padded.append(16, '\0');
        return SSL_CTX_set_cipher_list_nid_postfix(Context(&ctx), padded.c_str());
    }

    const LibreSslStack* Ordered() const { return static_cast<const LibreSslStack*>(ctx.cipherList); }
    const LibreSslStack* ById() const { return static_cast<const LibreSslStack*>(internal.cipherListById); }
    std::vector<std::string> OrderedNames() const { return Names(Ordered()); }
};

static void RequireShape(const Fixture& fixture, std::source_location location = std::source_location::current()) {
    const LibreSslStack* ordered = fixture.Ordered();
    const LibreSslStack* byId = fixture.ById();
    Require(ordered && byId && ordered != byId, location);
    Require(ordered->data && byId->data && ordered->data != byId->data, location);
    Require(ordered->num == byId->num && ordered->num >= 0, location);
    Require(ordered->sorted == 0 && ordered->comp == nullptr, location);
    Require(byId->sorted == 1 && byId->comp != nullptr, location);
    Require(ordered->numAlloc == ExpectedAllocation(ordered->num) && byId->numAlloc == ordered->numAlloc, location);
    for (int index = 0; index < ordered->num; ++index) {
        Require(ordered->data[index] && byId->data[index], location);
        const void* element = ordered->data[index];
        Require(element == LibreSslCipherById_nid_no_patch(IdOf(element)), location);
        Require(std::find(byId->data, byId->data + byId->num, ordered->data[index]) != byId->data + byId->num, location);
        for (int other = 0; other < index; ++other) Require(ordered->data[other] != ordered->data[index], location);
        if (index > 0) Require(IdOf(byId->data[index - 1]) < IdOf(byId->data[index]), location);
    }
    for (int left = 0; left < byId->num; ++left) {
        for (int right = 0; right < byId->num; ++right) {
            const int expected = Sign(IdOf(byId->data[left]), IdOf(byId->data[right]));
            Require(byId->comp(&byId->data[left], &byId->data[right]) == expected, location);
            Require(byId->comp(&ordered->data[left], &ordered->data[right]) == Sign(IdOf(ordered->data[left]), IdOf(ordered->data[right])), location);
        }
    }
}

static void RequireList(Fixture& fixture, const std::string& rules, const std::vector<std::string>& expected, std::source_location location = std::source_location::current()) {
    ERR_clear_error_nid_postfix();
    Require(fixture.Set(rules) == 1, location);
    Require(DrainErrors().empty(), location);
    Require(fixture.OrderedNames() == expected, location);
    RequireShape(fixture, location);
}

static void RequireNoMatch(Fixture& fixture, const std::string& rules, std::source_location location = std::source_location::current()) {
    ERR_clear_error_nid_postfix();
    Require(fixture.Set(rules) == 0, location);
    Require(DrainErrors() == std::vector<std::uint64_t>{NoCipherMatch}, location);
    Require(fixture.Ordered() && fixture.Ordered()->num == 0 && fixture.ById() && fixture.ById()->num == 0, location);
    RequireShape(fixture, location);
}

static void TestTable() {
    Require(LibreSslCipherCount_nid_no_patch() == TableSize);
    Require(std::is_sorted(std::begin(table), std::end(table), [](const ExpectedCipher& left, const ExpectedCipher& right) { return left.id < right.id; }));
    std::set<const void*> pointers;
    std::set<std::string> names;
    for (int position = 0; position < TableSize; ++position) {
        const ExpectedCipher& expected = table[position];
        const void* cipher = LibreSslCipherAt_nid_no_patch(static_cast<unsigned int>(TableSize - 1 - position));
        Require(cipher != nullptr);
        Require(cipher == LibreSslCipherAt_nid_no_patch(static_cast<unsigned int>(TableSize - 1 - position)));
        Require(cipher == LibreSslCipherById_nid_no_patch(expected.id));
        Require(std::string(NameOf(cipher)) == expected.name);
        Require(IdOf(cipher) == expected.id);
        Require(static_cast<const CipherView*>(cipher)->valid == 1);
        std::int32_t algorithmBits = -1;
        Require(SSL_CIPHER_get_bits_nid_postfix(cipher, &algorithmBits) == expected.strengthBits);
        Require(algorithmBits == expected.algorithmBits);
        Require(pointers.insert(cipher).second && names.insert(expected.name).second);
    }
    Require(std::string(NameOf(LibreSslCipherAt_nid_no_patch(0))) == "GOST2012256-NULL-STREEBOG256");
    Require(std::string(NameOf(LibreSslCipherAt_nid_no_patch(TableSize - 1))) == "NULL-MD5");
    Require(LibreSslCipherAt_nid_no_patch(0) == LibreSslCipherById_nid_no_patch(0x0300FF87));
    Require(LibreSslCipherAt_nid_no_patch(TableSize - 1) == LibreSslCipherById_nid_no_patch(0x03000001));
    for (unsigned int index : {87u, 88u, 100u, 1000u, 0x7fffffffu, 0x80000000u, 0xffffffffu}) Require(LibreSslCipherAt_nid_no_patch(index) == nullptr);
}

static void TestKnownCiphers() {
    struct Known {
        std::uint64_t id;
        const char* name;
        int strengthBits;
        int algorithmBits;
    };
    for (const Known& known : {Known{0x0300C030, "ECDHE-RSA-AES256-GCM-SHA384", 256, 256}, Known{0x0300002F, "AES128-SHA", 128, 128},
                               Known{0x0300CCA8, "ECDHE-RSA-CHACHA20-POLY1305", 256, 256}, Known{0x0300000A, "DES-CBC3-SHA", 112, 168},
                               Known{0x03000009, "DES-CBC-SHA", 56, 56}, Known{0x03000001, "NULL-MD5", 0, 0}}) {
        const void* cipher = LibreSslCipherById_nid_no_patch(known.id);
        Require(cipher != nullptr);
        Require(std::string(SSL_CIPHER_get_name_nid_postfix(cipher)) == known.name);
        std::int32_t algorithmBits = -1;
        Require(SSL_CIPHER_get_bits_nid_postfix(cipher, &algorithmBits) == known.strengthBits && algorithmBits == known.algorithmBits);
        Require(SSL_CIPHER_get_bits_nid_postfix(cipher, nullptr) == known.strengthBits);
    }
    for (std::uint64_t id : {0x0ull, 0x1ull, 0x2Full, 0x03000000ull, 0x03000003ull, 0x03000006ull, 0x0300002Eull, 0x03000030ull, 0x0300FF86ull, 0x0300FF88ull, 0x0300FFFFull,
                             0x0200002Full, 0x0000002Full, 0x1300002Full, 0x8300002Full, 0x10300FF87ull, 0x10300002Full, 0xffffffffffffffffull, 0xffffffff0300002Full}) {
        Require(LibreSslCipherById_nid_no_patch(id) == nullptr);
    }
}

static void TestArguments() {
    Fixture nullInternal;
    nullInternal.ctx.internal = nullptr;
    RequireThrows([] { SSL_CTX_set_cipher_list_nid_postfix(nullptr, "AES128-SHA"); });
    RequireThrows([] { SSL_CTX_set_cipher_list_nid_postfix(nullptr, nullptr); });
    RequireThrows([&] { SSL_CTX_set_cipher_list_nid_postfix(Context(&nullInternal.ctx), "AES128-SHA"); });
    RequireThrows([&] { SSL_CTX_set_cipher_list_nid_postfix(Context(&nullInternal.ctx), nullptr); });
    Require(nullInternal.ctx.cipherList == nullptr);

    Fixture fresh;
    ERR_clear_error_nid_postfix();
    Require(SSL_CTX_set_cipher_list_nid_postfix(Context(&fresh.ctx), nullptr) == 0);
    Require(fresh.ctx.cipherList == nullptr && fresh.internal.cipherListById == nullptr && DrainErrors().empty());

    Fixture existing;
    RequireList(existing, "AES128-SHA:AES256-SHA", {"AES128-SHA", "AES256-SHA"});
    void* ordered = existing.ctx.cipherList;
    void* byId = existing.internal.cipherListById;
    Require(SSL_CTX_set_cipher_list_nid_postfix(Context(&existing.ctx), nullptr) == 0);
    Require(existing.ctx.cipherList == ordered && existing.internal.cipherListById == byId && DrainErrors().empty());
    Require(existing.OrderedNames() == std::vector<std::string>{"AES128-SHA", "AES256-SHA"});
    RequireShape(existing);
}

static void TestSingleCipher() {
    Fixture fixture;
    RequireList(fixture, "AES128-SHA", {"AES128-SHA"});
    Require(fixture.Ordered()->num == 1 && fixture.Ordered()->data[0] == LibreSslCipherById_nid_no_patch(0x0300002F));
    Require(fixture.ById()->num == 1 && fixture.ById()->data[0] == LibreSslCipherById_nid_no_patch(0x0300002F));
    Require(fixture.Ordered()->numAlloc == 4 && fixture.ById()->numAlloc == 4);
}

static void TestNoMatch() {
    for (const char* rules : {"NOSUCHCIPHER", "!ALL", "", "EXPORT", "aes128-sha", "AES128-SH", "kDHE+AES256", ":::", "AES128-SHA:!AES128-SHA", "AES128-SHA:-ALL", "AES+RC4", "AES128+AES256",
                              ",", ";", " ", "@STRENGTH", "AES128-SHA:!ALL", "XYZ:ABC", "AES128-SHA:AES256-SHA:!ALL:AES256-SHA"}) {
        Fixture fresh;
        RequireNoMatch(fresh, rules);
        Require(fresh.Ordered()->numAlloc == 4 && fresh.ById()->numAlloc == 4);
        Fixture existing;
        RequireList(existing, "AES128-SHA:AES256-SHA:DES-CBC3-SHA", {"AES128-SHA", "AES256-SHA", "DES-CBC3-SHA"});
        RequireNoMatch(existing, rules);
        RequireList(existing, "RC4-SHA", {"RC4-SHA"});
    }
}

static void TestInvalidCommand() {
    struct Invalid {
        const char* rules;
        std::size_t errors;
    };
    for (const Invalid& invalid : {Invalid{"@FOO", 1}, Invalid{"AES128-SHA:@FOO:AES256-SHA", 1}, Invalid{"AES128-SHA:$:AES256-SHA", 1}, Invalid{"AES128-SHA:$:@FOO", 2},
                                   Invalid{"@STRENGTHX", 1}, Invalid{"@", 2}, Invalid{"!", 1}, Invalid{"-", 1}, Invalid{"+", 1}, Invalid{"AES128-SHA:!", 1},
                                   Invalid{"AES128-SHA$", 1}, Invalid{"AES+", 1}, Invalid{"AES128-SHA:@STRENGTH.X", 1}, Invalid{"$$", 1}, Invalid{"DEFAULT:@FOO", 1},
                                   Invalid{"DEFAULT@FOO", 1}}) {
        Fixture fresh;
        ERR_clear_error_nid_postfix();
        Require(fresh.Set(invalid.rules) == 0);
        Require(DrainErrors() == std::vector<std::uint64_t>(invalid.errors, InvalidCommand));
        Require(fresh.ctx.cipherList == nullptr && fresh.internal.cipherListById == nullptr);

        Fixture existing;
        RequireList(existing, "AES256-SHA:AES128-SHA", {"AES256-SHA", "AES128-SHA"});
        void* ordered = existing.ctx.cipherList;
        void* byId = existing.internal.cipherListById;
        ERR_clear_error_nid_postfix();
        Require(existing.Set(invalid.rules) == 0);
        Require(DrainErrors() == std::vector<std::uint64_t>(invalid.errors, InvalidCommand));
        Require(existing.ctx.cipherList == ordered && existing.internal.cipherListById == byId);
        Require(existing.OrderedNames() == std::vector<std::string>{"AES256-SHA", "AES128-SHA"});
        RequireShape(existing);
    }
}

static void TestOrdering() {
    struct Case {
        const char* rules;
        std::vector<std::string> names;
    };
    const std::vector<Case> cases = {
        {"AES128-SHA", {"AES128-SHA"}},
        {"AES128-SHA:AES256-SHA", {"AES128-SHA", "AES256-SHA"}},
        {"AES256-SHA:AES128-SHA", {"AES256-SHA", "AES128-SHA"}},
        {"AES128-SHA:AES256-SHA:-AES128-SHA", {"AES256-SHA"}},
        {"AES128-SHA:AES256-SHA:!AES128-SHA:AES128-SHA", {"AES256-SHA"}},
        {"AES128-SHA:AES256-SHA:+AES128-SHA", {"AES256-SHA", "AES128-SHA"}},
        {"AES128-SHA AES256-SHA", {"AES128-SHA", "AES256-SHA"}},
        {"AES128-SHA,AES256-SHA", {"AES128-SHA", "AES256-SHA"}},
        {"AES128-SHA;AES256-SHA", {"AES128-SHA", "AES256-SHA"}},
        {"AES256-SHA:AES128-SHA:@STRENGTH", {"AES256-SHA", "AES128-SHA"}},
        {"AES128-SHA:AES256-SHA:@STRENGTH", {"AES256-SHA", "AES128-SHA"}},
        {"AES128-SHA:DES-CBC3-SHA:AES256-SHA:@STRENGTH", {"AES256-SHA", "AES128-SHA", "DES-CBC3-SHA"}},
        {"AES128-SHA:AES256-SHA:-AES128-SHA:AES128-SHA", {"AES256-SHA", "AES128-SHA"}},
        {"AES128-SHA:AES256-SHA:-AES128-SHA:+AES128-SHA", {"AES256-SHA"}},
        {"AES128-SHA:AES128-SHA", {"AES128-SHA"}},
        {"-ALL:AES128-SHA", {"AES128-SHA"}},
        {"AES256-SHA:AES128-SHA:@STRENGTH:+AES256-SHA", {"AES128-SHA", "AES256-SHA"}},
        {":::AES128-SHA:::", {"AES128-SHA"}},
        {"AES128-SHA:-AES256-SHA", {"AES128-SHA"}},
        {"AES128+kRSA", {"AES128-GCM-SHA256", "AES128-SHA256", "AES128-SHA"}},
        {"kRSA+AES128", {"AES128-GCM-SHA256", "AES128-SHA256", "AES128-SHA"}},
        {"kEECDH+aRSA+AES256", {"ECDHE-RSA-AES256-GCM-SHA384", "ECDHE-RSA-AES256-SHA384", "ECDHE-RSA-AES256-SHA"}},
        {"AESGCM", {"ECDHE-RSA-AES256-GCM-SHA384", "ECDHE-ECDSA-AES256-GCM-SHA384", "DHE-DSS-AES256-GCM-SHA384", "DHE-RSA-AES256-GCM-SHA384", "ADH-AES256-GCM-SHA384", "AES256-GCM-SHA384",
                    "ECDHE-RSA-AES128-GCM-SHA256", "ECDHE-ECDSA-AES128-GCM-SHA256", "DHE-DSS-AES128-GCM-SHA256", "DHE-RSA-AES128-GCM-SHA256", "ADH-AES128-GCM-SHA256", "AES128-GCM-SHA256"}},
        {"ADH", {"ADH-AES256-GCM-SHA384", "ADH-AES256-SHA256", "ADH-AES256-SHA", "ADH-CAMELLIA256-SHA256", "ADH-CAMELLIA256-SHA", "ADH-AES128-GCM-SHA256", "ADH-AES128-SHA256",
                 "ADH-AES128-SHA", "ADH-CAMELLIA128-SHA256", "ADH-CAMELLIA128-SHA", "ADH-RC4-MD5", "ADH-DES-CBC3-SHA", "ADH-DES-CBC-SHA"}},
        {"3DES", {"ECDHE-RSA-DES-CBC3-SHA", "ECDHE-ECDSA-DES-CBC3-SHA", "EDH-RSA-DES-CBC3-SHA", "EDH-DSS-DES-CBC3-SHA", "AECDH-DES-CBC3-SHA", "ADH-DES-CBC3-SHA", "DES-CBC3-SHA"}},
        {"RC4", {"ECDHE-RSA-RC4-SHA", "ECDHE-ECDSA-RC4-SHA", "AECDH-RC4-SHA", "ADH-RC4-MD5", "RC4-SHA", "RC4-MD5"}},
        {"DES", {"EDH-RSA-DES-CBC-SHA", "EDH-DSS-DES-CBC-SHA", "ADH-DES-CBC-SHA", "DES-CBC-SHA"}},
        {"kEECDH+AESGCM:kEDH+AESGCM", {"ECDHE-RSA-AES256-GCM-SHA384", "ECDHE-ECDSA-AES256-GCM-SHA384", "ECDHE-RSA-AES128-GCM-SHA256", "ECDHE-ECDSA-AES128-GCM-SHA256",
                                       "DHE-DSS-AES256-GCM-SHA384", "DHE-RSA-AES256-GCM-SHA384", "ADH-AES256-GCM-SHA384", "DHE-DSS-AES128-GCM-SHA256", "DHE-RSA-AES128-GCM-SHA256",
                                       "ADH-AES128-GCM-SHA256"}},
        {"MEDIUM", {"ECDHE-ECDSA-DES-CBC3-SHA", "EDH-RSA-DES-CBC3-SHA", "EDH-DSS-DES-CBC3-SHA", "AECDH-DES-CBC3-SHA", "ADH-DES-CBC3-SHA", "DES-CBC3-SHA"}},
        {"LOW", {"ECDHE-RSA-RC4-SHA", "ECDHE-ECDSA-RC4-SHA", "AECDH-RC4-SHA", "ADH-RC4-MD5", "RC4-SHA", "RC4-MD5", "EDH-RSA-DES-CBC-SHA", "EDH-DSS-DES-CBC-SHA", "ADH-DES-CBC-SHA",
                 "DES-CBC-SHA"}},
        {"ECDHE-RSA-AES128-GCM-SHA256:ECDHE-ECDSA-AES128-GCM-SHA256", {"ECDHE-RSA-AES128-GCM-SHA256", "ECDHE-ECDSA-AES128-GCM-SHA256"}},
        {"ECDHE-ECDSA-AES128-GCM-SHA256:ECDHE-RSA-AES128-GCM-SHA256", {"ECDHE-ECDSA-AES128-GCM-SHA256", "ECDHE-RSA-AES128-GCM-SHA256"}},
        {"@STRENGTH:AES128-SHA", {"AES128-SHA"}},
        {"AES128-SHA:@STRENGTH+AES", {"AES128-SHA"}},
        {"AES128-SHA:AES256-SHA:+AES256-SHA:+AES128-SHA", {"AES256-SHA", "AES128-SHA"}},
        {"AES256-SHA:!AES256-SHA:AES128-SHA", {"AES128-SHA"}},
        {"AES128-SHA:XYZ:AES256-SHA", {"AES128-SHA", "AES256-SHA"}},
        {"AES128-SHA:AES+RC4:AES256-SHA", {"AES128-SHA", "AES256-SHA"}},
        {"AES128-SHA:AES256-SHA:-ALL:AES256-SHA", {"AES256-SHA"}},
        {"AES128-SHA:DES-CBC3-SHA:RC4-SHA:@STRENGTH", {"AES128-SHA", "RC4-SHA", "DES-CBC3-SHA"}},
        {"RC4-SHA:DES-CBC-SHA:AES128-SHA:@STRENGTH:+RC4-SHA", {"AES128-SHA", "DES-CBC-SHA", "RC4-SHA"}},
    };
    for (const Case& test : cases) {
        Fixture fixture;
        RequireList(fixture, test.rules, test.names);
        RequireList(fixture, test.rules, test.names);
    }
}

static void TestDefaultList() {
    Fixture standard;
    ERR_clear_error_nid_postfix();
    Require(standard.Set("ALL:!aNULL:!eNULL:!SSLv2") == 1 && DrainErrors().empty());
    const std::vector<std::string> defaults = standard.OrderedNames();
    RequireShape(standard);
    Require(!defaults.empty());
    Require(std::set<std::string>(defaults.begin(), defaults.end()).size() == defaults.size());
    for (const std::string& name : defaults) Require(!Contains(name, "NULL") && !StartsWith(name, "ADH-") && !StartsWith(name, "AECDH-"));
    Require(std::find(defaults.begin(), defaults.end(), "AES128-SHA") != defaults.end());

    std::vector<std::string> without;
    for (const std::string& name : defaults) if (name != "AES128-SHA") without.push_back(name);
    std::vector<std::string> moved = without;
    moved.push_back("AES128-SHA");
    Require(without.size() + 1 == defaults.size());

    Fixture fixture;
    RequireList(fixture, "DEFAULT", defaults);
    RequireList(fixture, "DEFAULT:", defaults);
    RequireList(fixture, "DEFAULT:AES128-SHA", defaults);
    RequireList(fixture, "DEFAULT:ALL", defaults);
    RequireList(fixture, "DEFAULT:ECDHE-RSA-NULL-SHA", defaults);
    RequireList(fixture, "DEFAULT:eNULL", defaults);
    RequireList(fixture, "DEFAULT:ADH-AES128-SHA", defaults);
    RequireList(fixture, "DEFAULT:-AES128-SHA", without);
    RequireList(fixture, "DEFAULT:!AES128-SHA", without);
    RequireList(fixture, "DEFAULT:+AES128-SHA", moved);
    RequireList(fixture, "DEFAULTX", defaults);
    RequireNoMatch(fixture, "DEFAULT:-ALL");
}

static void TestNullEncryption() {
    Fixture first;
    Fixture second;
    Fixture third;
    Fixture everything;
    ERR_clear_error_nid_postfix();
    Require(first.Set("eNULL") == 1 && second.Set("NULL") == 1 && third.Set("COMPLEMENTOFALL") == 1 && everything.Set("ALL") == 1 && DrainErrors().empty());
    const std::vector<std::string> nulls = first.OrderedNames();
    Require(!nulls.empty() && second.OrderedNames() == nulls && third.OrderedNames() == nulls);
    for (const std::string& name : nulls) Require(Contains(name, "NULL"));
    const std::vector<std::string> all = everything.OrderedNames();
    Require(!all.empty());
    for (const std::string& name : all) Require(!Contains(name, "NULL"));
    for (const std::string& name : all) Require(std::find(nulls.begin(), nulls.end(), name) == nulls.end());
    Require(std::find(all.begin(), all.end(), "ADH-AES128-SHA") != all.end());
    RequireShape(first);
    RequireShape(everything);
}

static void TestById() {
    Fixture fixture;
    RequireList(fixture, "AES256-SHA:AES128-SHA:DES-CBC3-SHA", {"AES256-SHA", "AES128-SHA", "DES-CBC3-SHA"});
    Require(Names(fixture.ById()) == std::vector<std::string>{"DES-CBC3-SHA", "AES128-SHA", "AES256-SHA"});
    RequireList(fixture, "ECDHE-RSA-AES256-GCM-SHA384:AES256-SHA:ECDHE-RSA-CHACHA20-POLY1305:DES-CBC3-SHA:AES128-SHA",
        {"ECDHE-RSA-AES256-GCM-SHA384", "AES256-SHA", "ECDHE-RSA-CHACHA20-POLY1305", "DES-CBC3-SHA", "AES128-SHA"});
    Require(Names(fixture.ById()) == std::vector<std::string>{"DES-CBC3-SHA", "AES128-SHA", "AES256-SHA", "ECDHE-RSA-AES256-GCM-SHA384", "ECDHE-RSA-CHACHA20-POLY1305"});
    Require(fixture.ById()->comp(&fixture.ById()->data[0], &fixture.ById()->data[1]) == -1);
    Require(fixture.ById()->comp(&fixture.ById()->data[4], &fixture.ById()->data[1]) == 1);
    Require(fixture.ById()->comp(&fixture.ById()->data[2], &fixture.ById()->data[2]) == 0);
    Require(fixture.Ordered()->data[0] == fixture.ById()->data[3] && fixture.Ordered()->data[2] == fixture.ById()->data[4]);
}

static void TestStackSizes() {
    static const char* const names[] = {"AES128-SHA", "AES256-SHA", "DES-CBC3-SHA", "RC4-SHA", "RC4-MD5", "DES-CBC-SHA", "CAMELLIA128-SHA", "CAMELLIA256-SHA", "AES128-SHA256", "AES256-SHA256",
                                        "DHE-RSA-AES128-SHA", "DHE-RSA-AES256-SHA", "ECDHE-RSA-AES128-SHA", "ECDHE-RSA-AES256-SHA", "DHE-DSS-AES128-SHA", "DHE-DSS-AES256-SHA"};
    static const int allocations[] = {4, 4, 4, 8, 8, 8, 8, 16, 16, 16, 16, 16, 16, 16, 16, 32};
    static_assert(std::size(names) == std::size(allocations));
    for (std::size_t count = 1; count <= std::size(names); ++count) {
        std::string rules;
        std::vector<std::string> expected;
        for (std::size_t index = 0; index < count; ++index) {
            if (index) rules += ':';
            rules += names[index];
            expected.emplace_back(names[index]);
        }
        Fixture fixture;
        RequireList(fixture, rules, expected);
        Require(fixture.Ordered()->num == static_cast<int>(count) && fixture.ById()->num == static_cast<int>(count));
        Require(fixture.Ordered()->numAlloc == allocations[count - 1] && fixture.ById()->numAlloc == allocations[count - 1]);
    }
}

static void TestReplacement() {
    Fixture fixture;
    RequireList(fixture, "AES128-SHA", {"AES128-SHA"});
    RequireList(fixture, "AES256-SHA:DES-CBC3-SHA", {"AES256-SHA", "DES-CBC3-SHA"});
    Require(Names(fixture.ById()) == std::vector<std::string>{"DES-CBC3-SHA", "AES256-SHA"});
    RequireNoMatch(fixture, "NOSUCHCIPHER");
    RequireList(fixture, "AES128-SHA", {"AES128-SHA"});
}

static void TestRepeatedReplacement() {
    Fixture fixture;
    for (int round = 0; round < 200; ++round) {
        switch (round % 4) {
        case 0: RequireList(fixture, "AES128-SHA:AES256-SHA", {"AES128-SHA", "AES256-SHA"}); break;
        case 1: RequireNoMatch(fixture, "NOSUCHCIPHER"); break;
        case 2: RequireList(fixture, "DES-CBC3-SHA:RC4-SHA:AES256-SHA:AES128-SHA:RC4-MD5", {"DES-CBC3-SHA", "RC4-SHA", "AES256-SHA", "AES128-SHA", "RC4-MD5"}); break;
        default: Require(fixture.Set("@FOO") == 0); DrainErrors(); RequireShape(fixture); break;
        }
    }
}

static int allocations = 0;
static int reallocations = 0;
static int frees = 0;
static int failAllocationAt = 0;
static int failReallocationAt = 0;

static void* APS5_VABI Allocate(std::size_t size) {
    if (++allocations == failAllocationAt) return nullptr;
    return std::malloc(size);
}

static void APS5_VABI Release(void* pointer) {
    if (pointer) ++frees;
    std::free(pointer);
}

static void* APS5_VABI Calloc(std::size_t count, std::size_t size) { return std::calloc(count, size); }

static void* APS5_VABI Reallocate(void* pointer, std::size_t size) {
    if (++reallocations == failReallocationAt) return nullptr;
    return std::realloc(pointer, size);
}

static void* APS5_VABI Align(std::size_t, std::size_t) { throw std::runtime_error("unexpected aligned allocation"); }
static void* APS5_VABI Realign(void*, std::size_t, std::size_t) { throw std::runtime_error("unexpected aligned reallocation"); }
static int APS5_VABI PosixAlign(void**, std::size_t, std::size_t) { throw std::runtime_error("unexpected posix_memalign"); }

static void ResetCounts() {
    allocations = reallocations = frees = 0;
    failAllocationAt = failReallocationAt = 0;
}

static void TestAllocator() {
    std::array<void*, 10> api{
        reinterpret_cast<void*>(Allocate), reinterpret_cast<void*>(Release), reinterpret_cast<void*>(Calloc),
        reinterpret_cast<void*>(Reallocate), reinterpret_cast<void*>(Align), reinterpret_cast<void*>(Realign),
        reinterpret_cast<void*>(PosixAlign)};
    ApplicationHeapRegister_nid_no_patch(api.data());
    Fixture fixture;
    ResetCounts();
    Require(fixture.Set("AES128-SHA") == 1);
    Require(allocations == 4 && reallocations == 1 && frees == 0);
    ResetCounts();
    Require(fixture.Set("AES256-SHA") == 1);
    Require(allocations == 4 && reallocations == 1 && frees == 4);
    for (int failing = 1; failing <= 4; ++failing) {
        const void* ordered = fixture.ctx.cipherList;
        const void* byId = fixture.internal.cipherListById;
        ERR_clear_error_nid_postfix();
        ResetCounts();
        failAllocationAt = failing;
        Require(fixture.Set("AES128-SHA:DES-CBC3-SHA") == 0);
        Require(allocations == failing && frees == failing - 1);
        Require(DrainErrors().empty());
        Require(fixture.ctx.cipherList == ordered && fixture.internal.cipherListById == byId);
        Require(fixture.OrderedNames() == std::vector<std::string>{"AES256-SHA"});
    }
    ERR_clear_error_nid_postfix();
    ResetCounts();
    failReallocationAt = 1;
    Require(fixture.Set("AES128-SHA:DES-CBC3-SHA") == 0);
    Require(allocations == 4 && frees == 4 && DrainErrors().empty());
    Require(fixture.OrderedNames() == std::vector<std::string>{"AES256-SHA"});
    ResetCounts();
    failReallocationAt = 1;
    Require(fixture.Set("DES-CBC3-SHA:RC4-SHA:AES256-SHA:AES128-SHA:RC4-MD5") == 1);
    Require(fixture.OrderedNames() == std::vector<std::string>{"DES-CBC3-SHA", "RC4-SHA", "AES256-SHA", "RC4-MD5"});
    Require(fixture.Ordered()->numAlloc == 8 && fixture.ById()->numAlloc == 8 && fixture.ById()->num == 4);
    ResetCounts();
}

int main(int argc, char**) {
    if (argc > 1) {
        TestAllocator();
        return 0;
    }
    std::array<void*, 10> api{};
    ApplicationHeapRegister_nid_no_patch(api.data());
    TestTable();
    TestKnownCiphers();
    TestArguments();
    TestSingleCipher();
    TestNoMatch();
    TestInvalidCommand();
    TestOrdering();
    TestDefaultList();
    TestNullEncryption();
    TestById();
    TestStackSizes();
    TestReplacement();
    TestRepeatedReplacement();
    return 0;
}
