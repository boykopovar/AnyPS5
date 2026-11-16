#include "prx/libSceLibreSsl3/libSceLibreSsl3.hpp"
#include "prx/libSceLibreSsl3/include/Objects.hpp"
#include "prx/libSceLibreSsl/include/Api.hpp"
#include "prx/libc/include/Api.hpp"
#include "prx/libc/include/ApplicationHeap.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <initializer_list>
#include <source_location>
#include <stdexcept>
#include <unordered_map>

namespace {

constexpr int Asn1Undefined = -1;
constexpr int Asn1AppChoose = -2;
constexpr int Asn1Utf8String = 12;
constexpr int Asn1PrintableString = 19;
constexpr int Asn1T61String = 20;
constexpr int Asn1Ia5String = 22;
constexpr int Asn1BmpString = 30;
constexpr int MbstringUtf8 = 0x1000;
constexpr int MbstringAsc = 0x1000 | 1;
constexpr int MbstringBmp = 0x1000 | 2;
constexpr int MbstringUniversal = 0x1000 | 4;
constexpr std::int32_t ObjectDynamic = 0x01;
constexpr std::int32_t DynamicData = 0x08;
constexpr std::int64_t StringFlagMstring = 0x040;

std::unordered_map<void*, std::size_t> allocations;
int remainingAllocations = -1;

void Require(bool condition, std::source_location location = std::source_location::current());

void* APS5_VABI Allocate_nid_no_patch(std::size_t size) {
    if (remainingAllocations == 0) return nullptr;
    if (remainingAllocations > 0) --remainingAllocations;
    void* result = std::malloc(size ? size : 1);
    Require(result != nullptr);
    allocations.emplace(result, size);
    return result;
}

void APS5_VABI Free_nid_no_patch(void* pointer) {
    if (!pointer) return;
    Require(allocations.erase(pointer) == 1);
    std::free(pointer);
}

void* APS5_VABI Callocate_nid_no_patch(std::size_t count, std::size_t size) {
    if (remainingAllocations == 0) return nullptr;
    if (remainingAllocations > 0) --remainingAllocations;
    void* result = std::calloc(count ? count : 1, size ? size : 1);
    Require(result != nullptr);
    allocations.emplace(result, count * size);
    return result;
}

void* APS5_VABI Reallocate_nid_no_patch(void* pointer, std::size_t size) {
    if (!pointer) return Allocate_nid_no_patch(size);
    if (remainingAllocations == 0) return nullptr;
    if (remainingAllocations > 0) --remainingAllocations;
    Require(allocations.erase(pointer) == 1);
    void* result = std::realloc(pointer, size ? size : 1);
    Require(result != nullptr);
    allocations.emplace(result, size);
    return result;
}

void* APS5_VABI UnexpectedAlign_nid_no_patch(std::size_t, std::size_t) {
    throw std::runtime_error("unexpected aligned allocation");
}

void* APS5_VABI UnexpectedRealign_nid_no_patch(void*, std::size_t, std::size_t) {
    throw std::runtime_error("unexpected aligned reallocation");
}

int APS5_VABI UnexpectedPosixAlign_nid_no_patch(void**, std::size_t, std::size_t) {
    throw std::runtime_error("unexpected posix_memalign");
}

void Require(bool condition, std::source_location location) {
    if (condition) return;
    std::fprintf(stderr, "LibreSSL3 X509_NAME_add_entry_by_txt check failed at line %u\n", location.line());
    std::abort();
}

LibreSslStack* MakeEntries() {
    return sk_new_nid_postfix(nullptr);
}

LibreSslX509Name* MakeName() {
    auto* name = static_cast<LibreSslX509Name*>(Callocate_nid_no_patch(1, sizeof(LibreSslX509Name)));
    name->entries = MakeEntries();
    name->modified = 0;
    return name;
}

LibreSslX509NameEntry* EntryOf(LibreSslX509Name* name, int index) {
    return reinterpret_cast<LibreSslX509NameEntry*>(name->entries->data[index]);
}

Asn1String* ValueOf(LibreSslX509Name* name, int index) {
    return static_cast<Asn1String*>(EntryOf(name, index)->value);
}

const LibreSslAsn1Object* ObjectOf(LibreSslX509Name* name, int index) {
    return static_cast<const LibreSslAsn1Object*>(EntryOf(name, index)->object);
}

void FreeName(LibreSslX509Name* name) {
    for (int index = 0; index < name->entries->num; ++index) {
        auto* entry = EntryOf(name, index);
        ASN1_OCTET_STRING_free_nid_postfix(static_cast<Asn1String*>(entry->value));
        ASN1_OBJECT_free_nid_postfix(static_cast<Asn1Object_nid_no_patch*>(entry->object));
        free_nid_postfix(entry);
    }
    sk_free_nid_postfix(name->entries);
    free_nid_postfix(name->canonEnc);
    free_nid_postfix(name);
}

int Add(LibreSslX509Name* name, const char* field, int type, const char* text, int loc, int set) {
    return X509_NAME_add_entry_by_txt_nid_postfix(name, field, type,
        reinterpret_cast<const unsigned char*>(text), static_cast<int>(std::strlen(text)), loc, set);
}

bool SameBytes(const Asn1String* value, const void* bytes, std::size_t length) {
    if (value->length != static_cast<std::int32_t>(length)) return false;
    if (length == 0) return true;
    return value->data != nullptr && std::memcmp(value->data, bytes, length) == 0;
}

}

