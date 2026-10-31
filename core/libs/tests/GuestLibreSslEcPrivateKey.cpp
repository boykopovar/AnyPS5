#include "prx/libSceLibreSsl/include/Api.hpp"
#include "prx/libc/include/ApplicationHeap.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include <climits>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <map>
#include <stdexcept>
#include <vector>

struct TestBignum {
    std::uint64_t* d;
    int top;
    int dmax;
    int neg;
    int flags;
};

static constexpr int Malloced = 0x01;
static constexpr int StaticData = 0x02;
static constexpr int ConstTime = 0x04;
static constexpr int MaxWords = INT_MAX / (4 * 64);

static void Require(bool value, const char* what) {
    if (value) return;
    std::fprintf(stderr, "failed: %s\n", what);
    std::abort();
}

struct Freed {
    void* pointer;
    std::size_t bytes;
    bool wiped;
};

static std::map<void*, std::size_t> live;
static std::vector<Freed> freed;
static int allocations;

static void* APS5_VABI Allocate(std::size_t bytes) {
    void* pointer = std::malloc(bytes ? bytes : 1);
    Require(pointer != nullptr, "test heap allocation");
    live[pointer] = bytes;
    ++allocations;
    return pointer;
}

static void APS5_VABI Release(void* pointer) {
    if (!pointer) return;
    const auto block = live.find(pointer);
    Require(block != live.end(), "free of a block from the guest heap");
    bool wiped = true;
    for (std::size_t index = 0; index < block->second; ++index) wiped = wiped && static_cast<unsigned char*>(pointer)[index] == 0;
    freed.push_back({pointer, block->second, wiped});
    live.erase(block);
    std::free(pointer);
}

static void* APS5_VABI AllocateZeroed(std::size_t, std::size_t) {
    Require(false, "guest calloc is not used");
    return nullptr;
}

static void* APS5_VABI Reallocate(void*, std::size_t) {
    Require(false, "guest realloc is not used");
    return nullptr;
}

static void* APS5_VABI Align(std::size_t, std::size_t) {
    Require(false, "guest memalign is not used");
    return nullptr;
}

static void* APS5_VABI Realign(void*, std::size_t, std::size_t) {
    Require(false, "guest reallocalign is not used");
    return nullptr;
}

static int APS5_VABI PosixAlign(void**, std::size_t, std::size_t) {
    Require(false, "guest posix_memalign is not used");
    return 0;
}

static void InitializeHeap() {
    void* const api[10] {reinterpret_cast<void*>(Allocate), reinterpret_cast<void*>(Release), reinterpret_cast<void*>(AllocateZeroed),
        reinterpret_cast<void*>(Reallocate), reinterpret_cast<void*>(Align), reinterpret_cast<void*>(Realign), reinterpret_cast<void*>(PosixAlign)};
    ApplicationHeapRegister_nid_no_patch(api);
}

static bool Live(const void* pointer, std::size_t bytes) {
    const auto block = live.find(const_cast<void*>(pointer));
    return block != live.end() && block->second == bytes;
}

static bool FreedWiped(const void* pointer, std::size_t bytes) {
    for (const Freed& entry : freed) {
        if (entry.pointer == pointer) return entry.bytes == bytes && entry.wiped;
    }
    return false;
}

template<typename TAction> static void RequireThrows(TAction action, const char* what) {
    try { action(); } catch (const std::exception&) { return; }
    Require(false, what);
}

static const TestBignum* Stored(const EcKey* key) {
    return static_cast<const TestBignum*>(EC_KEY_get0_private_key_nid_postfix(key));
}

static void RequireCopy(const EcKey* key, const TestBignum& source, int top, int neg, const std::uint64_t* words, const char* what) {
    const TestBignum* stored = Stored(key);
    Require(stored != nullptr && stored != &source, what);
    Require(stored->top == top && stored->dmax == top && stored->neg == neg && stored->flags == Malloced, what);
    if (top == 0) {
        Require(stored->d == nullptr, what);
        return;
    }
    Require(stored->d != nullptr && stored->d != source.d, what);
    for (int word = 0; word < top; ++word) Require(stored->d[word] == words[word], what);
}

