#include "prx/libSceIcu/libSceIcu.hpp"
#include "prx/libc/include/GuestArena.hpp"
#include "prx/libkernel/include/Api.hpp"
#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <exception>
#include <vector>

namespace {

constexpr std::size_t Page = 0x4000;
constexpr std::size_t Bytes = 5 * Page;

void Require(bool condition, const char* message) {
    if (condition) return;
    std::fprintf(stderr, "ICU UTF guest memory: %s\n", message);
    std::abort();
}

struct Mapping {
    std::int64_t physical = 0;
    void* view = nullptr;
    void* twin = nullptr;

    Mapping() {
        Require(sceKernelAllocateDirectMemory(0, 0x7fffffffffll, Bytes, 0, 0, &physical) == 0, "allocate shared memory");
        Require(sceKernelMapDirectMemory(&view, Bytes, 3, 0, physical, 0) == 0, "map shared memory");
        Require(sceKernelMapDirectMemory(&twin, Bytes, 3, 0, physical, 0) == 0, "map shared alias");
        Require(GuestArena::GuestArenaContains_nid_postfix(view, Bytes), "mapping belongs to guest arena");
    }

    ~Mapping() {
        Require(sceKernelMunmap(reinterpret_cast<std::uintptr_t>(view), Bytes) == 0, "unmap shared memory");
        Require(sceKernelMunmap(reinterpret_cast<std::uintptr_t>(twin), Bytes) == 0, "unmap shared alias");
        Require(sceKernelReleaseDirectMemory(physical, Bytes) == 0, "release shared memory");
    }

    char* Data() const { return static_cast<char*>(view); }
    void* Destination() const { return Data() + Page - 2; }
    std::int32_t* Length() const { return reinterpret_cast<std::int32_t*>(Data() + 2 * Page + 32); }
    std::int32_t* Substitutions() const { return reinterpret_cast<std::int32_t*>(Data() + 3 * Page + 32); }
    std::int32_t* Error() const { return reinterpret_cast<std::int32_t*>(Data() + 4 * Page + 32); }

    void Reset() const {
        const GuestArena::HostWrite access(view, Bytes);
        Require(access.Open(), "open mapped reset");
        std::memset(view, 0x55, Bytes);
        *Length() = 77;
        *Substitutions() = 99;
        *Error() = 0;
    }

