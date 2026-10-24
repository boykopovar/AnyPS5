#include "prx/libc/include/general/VabiMacros.hpp"
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <map>
#include <string>

struct Value { Value* parent; void* auxiliary; std::uint64_t payload; std::uint32_t reserved; std::int32_t type; };
struct String { void* storage; };
struct Pair { String key; std::uint64_t reserved; Value value; };
struct Parameter { void* allocator; void* context; std::size_t bufferSize; std::int32_t mode; std::uint32_t reserved[3]; };
extern "C" {
void APS5_VABI _ZN3sce4Json11InitializerC1Ev(void*);
int APS5_VABI _ZN3sce4Json11Initializer10initializeEPKNS0_14InitParameter2E(void*, const Parameter*);
int APS5_VABI _ZN3sce4Json11Initializer9terminateEv(void*);
void APS5_VABI _ZN3sce4Json5ValueC1Ev(Value*);
void APS5_VABI _ZN3sce4Json5ValueD1Ev(Value*);
int APS5_VABI _ZN3sce4Json6Parser5parseERNS0_5ValueEPKcm(Value*, const char*, std::size_t);
int APS5_VABI _ZN3sce4Json5Value9serializeERNS0_6StringE(Value*, String*);
void APS5_VABI _ZN3sce4Json6StringC1EPKc(String*, const char*);
void APS5_VABI _ZN3sce4Json6StringD1Ev(String*);
const char* APS5_VABI _ZNK3sce4Json6String5c_strEv(const String*);
void APS5_VABI _ZN3sce4Json6Object4PairC1Ev(Pair*);
void APS5_VABI _ZN3sce4Json6Object4PairC2Ev(Pair*);
void APS5_VABI _ZN3sce4Json6Object4PairC1ERKNS0_6StringERKNS0_5ValueE(Pair*, const String*, const Value*);
void APS5_VABI _ZN3sce4Json6Object4PairC2ERKNS0_6StringERKNS0_5ValueE(Pair*, const String*, const Value*);
void APS5_VABI _ZN3sce4Json6Object4PairD1Ev(Pair*);
void APS5_VABI _ZN3sce4Json6Object4PairD2Ev(Pair*);
}
static void Require(bool condition, const char* message) {
    if (!condition) { std::fprintf(stderr, "%s\n", message); std::abort(); }
}
struct PairTrace { std::map<void*, std::size_t> live; std::size_t generation = 0; };
static void* APS5_VABI AllocatePair(void*, std::size_t bytes, void* context) {
    auto& trace = *static_cast<PairTrace*>(context);
    void* memory = std::malloc(bytes);
    Require(memory != nullptr, "Tracking allocator requires host memory");
    std::memset(memory, 0xa5, bytes);
    Require(trace.live.emplace(memory, ++trace.generation).second, "Pair storage must have an independently owned allocation generation");
    return memory;
}
static void APS5_VABI FreePair(void*, void* memory, void* context) {
    if (!memory) return;
    Require(static_cast<PairTrace*>(context)->live.erase(memory) == 1, "Pair teardown must release each allocation through its owner exactly once");
    std::free(memory);
}
static void APS5_VABI NotifyPair(void*, int, std::size_t, void*) { Require(false, "Healthy pair construction must not fail allocation"); }
static void* const PairVtable[]{nullptr, nullptr, reinterpret_cast<void*>(AllocatePair), reinterpret_cast<void*>(FreePair), reinterpret_cast<void*>(NotifyPair)};
struct PairAllocator { void* const* vtable = PairVtable; };
static std::string Serialize(Value& value) {
    String output{}; _ZN3sce4Json6StringC1EPKc(&output, "");
    Require(_ZN3sce4Json5Value9serializeERNS0_6StringE(&value, &output) == 0, "Constructed pair values must remain serializable");
    std::string text = _ZNK3sce4Json6String5c_strEv(&output);
    _ZN3sce4Json6StringD1Ev(&output);
    return text;
}
static void CheckMetadata(const Pair& pair, std::uint64_t pattern) {
    Require(pair.reserved == pattern, "Pair construction must preserve the eight inactive bytes between key and value");
    Require(!pair.value.parent && !pair.value.auxiliary && pair.value.reserved == static_cast<std::uint32_t>(pattern), "Pair value construction must initialize ownership without overwriting inactive reserved bytes");
}
int main() {
    PairTrace trace; PairAllocator allocator; Parameter parameter{&allocator, &trace, 8192, 2, {}};
    std::uint8_t initializer = 0; _ZN3sce4Json11InitializerC1Ev(&initializer);
    Require(_ZN3sce4Json11Initializer10initializeEPKNS0_14InitParameter2E(&initializer, &parameter) == 0, "Initialize pair layout contracts");
    const auto baseline = trace.live;
    for (unsigned char poison : {0x5a, 0xa5}) for (bool base : {false, true}) {
        const auto pattern = UINT64_C(0x0101010101010101) * poison;
        Pair empty; std::memset(&empty, poison, sizeof(empty));
        if (base) _ZN3sce4Json6Object4PairC2Ev(&empty);
        else _ZN3sce4Json6Object4PairC1Ev(&empty);
        CheckMetadata(empty, pattern);
        Require(empty.key.storage && !_ZNK3sce4Json6String5c_strEv(&empty.key)[0] && empty.value.type == 0 && empty.value.payload == pattern, "Default pairs must construct an empty key and null value while preserving inactive payload bytes");
        if (base) _ZN3sce4Json6Object4PairD2Ev(&empty);
        else _ZN3sce4Json6Object4PairD1Ev(&empty);
        Require(trace.live == baseline, "Default pair destruction must preserve module allocation identities");
        for (const char* document : {"null", "false", "true", "42", "-7", "1.25", "\"x\"", "[1]", "{\"a\":1}"}) {
            String key{}; _ZN3sce4Json6StringC1EPKc(&key, "key");
            Value source{}; _ZN3sce4Json5ValueC1Ev(&source);
            Require(_ZN3sce4Json6Parser5parseERNS0_5ValueEPKcm(&source, document, std::strlen(document)) == 0, "Construct the independently owned source value");
            Pair copy; std::memset(&copy, poison, sizeof(copy));
            if (base) _ZN3sce4Json6Object4PairC2ERKNS0_6StringERKNS0_5ValueE(&copy, &key, &source);
            else _ZN3sce4Json6Object4PairC1ERKNS0_6StringERKNS0_5ValueE(&copy, &key, &source);
            CheckMetadata(copy, pattern);
            Require(copy.key.storage != key.storage && std::strcmp(_ZNK3sce4Json6String5c_strEv(&copy.key), "key") == 0 && copy.value.type == source.type, "Pair construction must independently own its key and copy the exact source type");
            if (source.type == 0) Require(copy.value.payload == pattern, "Null pair values must preserve inactive payload bytes");
            else if (source.type == 1) Require(copy.value.payload == ((pattern & ~UINT64_C(0xff)) | (source.payload & 0xff)), "Boolean pair values must write only their active payload byte");
            else if (source.type <= 4) Require(copy.value.payload == source.payload, "Numeric pair values must retain exact source payload bits");
            else Require(copy.value.payload != source.payload, "Complex pair values must own independent outer handles");
            Require(Serialize(copy.value) == document && Serialize(source) == document, "Pair copies must preserve source and destination contents independently");
            if (base) _ZN3sce4Json6Object4PairD2Ev(&copy);
            else _ZN3sce4Json6Object4PairD1Ev(&copy);
            _ZN3sce4Json5ValueD1Ev(&source); _ZN3sce4Json6StringD1Ev(&key);
            Require(trace.live == baseline, "Pair copies must release their complete independent ownership");
        }
    }
    Require(_ZN3sce4Json11Initializer9terminateEv(&initializer) == 0 && trace.live.empty(), "Release pair layout resources");
    return 0;
}