static void CheckNewKeys() {
    EcKey* key = EC_KEY_new_nid_postfix();
    Require(Stored(key) == nullptr, "EC_KEY_new has no private key");
    EC_KEY_free_nid_postfix(key);
    key = EC_KEY_new_by_curve_name_nid_postfix(415);
    Require(Stored(key) == nullptr, "EC_KEY_new_by_curve_name has no private key");
    EC_KEY_free_nid_postfix(key);
}

static void CheckCopy() {
    std::uint64_t words[] = {0x1122334455667788, 0x99aabbccddeeff00, 0x0102030405060708};
    const std::uint64_t expected[] = {0x1122334455667788, 0x99aabbccddeeff00, 0x0102030405060708};
    TestBignum source{words, 3, 3, 0, 0};
    EcKey* key = EC_KEY_new_by_curve_name_nid_postfix(415);
    Require(EC_KEY_set_private_key_nid_postfix(key, &source) == 1, "EC_KEY_set_private_key");
    RequireCopy(key, source, 3, 0, expected, "a copy of three words");
    words[0] = 0;
    words[2] = 0;
    source.top = 1;
    RequireCopy(key, source, 3, 0, expected, "the copy keeps its words after the source changes");
    EC_KEY_free_nid_postfix(key);
}

static void CheckCopiedFields() {
    std::uint64_t words[] = {0xfedcba9876543210, 0x5, 0xdead, 0xbeef, 0xcafe};
    const std::uint64_t expected[] = {0xfedcba9876543210, 0x5};
    TestBignum source{words, 2, 5, 1, StaticData | ConstTime};
    EcKey* key = EC_KEY_new_nid_postfix();
    Require(EC_KEY_set_private_key_nid_postfix(key, &source) == 1, "EC_KEY_set_private_key without a group");
    RequireCopy(key, source, 2, 1, expected, "a copy has dmax equal to top, the sign and only BN_FLG_MALLOCED");
    EC_KEY_free_nid_postfix(key);
}

static void CheckZero() {
    std::uint64_t words[] = {7, 8, 9, 10};
    TestBignum withWords{words, 0, 4, 1, 0};
    EcKey* key = EC_KEY_new_by_curve_name_nid_postfix(415);
    Require(EC_KEY_set_private_key_nid_postfix(key, &withWords) == 1, "EC_KEY_set_private_key of zero with words");
    RequireCopy(key, withWords, 0, 1, nullptr, "a zero copy has no words and keeps its sign");
    TestBignum withoutWords{nullptr, 0, 0, 0, 0};
    Require(EC_KEY_set_private_key_nid_postfix(key, &withoutWords) == 1, "EC_KEY_set_private_key of zero without words");
    RequireCopy(key, withoutWords, 0, 0, nullptr, "a zero copy without words");
    EC_KEY_free_nid_postfix(key);
}

static void CheckReplaceAndClear() {
    std::uint64_t first[] = {0x11};
    std::uint64_t second[] = {0x2222, 0x3333};
    TestBignum one{first, 1, 1, 0, 0};
    TestBignum two{second, 2, 2, 0, 0};
    EcKey* key = EC_KEY_new_by_curve_name_nid_postfix(415);
    Require(EC_KEY_set_private_key_nid_postfix(key, &one) == 1, "EC_KEY_set_private_key of a first key");
    Require(EC_KEY_set_private_key_nid_postfix(key, &two) == 1, "EC_KEY_set_private_key replacing a key");
    RequireCopy(key, two, 2, 0, second, "the replaced private key");
    Require(EC_KEY_set_private_key_nid_postfix(key, nullptr) == 0, "EC_KEY_set_private_key with a null key");
    Require(Stored(key) == nullptr, "a null key drops the previous one");
    Require(EC_KEY_set_private_key_nid_postfix(key, nullptr) == 0, "EC_KEY_set_private_key with a null key and no key");
    Require(Stored(key) == nullptr, "a null key leaves no key");
    EC_KEY_free_nid_postfix(key);
}

