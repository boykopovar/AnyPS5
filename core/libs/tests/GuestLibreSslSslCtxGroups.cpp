#include "prx/libc/include/Api.hpp"
#include "prx/libSceLibreSsl/include/Api.hpp"
#include "prx/libc/include/ApplicationHeap.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <initializer_list>
#include <new>
#include <source_location>
#include <stdexcept>
#include <string>
#include <vector>

constexpr std::size_t CtxSize = 120;
constexpr std::size_t InternalSize = 464;
constexpr std::size_t AlpnList = 416;
constexpr std::size_t AlpnLength = 424;
constexpr std::size_t GroupsLength = 448;
constexpr std::size_t Groups = 456;

static_assert(sizeof(LibreSslCtx) == CtxSize && sizeof(LibreSslCtxInternal) == InternalSize);

static void Require(bool condition, std::source_location location = std::source_location::current()) {
    if (condition) return;
    std::fprintf(stderr, "LibreSSL SSL_CTX groups check failed at line %u\n", location.line());
    std::abort();
}

template<typename TAction> static void RequireThrows(TAction action, std::source_location location = std::source_location::current()) {
    try { action(); } catch (const std::exception&) { return; }
    Require(false, location);
}

static bool failAllocation = false;
static int liveAllocations = 0;
static int hostBudget = -1;
static bool hostFailed = false;
static int liveHostAllocations = 0;

void* operator new(std::size_t size) {
    if (hostBudget == 0) {
        hostBudget = -1;
        hostFailed = true;
        throw std::bad_alloc();
    }
    if (hostBudget > 0) --hostBudget;
    void* pointer = std::malloc(size == 0 ? 1 : size);
    if (!pointer) throw std::bad_alloc();
    ++liveHostAllocations;
    return pointer;
}

void operator delete(void* pointer) noexcept {
    if (pointer) --liveHostAllocations;
    std::free(pointer);
}

void operator delete(void* pointer, std::size_t) noexcept {
    ::operator delete(pointer);
}

static void* APS5_VABI Allocate_nid_no_patch(std::size_t size) {
    if (failAllocation) return nullptr;
    ++liveAllocations;
    return std::malloc(size == 0 ? 1 : size);
}

static void APS5_VABI Free_nid_no_patch(void* pointer) {
    if (pointer) --liveAllocations;
    std::free(pointer);
}

static void* APS5_VABI Unexpected_nid_no_patch() {
    throw std::runtime_error("unexpected allocator call");
}

static void InitializeHeap() {
    static const std::array<void*, 10> api{
        reinterpret_cast<void*>(Allocate_nid_no_patch), reinterpret_cast<void*>(Free_nid_no_patch), reinterpret_cast<void*>(Unexpected_nid_no_patch),
        reinterpret_cast<void*>(Unexpected_nid_no_patch), reinterpret_cast<void*>(Unexpected_nid_no_patch), reinterpret_cast<void*>(Unexpected_nid_no_patch),
        reinterpret_cast<void*>(Unexpected_nid_no_patch)};
    ApplicationHeapRegister_nid_no_patch(api.data());
}

template<typename TValue> static TValue Load(const unsigned char* base, std::size_t offset) {
    TValue value;
    std::memcpy(&value, base + offset, sizeof value);
    return value;
}

template<typename TValue> static void Store(unsigned char* base, std::size_t offset, TValue value) {
    std::memcpy(base + offset, &value, sizeof value);
}

struct Fixture {
    alignas(8) unsigned char ctx[CtxSize];
    alignas(8) unsigned char internal[InternalSize];
    alignas(8) unsigned char expected[InternalSize];

    Fixture() {
        for (std::size_t i = 0; i < CtxSize; ++i) ctx[i] = static_cast<unsigned char>(i * 7 + 3);
        for (std::size_t i = 0; i < InternalSize; ++i) internal[i] = static_cast<unsigned char>(i * 13 + 5);
        Store<void*>(ctx, offsetof(LibreSslCtx, internal), internal);
        Store<void*>(internal, AlpnList, nullptr);
        Store<unsigned int>(internal, AlpnLength, 0);
        Store<std::uint64_t>(internal, GroupsLength, 0);
        Store<void*>(internal, Groups, nullptr);
        std::memcpy(expected, internal, InternalSize);
    }

