#include "prx/libSceNpCppWebApi/include/Api.hpp"
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <stdexcept>
#ifdef _WIN32
#include <windows.h>
#else
#include <sys/mman.h>
#include <unistd.h>
#endif

namespace {
void Require(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}
struct Method {
    const char* nid;
    char operation;
    std::size_t stride;
    std::uintptr_t (*call)(const void*, std::uint64_t);
};
const Method Methods[]{
    {"ZL5Znq6YsMY", 'i', 24, [](const void* self, std::uint64_t index) -> std::uintptr_t { return reinterpret_cast<std::uintptr_t>(_ZN3sce2Np9CppWebApi6Common6VectorINS2_12IntrusivePtrINS1_14SessionManager2V113PlayerSessionEEEEixEm(const_cast<void*>(self), index)); }},
    {"H0XjNhflED4", 'i', 24, [](const void* self, std::uint64_t index) -> std::uintptr_t { return reinterpret_cast<std::uintptr_t>(_ZN3sce2Np9CppWebApi6Common6VectorINS2_12IntrusivePtrINS1_13InGameCatalog2V59ContainerEEEEixEm(const_cast<void*>(self), index)); }},
    {"TSoQhYKyq5g", 's', 24, [](const void* self, std::uint64_t index) -> std::uintptr_t { (void)index; return static_cast<std::uintptr_t>(_ZNK3sce2Np9CppWebApi6Common6VectorINS2_12IntrusivePtrINS1_13InGameCatalog2V59ContainerEEEE4sizeEv(self)); }},
    {"12wY179+CE8", 's', 24, [](const void* self, std::uint64_t index) -> std::uintptr_t { (void)index; return static_cast<std::uintptr_t>(_ZNK3sce2Np9CppWebApi6Common6VectorINS2_12IntrusivePtrINS1_13InGameCatalog2V53SkuEEEE4sizeEv(self)); }},
    {"OFUPL4Enbf8", 's', 24, [](const void* self, std::uint64_t index) -> std::uintptr_t { (void)index; return static_cast<std::uintptr_t>(_ZNK3sce2Np9CppWebApi6Common6VectorINS2_12IntrusivePtrINS1_7Matches2V119AdditionalStatisticEEEE4sizeEv(self)); }},
    {"8ztJtiS2N8s", 's', 24, [](const void* self, std::uint64_t index) -> std::uintptr_t { (void)index; return static_cast<std::uintptr_t>(_ZNK3sce2Np9CppWebApi6Common6VectorINS2_12IntrusivePtrINS1_7Matches2V122RequestPlayerStatisticEEEE4sizeEv(self)); }},
    {"K5jUzaoyT60", 'e', 24, [](const void* self, std::uint64_t index) -> std::uintptr_t { (void)index; return static_cast<std::uintptr_t>(_ZNK3sce2Np9CppWebApi6Common6VectorINS2_12IntrusivePtrINS1_14SessionManager2V120PlayerSessionForReadEEEE5emptyEv(self)); }},
    {"vzKNY2nY7Jk", 's', 8, [](const void* self, std::uint64_t index) -> std::uintptr_t { (void)index; return static_cast<std::uintptr_t>(_ZNK3sce2Np9CppWebApi6Common6VectorImE4sizeEv(self)); }},
    {"UbsLM8E0PDI", 'i', 24, [](const void* self, std::uint64_t index) -> std::uintptr_t { return reinterpret_cast<std::uintptr_t>(_ZN3sce2Np9CppWebApi6Common6VectorINS2_12IntrusivePtrINS1_14SessionManager2V127ResponsePlayerSessionPlayerEEEEixEm(static_cast<IntrusivePtr*>(const_cast<void*>(self)), index)); }},
    {"kNiWFq7jWQ0", 's', 24, [](const void* self, std::uint64_t index) -> std::uintptr_t { (void)index; return static_cast<std::uintptr_t>(_ZNK3sce2Np9CppWebApi6Common6VectorINS2_12IntrusivePtrINS1_14SessionManager2V127ResponsePlayerSessionPlayerEEEE4sizeEv(self)); }},
    {"I4YvCYkhIqM", 'e', 24, [](const void* self, std::uint64_t index) -> std::uintptr_t { (void)index; return static_cast<std::uintptr_t>(_ZNK3sce2Np9CppWebApi6Common6VectorINS2_12IntrusivePtrINS1_13InGameCatalog2V57ProductEEEE5emptyEv(self)); }},
    {"zlS+hnNf1vE", 'e', 24, [](const void* self, std::uint64_t index) -> std::uintptr_t { (void)index; return static_cast<std::uintptr_t>(_ZNK3sce2Np9CppWebApi6Common6VectorINS2_12IntrusivePtrINS1_13InGameCatalog2V59ContainerEEEE5emptyEv(self)); }},
    {"MCAsOwSWmdk", 'i', 24, [](const void* self, std::uint64_t index) -> std::uintptr_t { return reinterpret_cast<std::uintptr_t>(_ZN3sce2Np9CppWebApi6Common6VectorINS2_12IntrusivePtrINS1_14SessionManager2V111GameSessionEEEEixEm(const_cast<void*>(self), index)); }},
    {"VhcO5ZV4APE", 'i', 24, [](const void* self, std::uint64_t index) -> std::uintptr_t { return reinterpret_cast<std::uintptr_t>(_ZN3sce2Np9CppWebApi6Common6VectorINS2_12IntrusivePtrINS1_14SessionManager2V118GameSessionForReadEEEEixEm(const_cast<void*>(self), index)); }},
    {"7yXITS65REs", 'i', 24, [](const void* self, std::uint64_t index) -> std::uintptr_t { return reinterpret_cast<std::uintptr_t>(_ZN3sce2Np9CppWebApi6Common6VectorINS2_12IntrusivePtrINS1_14SessionManager2V120PlayerSessionForReadEEEEixEm(const_cast<void*>(self), index)); }},
    {"CZbH3YP7JV4", 's', 24, [](const void* self, std::uint64_t index) -> std::uintptr_t { (void)index; return static_cast<std::uintptr_t>(_ZNK3sce2Np9CppWebApi6Common6VectorINS2_12IntrusivePtrINS1_12Leaderboards2V15EntryEEEE4sizeEv(self)); }},
    {"ReOTBNjACr0", 's', 24, [](const void* self, std::uint64_t index) -> std::uintptr_t { (void)index; return static_cast<std::uintptr_t>(_ZNK3sce2Np9CppWebApi6Common6VectorINS2_12IntrusivePtrINS1_13InGameCatalog2V57ProductEEEE4sizeEv(self)); }},
    {"9dRg+ERq5hI", 's', 24, [](const void* self, std::uint64_t index) -> std::uintptr_t { (void)index; return static_cast<std::uintptr_t>(_ZNK3sce2Np9CppWebApi6Common6VectorINS2_12IntrusivePtrINS1_14SessionManager2V118GameSessionForReadEEEE4sizeEv(self)); }},
    {"+ljJoKsr054", 's', 24, [](const void* self, std::uint64_t index) -> std::uintptr_t { (void)index; return static_cast<std::uintptr_t>(_ZNK3sce2Np9CppWebApi6Common6VectorINS2_12IntrusivePtrINS1_14SessionManager2V120PlayerSessionForReadEEEE4sizeEv(self)); }},
    {"Z+9VHJoCmX0", 's', 24, [](const void* self, std::uint64_t index) -> std::uintptr_t { (void)index; return static_cast<std::uintptr_t>(_ZNK3sce2Np9CppWebApi6Common6VectorINS2_12IntrusivePtrINS1_7Matches2V113RequestMemberEEEE4sizeEv(self)); }},
    {"l7FaTnbBF8A", 's', 24, [](const void* self, std::uint64_t index) -> std::uintptr_t { (void)index; return static_cast<std::uintptr_t>(_ZNK3sce2Np9CppWebApi6Common6VectorINS2_12IntrusivePtrINS1_7Matches2V116RequestMatchTeamEEEE4sizeEv(self)); }},
    {"Bufewfs6LUc", 's', 24, [](const void* self, std::uint64_t index) -> std::uintptr_t { (void)index; return static_cast<std::uintptr_t>(_ZNK3sce2Np9CppWebApi6Common6VectorINS2_12IntrusivePtrINS1_7Matches2V118RequestMatchPlayerEEEE4sizeEv(self)); }},
    {"qXL7ilwIZUw", 's', 24, [](const void* self, std::uint64_t index) -> std::uintptr_t { (void)index; return static_cast<std::uintptr_t>(_ZNK3sce2Np9CppWebApi6Common6VectorINS2_12IntrusivePtrINS1_7Matches2V120RequestTeamStatisticEEEE4sizeEv(self)); }},
    {"bvfjdByaA6Q", 's', 24, [](const void* self, std::uint64_t index) -> std::uintptr_t { (void)index; return static_cast<std::uintptr_t>(_ZNK3sce2Np9CppWebApi6Common6VectorINS2_12IntrusivePtrINS1_11UserProfile2V113BasicPresenceEEEE4sizeEv(self)); }},
    {"Rfm9T4Fhns0", 's', 24, [](const void* self, std::uint64_t index) -> std::uintptr_t { (void)index; return static_cast<std::uintptr_t>(_ZNK3sce2Np9CppWebApi6Common6VectorINS2_12IntrusivePtrINS1_11UserProfile2V112BasicProfileEEEE4sizeEv(self)); }},
    {"vtjezVIyVFw", 'i', 24, [](const void* self, std::uint64_t index) -> std::uintptr_t { return reinterpret_cast<std::uintptr_t>(_ZN3sce2Np9CppWebApi6Common6VectorINS2_12IntrusivePtrINS1_14SessionManager2V131JoinedPlayerSessionWithPlatformEEEEixEm(const_cast<void*>(self), index)); }},
    {"Fbhq5mnuB4I", 's', 24, [](const void* self, std::uint64_t index) -> std::uintptr_t { (void)index; return static_cast<std::uintptr_t>(_ZNK3sce2Np9CppWebApi6Common6VectorINS2_12IntrusivePtrINS1_14SessionManager2V131JoinedPlayerSessionWithPlatformEEEE4sizeEv(self)); }},
    {"vpen+AiTSKY", 'e', 24, [](const void* self, std::uint64_t index) -> std::uintptr_t { (void)index; return static_cast<std::uintptr_t>(_ZNK3sce2Np9CppWebApi6Common6VectorINS2_12IntrusivePtrINS1_14SessionManager2V118GameSessionForReadEEEE5emptyEv(self)); }},
    {"vRkef+KyE4A", 's', 24, [](const void* self, std::uint64_t index) -> std::uintptr_t { (void)index; return static_cast<std::uintptr_t>(_ZNK3sce2Np9CppWebApi6Common6VectorINS2_12IntrusivePtrINS1_14SessionManager2V131ResponsePlayerSessionInvitationEEEE4sizeEv(self)); }},
    {"DxyfpphZzhM", 'i', 24, [](const void* self, std::uint64_t index) -> std::uintptr_t { return reinterpret_cast<std::uintptr_t>(_ZN3sce2Np9CppWebApi6Common6VectorINS2_12IntrusivePtrINS1_11UserProfile2V112BasicProfileEEEEixEm(const_cast<void*>(self), index)); }},
    {"OS7yY7NTTok", 'i', 24, [](const void* self, std::uint64_t index) -> std::uintptr_t { return reinterpret_cast<std::uintptr_t>(_ZN3sce2Np9CppWebApi6Common6VectorINS2_12IntrusivePtrINS1_11UserProfile2V16AvatarEEEEixEm(const_cast<void*>(self), index)); }},
    {"cYhnRR03jXU", 'i', 24, [](const void* self, std::uint64_t index) -> std::uintptr_t { return reinterpret_cast<std::uintptr_t>(_ZNK3sce2Np9CppWebApi6Common6VectorINS2_12IntrusivePtrINS1_11UserProfile2V112BasicProfileEEEEixEm(self, index)); }},
    {"ydHbnzAObIs", 's', 24, [](const void* self, std::uint64_t index) -> std::uintptr_t { (void)index; return static_cast<std::uintptr_t>(_ZNK3sce2Np9CppWebApi6Common6VectorINS2_12IntrusivePtrINS1_11UserProfile2V16AvatarEEEE4sizeEv(self)); }},
};

void Check(const Method& method, std::uint8_t* first, std::size_t count, std::uint64_t index) {
    std::array<std::uintptr_t, 16> vector{};
    vector.fill(0xa5a5a5a5a5a5a5a5ULL);
    vector[0] = reinterpret_cast<std::uintptr_t>(first);
    vector[1] = first != nullptr ? reinterpret_cast<std::uintptr_t>(first + 8 * method.stride) : 0;
    vector[2] = first != nullptr ? reinterpret_cast<std::uintptr_t>(first + count * method.stride) : 0;
    const auto before = vector;
    const std::uintptr_t expected = method.operation == 's' ? count : method.operation == 'e' ? count == 0 : reinterpret_cast<std::uintptr_t>(first + index * method.stride);
    try {
        Require(method.call(vector.data(), index) == expected, "native accessor value differs");
        Require(vector == before, "native accessor changed the source object");
    } catch (const std::exception& error) {
        std::fprintf(stderr, "%s: %s\n", method.nid, error.what());
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


void CheckNative() {
    alignas(16) std::array<std::uint8_t, 512> elements{};
    const auto before = elements;
    ProtectedElements protectedElements;
    for (const auto& method : Methods) {
        if (method.operation != 'i') {
            Check(method, nullptr, 0, 0);
            Check(method, elements.data(), 0, 0);
        }
        Check(method, elements.data(), 1, 0);
        Check(method, elements.data(), 3, 1);
        Check(method, elements.data() + method.stride, 7, 6);
        auto* first = static_cast<std::uint8_t*>(protectedElements.Data());
        Check(method, first, 5, 2);
        Require(elements == before, "native accessor changed element storage");
        if (method.stride == 8) {
            for (const auto value : {std::uint64_t{0}, ~std::uint64_t{0}, std::uint64_t{0x434e507056656342ULL}}) {
                std::memcpy(elements.data(), &value, sizeof(value));
                Check(method, elements.data(), 3, 1);
                std::uint64_t after = 0;
                std::memcpy(&after, elements.data(), sizeof(after));
                Require(after == value, "native accessor changed uint64 payload");
            }
            elements.fill(0);
        }
    }
}

void CheckEmptyHost() {
    int context = 1;
    IntrusivePtr handle{nullptr, nullptr, &context};
    for (const auto& method : Methods) {
        if (method.operation == 'i') continue;
        Require(method.call(&handle, 0) == static_cast<std::uintptr_t>(method.operation == 'e'), "empty host handle changed");
        Require(handle.object == nullptr && handle.deleter == nullptr && handle.libContext == &context, "accessor changed empty host handle");
    }
}
}

int main() {
    try {
        CheckNative();
        CheckEmptyHost();
        std::printf("33 native vector accessors, protected elements and empty host handles passed\n");
        return 0;
    } catch (const std::exception& error) {
        std::fprintf(stderr, "native vector accessor regression failed: %s\n", error.what());
        return 1;
    }
}