static void CheckOtherKey() {
    std::uint64_t words[] = {0xaaaa, 0xbbbb, 0xcccc};
    TestBignum source{words, 3, 3, 1, 0};
    EcKey* first = EC_KEY_new_by_curve_name_nid_postfix(415);
    EcKey* second = EC_KEY_new_by_curve_name_nid_postfix(715);
    Require(EC_KEY_set_private_key_nid_postfix(first, &source) == 1, "EC_KEY_set_private_key of the first key");
    const TestBignum* original = Stored(first);
    Require(EC_KEY_set_private_key_nid_postfix(second, original) == 1, "EC_KEY_set_private_key from another key");
    Require(Stored(second) != original, "the second key has its own copy");
    EC_KEY_free_nid_postfix(first);
    RequireCopy(second, source, 3, 1, words, "the copy outlives the other key");
    EC_KEY_free_nid_postfix(second);
}

static void CheckTooLong() {
    std::uint64_t words[] = {0x55};
    std::uint64_t small[] = {1, 2, 3, 4};
    TestBignum valid{words, 1, 1, 0, 0};
    TestBignum longWords{small, MaxWords + 1, 4, 0, 0};
    TestBignum longNull{nullptr, MaxWords + 1, 0, 0, 0};
    EcKey* key = EC_KEY_new_by_curve_name_nid_postfix(415);
    Require(EC_KEY_set_private_key_nid_postfix(key, &valid) == 1, "EC_KEY_set_private_key before a long key");
    Require(EC_KEY_set_private_key_nid_postfix(key, &longWords) == 0, "EC_KEY_set_private_key over the word limit");
    Require(Stored(key) == nullptr, "a key over the word limit drops the previous one");
    Require(EC_KEY_set_private_key_nid_postfix(key, &longNull) == 0, "EC_KEY_set_private_key over the word limit with null words");
    Require(Stored(key) == nullptr, "a key over the word limit with null words leaves no key");
    EC_KEY_free_nid_postfix(key);
}

static void CheckGroupChange() {
    std::uint64_t words[] = {0x1234, 0x5678};
    TestBignum source{words, 2, 2, 0, 0};
    EcKey* key = EC_KEY_new_by_curve_name_nid_postfix(415);
    EcGroup* group = EC_GROUP_new_by_curve_name_nid_postfix(715);
    Require(EC_KEY_set_private_key_nid_postfix(key, &source) == 1, "EC_KEY_set_private_key before EC_KEY_set_group");
    const TestBignum* stored = Stored(key);
    Require(EC_KEY_set_group_nid_postfix(key, group) == 1, "EC_KEY_set_group with a private key");
    Require(Stored(key) == stored, "EC_KEY_set_group keeps the private key");
    RequireCopy(key, source, 2, 0, words, "the private key after EC_KEY_set_group");
    Require(EC_KEY_set_group_nid_postfix(key, nullptr) == 0, "EC_KEY_set_group with a null group");
    Require(Stored(key) == stored, "a null group keeps the private key");
    EC_GROUP_free_nid_postfix(group);
    EC_KEY_free_nid_postfix(key);
}