    Fixture(const Fixture&) = delete;
    Fixture& operator=(const Fixture&) = delete;

    ~Fixture() {
        free_nid_postfix(Load<void*>(internal, AlpnList));
        free_nid_postfix(Load<void*>(internal, Groups));
    }

    LibreSslCtx* Context() {
        return reinterpret_cast<LibreSslCtx*>(ctx);
    }

    const unsigned char* Alpn() const {
        return Load<const unsigned char*>(internal, AlpnList);
    }

    const std::uint16_t* GroupList() const {
        return Load<const std::uint16_t*>(internal, Groups);
    }

    void RequireAlpn(const unsigned char* protos, unsigned int length, std::source_location location = std::source_location::current()) {
        Require(Alpn() != nullptr && Alpn() != protos, location);
        Require(Load<unsigned int>(internal, AlpnLength) == length, location);
        Require(length == 0 || std::memcmp(Alpn(), protos, length) == 0, location);
        Store<const unsigned char*>(expected, AlpnList, Alpn());
        Store<unsigned int>(expected, AlpnLength, length);
        Require(std::memcmp(internal, expected, InternalSize) == 0, location);
    }

    void RequireGroups(const std::vector<std::uint16_t>& wanted, std::source_location location = std::source_location::current()) {
        Require(GroupList() != nullptr, location);
        Require(Load<std::uint64_t>(internal, GroupsLength) == wanted.size(), location);
        Require(std::memcmp(GroupList(), wanted.data(), wanted.size() * sizeof(std::uint16_t)) == 0, location);
        Store<const std::uint16_t*>(expected, Groups, GroupList());
        Store<std::uint64_t>(expected, GroupsLength, wanted.size());
        Require(std::memcmp(internal, expected, InternalSize) == 0, location);
    }
};

static void TestAlpn() {
    Fixture fixture;
    const unsigned char protocols[] = "\x02h2\x08http/1.1";
    Require(SSL_CTX_set_alpn_protos_nid_postfix(fixture.Context(), protocols, 12) == 0);
    fixture.RequireAlpn(protocols, 12);
    const unsigned char shorter[] = "\x02h2";
    Require(SSL_CTX_set_alpn_protos_nid_postfix(fixture.Context(), shorter, 3) == 0);
    fixture.RequireAlpn(shorter, 3);
    const unsigned char malformed[] = "\x05h2";
    Require(SSL_CTX_set_alpn_protos_nid_postfix(fixture.Context(), malformed, 3) == 0);
    fixture.RequireAlpn(malformed, 3);
    Require(SSL_CTX_set_alpn_protos_nid_postfix(fixture.Context(), nullptr, 0) == 0);
    fixture.RequireAlpn(nullptr, 0);
    Require(SSL_CTX_set_alpn_protos_nid_postfix(fixture.Context(), protocols, 0) == 0);
    fixture.RequireAlpn(protocols, 0);

    failAllocation = true;
    Require(SSL_CTX_set_alpn_protos_nid_postfix(fixture.Context(), shorter, 3) == 1);
    failAllocation = false;
    Require(fixture.Alpn() == nullptr && Load<unsigned int>(fixture.internal, AlpnLength) == 0);
    Require(SSL_CTX_set_alpn_protos_nid_postfix(fixture.Context(), protocols, 12) == 0);
    fixture.RequireAlpn(protocols, 12);
    failAllocation = true;
    Require(SSL_CTX_set_alpn_protos_nid_postfix(fixture.Context(), shorter, 3) == 1);
    failAllocation = false;
    Require(fixture.Alpn() == nullptr && Load<unsigned int>(fixture.internal, AlpnLength) == 12);
    Store<const unsigned char*>(fixture.expected, AlpnList, nullptr);
    Require(std::memcmp(fixture.internal, fixture.expected, InternalSize) == 0);
    Require(SSL_CTX_set_alpn_protos_nid_postfix(fixture.Context(), protocols, 0) == 0);
    fixture.RequireAlpn(protocols, 0);

    RequireThrows([] { SSL_CTX_set_alpn_protos_nid_postfix(nullptr, nullptr, 0); });
    RequireThrows([&] { SSL_CTX_set_alpn_protos_nid_postfix(fixture.Context(), nullptr, 1); });
    fixture.RequireAlpn(protocols, 0);
}

