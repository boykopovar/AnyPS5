#include "prx/libSceLibreSsl/libSceLibreSsl.hpp"
#include "prx/libSceLibreSsl/include/ObjectTable.hpp"
#include "prx/libc/include/Api.hpp"
#include "prx/libc/include/ApplicationHeap.hpp"
#include <array>
#include <cerrno>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <climits>
#include <cstring>
#include <new>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>
#ifndef _WIN32
#include <sys/mman.h>
#include <unistd.h>
#endif

#ifndef _WIN32
std::size_t hostAllocationLimit = SIZE_MAX;
void* operator new(std::size_t bytes) {
    if (bytes > hostAllocationLimit) {
        std::fprintf(stderr, "host allocation of %zu bytes exceeds the test cap\n", bytes);
        std::abort();
    }
    if (void* pointer = std::malloc(bytes ? bytes : 1)) return pointer;
    throw std::bad_alloc();
}
void* operator new(std::size_t bytes, const std::nothrow_t&) noexcept {
    if (bytes > hostAllocationLimit) {
        std::fprintf(stderr, "host allocation of %zu bytes exceeds the test cap\n", bytes);
        std::abort();
    }
    return std::malloc(bytes ? bytes : 1);
}
void operator delete(void* pointer) noexcept { std::free(pointer); }
void operator delete(void* pointer, std::size_t) noexcept { std::free(pointer); }
void operator delete(void* pointer, const std::nothrow_t&) noexcept { std::free(pointer); }
#endif

namespace {

template<typename TCall>
void Capped(TCall call) {
#ifndef _WIN32
    hostAllocationLimit = 1 << 20;
#endif
    call();
#ifndef _WIN32
    hostAllocationLimit = SIZE_MAX;
#endif
}

constexpr std::uint64_t UnknownNid = (8ull << 24) | (0xfffull << 12) | 101;
constexpr std::uint64_t BufferMalloc = (7ull << 24) | (0xfffull << 12) | 65;
constexpr std::uint64_t X509Malloc = (11ull << 24) | (0xfffull << 12) | 65;

void Require(bool condition, const char* message) {
    if (condition) return;
    std::fprintf(stderr, "failed: %s\n", message);
    std::abort();
}

template<typename TAction>
void RequireThrows(TAction action, const char* message) {
    try {
        action();
    } catch (const std::invalid_argument&) {
        return;
    }
    Require(false, message);
}

class Builder {
public:
    Builder& Add(const LibreSslAsn1Object* object, std::string bytes, int type = 12) {
        values.push_back(std::move(bytes));
        strings.push_back(Asn1String{});
        entries.push_back(LibreSslX509NameEntry{const_cast<LibreSslAsn1Object*>(object), nullptr, static_cast<std::int32_t>(entries.size()), 0});
        types.push_back(type);
        return *this;
    }