static void CheckGuestHeap() {
    std::uint64_t first[] = {0x1111, 0x2222, 0x3333};
    std::uint64_t second[] = {0x4444};
    TestBignum three{first, 3, 3, 0, 0};
    TestBignum one{second, 1, 1, 0, 0};
    TestBignum zero{nullptr, 0, 0, 0, 0};
    EcKey* key = EC_KEY_new_by_curve_name_nid_postfix(415);
    allocations = 0;
    freed.clear();
    Require(EC_KEY_set_private_key_nid_postfix(key, &three) == 1, "EC_KEY_set_private_key on the test heap");
    const TestBignum* stored = Stored(key);
    Require(allocations == 2 && freed.empty(), "a copy of three words takes two guest allocations");
    Require(Live(stored, sizeof(TestBignum)) && Live(stored->d, 3 * sizeof(std::uint64_t)), "the copy and its words come from the guest heap");
    const void* oldNumber = stored;
    const void* oldWords = stored->d;
    Require(EC_KEY_set_private_key_nid_postfix(key, &one) == 1, "EC_KEY_set_private_key replacing a key on the test heap");
    Require(allocations == 4 && freed.size() == 2, "replacing a key frees the old copy and its words");
    Require(FreedWiped(oldWords, 3 * sizeof(std::uint64_t)) && FreedWiped(oldNumber, sizeof(TestBignum)), "the old words and BIGNUM are cleared before they are freed");
    Require(EC_KEY_set_private_key_nid_postfix(key, &zero) == 1, "EC_KEY_set_private_key of zero on the test heap");
    Require(allocations == 5 && freed.size() == 4, "a zero copy takes one guest allocation");
    Require(Live(Stored(key), sizeof(TestBignum)), "the zero copy comes from the guest heap");
    Require(EC_KEY_set_private_key_nid_postfix(key, nullptr) == 0, "EC_KEY_set_private_key with a null key on the test heap");
    Require(allocations == 5 && freed.size() == 5, "a null key frees the old copy");
    Require(EC_KEY_set_private_key_nid_postfix(key, &three) == 1, "EC_KEY_set_private_key before EC_KEY_free");
    stored = Stored(key);
    oldNumber = stored;
    oldWords = stored->d;
    freed.clear();
    EC_KEY_free_nid_postfix(key);
    Require(freed.size() == 2 && FreedWiped(oldWords, 3 * sizeof(std::uint64_t)) && FreedWiped(oldNumber, sizeof(TestBignum)), "EC_KEY_free clears and frees the private key");
    Require(live.empty(), "no guest block is left");
}

static void CheckThrows() {
    std::uint64_t words[] = {0x99, 0x77};
    TestBignum valid{words, 2, 2, 0, 0};
    TestBignum negative{words, -1, 2, 0, 0};
    TestBignum nullWords{nullptr, 2, 2, 0, 0};
    TestBignum nullAtLimit{nullptr, MaxWords, 0, 0, 0};
    RequireThrows([] { EC_KEY_get0_private_key_nid_postfix(nullptr); }, "EC_KEY_get0_private_key of a null key");
    RequireThrows([&] { EC_KEY_set_private_key_nid_postfix(nullptr, &valid); }, "EC_KEY_set_private_key of a null key");
    EcKey* key = EC_KEY_new_by_curve_name_nid_postfix(415);
    Require(EC_KEY_set_private_key_nid_postfix(key, &valid) == 1, "EC_KEY_set_private_key before the throwing calls");
    const TestBignum* stored = Stored(key);
    RequireThrows([&] { EC_KEY_set_private_key_nid_postfix(key, stored); }, "EC_KEY_set_private_key with the key's own private key");
    TestBignum sharedWords{stored->d, 2, 2, 0, 0};
    RequireThrows([&] { EC_KEY_set_private_key_nid_postfix(key, &sharedWords); }, "EC_KEY_set_private_key with the words of the key's own private key");
    TestBignum lastWord{stored->d + 1, 1, 1, 0, 0};
    RequireThrows([&] { EC_KEY_set_private_key_nid_postfix(key, &lastWord); }, "EC_KEY_set_private_key with a word of the key's own private key");
    auto* writable = const_cast<TestBignum*>(stored);
    writable->top = MaxWords + 1;
    RequireThrows([&] { EC_KEY_set_private_key_nid_postfix(key, stored); }, "EC_KEY_set_private_key with the key's own private key over the word limit");
    writable->top = 2;
    TestBignum sharedZero{stored->d + 1, 0, 2, 0, 0};
    RequireThrows([&] { EC_KEY_set_private_key_nid_postfix(key, &negative); }, "EC_KEY_set_private_key with a negative word count");
    RequireThrows([&] { EC_KEY_set_private_key_nid_postfix(key, &nullWords); }, "EC_KEY_set_private_key with null words");
    RequireThrows([&] { EC_KEY_set_private_key_nid_postfix(key, &nullAtLimit); }, "EC_KEY_set_private_key with null words at the word limit");
    Require(Stored(key) == stored, "a throwing call keeps the private key");
    RequireCopy(key, valid, 2, 0, words, "the private key after the throwing calls");
    Require(EC_KEY_set_private_key_nid_postfix(key, &sharedZero) == 1, "EC_KEY_set_private_key of zero over the key's own words");
    RequireCopy(key, sharedZero, 0, 0, nullptr, "a zero copy reads no words");
    EC_KEY_free_nid_postfix(key);
}