static void TestGroups() {
    Fixture fixture;
    Require(SSL_CTX_set1_groups_list_nid_postfix(fixture.Context(), "X25519:P-256:secp384r1") == 1);
    fixture.RequireGroups({29, 23, 24});
    Require(SSL_CTX_set1_groups_list_nid_postfix(fixture.Context(), "sect163k1:sect163r1:sect163r2:sect193r1:sect193r2:sect233k1:sect233r1:sect239k1:sect283k1:sect283r1:sect409k1:sect409r1:sect571k1:sect571r1:secp160k1:secp160r1:secp160r2:secp192k1:prime192v1:secp224k1:secp224r1:secp256k1:prime256v1:secp384r1:secp521r1:brainpoolP256r1:brainpoolP384r1:brainpoolP512r1:X25519") == 1);
    fixture.RequireGroups({1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29});
    Require(SSL_CTX_set1_groups_list_nid_postfix(fixture.Context(), "B-163:B-233:B-283:B-409:B-571:K-163:K-233:K-283:K-409:K-571:P-192:P-224:P-256:P-384:P-521") == 1);
    fixture.RequireGroups({3, 7, 10, 12, 14, 1, 6, 9, 11, 13, 19, 21, 23, 24, 25});
    Require(SSL_CTX_set1_groups_list_nid_postfix(fixture.Context(), "B-163:K-163:sect163r2") == 1);
    fixture.RequireGroups({3, 1, 3});
    Require(SSL_CTX_set1_groups_list_nid_postfix(fixture.Context(), "X25519:X25519") == 1);
    fixture.RequireGroups({29, 29});
    Require(SSL_CTX_set1_groups_list_nid_postfix(fixture.Context(), "prime256v1") == 1);
    fixture.RequireGroups({23});

    const std::uint16_t* kept = fixture.GroupList();
    for (const char* list : {"", ":", "P-256:", ":P-256", "P-256::X25519", "rsaEncryption", "x25519", "secp256r1", "secp192r1", "NULL", "X25519 ", "P-256:rsaEncryption", "prime239v1", "brainpoolP256t1"}) {
        Require(SSL_CTX_set1_groups_list_nid_postfix(fixture.Context(), list) == 0);
        Require(fixture.GroupList() == kept);
        fixture.RequireGroups({23});
    }

    failAllocation = true;
    Require(SSL_CTX_set1_groups_list_nid_postfix(fixture.Context(), "X25519") == 0);
    failAllocation = false;
    Require(fixture.GroupList() == kept);
    fixture.RequireGroups({23});

    RequireThrows([] { SSL_CTX_set1_groups_list_nid_postfix(nullptr, "X25519"); });
    RequireThrows([&] { SSL_CTX_set1_groups_list_nid_postfix(fixture.Context(), nullptr); });
    fixture.RequireGroups({23});
}

