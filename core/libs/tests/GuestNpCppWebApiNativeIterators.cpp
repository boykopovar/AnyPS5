#include "prx/libSceNpCppWebApi/include/Api.hpp"
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <memory>
#include <new>
#include <stdexcept>
#ifdef _WIN32
#include <windows.h>
#else
#include <sys/mman.h>
#include <unistd.h>
#endif

namespace {

void Require(bool value, const char* message) {
    if (!value) throw std::runtime_error(message);
}

struct Method {
    const char* name;
    bool end;
    std::size_t stride;
    void* (*call)(void*, void*);
    bool byValue = false;
};

const Method Methods[]{
    {"ptE7eJxWp0c", true, 24, [](void* out, void* self) -> void* { return _ZN3sce2Np9CppWebApi6Common6VectorINS2_12IntrusivePtrINS1_12Leaderboards2V15EntryEEEE3endEv(out, self); }},
    {"rdEN7D3ZN1E", false, 24, [](void* out, void* self) -> void* { return _ZN3sce2Np9CppWebApi6Common6VectorINS2_12IntrusivePtrINS1_12Leaderboards2V15EntryEEEE5beginEv(out, self); }},
    {"-4qn5SHAuoQ", true, 24, [](void* out, void* self) -> void* { return _ZN3sce2Np9CppWebApi6Common6VectorINS2_12IntrusivePtrINS1_14SessionManager2V136UsersPlayerSessionsInvitationForReadEEEE3endEv(out, self); }},
    {"88Rk5FV9QAs", false, 24, [](void* out, void* self) -> void* { return _ZN3sce2Np9CppWebApi6Common6VectorINS2_12IntrusivePtrINS1_14SessionManager2V136UsersPlayerSessionsInvitationForReadEEEE5beginEv(out, self); }},
    {"zjqTmP0ST9A", false, 24, [](void* out, void* self) -> void* { return _ZN3sce2Np9CppWebApi6Common6VectorINS2_12IntrusivePtrINS1_13InGameCatalog2V57ProductEEEE5beginEv(out, self); }},
    {"b7leY-LNVnI", true, 24, [](void* out, void* self) -> void* { return _ZN3sce2Np9CppWebApi6Common6VectorINS2_12IntrusivePtrINS1_13InGameCatalog2V57ProductEEEE3endEv(out, self); }},
    {"hMxydTJQ670", true, 24, [](void* out, void* self) -> void* { return _ZN3sce2Np9CppWebApi6Common6VectorINS2_12IntrusivePtrINS1_11UserProfile2V113BasicPresenceEEEE3endEv(out, self); }},
    {"t84AiYnMwPg", false, 24, [](void* out, void* self) -> void* { return _ZN3sce2Np9CppWebApi6Common6VectorINS2_12IntrusivePtrINS1_11UserProfile2V113BasicPresenceEEEE5beginEv(out, self); }},
    {"Jv4JETvYzWg", true, 8, [](void* out, void* self) -> void* { return _ZN3sce2Np9CppWebApi6Common6VectorImE3endEv(out, self); }},
    {"8eqBiqB72AE", false, 8, [](void* out, void* self) -> void* { return _ZN3sce2Np9CppWebApi6Common6VectorImE5beginEv(out, self); }},
    {"nQdampVIgR4", false, 24, [](void* out, void* self) -> void* { return _ZN3sce2Np9CppWebApi6Common6VectorINS2_12IntrusivePtrINS1_14SessionManager2V120PlayerSessionForReadEEEE5beginEv(out, self); }},
    {"NA46RXga9OQ", false, 24, [](void* out, void* self) -> void* { return _ZN3sce2Np9CppWebApi6Common6VectorINS2_12IntrusivePtrINS1_13InGameCatalog2V53SkuEEEE5beginEv(out, self); }},
    {"WI0pT1qPXvI", true, 24, [](void* out, void* self) -> void* { return _ZN3sce2Np9CppWebApi6Common6VectorINS2_12IntrusivePtrINS1_13InGameCatalog2V59ContainerEEEE3endEv(out, self); }},
    {"t4-KJiSB1hA", false, 24, [](void* out, void* self) -> void* { return _ZN3sce2Np9CppWebApi6Common6VectorINS2_12IntrusivePtrINS1_13InGameCatalog2V59ContainerEEEE5beginEv(out, self); }},
    {"vrZgN7nMcGM", true, 24, [](void* out, void* self) -> void* { return _ZN3sce2Np9CppWebApi6Common6VectorINS2_12IntrusivePtrINS1_13InGameCatalog2V53SkuEEEE3endEv(out, self); }},
    {"ZDwprZa5o10", true, 32, [](void* out, void* self) -> void* { return _ZN3sce2Np9CppWebApi6Common6VectorINS2_6StringEE3endEv(static_cast<CommonIterator*>(out), static_cast<IntrusivePtr*>(self)); }},
    {"tjOYtZ4xFN0", false, 32, [](void* out, void* self) -> void* { return _ZN3sce2Np9CppWebApi6Common6VectorINS2_6StringEE5beginEv(static_cast<CommonIterator*>(out), static_cast<IntrusivePtr*>(self)); }},
    {"NqoGDM7dSPs", false, 24, [](void* out, void* self) -> void* { return _ZN3sce2Np9CppWebApi6Common6VectorINS2_12IntrusivePtrINS1_11UserProfile2V112BasicProfileEEEE5beginEv(out, self); }},
    {"wS6DC0qTbqk", true, 24, [](void* out, void* self) -> void* { return _ZN3sce2Np9CppWebApi6Common6VectorINS2_12IntrusivePtrINS1_11UserProfile2V112BasicProfileEEEE3endEv(out, self); }},
    {"eDTgkEgXMkQ", false, 24, [](void* out, void* self) -> void* { return _ZN3sce2Np9CppWebApi6Common6VectorINS2_12IntrusivePtrINS1_14SessionManager2V111GameSessionEEEE5beginEv(out, self); }},
    {"yQL6+uRJKgc", false, 24, [](void* out, void* self) -> void* { return _ZN3sce2Np9CppWebApi6Common6VectorINS2_12IntrusivePtrINS1_14SessionManager2V113PlayerSessionEEEE5beginEv(out, self); }},
    {"VDv4UrXnxF4", true, 24, [](void* out, void* self) -> void* { return _ZN3sce2Np9CppWebApi6Common6VectorINS2_12IntrusivePtrINS1_14SessionManager2V117GameSessionPlayerEEEE3endEv(out, self); }},
    {"iGNQ7R33THQ", false, 24, [](void* out, void* self) -> void* { return _ZN3sce2Np9CppWebApi6Common6VectorINS2_12IntrusivePtrINS1_14SessionManager2V117GameSessionPlayerEEEE5beginEv(out, self); }},
    {"TwNp57ufLks", true, 24, [](void* out, void* self) -> void* { return _ZN3sce2Np9CppWebApi6Common6VectorINS2_12IntrusivePtrINS1_14SessionManager2V120PlayerSessionForReadEEEE3endEv(out, self); }},
    {"MObuWNuwKTo", true, 24, [](void* out, void* self) -> void* { return _ZN3sce2Np9CppWebApi6Common6VectorINS2_12IntrusivePtrINS1_14SessionManager2V131ResponsePlayerSessionInvitationEEEE3endEv(out, self); }},
    {"DhOq7Ks23EA", false, 24, [](void* out, void* self) -> void* { return _ZN3sce2Np9CppWebApi6Common6VectorINS2_12IntrusivePtrINS1_14SessionManager2V131ResponsePlayerSessionInvitationEEEE5beginEv(out, self); }},
    {"ELZCcxSXaX8", true, 24, [](void* out, void* self) -> void* { return _ZN3sce2Np9CppWebApi6Common6VectorINS2_12IntrusivePtrINS1_11UserProfile2V16AvatarEEEE3endEv(out, self); }},
    {"LFCfPxGTF9w", false, 24, [](void* out, void* self) -> void* { return _ZN3sce2Np9CppWebApi6Common6VectorINS2_12IntrusivePtrINS1_11UserProfile2V16AvatarEEEE5beginEv(out, self); }},
    {"Dy+2XhOLMbg", false, 24, [](void* out, void* self) -> void* { return _ZN3sce2Np9CppWebApi6Common6VectorINS2_12IntrusivePtrINS1_10Activities2V114UserActivities14UserActivitiesEEEE5beginEv(out, self); }},
    {"0gXEfysz4-g", true, 24, [](void* out, void* self) -> void* { return _ZN3sce2Np9CppWebApi6Common6VectorINS2_12IntrusivePtrINS1_10Activities2V114UserActivities14UserActivitiesEEEE3endEv(out, self); }},
    {"7ouO+CipUSI", false, 24, [](void* out, void* self) -> void* { return _ZN3sce2Np9CppWebApi6Common6VectorINS2_12IntrusivePtrINS1_10Activities2V114UserActivities8ActivityEEEE5beginEv(out, self); }},
    {"mcVR86OUL-Q", true, 24, [](void* out, void* self) -> void* { return _ZN3sce2Np9CppWebApi6Common6VectorINS2_12IntrusivePtrINS1_10Activities2V114UserActivities8ActivityEEEE3endEv(out, self); }},
    {"RvH9AZ5rCkQ", false, 24, [](void* out, void* self) -> void* { return _ZN3sce2Np9CppWebApi6Common6VectorINS2_12IntrusivePtrINS1_17TitleCloudStorage2V110DataStatusEEEE5beginEv(static_cast<CommonIterator*>(out), static_cast<const IntrusivePtr*>(self)); }},
    {"n1yCLMxpseQ", true, 24, [](void* out, void* self) -> void* { return _ZN3sce2Np9CppWebApi6Common6VectorINS2_12IntrusivePtrINS1_17TitleCloudStorage2V110DataStatusEEEE3endEv(static_cast<CommonIterator*>(out), static_cast<const IntrusivePtr*>(self)); }},
    {"u6NRNJREiEE", true, 24, [](void* out, void* self) -> void* { return _ZN3sce2Np9CppWebApi6Common6VectorINS2_12IntrusivePtrINS1_11Matchmaking2V113PlayerForReadEEEE3endEv(static_cast<CommonIterator*>(out), static_cast<const IntrusivePtr*>(self)); }},
    {"woPfFeWQXBs", false, 24, [](void* out, void* self) -> void* { return _ZN3sce2Np9CppWebApi6Common6VectorINS2_12IntrusivePtrINS1_11Matchmaking2V113PlayerForReadEEEE5beginEv(static_cast<CommonIterator*>(out), static_cast<const IntrusivePtr*>(self)); }},
    {"f4gEI1xHQvE", true, 24, [](void* out, void* self) -> void* { return _ZN3sce2Np9CppWebApi6Common6VectorINS2_12IntrusivePtrINS1_11Matchmaking2V118PlayerForOfferReadEEEE3endEv(static_cast<CommonIterator*>(out), static_cast<const IntrusivePtr*>(self)); }},
    {"NJw4sqe8Ci0", false, 24, [](void* out, void* self) -> void* { return _ZN3sce2Np9CppWebApi6Common6VectorINS2_12IntrusivePtrINS1_11Matchmaking2V118PlayerForOfferReadEEEE5beginEv(static_cast<CommonIterator*>(out), static_cast<const IntrusivePtr*>(self)); }},
    {"ucOP9llCucE", true, 24, [](void* out, void* self) -> void* { return _ZN3sce2Np9CppWebApi6Common6VectorINS2_12IntrusivePtrINS1_14SessionManager2V119PlayerSessionPlayerEEEE3endEv(static_cast<CommonIterator*>(out), static_cast<const IntrusivePtr*>(self)); }},
    {"dA0BfgUC88k", false, 24, [](void* out, void* self) -> void* { return _ZN3sce2Np9CppWebApi6Common6VectorINS2_12IntrusivePtrINS1_14SessionManager2V119PlayerSessionPlayerEEEE5beginEv(static_cast<CommonIterator*>(out), static_cast<const IntrusivePtr*>(self)); }},
    {"YdOv0ED76ik", true, 24, [](void* out, void* self) -> void* { return _ZN3sce2Np9CppWebApi6Common6VectorINS2_12IntrusivePtrINS1_14SessionManager2V122PlayerSessionSpectatorEEEE3endEv(static_cast<CommonIterator*>(out), static_cast<const IntrusivePtr*>(self)); }},
    {"a7sI39vUIu0", false, 24, [](void* out, void* self) -> void* { return _ZN3sce2Np9CppWebApi6Common6VectorINS2_12IntrusivePtrINS1_14SessionManager2V122PlayerSessionSpectatorEEEE5beginEv(static_cast<CommonIterator*>(out), static_cast<const IntrusivePtr*>(self)); }},
    {"rQGVRbZAhOI", true, 24, [](void* out, void* self) -> void* { return ::new (out) CommonConstIterator(_ZNK3sce2Np9CppWebApi6Common6VectorINS2_12IntrusivePtrINS1_11Matchmaking2V19AttributeEEEE3endEv(self)); }, true},
    {"O3du3YcB2hw", false, 24, [](void* out, void* self) -> void* { return ::new (out) CommonConstIterator(_ZNK3sce2Np9CppWebApi6Common6VectorINS2_12IntrusivePtrINS1_11Matchmaking2V19AttributeEEEE5beginEv(self)); }, true},
    {"ndoO2leAiXs", true, 24, [](void* out, void* self) -> void* { return ::new (out) CommonConstIterator(_ZNK3sce2Np9CppWebApi6Common6VectorINS2_12IntrusivePtrINS1_11UserProfile2V113BasicPresenceEEEE3endEv(self)); }, true},
    {"CmzH9wXBu9g", false, 24, [](void* out, void* self) -> void* { return ::new (out) CommonConstIterator(_ZNK3sce2Np9CppWebApi6Common6VectorINS2_12IntrusivePtrINS1_11UserProfile2V113BasicPresenceEEEE5beginEv(self)); }, true},
};

struct Output {
    alignas(16) std::array<std::uint8_t, 32> bytes;
};

void Check(const Method& method, void* first, void* capacity, void* last) {
    std::array<void*, 3> vector{first, capacity, last};
    const auto beforeVector = vector;
    Output output;
    output.bytes.fill(0xa5);
    auto expected = output;
    const std::uint32_t count = 0;
    std::memcpy(expected.bytes.data(), &count, sizeof(count));
    void* pointer = method.end ? last : first;
    std::memcpy(expected.bytes.data() + 8, &pointer, sizeof(pointer));
    try {
        void* returned = method.call(output.bytes.data(), vector.data());
        if (method.byValue) {
            for (std::size_t i = 4; i < 8; ++i) expected.bytes[i] = output.bytes[i];
            reinterpret_cast<CommonConstIterator*>(output.bytes.data())->~CommonConstIterator();
        }
        Require(returned == output.bytes.data(), "iterator return address changed");
        Require(output.bytes == expected.bytes, "iterator value or surrounding bytes changed");
        Require(vector == beforeVector, "iterator changed the source vector");
    } catch (const std::exception& error) {
        std::fprintf(stderr, "%s: %s\n", method.name, error.what());
        throw;
    }
}

class ProtectedElements {
public:
    ProtectedElements() {
#ifdef _WIN32
        size = 4096;
        memory = VirtualAlloc(nullptr, size, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
        Require(memory != nullptr, "cannot allocate element page");
        std::memset(memory, 0, size);
        DWORD previous = 0;
        if (!VirtualProtect(memory, size, PAGE_NOACCESS, &previous)) {
            VirtualFree(memory, 0, MEM_RELEASE);
            memory = nullptr;
            throw std::runtime_error("cannot protect element page");
        }
#else
        const long pageSize = sysconf(_SC_PAGESIZE);
        Require(pageSize > 0, "cannot get page size");
        size = static_cast<std::size_t>(pageSize);
        memory = mmap(nullptr, size, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
        Require(memory != MAP_FAILED, "cannot allocate element page");
        std::memset(memory, 0, size);
        if (mprotect(memory, size, PROT_NONE) != 0) {
            munmap(memory, size);
            memory = nullptr;
            throw std::runtime_error("cannot protect element page");
        }
#endif
    }

    ~ProtectedElements() {
#ifdef _WIN32
        if (memory != nullptr) VirtualFree(memory, 0, MEM_RELEASE);
#else
        if (memory != nullptr) munmap(memory, size);
#endif
    }

    void* Data() const { return memory; }

private:
    void* memory = nullptr;
    std::size_t size = 0;
};

void CheckNativeVectors(bool constOnly = false) {
    alignas(16) std::array<std::uint8_t, 512> elements{};
    const ProtectedElements protectedElements;
    for (const auto& method : Methods) {
        if (constOnly && !method.byValue) continue;
        Check(method, nullptr, nullptr, nullptr);
        Check(method, elements.data(), elements.data() + 8 * method.stride, elements.data());
        Check(method, elements.data(), elements.data() + 8 * method.stride, elements.data() + method.stride);
        Check(method, elements.data() + 2 * method.stride, elements.data() + 8 * method.stride, elements.data() + 5 * method.stride);
        if (method.stride == sizeof(std::uint64_t)) {
            for (const auto value : {std::uint64_t{0}, ~std::uint64_t{0}, std::uint64_t{0x434e507056656342ULL}}) {
                std::memcpy(elements.data(), &value, sizeof(value));
                Check(method, elements.data(), elements.data() + 8 * method.stride, elements.data() + 3 * method.stride);
            }
            elements.fill(0);
        }
        auto* first = static_cast<std::uint8_t*>(protectedElements.Data());
        Check(method, first, first + 8 * method.stride, first + 3 * method.stride);
    }
}

void CheckHostVectors() {
    int context = 1;
    for (int repeat = 0; repeat < 32; ++repeat) {
        IntrusivePtr vector{nullptr, nullptr, &context};
        CommonIterator first{}, last{};
        _ZN3sce2Np9CppWebApi6Common6VectorINS2_6StringEE5beginEv(&first, &vector);
        _ZN3sce2Np9CppWebApi6Common6VectorINS2_6StringEE3endEv(&last, &vector);
        Require(first.element == nullptr && last.element == nullptr, "empty host vector changed");
        CommonString value{0, const_cast<char*>("native iterator"), 16, &context};
        Require(_ZN3sce2Np9CppWebApi6Common6VectorINS2_6StringEE8pushBackERKS4_(&vector, &value) == 0, "host append failed");
        for (void* self : {static_cast<void*>(&vector), vector.object}) {
            _ZN3sce2Np9CppWebApi6Common6VectorINS2_6StringEE5beginEv(&first, static_cast<IntrusivePtr*>(self));
            _ZN3sce2Np9CppWebApi6Common6VectorINS2_6StringEE3endEv(&last, static_cast<IntrusivePtr*>(self));
            const auto* entry = static_cast<const CommonString*>(first.element);
            Require(entry != nullptr && last.element == entry + 1, "host vector bounds changed");
            Require(std::strcmp(entry->buffer, value.buffer) == 0 && entry->buffer != value.buffer, "host string ownership changed");
        }
        Require(vector.deleter != nullptr, "host vector has no deleter");
        vector.deleter(vector.object);
    }
}

void CheckHandleResult(const Method& method, void* self, void* expectedElement) {
    std::array<std::uint8_t, sizeof(IntrusivePtr)> before{};
    std::memcpy(before.data(), self, before.size());
    Output output;
    output.bytes.fill(0xa5);
    auto expected = output;
    const std::int32_t zero = 0;
    std::memcpy(expected.bytes.data(), &zero, sizeof(zero));
    std::memcpy(expected.bytes.data() + 8, &expectedElement, sizeof(expectedElement));
    Require(method.call(output.bytes.data(), self) == output.bytes.data(), "handle iterator return changed");
    if (method.byValue) {
        for (std::size_t i = 4; i < 8; ++i) expected.bytes[i] = output.bytes[i];
        reinterpret_cast<CommonConstIterator*>(output.bytes.data())->~CommonConstIterator();
    }
    Require(output.bytes == expected.bytes, "handle identity selected wrong iterator");
    Require(std::memcmp(before.data(), self, before.size()) == 0, "handle lookup changed source bytes");
}

void CheckHandleIdentity() {
    int context = 1;
    IntrusivePtr donor{nullptr, nullptr, &context};
    CommonString text{0, const_cast<char*>("identity"), 9, &context};
    Require(_ZN3sce2Np9CppWebApi6Common6VectorINS2_6StringEE8pushBackERKS4_(&donor, &text) == 0, "identity donor allocation failed");
    std::unique_ptr<void, decltype(&std::free)> body(std::calloc(1, 0x50), &std::free);
    std::unique_ptr<void, decltype(&std::free)> elements(std::calloc(3, sizeof(IntrusivePtr)), &std::free);
    Require(body != nullptr && elements != nullptr, "borrowed body allocation failed");
    auto* bytes = static_cast<std::uint8_t*>(body.get());
    const std::uint64_t magic = 0x434e507056656342ULL, count = 3;
    const std::int32_t references = 1, kind = 1;
    void* data = elements.get();
    std::memcpy(bytes, &magic, sizeof(magic));
    std::memcpy(bytes + 0x30, &references, sizeof(references));
    std::memcpy(bytes + 0x34, &kind, sizeof(kind));
    std::memcpy(bytes + 0x38, &data, sizeof(data));
    std::memcpy(bytes + 0x40, &count, sizeof(count));
    auto callback = donor.deleter;
#ifndef _WIN32
    callback = &std::free;
#endif
    IntrusivePtr borrowed{body.get(), callback, bytes + 72}, copied{};
    Require(reinterpret_cast<std::uintptr_t>(borrowed.object) <= reinterpret_cast<std::uintptr_t>(borrowed.libContext) &&
            reinterpret_cast<std::uintptr_t>(borrowed.libContext) <= reinterpret_cast<std::uintptr_t>(borrowed.deleter), "borrowed fixture lacks ambiguous pointer order");
    _ZN3sce2Np9CppWebApi6Common12IntrusivePtrINS2_6VectorINS3_INS1_13InGameCatalog2V59ContainerEEEEEEC1ERSA_(&copied, &borrowed);
    for (const auto& method : Methods) {
        if (method.stride != sizeof(IntrusivePtr)) continue;
        CheckHandleResult(method, &copied, method.end ? static_cast<std::uint8_t*>(data) + count * sizeof(IntrusivePtr) : data);
    }
    _ZN3sce2Np9CppWebApi6Common12IntrusivePtrINS2_6VectorINS3_INS1_13InGameCatalog2V59ContainerEEEEEED1Ev(&copied);
    std::int32_t after = 0;
    std::memcpy(&after, bytes + 0x30, sizeof(after));
    Require(after == references, "borrowed copy lifetime changed its owner count");
    for (const auto& method : Methods) {
        if (method.stride != sizeof(IntrusivePtr)) continue;
        CheckHandleResult(method, &copied, method.end ? borrowed.libContext : borrowed.object);
    }
    auto* donorBytes = static_cast<std::uint8_t*>(donor.object);
    std::array<void*, 3> native{donorBytes, donorBytes + 64, donorBytes + 8};
    for (const auto& method : Methods) {
        if (method.stride != sizeof(std::uint64_t)) continue;
        CheckHandleResult(method, native.data(), method.end ? native[2] : native[0]);
    }
    std::array<void*, 3> nativeStrings{donorBytes, donorBytes + 64, donorBytes + 32};
    bool refused = false;
    try {
        _ZN3sce2Np9CppWebApi6Common6VectorINS2_6StringEE8pushBackERKS4_(reinterpret_cast<IntrusivePtr*>(nativeStrings.data()), &text);
    } catch (const std::runtime_error&) {
        refused = true;
    }
    Require(refused, "native mutation misidentified a registered body as its handle");
    CommonIterator donorBefore{};
    _ZN3sce2Np9CppWebApi6Common6VectorINS2_6StringEE5beginEv(&donorBefore, &donor);
    IntrusivePtr adopted{}, alias{};
    _ZN3sce2Np9CppWebApi6Common12IntrusivePtrINS2_6VectorINS2_6StringEEEEC1EPS6_PFvS8_EPNS2_10LibContextE(&adopted, donor.object, donor.deleter, donor.libContext);
    _ZN3sce2Np9CppWebApi6Common12IntrusivePtrINS2_6VectorINS2_6StringEEEEC1ERS7_(&alias, &adopted);
    CommonIterator original{}, adoptedFirst{}, aliasFirst{};
    _ZN3sce2Np9CppWebApi6Common6VectorINS2_6StringEE5beginEv(&original, &donor);
    _ZN3sce2Np9CppWebApi6Common6VectorINS2_6StringEE5beginEv(&adoptedFirst, &adopted);
    _ZN3sce2Np9CppWebApi6Common6VectorINS2_6StringEE5beginEv(&aliasFirst, &alias);
    Require(donorBefore.element != nullptr && original.element == donorBefore.element && adoptedFirst.element == original.element && aliasFirst.element == original.element, "tagged String adoption changed element storage");
    std::int32_t sharedReferences = 0;
    std::memcpy(&sharedReferences, donorBytes + 0x30, sizeof(sharedReferences));
    Require(sharedReferences == 3, "tagged String ownership used the wrong counter");
    _ZN3sce2Np9CppWebApi6Common12IntrusivePtrINS2_6VectorINS2_6StringEEEED1Ev(&alias);
    _ZN3sce2Np9CppWebApi6Common12IntrusivePtrINS2_6VectorINS2_6StringEEEED1Ev(&adopted);
    std::memcpy(&sharedReferences, donorBytes + 0x30, sizeof(sharedReferences));
    Require(sharedReferences == 1, "tagged String copy references did not release");
    _ZN3sce2Np9CppWebApi6Common6VectorINS2_6StringEED1Ev(&donor);
    Require(donor.object == nullptr, "last host vector reference did not release its body");
}

}

int main(int argc, char** argv) {
    try {
        const bool constOnly = argc == 2 && std::strcmp(argv[1], "--const-only") == 0;
        const bool identityOnly = argc == 2 && std::strcmp(argv[1], "--handle-identity-only") == 0;
        Require(argc == 1 || constOnly || identityOnly, "expected no arguments, --const-only or --handle-identity-only");
        if (identityOnly) {
            CheckHandleIdentity();
            std::printf("explicit vector handle identity, borrowed ambiguity and lifetime passed\n");
            return 0;
        }
        CheckNativeVectors(constOnly);
        if (constOnly) {
            std::printf("4 native const vector iterator exports passed\n");
        } else {
            CheckHostVectors();
            CheckHandleIdentity();
            std::printf("42 native vector iterator exports, protected element storage and host lifecycle passed\n");
        }
        return 0;
    } catch (const std::exception& error) {
        std::fprintf(stderr, "native vector iterator regression failed: %s\n", error.what());
        return 1;
    }
}