struct GeneratedSize {
    int nid;
    int words;
};

static constexpr GeneratedSize GeneratedSizes[] = {
    {409, 3}, {415, 4}, {711, 3}, {712, 4}, {713, 4}, {714, 4}, {715, 6}, {716, 9}, {928, 4}, {932, 7}, {934, 8},
};

static constexpr unsigned char P256RfcPublicKey[] = {
    0x04, 0x60, 0xfe, 0xd4, 0xba, 0x25, 0x5a, 0x9d, 0x31, 0xc9, 0x61, 0xeb, 0x74, 0xc6, 0x35, 0x6d,
    0x68, 0xc0, 0x49, 0xb8, 0x92, 0x3b, 0x61, 0xfa, 0x6c, 0xe6, 0x69, 0x62, 0x2e, 0x60, 0xf2, 0x9f,
    0xb6, 0x79, 0x03, 0xfe, 0x10, 0x08, 0xb8, 0xbc, 0x99, 0xa4, 0x1a, 0xe9, 0xe9, 0x56, 0x28, 0xbc,
    0x64, 0xf2, 0xf1, 0xb2, 0x0c, 0x2d, 0x7e, 0x9f, 0x51, 0x77, 0xa3, 0xc2, 0x94, 0xd4, 0x46, 0x22,
    0x99,
};

static void RequireGenerated(const EcKey* key, int words, const char* what) {
    const TestBignum* stored = Stored(key);
    Require(stored != nullptr && stored->d != nullptr && stored->dmax == words && stored->neg == 0 && stored->flags == Malloced, what);
    Require(Live(stored, sizeof(TestBignum)), what);
    Require(stored->top > 0 && stored->top <= words && stored->d[stored->top - 1] != 0, what);
    for (int word = stored->top; word < words; ++word) Require(stored->d[word] == 0, what);
}

static void CheckGeneratedSizes() {
    for (const GeneratedSize& size : GeneratedSizes) {
        EcKey* key = EC_KEY_new_by_curve_name_nid_postfix(size.nid);
        allocations = 0;
        freed.clear();
        Require(EC_KEY_generate_key_nid_postfix(key) == 1, "EC_KEY_generate_key");
        Require(allocations == 2 && freed.empty(), "a first EC_KEY_generate_key takes a BIGNUM and its words from the guest heap");
        RequireGenerated(key, size.words, "the generated private key has the words of BN_rand_range");
        Require(Live(Stored(key)->d, static_cast<std::size_t>(size.words) * sizeof(std::uint64_t)), "the generated words come from the guest heap");
        const TestBignum* stored = Stored(key);
        const std::uint64_t* words = stored->d;
        Require(EC_KEY_generate_key_nid_postfix(key) == 1, "EC_KEY_generate_key again");
        Require(Stored(key) == stored && stored->d == words && allocations == 2 && freed.empty(), "a second EC_KEY_generate_key reuses the BIGNUM and its words");
        RequireGenerated(key, size.words, "the regenerated private key");
        EC_KEY_free_nid_postfix(key);
    }
}