static void TestGroupsHostAllocationFailure() {
    struct Case {
        const char* list;
        std::vector<std::uint16_t> groups;
    };
    const std::string unknown(128, 'a');
    const Case cases[] = {
        {"X25519", {29}},
        {"sect163k1:sect163r1:sect163r2:sect193r1:sect193r2:sect233k1:sect233r1:sect239k1:sect283k1:sect283r1:sect409k1:sect409r1:sect571k1:sect571r1:secp160k1:secp160r1:secp160r2:secp192k1:prime192v1:secp224k1:secp224r1:secp256k1:prime256v1:secp384r1:secp521r1:brainpoolP256r1:brainpoolP384r1:brainpoolP512r1:X25519",
         {1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29}},
        {"X25519:X25519:P-256:P-256", {29, 29, 23, 23}},
        {unknown.c_str(), {}},
    };
    for (const Case& test : cases) {
        hostFailed = true;
        for (int budget = 0; hostFailed; ++budget) {
            Fixture fixture;
            Require(SSL_CTX_set1_groups_list_nid_postfix(fixture.Context(), "prime256v1") == 1);
            const std::uint16_t* kept = fixture.GroupList();
            const int guest = liveAllocations;
            const int host = liveHostAllocations;
            hostFailed = false;
            hostBudget = budget;
            const int result = SSL_CTX_set1_groups_list_nid_postfix(fixture.Context(), test.list);
            hostBudget = -1;
            Require(liveHostAllocations == host);
            if (hostFailed || test.groups.empty()) {
                Require(result == 0);
                Require(fixture.GroupList() == kept);
                Require(liveAllocations == guest);
                fixture.RequireGroups({23});
            } else {
                Require(result == 1);
                Require(liveAllocations == guest);
                fixture.RequireGroups(test.groups);
            }
        }
    }
}

static void TestNullInternal() {
    Fixture fixture;
    Store<void*>(fixture.ctx, offsetof(LibreSslCtx, internal), nullptr);
    RequireThrows([&] { SSL_CTX_set_alpn_protos_nid_postfix(fixture.Context(), nullptr, 0); });
    RequireThrows([&] { SSL_CTX_set1_groups_list_nid_postfix(fixture.Context(), "X25519"); });
}

static bool TestAddedGroupNames() {
    bool passed = true;
    for (const auto& test : std::array<std::array<const char*, 2>, 3>{{
        {"prime256v1", "unrelated long name"}, {"P-256", "unrelated long name"}, {nullptr, "P-256"}
    }}) {
        Fixture fixture;
        Require(SSL_CTX_set1_groups_list_nid_postfix(fixture.Context(), "X25519") == 1);
        fixture.RequireGroups({29});
        const auto* kept = fixture.GroupList();
        OBJ_cleanup_nid_postfix();
        ERR_clear_error_nid_postfix();
        Require(OBJ_create_nid_postfix("1.2.3.4", test[0], test[1]) >= 956);
        bool correct = false;
        const char* name = test[0] ? test[0] : test[1];
        try { correct = SSL_CTX_set1_groups_list_nid_postfix(fixture.Context(), name) == 0; }
        catch (const std::runtime_error& error) {
            const std::string text = error.what();
            const char* function = test[0] ? "OBJ_sn2nid" : "SSL_CTX_set1_groups_list";
            correct = text.find(function) != std::string::npos && text.find("not implemented") != std::string::npos;
        }
        OBJ_cleanup_nid_postfix();
        ERR_clear_error_nid_postfix();
        if (!correct) std::fprintf(stderr, "registered group name %s is incorrectly accepted\n", name);
        passed &= correct;
        if (correct) {
            Require(fixture.GroupList() == kept);
            fixture.RequireGroups({29});
        }
    }
    {
        Fixture fixture;
        OBJ_cleanup_nid_postfix();
        ERR_clear_error_nid_postfix();
        Require(OBJ_create_nid_postfix("1.2.3.4", nullptr, "prime256v1") >= 956);
        const int result = SSL_CTX_set1_groups_list_nid_postfix(fixture.Context(), "prime256v1");
        OBJ_cleanup_nid_postfix();
        ERR_clear_error_nid_postfix();
        Require(result == 1);
        fixture.RequireGroups({23});
    }
    return passed;
}

int main() {
    InitializeHeap();
    TestAlpn();
    TestGroups();
    TestGroupsHostAllocationFailure();
    TestNullInternal();
    const bool addedNames = TestAddedGroupNames();
    Require(liveAllocations == 0);
    std::puts("LibreSSL SSL_CTX ALPN and group list checks passed");
    return addedNames ? 0 : 1;
}