int main() {
    std::array<void*, 7> api{
        reinterpret_cast<void*>(Allocate_nid_no_patch), reinterpret_cast<void*>(Free_nid_no_patch),
        reinterpret_cast<void*>(Callocate_nid_no_patch), reinterpret_cast<void*>(Reallocate_nid_no_patch),
        reinterpret_cast<void*>(UnexpectedAlign_nid_no_patch), reinterpret_cast<void*>(UnexpectedRealign_nid_no_patch),
        reinterpret_cast<void*>(UnexpectedPosixAlign_nid_no_patch)};
    ApplicationHeapRegister_nid_no_patch(api.data());

    Require(X509_NAME_add_entry_by_txt_nid_postfix(nullptr, "CN", MbstringUtf8,
        reinterpret_cast<const unsigned char*>("x"), 1, -1, -1) == 0);

    LibreSslX509Name* name = MakeName();
    try {
        X509_NAME_add_entry_by_txt_nid_postfix(name, nullptr, MbstringUtf8,
            reinterpret_cast<const unsigned char*>("x"), 1, -1, -1);
        Require(false);
    } catch (const std::invalid_argument&) {
    }
    for (const char* field : {"", "3.1", "1", "bogus", "1.2.x"}) {
        Require(X509_NAME_add_entry_by_txt_nid_postfix(name, field, Asn1Utf8String,
            reinterpret_cast<const unsigned char*>("x"), 1, -1, -1) == 0);
        Require(name->entries->num == 0);
        Require(name->modified == 0);
    }
    Require(Add(name, "CN", Asn1Utf8String, "x", -1, -1) == 1);
    Require(name->entries->num == 1);
    FreeName(name);

    name = MakeName();
    auto* canonical = static_cast<unsigned char*>(Allocate_nid_no_patch(4));
    canonical[0] = 0x30;
    name->canonEnc = canonical;
    name->canonEnclen = 4;
    Require(Add(name, "CN", MbstringUtf8, "example.com", -1, -1) == 1);
    Require(name->entries->num == 1);
    Require(name->modified == 1);
    Require(name->canonEnc == nullptr);
    Require(name->canonEnclen == 0);
    Require(allocations.find(canonical) == allocations.end());
    Require(ObjectOf(name, 0) == LibreSsl3::ObjectByNid(13));
    Require(EntryOf(name, 0)->set == 0);
    Require(ValueOf(name, 0)->type == Asn1Utf8String);
    Require(SameBytes(ValueOf(name, 0), "example.com", 11));
    Require((ValueOf(name, 0)->flags & StringFlagMstring) != 0);
    std::array<char, 32> text{};
    Require(X509_NAME_get_text_by_NID_nid_postfix(name, 13, text.data(), static_cast<int>(text.size())) == 11);
    Require(std::strcmp(text.data(), "example.com") == 0);
    Require(X509_NAME_get_index_by_NID_nid_postfix(name, 13, -1) == 0);
    Require(X509_NAME_get_index_by_NID_nid_postfix(name, 13, 0) == -1);
    FreeName(name);

    name = MakeName();
    Require(Add(name, "O", MbstringAsc, "o1", -1, -1) == 1);
    Require(Add(name, "OU", MbstringAsc, "ou1", -1, -1) == 1);
    Require(Add(name, "L", MbstringAsc, "l1", -1, 0) == 1);
    Require(EntryOf(name, 0)->set == 0);
    Require(EntryOf(name, 1)->set == 0);
    Require(EntryOf(name, 2)->set == 1);
    Require(Add(name, "ST", MbstringAsc, "st1", 1, 0) == 1);
    Require(name->entries->num == 4);
    Require(EntryOf(name, 0)->set == 0);
    Require(EntryOf(name, 1)->set == 0);
    Require(EntryOf(name, 2)->set == 1);
    Require(EntryOf(name, 3)->set == 2);
    Require(ObjectOf(name, 1) == LibreSsl3::ObjectByNid(16));
    Require(Add(name, "C", MbstringAsc, "DE", 0, 0) == 1);
    Require(name->entries->num == 5);
    Require(EntryOf(name, 0)->set == 0);
    Require(EntryOf(name, 1)->set == 1);
    Require(EntryOf(name, 2)->set == 1);
    Require(EntryOf(name, 3)->set == 2);
    Require(EntryOf(name, 4)->set == 3);
    Require(Add(name, "OU", MbstringAsc, "ou2", 99, 5) == 1);
    Require(name->entries->num == 6);
    Require(EntryOf(name, 5)->set == 4);
    Require(Add(name, "OU", MbstringAsc, "ou3", -99, -1) == 1);
    Require(name->entries->num == 7);
    Require(EntryOf(name, 6)->set == 4);
    Require(Add(name, "OU", MbstringAsc, "ou4", 2, 1) == 1);
    Require(name->entries->num == 8);
    Require(EntryOf(name, 2)->set == 1);
    Require(EntryOf(name, 3)->set == 1);
    Require(ValueOf(name, 2)->length == 3);
    FreeName(name);

    name = MakeName();
    Require(Add(name, "O", MbstringAsc, "o1", -1, -1) == 1);
    Require(Add(name, "L", MbstringAsc, "l1", 0, -1) == 1);
    Require(name->entries->num == 2);
    Require(EntryOf(name, 0)->set == 0);
    Require(SameBytes(ValueOf(name, 0), "l1", 2));
    Require(EntryOf(name, 1)->set == 1);
    FreeName(name);

    name = MakeName();
    Require(Add(name, "CN", MbstringAsc, "US", -1, -1) == 1);
    Require(ValueOf(name, 0)->type == Asn1Utf8String);
    Require(Add(name, "CN", MbstringAsc, "\xe9", -1, -1) == 1);
    static const unsigned char eAcute[] = {0xc3, 0xa9};
    Require(ValueOf(name, 1)->type == Asn1Utf8String);
    Require(SameBytes(ValueOf(name, 1), eAcute, sizeof(eAcute)));
    Require(Add(name, "CN", MbstringUtf8, "M\xc3\xbcller", -1, -1) == 1);
    Require(ValueOf(name, 2)->type == Asn1Utf8String);
    Require(SameBytes(ValueOf(name, 2), "M\xc3\xbcller", 7));
    Require(Add(name, "CN", MbstringUtf8, "\xff", -1, -1) == 0);
    static const unsigned char bmpAB[] = {0x00, 0x41, 0x00, 0x42};
    Require(X509_NAME_add_entry_by_txt_nid_postfix(name, "CN", MbstringBmp, bmpAB, 4, -1, -1) == 1);
    Require(ValueOf(name, 3)->type == Asn1Utf8String);
    Require(SameBytes(ValueOf(name, 3), "AB", 2));
    static const unsigned char oddBmp[] = {0x00, 0x41, 0x00};
    Require(X509_NAME_add_entry_by_txt_nid_postfix(name, "CN", MbstringBmp, oddBmp, 3, -1, -1) == 0);
    static const unsigned char universalA[] = {0x00, 0x00, 0x00, 0x41};
    Require(X509_NAME_add_entry_by_txt_nid_postfix(name, "CN", MbstringUniversal, universalA, 4, -1, -1) == 1);
    Require(ValueOf(name, 4)->type == Asn1Utf8String);
    Require(SameBytes(ValueOf(name, 4), "A", 1));
    static const unsigned char surrogate[] = {0x00, 0x00, 0xd8, 0x00};
    Require(X509_NAME_add_entry_by_txt_nid_postfix(name, "CN", MbstringUniversal, surrogate, 4, -1, -1) == 0);
    static const unsigned char bmpA[] = {0x00, 0x41};
    Require(X509_NAME_add_entry_by_txt_nid_postfix(name, "CN", 0x1008, bmpA, 2, -1, -1) == 0);
    Require(Add(name, "C", MbstringAsc, "US", -1, -1) == 1);
    Require(ValueOf(name, 5)->type == Asn1PrintableString);
    Require(Add(name, "C", MbstringAsc, "U", -1, -1) == 0);
    Require(Add(name, "C", MbstringAsc, "USA", -1, -1) == 0);
    Require(Add(name, "emailAddress", MbstringUtf8, "a@b", -1, -1) == 1);
    Require(ValueOf(name, 6)->type == Asn1Ia5String);
    Require(Add(name, "DC", MbstringUtf8, "example", -1, -1) == 1);
    Require(ValueOf(name, 7)->type == Asn1Ia5String);
    Require(Add(name, "DC", MbstringUtf8, "\xe9", -1, -1) == 0);
    Require(Add(name, "friendlyName", MbstringUtf8, "hi", -1, -1) == 1);
    Require(ValueOf(name, 8)->type == Asn1BmpString);
    static const unsigned char bmpHi[] = {0x00, 'h', 0x00, 'i'};
    Require(SameBytes(ValueOf(name, 8), bmpHi, sizeof(bmpHi)));
    Require(Add(name, "serialNumber", MbstringAsc, "1234", -1, -1) == 1);
    Require(ValueOf(name, 9)->type == Asn1PrintableString);
    std::array<char, 66> longName{};
    longName.fill('a');
    longName[65] = '\0';
    Require(Add(name, "CN", MbstringAsc, longName.data(), -1, -1) == 0);
    FreeName(name);

    name = MakeName();
    Require(Add(name, "CN", Asn1Ia5String, "abc", -1, -1) == 1);
    Require(ValueOf(name, 0)->type == Asn1Ia5String);
    Require(SameBytes(ValueOf(name, 0), "abc", 3));
    Require(X509_NAME_add_entry_by_txt_nid_postfix(name, "CN", Asn1Ia5String,
        reinterpret_cast<const unsigned char*>("abc"), -1, -1, -1) == 1);
    Require(ValueOf(name, 1)->type == Asn1Ia5String);
    Require(Add(name, "CN", Asn1AppChoose, "abc", -1, -1) == 1);
    Require(ValueOf(name, 2)->type == Asn1PrintableString);
    Require(Add(name, "CN", Asn1AppChoose, "a@b", -1, -1) == 1);
    Require(ValueOf(name, 3)->type == Asn1Ia5String);
    Require(Add(name, "CN", Asn1AppChoose, "\xe9", -1, -1) == 1);
    Require(ValueOf(name, 4)->type == Asn1T61String);
    Require(Add(name, "CN", Asn1Undefined, "abc", -1, -1) == 1);
    Require(ValueOf(name, 5)->type == Asn1Undefined);
    Require(X509_NAME_add_entry_by_txt_nid_postfix(name, "CN", Asn1Utf8String, nullptr, 0, -1, -1) == 1);
    Require(ValueOf(name, 6)->type == Asn1Utf8String);
    Require(ValueOf(name, 6)->length == 0);
    Require(ValueOf(name, 6)->data != nullptr);
    Require(X509_NAME_add_entry_by_txt_nid_postfix(name, "CN", Asn1Utf8String,
        reinterpret_cast<const unsigned char*>("abc"), 0, -1, -1) == 1);
    Require(ValueOf(name, 7)->length == 0);
    Require(X509_NAME_add_entry_by_txt_nid_postfix(name, "CN", Asn1Utf8String, nullptr, 3, -1, -1) == 0);
    Require(name->entries->num == 8);
    for (int type : {1, 5, 6}) {
        try {
            X509_NAME_add_entry_by_txt_nid_postfix(name, "CN", type,
                reinterpret_cast<const unsigned char*>("x"), 1, -1, -1);
            Require(false);
        } catch (const std::runtime_error&) {
        }
        Require(name->entries->num == 8);
    }
    FreeName(name);

    name = MakeName();
    static const unsigned char cnOid[] = {0x55, 0x04, 0x03};
    Require(Add(name, "2.5.4.3", MbstringAsc, "US", -1, -1) == 1);
    Require(ObjectOf(name, 0) != LibreSsl3::ObjectByNid(13));
    Require(ObjectOf(name, 0)->nid == 0);
    Require(ObjectOf(name, 0)->length == 3);
    Require((ObjectOf(name, 0)->flags & (ObjectDynamic | DynamicData)) == (ObjectDynamic | DynamicData));
    Require(std::memcmp(ObjectOf(name, 0)->data, cnOid, sizeof(cnOid)) == 0);
    Require(ValueOf(name, 0)->type == Asn1Utf8String);
    Require(Add(name, "2.5.4.3", MbstringAsc, longName.data(), -1, -1) == 0);
    Require(Add(name, "1.2.3.4", Asn1Utf8String, "x", -1, -1) == 1);
    static const unsigned char arcData[] = {0x2a, 0x03, 0x04};
    Require(ObjectOf(name, 1)->length == 3);
    Require(std::memcmp(ObjectOf(name, 1)->data, arcData, sizeof(arcData)) == 0);
    Require(Add(name, "1 2 3", Asn1Utf8String, "x", -1, -1) == 1);
    static const unsigned char spacedData[] = {0x2a, 0x03};
    Require(ObjectOf(name, 2)->length == 2);
    Require(std::memcmp(ObjectOf(name, 2)->data, spacedData, sizeof(spacedData)) == 0);
    Require(Add(name, "2.5.4294967296", Asn1Utf8String, "x", -1, -1) == 1);
    static const unsigned char hugeData[] = {0x55, 0x90, 0x80, 0x80, 0x80, 0x00};
    Require(ObjectOf(name, 3)->length == static_cast<std::int32_t>(sizeof(hugeData)));
    Require(std::memcmp(ObjectOf(name, 3)->data, hugeData, sizeof(hugeData)) == 0);
    Require(Add(name, "2.5.18446744073709551616", Asn1Utf8String, "x", -1, -1) == 1);
    Require(ObjectOf(name, 4)->length == 11);
    Require(ObjectOf(name, 4)->data[0] == 0x55);
    Require(ObjectOf(name, 4)->data[10] == 0x00);
    Require((ObjectOf(name, 4)->data[1] & 0x80) == 0x80);
    Require(Add(name, "3.1", Asn1Utf8String, "x", -1, -1) == 0);
    Require(Add(name, "1.40", Asn1Utf8String, "x", -1, -1) == 0);
    FreeName(name);

    name = MakeName();
    remainingAllocations = 0;
    Require(Add(name, "CN", MbstringUtf8, "abc", -1, -1) == 0);
    remainingAllocations = -1;
    Require(name->entries->num == 0);
    Require(name->modified == 0);
    remainingAllocations = 1;
    Require(Add(name, "CN", MbstringUtf8, "abc", -1, -1) == 0);
    remainingAllocations = -1;
    Require(name->entries->num == 0);
    remainingAllocations = 2;
    Require(Add(name, "CN", MbstringUtf8, "abc", -1, -1) == 0);
    remainingAllocations = -1;
    Require(name->entries->num == 0);
    remainingAllocations = 0;
    Require(Add(name, "1.2.3", Asn1Utf8String, "abc", -1, -1) == 0);
    remainingAllocations = 1;
    Require(Add(name, "1.2.3", Asn1Utf8String, "abc", -1, -1) == 0);
    remainingAllocations = -1;
    Require(name->entries->num == 0);
    Require(Add(name, "O", MbstringAsc, "o1", -1, -1) == 1);
    Require(Add(name, "O", MbstringAsc, "o2", -1, -1) == 1);
    Require(Add(name, "O", MbstringAsc, "o3", -1, -1) == 1);
    Require(name->entries->num == 3);
    const int capacity = name->entries->numAlloc;
    remainingAllocations = 3;
    Require(Add(name, "O", MbstringAsc, "o4", -1, -1) == 0);
    remainingAllocations = -1;
    Require(name->entries->num == 3);
    Require(name->entries->numAlloc == capacity);
    Require(Add(name, "O", MbstringAsc, "o4", -1, -1) == 1);
    Require(name->entries->num == 4);
    Require(name->entries->numAlloc == capacity * 2);
    for (int index = 0; index < 4; ++index)
        Require(SameBytes(ValueOf(name, index), index == 0 ? "o1" : index == 1 ? "o2" : index == 2 ? "o3" : "o4", 2));
    Require(Add(name, "O", MbstringAsc, "o5", -1, -1) == 1);
    Require(Add(name, "O", MbstringAsc, "o6", -1, -1) == 1);
    Require(name->entries->num == 6);
    Require(Add(name, "O", MbstringAsc, "o7", -1, -1) == 1);
    Require(name->entries->num == 7);
    Require(SameBytes(ValueOf(name, 6), "o7", 2));
    FreeName(name);

    Require(allocations.empty());
    std::puts("libSceLibreSsl3 X509_NAME_add_entry_by_txt tests passed");
}