    void Protect(std::size_t page, int protection) const {
        Require(sceKernelMprotect(Data() + page * Page, Page, protection) == 0, "protect shared view");
    }
};

bool WithSub(int operation) { return operation == 2 || operation == 3; }
bool Wide(int operation) { return operation != 1 && operation != 3; }

void* Convert(int operation, void* destination, std::int32_t capacity, std::int32_t* length,
              std::int32_t* substitutions, std::int32_t* error, bool inaccessibleSource = false, bool empty = false) {
    const char bytes[] = "A\xf0\x9f\x99\x82";
    const char16_t units[] = {u'A', 0xd83d, 0xde42};
    const auto* source8 = inaccessibleSource ? reinterpret_cast<const char*>(1) : bytes;
    const auto* source16 = inaccessibleSource ? reinterpret_cast<const char16_t*>(1) : units;
    const auto length8 = empty ? 0 : 5;
    const auto length16 = empty ? 0 : 3;
    switch (operation) {
        case 0: return u_strFromUTF8_nid_postfix(static_cast<char16_t*>(destination), capacity, length, source8, length8, error);
        case 1: return u_strToUTF8_nid_postfix(static_cast<char*>(destination), capacity, length, source16, length16, error);
        case 2: return u_strFromUTF8WithSub_nid_postfix(static_cast<char16_t*>(destination), capacity, length, source8, length8, '?', substitutions, error);
        case 3: return u_strToUTF8WithSub_nid_postfix(static_cast<char*>(destination), capacity, length, source16, length16, '?', substitutions, error);
        default: return u_strFromUTF8Lenient_nid_postfix(static_cast<char16_t*>(destination), capacity, length, source8, length8, error);
    }
}

void Writable(Mapping& mapping) {
    const char bytes[] = "A\xf0\x9f\x99\x82";
    const char16_t units[] = {u'A', 0xd83d, 0xde42, 0};
    for (int operation = 0; operation < 5; ++operation) {
        mapping.Reset();
        auto* destination = mapping.Destination();
        Require(Convert(operation, destination, 8, mapping.Length(), mapping.Substitutions(), mapping.Error()) == destination,
                "mapped conversion return");
        const auto bytesWritten = Wide(operation) ? sizeof(units) : sizeof(bytes);
        Require(*mapping.Error() == 0 && *mapping.Length() == (Wide(operation) ? 3 : 5), "mapped output scalars");
        Require(*mapping.Substitutions() == (WithSub(operation) ? 0 : 99), "mapped substitution count");
        Require(std::memcmp(destination, Wide(operation) ? static_cast<const void*>(units) : bytes, bytesWritten) == 0,
                "mapped output and terminator across page boundary");
        Require(static_cast<char*>(destination)[-1] == 0x55 && static_cast<char*>(destination)[bytesWritten] == 0x55,
                "mapped output canaries");
        Require(std::memcmp(mapping.view, mapping.twin, Bytes) == 0, "shared alias sees destination and scalar writes");
        mapping.Reset();
        Require(Convert(operation, destination, 8, nullptr, nullptr, mapping.Error()) == destination && *mapping.Error() == 0,
                "optional scalar outputs may be absent");
        Require(*mapping.Length() == 77 && *mapping.Substitutions() == 99, "absent scalar outputs untouched");
        mapping.Reset();
        Require(Convert(operation, destination, 1, mapping.Length(), mapping.Substitutions(), mapping.Error(), false, true) == destination,
                "mapped empty conversion return");
        Require(*mapping.Length() == 0 && *mapping.Error() == 0 && static_cast<char*>(destination)[0] == 0 &&
                static_cast<char*>(destination)[Wide(operation) ? 2 : 1] == 0x55,
                "mapped terminator-only write and canary");
        if (Wide(operation)) Require(static_cast<char16_t*>(destination)[0] == 0, "wide empty terminator width");
        mapping.Reset();
        auto* status = mapping.Error();
        Require(Convert(operation, destination, 8, status, WithSub(operation) ? status : nullptr, status) == destination,
                "aliased scalar return");
        Require(*status == (Wide(operation) ? 3 : 5), "length substitution and error alias order");
        Require(Wide(operation) ? static_cast<char16_t*>(destination)[3] == 0x5555 : static_cast<char*>(destination)[5] == 0x55,
                "positive aliased status preserves terminator canary");
        mapping.Reset();
        Require(Convert(operation, nullptr, 0, mapping.Length(), mapping.Substitutions(), mapping.Error()) == nullptr,
                "null preflight return");
        Require(*mapping.Error() == 15 && *mapping.Length() == (operation == 4 ? 5 : Wide(operation) ? 3 : 5),
                "mapped null preflight output");
        Require(*mapping.Substitutions() == (WithSub(operation) ? 0 : 99), "mapped preflight substitution output");
    }
}

enum class AliasInput { Valid, Replacing, Empty };

void* AliasConversion(int operation, AliasInput input, void* destination, std::int32_t capacity,
                      std::int32_t* length, std::int32_t* substitutions, std::int32_t* error) {
    const char16_t malformed[] = {u'A', 0xd800, u'B', 0};
    const char* source8 = input == AliasInput::Empty ? "" : input == AliasInput::Replacing ? "A\x80" "B" : "abc";
    const char16_t* source16 = input == AliasInput::Empty ? u"" : input == AliasInput::Replacing ? malformed : u"abc";
    switch (operation) {
        case 0: return u_strFromUTF8_nid_postfix(static_cast<char16_t*>(destination), capacity, length, source8, -1, error);
        case 1: return u_strToUTF8_nid_postfix(static_cast<char*>(destination), capacity, length, source16, -1, error);
        case 2: return u_strFromUTF8WithSub_nid_postfix(static_cast<char16_t*>(destination), capacity, length, source8, -1, '?', substitutions, error);
        case 3: return u_strToUTF8WithSub_nid_postfix(static_cast<char*>(destination), capacity, length, source16, -1, '?', substitutions, error);
        default: return u_strFromUTF8Lenient_nid_postfix(static_cast<char16_t*>(destination), capacity, length, source8, -1, error);
    }
}

void ScalarAliases(Mapping& mapping) {
    std::size_t cases = 0;
    for (int operation = 0; operation < 5; ++operation) {
        for (const auto input : {AliasInput::Valid, AliasInput::Replacing, AliasInput::Empty}) {
            if (input == AliasInput::Replacing && !WithSub(operation)) continue;
            const std::int32_t count = input == AliasInput::Empty ? 0 : 3;
            const std::int32_t replacements = input == AliasInput::Replacing ? 1 : 0;
            const std::vector<std::int32_t> capacities = count == 0 ? std::vector<std::int32_t>{0, 1, 2} :
                std::vector<std::int32_t>{0, 2, 3, 4};
            for (int alias = 0; alias < (WithSub(operation) ? 4 : 1); ++alias) {
                for (const std::int32_t initial : {0, -124, -127, 10}) {
                    for (const auto capacity : capacities) {
                        for (const bool nullDestination : {false, true}) {
                            if (nullDestination && capacity != 0) continue;
                            mapping.Reset();
                            *mapping.Error() = initial;
                            auto* length = alias == 0 || alias == 3 ? mapping.Error() : mapping.Length();
                            auto* substitutions = alias == 1 || alias == 3 ? mapping.Error() :
                                alias == 2 ? mapping.Length() : mapping.Substitutions();
                            void* destination = nullDestination ? nullptr : mapping.Destination();
                            const std::vector<char> before(mapping.Data(), mapping.Data() + Bytes);
                            auto* result = AliasConversion(operation, input, destination, capacity, length, substitutions, mapping.Error());
                            ++cases;
                            if (initial > 0) {
                                Require(result == nullptr && std::memcmp(before.data(), mapping.view, Bytes) == 0,
                                        "latched aliased error preserves all output bytes");
                                continue;
                            }
                            Require(result == destination, "aliased positive completion retains destination return");
                            auto status = length == mapping.Error() ? count : substitutions == mapping.Error() ? replacements : initial;
                            const bool terminate = status <= 0 && count < capacity;
                            if (status <= 0) {
                                if (count < capacity) {
                                    if (status == -124) status = 0;
                                } else status = count == capacity ? -124 : 15;
                            }
                            Require(*mapping.Error() == status, "aliased scalar status preserves ICU success gate");
                            Require(*length == (length == mapping.Error() ? status : count), "aliased destination length store order");
                            if (WithSub(operation)) {
                                Require(*substitutions == (substitutions == mapping.Error() ? status : substitutions == length ? count : replacements),
                                        "aliased substitution store precedes length and termination");
                            }
                            const char* expected = input == AliasInput::Replacing ? "A?B" : "abc";
                            for (std::int32_t unit = 0; unit < 9; ++unit) {
                                const auto actual = Wide(operation) ? static_cast<char16_t*>(mapping.Destination())[unit] :
                                    static_cast<std::uint8_t*>(mapping.Destination())[unit];
                                const auto canary = Wide(operation) ? 0x5555 : 0x55;
                                const auto wanted = unit < count && unit < capacity ? expected[unit] :
                                    unit == count && terminate ? 0 : canary;
                                Require(actual == wanted, "aliased completion preserves exact prefix and terminator canaries");
                            }
                            Require(static_cast<char*>(mapping.Destination())[-1] == 0x55, "aliased output prefix canary");
                            Require(std::memcmp(mapping.view, mapping.twin, Bytes) == 0, "shared alias sees scalar-alias completion");
                        }
                    }
                }
            }
        }
    }
    std::printf("ICU UTF scalar alias cases: %zu\n", cases);
}

void NoWritePaths(Mapping& mapping) {
    for (int operation = 0; operation < 5; ++operation) {
        mapping.Reset();
        *mapping.Error() = 10;
        for (std::size_t page = 0; page < 5; ++page) mapping.Protect(page, 1);
        Require(Convert(operation, reinterpret_cast<void*>(1), -1, mapping.Length(), mapping.Substitutions(), mapping.Error(), true) == nullptr,
                "latched error needs no writable output or source access");
        Require(*mapping.Error() == 10 && *mapping.Length() == 77 && *mapping.Substitutions() == 99,
                "latched error preserves read-only outputs");
        for (std::size_t page = 0; page < 5; ++page) mapping.Protect(page, 3);
        mapping.Reset();
        mapping.Protect(0, 1);
        mapping.Protect(1, 1);
        Require(Convert(operation, mapping.Destination(), 0, mapping.Length(), mapping.Substitutions(), mapping.Error()) ==
                    (operation == 4 ? nullptr : mapping.Destination()),
                "zero capacity does not open a read-only destination");
        Require(*mapping.Error() == 15 && *mapping.Length() == (operation == 4 ? 5 : Wide(operation) ? 3 : 5),
                "non-null preflight scalar output");
        mapping.Protect(0, 3);
        mapping.Protect(1, 3);
        mapping.Reset();
        for (std::size_t page = 0; page < 4; ++page) mapping.Protect(page, 1);
        Require(Convert(operation, mapping.Destination(), -1, mapping.Length(), mapping.Substitutions(), mapping.Error(), true) == nullptr,
                "negative capacity does not access source or other outputs");
        Require(*mapping.Error() == 1 && *mapping.Length() == 77 && *mapping.Substitutions() == 99,
                "negative capacity updates only error");
        for (std::size_t page = 0; page < 4; ++page) mapping.Protect(page, 3);
    }
    for (const bool aliasLength : {false, true}) {
        mapping.Reset();
        mapping.Protect(0, 1);
        mapping.Protect(1, 1);
        auto* length = aliasLength ? mapping.Error() : mapping.Length();
        Require(Convert(4, mapping.Destination(), 2, length, mapping.Substitutions(), mapping.Error(), true) == nullptr,
                "lenient overflow does not access source or read-only destination");
        Require(*mapping.Error() == 15 && *mapping.Length() == (aliasLength ? 77 : 5) && *mapping.Substitutions() == 99,
                "lenient overflow preserves length-before-error alias order");
        mapping.Protect(0, 3);
        mapping.Protect(1, 3);
    }
}

#ifdef _WIN32
void RejectedWrites(Mapping& mapping) {
    for (int operation = 0; operation < 5; ++operation) {
        for (const std::size_t page : {std::size_t{0}, std::size_t{2}, std::size_t{3}, std::size_t{4}}) {
            if (page == 3 && !WithSub(operation)) continue;
            mapping.Reset();
            const std::vector<char> before(mapping.Data(), mapping.Data() + Bytes);
            mapping.Protect(page, 1);
            {
                const GuestArena::HostWrite access(mapping.Data() + page * Page, sizeof(std::int32_t));
                Require(!access.Open(), "read-only tracked view rejects HostWrite");
            }
            bool rejected = false;
            try {
                Convert(operation, mapping.Destination(), 8, mapping.Length(), mapping.Substitutions(), mapping.Error());
            } catch (const std::exception&) {
                rejected = true;
            }
            Require(rejected, "conversion rejects an unopened output guard");
            Require(std::memcmp(before.data(), mapping.view, Bytes) == 0, "rejected write preserves every output");
            mapping.Protect(page, 3);
        }
        mapping.Reset();
        const std::vector<char> before(mapping.Data(), mapping.Data() + Bytes);
        mapping.Protect(4, 1);
        bool rejected = false;
        try {
            Convert(operation, mapping.Destination(), -1, mapping.Length(), mapping.Substitutions(), mapping.Error(), true);
        } catch (const std::exception&) {
            rejected = true;
        }
        Require(rejected && std::memcmp(before.data(), mapping.view, Bytes) == 0,
                "invalid-argument status write checks a rejected guard");
        mapping.Protect(4, 3);
    }
}
#endif

} // namespace

int main() {
    Mapping mapping;
    Writable(mapping);
    ScalarAliases(mapping);
    NoWritePaths(mapping);
#ifdef _WIN32
    RejectedWrites(mapping);
#endif
    std::puts("ICU UTF guest mappings passed");
}
