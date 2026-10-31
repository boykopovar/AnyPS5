#include "prx/libSceLibreSsl/include/Api.hpp"
#include "prx/libc/include/Api.hpp"
#include "prx/libc/include/ApplicationHeap.hpp"
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <source_location>
#include <stdexcept>
#include <type_traits>
#include <unordered_map>
#include <vector>

using FreeName = void (APS5_VABI*)(LibreSslGeneralName*);
using FreeNames = void (APS5_VABI*)(LibreSslStack*);
static_assert(std::is_same_v<decltype(&GENERAL_NAME_free_nid_postfix), FreeName>);
static_assert(std::is_same_v<decltype(&GENERAL_NAMES_free_nid_postfix), FreeNames>);
static_assert(sizeof(LibreSslGeneralName) == 16);

namespace {

void Require(bool success, std::source_location where = std::source_location::current()) {
    if (success) return;
    std::fprintf(stderr, "GENERAL_NAME free test failed at line %u\n", where.line());
    std::abort();
}

std::unordered_map<void*, std::size_t> live;
std::unordered_map<void*, std::size_t> wipes;
std::vector<void*> freed;

void* APS5_VABI Allocate(std::size_t bytes) {
    void* pointer = std::malloc(bytes);
    if (pointer) live.emplace(pointer, bytes);
    return pointer;
}

void APS5_VABI Free(void* pointer) {
    if (!pointer) return;
    Require(live.erase(pointer) == 1); // catches wrong-allocator and double-free
    const auto it = wipes.find(pointer);
    if (it != wipes.end()) {
        const auto* bytes = static_cast<const unsigned char*>(pointer);
        for (std::size_t i = 0; i < it->second; ++i) Require(bytes[i] == 0);
        wipes.erase(it);
    }
    freed.push_back(pointer);
    std::free(pointer);
}

void* APS5_VABI NoCalloc(std::size_t, std::size_t) { throw std::runtime_error("unexpected calloc"); }
void* APS5_VABI NoRealloc(void*, std::size_t) { throw std::runtime_error("unexpected realloc"); }
void* APS5_VABI NoAlign(std::size_t, std::size_t) { throw std::runtime_error("unexpected align"); }
void* APS5_VABI NoRealign(void*, std::size_t, std::size_t) { throw std::runtime_error("unexpected realign"); }
int APS5_VABI NoPosixAlign(void**, std::size_t, std::size_t) { throw std::runtime_error("unexpected posix_memalign"); }

void InitializeHeap() {
    void* const api[10] {reinterpret_cast<void*>(Allocate), reinterpret_cast<void*>(Free),
        reinterpret_cast<void*>(NoCalloc), reinterpret_cast<void*>(NoRealloc),
        reinterpret_cast<void*>(NoAlign), reinterpret_cast<void*>(NoRealign),
        reinterpret_cast<void*>(NoPosixAlign)};
    ApplicationHeapRegister_nid_no_patch(api);
}

template<typename T> T* New() {
    auto* pointer = static_cast<T*>(malloc_nid_postfix(sizeof(T)));
    Require(pointer != nullptr);
    std::memset(pointer, 0, sizeof(T));
    return pointer;
}

unsigned char* Bytes(std::size_t size, std::size_t wipeLength = 0) {
    auto* pointer = static_cast<unsigned char*>(malloc_nid_postfix(size));
    Require(pointer != nullptr);
    std::memset(pointer, 0xa5, size);
    if (wipeLength) wipes.emplace(pointer, wipeLength);
    return pointer;
}

Asn1String* String(int size = 7, int flags = 0) {
    auto* result = New<Asn1String>();
    result->length = size;
    result->type = 22; // ASN1_IA5STRING
    result->flags = flags;
    if (size > 0 && !(flags & 0x10)) result->data = Bytes(static_cast<std::size_t>(size), static_cast<std::size_t>(size));
    return result;
}

LibreSslAsn1Object* Object(int flags = 0x0d) {
    auto* object = New<LibreSslAsn1Object>();
    object->flags = flags;
    if (flags & 0x04) {
        object->shortName = reinterpret_cast<char*>(Bytes(6));
        object->longName = reinterpret_cast<char*>(Bytes(9));
    }
    if (flags & 0x08) {
        object->length = 5;
        object->data = Bytes(5, 5);
    }
    return object;
}

LibreSslGeneralName* Name(int type, void* value) {
    auto* result = New<LibreSslGeneralName>();
    result->type = type;
    result->value = value;
    return result;
}

struct Any { int type; void* value; };
struct Other { LibreSslAsn1Object* typeId; Any* value; };
struct Edi { Asn1String* assigner; Asn1String* party; };
struct Buffer { std::size_t length; char* data; std::size_t capacity; };
struct NameEntry { LibreSslAsn1Object* object; Asn1String* value; int set; int size; };
struct X509Name { LibreSslStack* entries; int modified; Buffer* bytes; unsigned char* canonical; int canonicalLength; };

static_assert(sizeof(Any) == 16 && sizeof(Other) == 16 && sizeof(Edi) == 16);
static_assert(sizeof(Buffer) == 24 && sizeof(NameEntry) == 24 && sizeof(X509Name) == 40);

LibreSslStack* Stack(const std::vector<void*>& elements) {
    auto* stack = New<LibreSslStack>();
    stack->num = static_cast<int>(elements.size());
    stack->numAlloc = stack->num;
    stack->data = reinterpret_cast<char**>(malloc_nid_postfix(sizeof(void*) * (elements.size() ? elements.size() : 1)));
    Require(stack->data != nullptr);
    for (std::size_t i = 0; i < elements.size(); ++i) stack->data[i] = static_cast<char*>(elements[i]);
    return stack;
}

void CheckClean() {
    Require(live.empty());
    Require(wipes.empty());
}

void TestSimpleNames() {
    GENERAL_NAME_free_nid_postfix(nullptr);
    GENERAL_NAMES_free_nid_postfix(nullptr);
    for (int choice : {1, 2, 3, 6, 7}) {
        GENERAL_NAME_free_nid_postfix(Name(choice, String()));
        CheckClean();
    }
    auto* string = String(0);
    GENERAL_NAME_free_nid_postfix(Name(1, string));
    CheckClean();

    // NDEF strings borrow their buffer. They release only the string wrapper.
    std::array<unsigned char, 6> borrowed{1, 2, 3, 4, 5, 6};
    string = String(0, 0x10);
    string->length = static_cast<int>(borrowed.size());
    string->data = borrowed.data();
    GENERAL_NAME_free_nid_postfix(Name(2, string));
    Require(borrowed == (std::array<unsigned char, 6>{1, 2, 3, 4, 5, 6}));
    CheckClean();
}

void TestOtherName() {
    for (int type : {1, 5, 6, 12, -4}) {
        auto* other = New<Other>();
        other->typeId = Object();
        other->value = New<Any>();
        other->value->type = type;
        if (type == 6) other->value->value = Object(0x09); // owned OID, borrowed names
        if (type == 12) other->value->value = String();
        if (type == -4) {
            auto* nested = New<Any>();
            nested->type = 12;
            nested->value = String();
            other->value->value = nested;
        }
        GENERAL_NAME_free_nid_postfix(Name(0, other));
        CheckClean();
    }
    // The enclosing OID may be static while the OTHERNAME structure is owned.
    auto* other = New<Other>();
    static LibreSslAsn1Object staticOid{};
    other->typeId = &staticOid;
    GENERAL_NAME_free_nid_postfix(Name(0, other));
    CheckClean();
}

void TestEdiPartyName() {
    auto* edi = New<Edi>();
    edi->assigner = String();
    edi->party = String();
    GENERAL_NAME_free_nid_postfix(Name(5, edi));
    CheckClean();
    edi = New<Edi>();
    edi->party = String();
    GENERAL_NAME_free_nid_postfix(Name(5, edi));
    CheckClean();
}

void TestDirectoryName() {
    auto* name = New<X509Name>();
    auto* buffer = New<Buffer>();
    buffer->capacity = 16;
    buffer->length = 4;
    buffer->data = reinterpret_cast<char*>(Bytes(16, 16)); // wipe capacity, not length
    name->bytes = buffer;
    auto* entry = New<NameEntry>();
    entry->object = Object();
    entry->value = String();
    auto* another = New<NameEntry>();
    another->object = Object(0x01);
    another->value = String();
    name->entries = Stack({entry, nullptr, another});
    name->canonical = Bytes(12); // X509_NAME frees without wiping canon_enc
    name->canonicalLength = 12;
    GENERAL_NAME_free_nid_postfix(Name(4, name));
    CheckClean();
}

void TestOidAndStack() {
    GENERAL_NAME_free_nid_postfix(Name(8, Object()));
    CheckClean();
    // A dynamically created OID can own its data but not its wrapper.
    static LibreSslAsn1Object staticOid{};
    staticOid.length = 5;
    staticOid.flags = 0x08;
    staticOid.data = Bytes(5, 5);
    GENERAL_NAME_free_nid_postfix(Name(8, &staticOid));
    CheckClean();

    auto* first = Name(1, String());
    auto* last = Name(7, String());
    freed.clear();
    GENERAL_NAMES_free_nid_postfix(Stack({first, nullptr, last}));
    CheckClean();
    Require(freed.size() >= 8); // children, their data, the element array and stack
    GENERAL_NAMES_free_nid_postfix(Stack({}));
    CheckClean();

    auto* unusual = Name(42, nullptr);
    GENERAL_NAME_free_nid_postfix(unusual); // out-of-range choice still frees wrapper
    CheckClean();
}

} // namespace

int main() {
    InitializeHeap();
    TestSimpleNames();
    TestOtherName();
    TestEdiPartyName();
    TestDirectoryName();
    TestOidAndStack();
    std::puts("libSceLibreSsl GENERAL_NAME tests passed");
}
