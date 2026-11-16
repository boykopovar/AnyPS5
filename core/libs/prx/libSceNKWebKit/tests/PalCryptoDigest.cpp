#include "prx/libc/include/general/VabiMacros.hpp"
#include "prx/libSceNKWebKit/libSceNKWebKit.hpp"
#include "prx/libc/include/ApplicationHeap.hpp"

#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <type_traits>
#include <unordered_set>
#include <vector>

static_assert(sizeof(PAL::CryptoDigest) == 8);
static_assert(offsetof(PAL::CryptoDigest, m_context) == 0);
static_assert(sizeof(NKWebKit::WtfAbi::Vector<std::uint8_t>) == 16);
static_assert(offsetof(NKWebKit::WtfAbi::Vector<std::uint8_t>, buffer) == 0);
static_assert(offsetof(NKWebKit::WtfAbi::Vector<std::uint8_t>, capacity) == 8);
static_assert(offsetof(NKWebKit::WtfAbi::Vector<std::uint8_t>, size) == 12);
static_assert(std::is_same_v<std::underlying_type_t<PAL::CryptoDigest::Algorithm>, int>);

static void Check(bool value, const char* name, int line) {
    if (!value) {
        std::fprintf(stderr, "PAL::CryptoDigest check failed for %s at line %d\n", name, line);
        std::abort();
    }
}
#define Require(value, name) Check((value), (name), __LINE__)

using Algorithm = PAL::CryptoDigest::Algorithm;

static std::unordered_set<void*> guestAllocations;

static void* APS5_VABI Allocate_nid_no_patch(std::size_t size) {
    void* pointer = std::malloc(size);
    if (pointer) guestAllocations.insert(pointer);
    return pointer;
}

static void APS5_VABI Free_nid_no_patch(void* pointer) {
    Require(guestAllocations.erase(pointer) == 1, "guest free");
    std::free(pointer);
}

static void* APS5_VABI Calloc_nid_no_patch(std::size_t count, std::size_t size) { return std::calloc(count, size); }
static void* APS5_VABI Reallocate_nid_no_patch(void* pointer, std::size_t size) { return std::realloc(pointer, size); }
static void* APS5_VABI Align_nid_no_patch(std::size_t, std::size_t size) { return Allocate_nid_no_patch(size); }
static void* APS5_VABI Realign_nid_no_patch(void* pointer, std::size_t size, std::size_t) { return Reallocate_nid_no_patch(pointer, size); }
static int APS5_VABI PosixAlign_nid_no_patch(void** pointer, std::size_t alignment, std::size_t size) {
    *pointer = Align_nid_no_patch(alignment, size);
    return *pointer ? 0 : 12;
}

struct Expected {
    Algorithm algorithm;
    std::size_t length;
    const char* abc;
    const char* empty;
    const char* twoBlock;
    const char* thousand;
};

static const Expected kExpected[] = {
    {Algorithm::SHA_1, 20,
     "a9993e364706816aba3e25717850c26c9cd0d89d",
     "da39a3ee5e6b4b0d3255bfef95601890afd80709",
     "84983e441c3bd26ebaae4aa1f95129e5e54670f1",
     "4231a8a50a10fa9758db8ec71fdef855b751048a"},
    {Algorithm::SHA_224, 28,
     "23097d223405d8228642a477bda255b32aadbce4bda0b3f7e36c9da7",
     "d14a028c2a3a2bc9476102bb288234c415a2b01f828ea62ac5b3e42f",
     "75388b16512776cc5dba5da1fd890150b0c6455cb4f58b1952522525",
     "23729dacbd480285c5c68439e2fe22ca5611a63cd6c14d2ec5ae2c85"},
    {Algorithm::SHA_256, 32,
     "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad",
     "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855",
     "248d6a61d20638b8e5c026930c3e6039a33ce45964ff2167f6ecedd419db06c1",
     "1e9bc38cbf860b9ec31918b065f9b52476c549a782e0e7990bed8ce3868d2371"},
    {Algorithm::SHA_384, 48,
     "cb00753f45a35e8bb5a03d699ac65007272c32ab0eded1631a8b605a43ff5bed8086072ba1e7cc2358baeca134c825a7",
     "38b060a751ac96384cd9327eb1b1e36a21fdb71114be07434c0cc7bf63f6e1da274edebfe76f65fbd51ad2f14898b95b",
     "3391fdddfc8dc7393707a65b1b4709397cf8b1d162af05abfe8f450de5f36bc6b0455a8520bc4e6f5fe95b1fe3c8452b",
     "94c38db521ca733b8904c2d14b6e82d33dcfcc26e1318c579dbee1fc2f472019034e792263b9d46e90e700de2f6e7e91"},
    {Algorithm::SHA_512, 64,
     "ddaf35a193617abacc417349ae20413112e6fa4e89a97ea20a9eeee64b55d39a2192992a274fc1a836ba3c23a3feebbd454d4423643ce80e2a9ac94fa54ca49f",
     "cf83e1357eefb8bdf1542850d66d8007d620e4050b5715dc83f4a921d36ce9ce47d0d13c5d85f2b0ff8318d2877eec2f63b931bd47417a81a538327af927da3e",
     "204a8fc6dda82f0a0ced7beb8e08a41657c16ef468b228a8279be331a703c33596fd15c13b1b07f9aa1d3bea57789ca031ad85c7a71dd70354ec631238ca3445",
     "00e36fccf193e59697a92b5ab24666ce6326d7fa16bf10832d0991ddc591112e9dfa6a636950ed9c4d67344a760654c2ff7785e1d60094d651038735b5dccabd"},
};