static void CheckGenerateOverShortKey() {
    std::uint64_t five[] = {5};
    TestBignum source{five, 1, 1, 0, ConstTime};
    EcKey* key = EC_KEY_new_by_curve_name_nid_postfix(415);
    Require(EC_KEY_set_private_key_nid_postfix(key, &source) == 1, "EC_KEY_set_private_key of 5");
    const TestBignum* stored = Stored(key);
    const void* oldWords = stored->d;
    allocations = 0;
    freed.clear();
    Require(EC_KEY_generate_key_nid_postfix(key) == 1, "EC_KEY_generate_key over a one-word key");
    Require(Stored(key) == stored && stored->d != oldWords, "EC_KEY_generate_key keeps the BIGNUM and grows its words");
    Require(allocations == 1 && freed.size() == 1 && FreedWiped(oldWords, sizeof(std::uint64_t)), "the one-word buffer is cleared and freed");
    Require(Live(stored->d, 4 * sizeof(std::uint64_t)), "the grown words come from the guest heap");
    RequireGenerated(key, 4, "the private key generated over a one-word key");
    EC_KEY_free_nid_postfix(key);
}

static void CheckGenerateOverLongKey() {
    std::uint64_t words[] = {1, 2, 3, 4, 0x99};
    TestBignum source{words, 5, 5, 0, 0};
    EcKey* key = EC_KEY_new_by_curve_name_nid_postfix(415);
    Require(EC_KEY_set_private_key_nid_postfix(key, &source) == 1, "EC_KEY_set_private_key of five words");
    const TestBignum* stored = Stored(key);
    const std::uint64_t* oldWords = stored->d;
    allocations = 0;
    freed.clear();
    Require(EC_KEY_generate_key_nid_postfix(key) == 1, "EC_KEY_generate_key over a five-word key");
    Require(Stored(key) == stored && stored->d == oldWords && allocations == 0 && freed.empty(), "EC_KEY_generate_key keeps a buffer that is large enough");
    Require(stored->dmax == 5 && stored->top > 0 && stored->top <= 4 && stored->neg == 0 && stored->flags == Malloced, "the five-word buffer keeps dmax 5");
    for (int word = stored->top; word < 4; ++word) Require(stored->d[word] == 0, "the words above the value up to four words are zero");
    Require(stored->d[4] == 0x99, "the word past the four written words is left as it was");
    EC_KEY_free_nid_postfix(key);
}

static void CheckGenerateOverStaticKey() {
    std::uint64_t five[] = {5};
    TestBignum source{five, 1, 1, 0, 0};
    EcKey* key = EC_KEY_new_by_curve_name_nid_postfix(415);
    Require(EC_KEY_set_private_key_nid_postfix(key, &source) == 1, "EC_KEY_set_private_key of 5");
    auto* stored = const_cast<TestBignum*>(Stored(key));
    const std::uint64_t* oldWords = stored->d;
    stored->flags |= StaticData;
    allocations = 0;
    freed.clear();
    Require(EC_KEY_generate_key_nid_postfix(key) == 0, "EC_KEY_generate_key cannot grow static words");
    Require(Stored(key) == stored && stored->d == oldWords && stored->d[0] == 5 && stored->top == 1 && stored->dmax == 1 && allocations == 0 && freed.empty(), "a failed EC_KEY_generate_key leaves static words as they were");
    stored->flags = Malloced;
    EC_KEY_free_nid_postfix(key);
}

