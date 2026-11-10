#include "prx/libSceLibreSsl/libSceLibreSsl.hpp"
#include "prx/libSceLibreSsl/include/ObjectTable.hpp"
#include "prx/libc/include/Api.hpp"
#include "prx/libc/include/ApplicationHeap.hpp"
#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <exception>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace {

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
    } catch (const std::exception&) {
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

void Unsupported() {
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
    Unsupported();
}