static const char kTwoBlock[] = "abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq";

static std::vector<std::uint8_t> Thousand() {
    std::vector<std::uint8_t> bytes(1000);
    for (std::size_t i = 0; i < bytes.size(); ++i) bytes[i] = static_cast<std::uint8_t>(i * 7 + 3);
    return bytes;
}

static std::string Hex(const NKWebKit::WtfAbi::Vector<std::uint8_t>& digest) {
    static const char digits[] = "0123456789abcdef";
    std::string text;
    for (unsigned i = 0; i < digest.size; ++i) {
        text.push_back(digits[digest.buffer[i] >> 4]);
        text.push_back(digits[digest.buffer[i] & 15]);
    }
    return text;
}

static PAL::CryptoDigestPtr Create(Algorithm algorithm) {
    PAL::CryptoDigestPtr digest = _ZN3PAL12CryptoDigest6createENS0_9AlgorithmE(algorithm);
    Require(digest != nullptr, "create");
    Require(digest->m_context != nullptr, "context");
    return digest;
}

static std::string Digest(Algorithm algorithm, const std::vector<std::size_t>& chunks, const std::uint8_t* data) {
    PAL::CryptoDigestPtr digest = Create(algorithm);
    std::size_t offset = 0;
    for (std::size_t chunk : chunks) {
        _ZN3PAL12CryptoDigest8addBytesEPKvm(digest.get(), data + offset, chunk);
        offset += chunk;
    }
    NKWebKit::WtfAbi::Vector<std::uint8_t> hash = _ZN3PAL12CryptoDigest11computeHashEv(digest.get());
    return Hex(hash);
}

static void TestVectors() {
    const auto* abc = reinterpret_cast<const std::uint8_t*>("abc");
    const auto* twoBlock = reinterpret_cast<const std::uint8_t*>(kTwoBlock);
    const std::vector<std::uint8_t> thousand = Thousand();
    for (const Expected& e : kExpected) {
        Require(Digest(e.algorithm, {3}, abc) == e.abc, e.abc);
        Require(Digest(e.algorithm, {1, 0, 2}, abc) == e.abc, e.abc);
        Require(Digest(e.algorithm, {}, abc) == e.empty, e.empty);
        Require(Digest(e.algorithm, {0}, nullptr) == e.empty, e.empty);
        Require(Digest(e.algorithm, {56}, twoBlock) == e.twoBlock, e.twoBlock);
        Require(Digest(e.algorithm, {55, 1}, twoBlock) == e.twoBlock, e.twoBlock);
        Require(Digest(e.algorithm, {1000}, thousand.data()) == e.thousand, e.thousand);
        Require(Digest(e.algorithm, {1, 63, 64, 0, 127, 129, 616}, thousand.data()) == e.thousand, e.thousand);
    }
}

static void TestResultLayout() {
    for (const Expected& e : kExpected) {
        PAL::CryptoDigestPtr digest = Create(e.algorithm);
        NKWebKit::WtfAbi::Vector<std::uint8_t> hash = _ZN3PAL12CryptoDigest11computeHashEv(digest.get());
        Require(hash.buffer != nullptr, "buffer");
        Require(hash.size == e.length, "size");
        Require(hash.capacity == e.length, "capacity");
    }
}

static void TestObjectIsGuestAllocated() {
    {
        PAL::CryptoDigestPtr digest = Create(Algorithm::SHA_1);
        Require(guestAllocations.size() == 1, "one guest allocation");
        Require(guestAllocations.count(digest.get()) == 1, "object in guest heap");
    }
    Require(guestAllocations.empty(), "delete frees object");
}

static void TestUnknownAlgorithm() {
    for (int value : {5, 6, -1, 0x7fffffff}) {
        PAL::CryptoDigestPtr digest = _ZN3PAL12CryptoDigest6createENS0_9AlgorithmE(static_cast<Algorithm>(value));
        Require(digest == nullptr, "unknown algorithm");
    }
}

static void TestDestructorsReleaseContext() {
    PAL::CryptoDigestPtr first = Create(Algorithm::SHA_256);
    PAL::CryptoDigest* complete = first.release();
    _ZN3PAL12CryptoDigestD1Ev(complete);
    Require(complete->m_context == nullptr, "D1");
    _ZdlPv_nid_postfix(complete);

    PAL::CryptoDigestPtr second = Create(Algorithm::SHA_512);
    PAL::CryptoDigest* base = second.release();
    _ZN3PAL12CryptoDigestD2Ev(base);
    Require(base->m_context == nullptr, "D2");
    _ZdlPv_nid_postfix(base);
}

int main() {
    void* api[10] = {reinterpret_cast<void*>(&Allocate_nid_no_patch), reinterpret_cast<void*>(&Free_nid_no_patch),
        reinterpret_cast<void*>(&Calloc_nid_no_patch), reinterpret_cast<void*>(&Reallocate_nid_no_patch),
        reinterpret_cast<void*>(&Align_nid_no_patch), reinterpret_cast<void*>(&Realign_nid_no_patch), reinterpret_cast<void*>(&PosixAlign_nid_no_patch)};
    ApplicationHeapRegister_nid_no_patch(api);
    TestVectors();
    TestResultLayout();
    TestObjectIsGuestAllocated();
    TestUnknownAlgorithm();
    Require(guestAllocations.empty(), "unknown algorithm allocates nothing");
    TestDestructorsReleaseContext();
    Require(guestAllocations.empty(), "all objects freed");
    return 0;
}