static void CheckStoredKeyUse() {
    std::uint64_t rfcWords[] = {0x7b8a622b120f6721, 0x4e50c3db36e89b12, 0x6b5c215767b1d693, 0xc9afa9d845ba7516};
    TestBignum rfcKey{rfcWords, 4, 4, 0, 0};
    EcKey* known = EC_KEY_new_by_curve_name_nid_postfix(415);
    EcPoint* rfcPoint = EC_POINT_new_nid_postfix(EC_KEY_get0_group_nid_postfix(known));
    Require(EC_POINT_oct2point_nid_postfix(EC_KEY_get0_group_nid_postfix(known), rfcPoint, P256RfcPublicKey, sizeof(P256RfcPublicKey), nullptr) == 1, "the RFC 6979 public key");
    Require(EC_KEY_set_private_key_nid_postfix(known, &rfcKey) == 1 && EC_KEY_set_public_key_nid_postfix(known, rfcPoint) == 1, "the RFC 6979 key pair");
    Require(EC_KEY_check_key_nid_postfix(known) == 1, "EC_KEY_check_key reads the stored words least significant first");
    EC_POINT_free_nid_postfix(rfcPoint);
    EC_KEY_free_nid_postfix(known);

    EcKey* first = EC_KEY_new_by_curve_name_nid_postfix(415);
    EcKey* second = EC_KEY_new_by_curve_name_nid_postfix(415);
    EcKey* copy = EC_KEY_new_by_curve_name_nid_postfix(415);
    Require(EC_KEY_generate_key_nid_postfix(first) == 1 && EC_KEY_generate_key_nid_postfix(second) == 1, "two generated P-256 keys");
    Require(EC_KEY_set_private_key_nid_postfix(copy, Stored(first)) == 1, "EC_KEY_set_private_key from a generated key");
    Require(EC_KEY_set_public_key_nid_postfix(copy, EC_KEY_get0_public_key_nid_postfix(first)) == 1, "EC_KEY_set_public_key from a generated key");
    Require(EC_KEY_check_key_nid_postfix(copy) == 1, "the generated private key matches its public key");

    const unsigned char digest[32] = {0x01, 0x23, 0x45, 0x67, 0x89, 0xab, 0xcd, 0xef, 0x10, 0x32, 0x54, 0x76, 0x98, 0xba, 0xdc, 0xfe};
    unsigned char signature[72] = {};
    unsigned int length = 0;
    Require(ECDSA_sign_nid_postfix(0, digest, sizeof(digest), signature, &length, copy) == 1, "ECDSA_sign with a copied private key");
    Require(ECDSA_verify_nid_postfix(0, digest, sizeof(digest), signature, static_cast<int>(length), first) == 1, "the signature verifies with the generated key");

    const EcPoint* peer = EC_KEY_get0_public_key_nid_postfix(second);
    unsigned char fromCopy[32] = {};
    unsigned char fromFirst[32] = {};
    Require(ECDH_compute_key_nid_postfix(fromCopy, sizeof(fromCopy), peer, copy, nullptr) == 32, "ECDH_compute_key with a copied private key");
    Require(ECDH_compute_key_nid_postfix(fromFirst, sizeof(fromFirst), peer, first, nullptr) == 32, "ECDH_compute_key with the generated key");
    Require(std::memcmp(fromCopy, fromFirst, sizeof(fromCopy)) == 0, "both keys give the same secret");

    Require(EC_KEY_set_private_key_nid_postfix(copy, Stored(second)) == 1, "EC_KEY_set_private_key from another generated key");
    Require(EC_KEY_check_key_nid_postfix(copy) == 0, "EC_KEY_check_key with a private key of another pair");
    EC_KEY_free_nid_postfix(copy);
    EC_KEY_free_nid_postfix(second);
    EC_KEY_free_nid_postfix(first);
}

int main() {
    InitializeHeap();
    CheckNewKeys();
    CheckCopy();
    CheckCopiedFields();
    CheckZero();
    CheckReplaceAndClear();
    CheckOtherKey();
    CheckTooLong();
    CheckGroupChange();
    CheckGuestHeap();
    CheckThrows();
    CheckGeneratedSizes();
    CheckGenerateOverShortKey();
    CheckGenerateOverLongKey();
    CheckGenerateOverStaticKey();
    CheckStoredKeyUse();
    return 0;
}