    LibreSslX509Name* Build() {
        for (std::size_t index = 0; index < entries.size(); ++index) {
            strings[index] = Asn1String{static_cast<std::int32_t>(values[index].size()), types[index],
                reinterpret_cast<unsigned char*>(values[index].data()), 0};
            entries[index].value = &strings[index];
            pointers.push_back(reinterpret_cast<char*>(&entries[index]));
        }
        stack = LibreSslStack{static_cast<std::int32_t>(entries.size()), pointers.data(), 0, static_cast<std::int32_t>(pointers.size()), nullptr};
        name = LibreSslX509Name{&stack, 0, nullptr, nullptr, 0};
        return &name;
    }

private:
    std::vector<std::string> values;
    std::vector<Asn1String> strings;
    std::vector<LibreSslX509NameEntry> entries;
    std::vector<int> types;
    std::vector<char*> pointers;
    LibreSslStack stack{};
    LibreSslX509Name name{};
};

const LibreSslAsn1Object* Object(int nid) {
    return &LibreSslObjects_nid_no_patch[nid];
}

std::string Oneline(const LibreSslX509Name* name, int length) {
    std::vector<char> buffer(static_cast<std::size_t>(length > 0 ? length : 1) + 16, 'Z');
    char* result = X509_NAME_oneline_nid_postfix(name, buffer.data(), length);
    Require(result == buffer.data(), "a caller buffer is returned");
    return result;
}

std::string Allocated(const LibreSslX509Name* name) {
    char* result = X509_NAME_oneline_nid_postfix(name, nullptr, 0);
    Require(result != nullptr, "an allocated result");
    std::string text = result;
    free_nid_postfix(result);
    return text;
}

void Basics() {
    Require(Allocated(nullptr) == "NO X509_NAME", "a null name");
    Require(Oneline(nullptr, 5) == "NO X", "a null name is truncated as strlcpy does");
    Builder empty;
    Require(Allocated(empty.Build()) == "" && Oneline(empty.Build(), 8) == "", "a name without entries");
    LibreSslX509Name noStack{nullptr, 0, nullptr, nullptr, 0};
    Require(Allocated(&noStack) == "", "a name without an entry stack");
    Builder pair;
    pair.Add(Object(14), "US").Add(Object(13), "example.com");
    LibreSslX509Name* name = pair.Build();
    Require(Allocated(name) == "/C=US/CN=example.com", "short names and values");
    Require(Oneline(name, 64) == "/C=US/CN=example.com", "the same text in a caller buffer");
    Require(Oneline(name, 21) == "/C=US/CN=example.com", "a buffer that just fits");
    Require(Oneline(name, 20) == "/C=US", "an entry that does not fit is left out with the ones after it");
    Require(Oneline(name, 6) == "/C=US", "the first entry alone");
    Require(Oneline(name, 5) == "", "nothing fits");
    Require(Oneline(name, 0) == "", "a zero length still terminates the buffer");
}

void Escapes() {
    Builder name;
    name.Add(Object(17), std::string("a\nb\x7f\xc3\xa9~", 7));
    Require(Allocated(name.Build()) == "/O=a\\x0Ab\\x7F\\xC3\\xA9~", "bytes outside ' ' to '~' are escaped");
    Builder wide;
    wide.Add(Object(17), std::string("\0\0\0A\0\0\0B", 8), 27);
    Require(Allocated(wide.Build()) == "/O=AB", "a four-byte GeneralString with only low bytes keeps those");
    Builder mixed;
    mixed.Add(Object(17), std::string("\0\x01\0A", 4), 27);
    Require(Allocated(mixed.Build()) == "/O=\\x00\\x01\\x00A", "any other GeneralString keeps every byte");
    Builder odd;
    odd.Add(Object(17), std::string("\0\0A", 3), 27);
    Require(Allocated(odd.Build()) == "/O=\\x00\\x00A", "a GeneralString not a multiple of four keeps every byte");
    Builder utf8;
    utf8.Add(Object(17), std::string("\0\0\0A", 4));
    Require(Allocated(utf8.Build()) == "/O=\\x00\\x00\\x00A", "the filter applies to GeneralString only");
}

void Objects() {
    ERR_clear_error_nid_postfix();
    unsigned char unknownData[] = {0x2a, 0x03, 0x04};
    LibreSslAsn1Object unknown{nullptr, nullptr, 0, 3, unknownData, 0};
    Builder name;
    name.Add(&unknown, "x");
    Require(Allocated(name.Build()) == "/1.2.3.4=x", "an unknown OID is printed in dotted form");
    LibreSslAsn1Object gap{nullptr, nullptr, 102, 3, unknownData, 0};
    Builder unassigned;
    unassigned.Add(&gap, "y");
    Require(Allocated(unassigned.Build()) == "/1.2.3.4=y", "an unassigned NID falls back to the dotted form");
    Require(ERR_get_error_nid_postfix() == UnknownNid && ERR_get_error_nid_postfix() == UnknownNid && ERR_get_error_nid_postfix() == UnknownNid
        && ERR_get_error_nid_postfix() == 0, "the short name lookup and both i2t lookups report OBJ_R_UNKNOWN_NID");
    Builder none;
    none.Add(nullptr, "z");
    Require(Allocated(none.Build()) == "/=z", "a null object prints an empty name");
}

void Growth() {
    Builder name;
    const std::string first(150, 'a');
    const std::string second(300, 'b');
    name.Add(Object(13), first).Add(Object(17), second);
    Require(Allocated(name.Build()) == "/CN=" + first + "/O=" + second, "a result longer than the first 200 bytes");
}

void UndefinedInputs() {
    Builder name;
    name.Add(Object(13), "a");
    LibreSslX509Name* built = name.Build();
    built->entries->data[0] = nullptr;
    RequireThrows([&] { X509_NAME_oneline_nid_postfix(built, nullptr, 0); }, "a null entry");
    Builder broken;
    broken.Add(Object(13), "a");
    built = broken.Build();
    reinterpret_cast<LibreSslX509NameEntry*>(built->entries->data[0])->value = nullptr;
    RequireThrows([&] { X509_NAME_oneline_nid_postfix(built, nullptr, 0); }, "an entry without a value");
    char buffer[64];
    Builder missingArray;
    missingArray.Add(Object(13), "a");
    built = missingArray.Build();
    built->entries->data = nullptr;
    RequireThrows([&] { X509_NAME_oneline_nid_postfix(built, buffer, sizeof(buffer)); }, "an entry stack without its array");
    Builder missingData;
    missingData.Add(Object(13), "a");
    built = missingData.Build();
    static_cast<Asn1String*>(reinterpret_cast<LibreSslX509NameEntry*>(built->entries->data[0])->value)->data = nullptr;
    RequireThrows([&] { X509_NAME_oneline_nid_postfix(built, buffer, sizeof(buffer)); }, "a value with a length and no data");
}

template<typename TCall>
void RejectsPreservingErrno(TCall call, const char* message) {
    errno = 71;
    bool rejected = false;
    try {
        call();
    } catch (const std::runtime_error&) {
        rejected = errno == 71;
    }
    Require(rejected, message);
}

void GuestPointers() {
    auto* unmapped = reinterpret_cast<void*>(std::uintptr_t{0x10});
    char buffer[64];
    RejectsPreservingErrno([&] { X509_NAME_oneline_nid_postfix(unmapped, buffer, sizeof(buffer)); }, "an unreadable name");
    RejectsPreservingErrno([&] { X509_NAME_oneline_nid_postfix(nullptr, static_cast<char*>(unmapped), 5); }, "an unwritable buffer for a null name");
    Builder pair;
    pair.Add(Object(14), "US").Add(Object(13), "example.com");
    LibreSslX509Name* name = pair.Build();
    RejectsPreservingErrno([&] { X509_NAME_oneline_nid_postfix(name, static_cast<char*>(unmapped), 64); }, "an unwritable caller buffer");
    RejectsPreservingErrno([&] { X509_NAME_oneline_nid_postfix(name, static_cast<char*>(unmapped), 0); }, "an unwritable buffer that only gets a terminator");
    Builder entries;
    entries.Add(Object(13), "a");
    LibreSslX509Name* built = entries.Build();
    built->entries = static_cast<LibreSslStack*>(unmapped);
    RejectsPreservingErrno([&] { X509_NAME_oneline_nid_postfix(built, nullptr, 0); }, "an unreadable entry stack");
    Builder slots;
    slots.Add(Object(13), "a");
    built = slots.Build();
    built->entries->data = static_cast<char**>(unmapped);
    RejectsPreservingErrno([&] { X509_NAME_oneline_nid_postfix(built, nullptr, 0); }, "an unreadable entry array");
    Builder entry;
    entry.Add(Object(13), "a");
    built = entry.Build();
    built->entries->data[0] = static_cast<char*>(unmapped);
    RejectsPreservingErrno([&] { X509_NAME_oneline_nid_postfix(built, nullptr, 0); }, "an unreadable entry");
    Builder value;
    value.Add(Object(13), "a");
    built = value.Build();
    reinterpret_cast<LibreSslX509NameEntry*>(built->entries->data[0])->value = unmapped;
    RejectsPreservingErrno([&] { X509_NAME_oneline_nid_postfix(built, nullptr, 0); }, "an unreadable value");
    Builder data;
    data.Add(Object(13), "a").Add(Object(17), "b");
    built = data.Build();
    static_cast<Asn1String*>(reinterpret_cast<LibreSslX509NameEntry*>(built->entries->data[1])->value)->data = static_cast<unsigned char*>(unmapped);
    std::memset(buffer, 'Z', sizeof(buffer));
    RejectsPreservingErrno([&] { X509_NAME_oneline_nid_postfix(built, buffer, sizeof(buffer)); }, "unreadable value bytes");
    Require(buffer[0] == 'Z', "a rejected call leaves the caller buffer unchanged");
}

std::vector<std::size_t> allocated;
int frees = 0;
int failAt = 0;
int calls = 0;

void* Record(void* pointer, std::size_t size) {
    if (pointer) allocated.push_back(size);
    return pointer;
}

void* APS5_VABI Allocate_nid_no_patch(std::size_t size) {
    if (++calls == failAt) return nullptr;
    return Record(std::malloc(size), size);
}

void* APS5_VABI Calloc_nid_no_patch(std::size_t count, std::size_t size) {
    if (++calls == failAt) return nullptr;
    return Record(std::calloc(count, size), count * size);
}

void APS5_VABI Free_nid_no_patch(void* pointer) {
    if (pointer) ++frees;
    std::free(pointer);
}

void* APS5_VABI Reallocate_nid_no_patch(void*, std::size_t) { throw std::runtime_error("unexpected realloc"); }
void* APS5_VABI Align_nid_no_patch(std::size_t, std::size_t) { throw std::runtime_error("unexpected aligned allocation"); }
void* APS5_VABI Realign_nid_no_patch(void*, std::size_t, std::size_t) { throw std::runtime_error("unexpected aligned reallocation"); }
int APS5_VABI PosixAlign_nid_no_patch(void**, std::size_t, std::size_t) { throw std::runtime_error("unexpected posix_memalign"); }

void Allocator() {
    std::array<void*, 10> api{
        reinterpret_cast<void*>(Allocate_nid_no_patch), reinterpret_cast<void*>(Free_nid_no_patch), reinterpret_cast<void*>(Calloc_nid_no_patch),
        reinterpret_cast<void*>(Reallocate_nid_no_patch), reinterpret_cast<void*>(Align_nid_no_patch), reinterpret_cast<void*>(Realign_nid_no_patch),
        reinterpret_cast<void*>(PosixAlign_nid_no_patch)};
    ApplicationHeapRegister_nid_no_patch(api.data());
    Builder name;
    name.Add(Object(13), std::string(150, 'a')).Add(Object(17), std::string(300, 'b'));
    LibreSslX509Name* built = name.Build();
    char* result = X509_NAME_oneline_nid_postfix(built, nullptr, 0);
    Require(result && allocated == std::vector<std::size_t>{24, 268, 612}, "the buffer header, 268 bytes for 200, then (len + 3) / 3 * 4 for the full text");
    Require(frees == 2, "the header and the first array are freed");
    free_nid_postfix(result);
    for (int failing = 1; failing <= 3; ++failing) {
        ERR_clear_error_nid_postfix();
        allocated.clear();
        frees = calls = 0;
        failAt = failing;
        Require(X509_NAME_oneline_nid_postfix(built, nullptr, 0) == nullptr, "a failed allocation returns null");
        Require(ERR_get_error_nid_postfix() == BufferMalloc && ERR_get_error_nid_postfix() == X509Malloc && ERR_get_error_nid_postfix() == 0,
            "a failed allocation reports the buffer and then the X509 malloc failure");
        Require(frees == static_cast<int>(allocated.size()), "a failed allocation frees what was allocated");
    }
    failAt = 0;
}

void OversizedValues() {
    Builder builder;
    builder.Add(Object(13), "first");
    LibreSslX509Name* name = builder.Build();
    auto* value = static_cast<Asn1String*>(reinterpret_cast<LibreSslX509NameEntry*>(name->entries->data[0])->value);
    char buffer[64];
    std::memset(buffer, 'Z', sizeof(buffer));
    value->length = INT_MAX;
    value->data = reinterpret_cast<unsigned char*>(std::uintptr_t{0x10});
    Capped([&] {
        RejectsPreservingErrno([&] { X509_NAME_oneline_nid_postfix(name, buffer, sizeof(buffer)); }, "an oversized unmapped value");
        RejectsPreservingErrno([&] { X509_NAME_oneline_nid_postfix(name, nullptr, 0); }, "an oversized unmapped value into a grown buffer");
    });
#ifndef _WIN32
    const auto page = static_cast<std::size_t>(sysconf(_SC_PAGESIZE));
    void* mapping = mmap(nullptr, page * 2, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    Require(mapping != MAP_FAILED && mprotect(static_cast<unsigned char*>(mapping) + page, page, PROT_NONE) == 0, "a guard page");
    auto* tail = static_cast<unsigned char*>(mapping) + page - 5;
    std::memcpy(tail, "first", 5);
    value->data = tail;
    Capped([&] {
        RejectsPreservingErrno([&] { X509_NAME_oneline_nid_postfix(name, buffer, sizeof(buffer)); }, "an oversized value before a guard page");
        RejectsPreservingErrno([&] { X509_NAME_oneline_nid_postfix(name, nullptr, 0); }, "an oversized value before a guard page into a grown buffer");
    });
    value->length = 5;
    Capped([&] { Require(Oneline(name, 64) == "/CN=first", "a value ending at a guard page"); });
    munmap(mapping, page * 2);
#endif
    Require(buffer[0] == 'Z', "rejected oversized values leave the caller buffer unchanged");
    Builder wide;
    std::string text(0x2345, 'x');
    for (std::size_t index = 0; index < text.size(); ++index) text[index] = static_cast<char>('a' + index % 26);
    wide.Add(Object(13), text);
    Require(Allocated(wide.Build()) == "/CN=" + text, "a value longer than one read chunk");
}

} // namespace

int main(int argc, char**) {
    if (argc > 1) {
        Allocator();
        return 0;
    }
    std::array<void*, 10> api{};
    ApplicationHeapRegister_nid_no_patch(api.data());
    Basics();
    Escapes();
    Objects();
    Growth();
    UndefinedInputs();
    GuestPointers();
    OversizedValues();
}
