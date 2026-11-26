#include "prx/libc/include/Api.hpp"
#include "prx/libc/include/ApplicationHeap.hpp"
#include "prx/libc/include/exceptions/Runtime.hpp"
#include <array>
#include <bit>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <stdexcept>
#include <string>

struct LibcxxString { unsigned char bytes[24]; };
using Name = const char* (APS5_VABI *)(const LibcException::CategoryObject*);
using Message = LibcxxString* (APS5_VABI *)(LibcxxString*, const LibcException::CategoryObject*, int);
using Condition = LibcException::LibcxxErrorCode (APS5_VABI *)(const LibcException::CategoryObject*, int);
using EquivalentCondition = bool (APS5_VABI *)(const LibcException::CategoryObject*, int, const LibcException::LibcxxErrorCode*);
using EquivalentCode = bool (APS5_VABI *)(const LibcException::CategoryObject*, const LibcException::LibcxxErrorCode*, int);
using Delete = void (APS5_VABI *)(const LibcException::CategoryObject*);
using What = const char* (APS5_VABI *)(const LibcException::ExceptionObject*);

static void Require(bool value, int line) {
    if (value) return;
    std::fprintf(stderr, "libc++ system_error check failed at line %d\n", line);
    std::exit(1);
}
#define REQUIRE(value) Require((value), __LINE__)

static const void* Slot(const LibcException::CategoryObject* category, int index) {
    return static_cast<const void* const*>(category->vtable)[index];
}

static const LibcException::TypeRecord* TypeOf(const void* vtable) {
    return static_cast<const LibcException::TypeRecord*>(static_cast<const void* const*>(vtable)[-1]);
}

static const char* KindOf(const LibcException::TypeRecord* type) {
    return static_cast<const LibcException::TypeRecord*>(type->vtable[-1])->name;
}

static std::string Text(const LibcxxString& value) {
    if ((value.bytes[0] & 1) == 0) return std::string(reinterpret_cast<const char*>(value.bytes + 1), value.bytes[0] >> 1);
    std::uint64_t words[3] {};
    std::memcpy(words, value.bytes, sizeof(words));
    return std::string(reinterpret_cast<const char*>(static_cast<std::uintptr_t>(words[2])), words[1]);
}

static LibcxxString MakeString(const char* text) {
    LibcxxString value {};
    const std::uint64_t size = std::strlen(text);
    if (size < 23) {
        value.bytes[0] = static_cast<unsigned char>(size << 1);
        std::memcpy(value.bytes + 1, text, size);
        return value;
    }
    const std::uint64_t words[] {48 | 1, size, reinterpret_cast<std::uintptr_t>(text)};
    std::memcpy(value.bytes, words, sizeof(words));
    return value;
}

static void CheckMessage(const LibcException::CategoryObject* category, int value, const std::string& expected) {
    LibcxxString result;
    std::memset(&result, 0xcd, sizeof(result));
    *__error_nid_postfix() = 61;
    REQUIRE(std::bit_cast<Message>(Slot(category, 6))(&result, category, value) == &result);
    REQUIRE(*__error_nid_postfix() == 61);
    REQUIRE(Text(result) == expected);
    if (expected.size() <= 22) {
        REQUIRE(result.bytes[0] == expected.size() << 1 && result.bytes[1 + expected.size()] == 0);
        return;
    }
    std::uint64_t words[3] {};
    std::memcpy(words, result.bytes, sizeof(words));
    const std::uint64_t allocation = words[0] & ~std::uint64_t{1};
    REQUIRE((words[0] & 1) == 1 && allocation % 16 == 0 && allocation > expected.size() && allocation <= expected.size() + 16);
    REQUIRE(reinterpret_cast<const char*>(static_cast<std::uintptr_t>(words[2]))[expected.size()] == 0);
    _ZdlPv_nid_postfix(reinterpret_cast<void*>(static_cast<std::uintptr_t>(words[2])));
}

static std::string Strerror(int value) {
    char buffer[1024] {};
    REQUIRE(strerror_r_nid_postfix(value, buffer, sizeof(buffer)) == 0);
    return buffer;
}

static void CheckCategoryBase(const LibcException::CategoryObject* category, const LibcException::CategoryObject* generic) {
    const auto condition = std::bit_cast<Condition>(Slot(category, 3));
    const auto equivalentCondition = std::bit_cast<EquivalentCondition>(Slot(category, 4));
    const auto equivalentCode = std::bit_cast<EquivalentCode>(Slot(category, 5));
    const LibcException::LibcxxErrorCode mapped = condition(category, 5);
    REQUIRE(mapped.value == 5 && mapped.category == generic);
    const LibcException::LibcxxErrorCode inGeneric {5, 0, generic};
    const LibcException::LibcxxErrorCode inSelf {5, 0, category};
    REQUIRE(equivalentCondition(category, 5, &inGeneric) && !equivalentCondition(category, 6, &inGeneric));
    REQUIRE(equivalentCondition(category, 5, &inSelf) == (category == generic));
    REQUIRE(equivalentCode(category, &inSelf, 5) && !equivalentCode(category, &inSelf, 6));
    REQUIRE(equivalentCode(category, &inGeneric, 5) == (category == generic));
    bool rejected = false;
    try { std::bit_cast<Delete>(Slot(category, 1))(category); }
    catch (const std::invalid_argument&) { rejected = true; }
    REQUIRE(rejected);
}

static void CheckGenericCategory(const LibcException::CategoryObject* generic) {
    REQUIRE(generic && generic == _ZNSt3__116generic_categoryEv_nid_postfix());
    const auto* type = TypeOf(generic->vtable);
    REQUIRE(std::strcmp(type->name, "NSt3__124__generic_error_categoryE") == 0);
    REQUIRE(std::strcmp(KindOf(type), "N10__cxxabiv120__si_class_type_infoE") == 0);
    REQUIRE(type->base && std::strcmp(type->base->name, "NSt3__112__do_messageE") == 0);
    REQUIRE(type->base->base == &_ZTINSt3__114error_categoryE_nid_postfix);
    REQUIRE(std::strcmp(_ZTINSt3__114error_categoryE_nid_postfix.name, "NSt3__114error_categoryE") == 0);
    REQUIRE(std::strcmp(KindOf(&_ZTINSt3__114error_categoryE_nid_postfix), "N10__cxxabiv117__class_type_infoE") == 0);
    REQUIRE(std::strcmp(std::bit_cast<Name>(Slot(generic, 2))(generic), "generic") == 0);
    CheckMessage(generic, 17, "File exists");
    CheckMessage(generic, 2, "No such file or directory");
    CheckMessage(generic, 96, Strerror(96));
    CheckMessage(generic, -1, Strerror(-1));
    CheckMessage(generic, 97, "unspecified generic_category error");
    CheckCategoryBase(generic, generic);
    const LibcException::LibcxxErrorCode beyond = std::bit_cast<Condition>(Slot(generic, 3))(generic, 97);
    REQUIRE(beyond.value == 97 && beyond.category == generic);
}

static void CheckSystemCategory(const LibcException::CategoryObject* system, const LibcException::CategoryObject* generic) {
    REQUIRE(system && system != generic);
    const auto* type = TypeOf(system->vtable);
    REQUIRE(std::strcmp(type->name, "NSt3__123__system_error_categoryE") == 0);
    REQUIRE(type->base == TypeOf(generic->vtable)->base);
    REQUIRE(std::strcmp(std::bit_cast<Name>(Slot(system, 2))(system), "system") == 0);
    CheckMessage(system, 22, "Invalid argument");
    CheckMessage(system, 97, "unspecified system_category error");
    CheckCategoryBase(system, generic);
    const auto condition = std::bit_cast<Condition>(Slot(system, 3));
    const LibcException::LibcxxErrorCode known = condition(system, -4);
    REQUIRE(known.value == -4 && known.category == generic);
    const LibcException::LibcxxErrorCode beyond = condition(system, 97);
    REQUIRE(beyond.value == 97 && beyond.category == system);
    const LibcException::LibcxxErrorCode inSelf {97, 0, system};
    REQUIRE(std::bit_cast<EquivalentCondition>(Slot(system, 4))(system, 97, &inSelf));
}

static void CheckTypeRecords() {
    const auto& type = _ZTINSt3__112system_errorE_nid_postfix;
    REQUIRE(std::strcmp(type.name, "NSt3__112system_errorE") == 0);
    REQUIRE(std::strcmp(KindOf(&type), "N10__cxxabiv120__si_class_type_infoE") == 0);
    REQUIRE(type.base && std::strcmp(type.base->name, "St13runtime_error") == 0);
    const auto& table = _ZTVNSt3__112system_errorE_nid_postfix;
    REQUIRE(table.offset == 0 && table.type == &type && table.destroy && table.deleteObject && table.what);
}

static void Construct(LibcException::LibcxxSystemErrorObject& object, LibcException::LibcxxErrorCode code, const char* what) {
    std::memset(&object, 0xcd, sizeof(object));
    _ZNSt3__112system_errorC1ENS_10error_codeEPKc_nid_postfix(&object, code, what);
}

static std::string WhatOf(const LibcException::LibcxxSystemErrorObject& object) {
    REQUIRE(object.base.vtable == &_ZTVNSt3__112system_errorE_nid_postfix.destroy);
    return std::bit_cast<What>(static_cast<const void* const*>(object.base.vtable)[2])(&object.base);
}

static LibcxxString* APS5_VABI CustomMessage(LibcxxString* result, const LibcException::CategoryObject*, int value) {
    const std::string text = "custom message number " + std::to_string(value) + " for the test";
    const auto size = text.size();
    char* storage = static_cast<char*>(_Znwm_nid_postfix(48));
    std::memcpy(storage, text.c_str(), size + 1);
    const std::uint64_t words[] {48 | 1, size, reinterpret_cast<std::uintptr_t>(storage)};
    std::memcpy(result, words, sizeof(words));
    return result;
}

static void CheckConstructors(const LibcException::CategoryObject* generic) {
    LibcException::LibcxxSystemErrorObject object;
    Construct(object, {17, 0, generic}, "mkdir");
    REQUIRE(WhatOf(object) == "mkdir: File exists");
    std::int32_t value = 0;
    const LibcException::CategoryObject* category = nullptr;
    std::memcpy(&value, reinterpret_cast<const unsigned char*>(&object) + 16, sizeof(value));
    std::memcpy(&category, reinterpret_cast<const unsigned char*>(&object) + 24, sizeof(category));
    REQUIRE(value == 17 && category == generic);
    _ZNSt3__112system_errorD1Ev_nid_postfix(&object);
    REQUIRE(object.base.message == nullptr && object.code.value == 17);

    std::memset(&object, 0xcd, sizeof(object));
    _ZNSt3__112system_errorC2ENS_10error_codeEPKc_nid_postfix(&object, {2, 0, generic}, "");
    REQUIRE(WhatOf(object) == "No such file or directory");
    _ZNSt3__112system_errorD2Ev_nid_postfix(&object);
    REQUIRE(object.base.message == nullptr);

    Construct(object, {0, 0, generic}, "nothing failed");
    REQUIRE(WhatOf(object) == "nothing failed" && object.code.value == 0);
    _ZNSt3__112system_errorD1Ev_nid_postfix(&object);

    const void* customVtable[] {nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, reinterpret_cast<const void*>(&CustomMessage)};
    const LibcException::CategoryObject custom {customVtable};
    Construct(object, {7, 0, &custom}, "open");
    REQUIRE(WhatOf(object) == "open: custom message number 7 for the test" && object.code.category == &custom);
    _ZNSt3__112system_errorD1Ev_nid_postfix(&object);

    const LibcxxString shortWhat = MakeString("rename");
    std::memset(&object, 0xcd, sizeof(object));
    _ZNSt3__112system_errorC1ENS_10error_codeERKNS_12basic_stringIcNS_11char_traitsIcEENS_9allocatorIcEEEE_nid_postfix(&object, {17, 0, generic}, &shortWhat);
    REQUIRE(WhatOf(object) == "rename: File exists");
    _ZNSt3__112system_errorD1Ev_nid_postfix(&object);

    const char* longText = "a what argument that does not fit inline";
    const LibcxxString longWhat = MakeString(longText);
    std::memset(&object, 0xcd, sizeof(object));
    _ZNSt3__112system_errorC1ENS_10error_codeERKNS_12basic_stringIcNS_11char_traitsIcEENS_9allocatorIcEEEE_nid_postfix(&object, {22, 0, generic}, &longWhat);
    REQUIRE(WhatOf(object) == std::string(longText) + ": Invalid argument");
    _ZNSt3__112system_errorD1Ev_nid_postfix(&object);

    bool rejected = false;
    try { Construct(object, {17, 0, generic}, nullptr); }
    catch (const std::invalid_argument&) { rejected = true; }
    REQUIRE(rejected);
    rejected = false;
    try { Construct(object, {17, 0, nullptr}, "x"); }
    catch (const std::invalid_argument&) { rejected = true; }
    REQUIRE(rejected);
    rejected = false;
    try { _ZNSt3__112system_errorC1ENS_10error_codeERKNS_12basic_stringIcNS_11char_traitsIcEENS_9allocatorIcEEEE_nid_postfix(&object, {17, 0, generic}, nullptr); }
    catch (const std::invalid_argument&) { rejected = true; }
    REQUIRE(rejected);
}

static const LibcException::CategoryObject* CheckThrow(int code, const char* what, const std::string& expected) {
    const LibcException::CategoryObject* category = nullptr;
    bool caught = false;
    try { _ZNSt3__120__throw_system_errorEiPKc_nid_postfix(code, what); }
    catch (const std::runtime_error& error) {
        caught = true;
        const auto* object = reinterpret_cast<const LibcException::LibcxxSystemErrorObject*>(&error);
        REQUIRE(TypeOf(object->base.vtable) == &_ZTINSt3__112system_errorE_nid_postfix);
        REQUIRE(WhatOf(*object) == expected && object->code.value == code);
        category = object->code.category;
    }
    REQUIRE(caught && category);
    return category;
}

static void CheckIostreamCategory(const LibcException::CategoryObject* iostream, const LibcException::CategoryObject* generic) {
    REQUIRE(iostream && iostream == _ZNSt3__117iostream_categoryEv_nid_postfix() && iostream != generic);
    const auto* type = TypeOf(iostream->vtable);
    REQUIRE(std::strcmp(type->name, "NSt3__119__iostream_categoryE") == 0);
    REQUIRE(std::strcmp(KindOf(type), "N10__cxxabiv120__si_class_type_infoE") == 0);
    REQUIRE(type->base == TypeOf(generic->vtable)->base);
    REQUIRE(std::strcmp(std::bit_cast<Name>(Slot(iostream, 2))(iostream), "iostream") == 0);
    CheckMessage(iostream, 1, "unspecified iostream_category error");
    CheckMessage(iostream, 5, Strerror(5));
    CheckMessage(iostream, 2, "No such file or directory");
    CheckMessage(iostream, sys_nerr_nid_postfix - 1, Strerror(sys_nerr_nid_postfix - 1));
    CheckMessage(iostream, -1, Strerror(-1));
    CheckMessage(iostream, sys_nerr_nid_postfix, "unspecified iostream_category error");
    CheckMessage(iostream, sys_nerr_nid_postfix + 1, "unspecified iostream_category error");
    const LibcException::LibcxxErrorCode mapped = std::bit_cast<Condition>(Slot(iostream, 3))(iostream, 5);
    REQUIRE(mapped.value == 5 && mapped.category == iostream);
    const auto equivalentCondition = std::bit_cast<EquivalentCondition>(Slot(iostream, 4));
    const auto equivalentCode = std::bit_cast<EquivalentCode>(Slot(iostream, 5));
    const LibcException::LibcxxErrorCode inSelf {5, 0, iostream};
    const LibcException::LibcxxErrorCode inGeneric {5, 0, generic};
    REQUIRE(equivalentCondition(iostream, 5, &inSelf) && !equivalentCondition(iostream, 6, &inSelf));
    REQUIRE(!equivalentCondition(iostream, 5, &inGeneric));
    REQUIRE(equivalentCode(iostream, &inSelf, 5) && !equivalentCode(iostream, &inSelf, 6));
    REQUIRE(!equivalentCode(iostream, &inGeneric, 5));
    bool rejected = false;
    try { std::bit_cast<Delete>(Slot(iostream, 1))(iostream); }
    catch (const std::invalid_argument&) { rejected = true; }
    REQUIRE(rejected);
}

static std::string FailureWhat(const LibcException::LibcxxSystemErrorObject& object) {
    REQUIRE(object.base.vtable == &_ZTVNSt3__18ios_base7failureE_nid_postfix.destroy);
    return std::bit_cast<What>(static_cast<const void* const*>(object.base.vtable)[2])(&object.base);
}

static void ConstructFailure(LibcException::LibcxxSystemErrorObject& object, const char* what, const LibcException::LibcxxErrorCode* code) {
    std::memset(&object, 0xcd, sizeof(object));
    _ZNSt3__18ios_base7failureC1EPKcRKNS_10error_codeE_nid_postfix(&object, what, code);
}

static void ConstructFailure(LibcException::LibcxxSystemErrorObject& object, const LibcxxString* what, const LibcException::LibcxxErrorCode* code) {
    std::memset(&object, 0xcd, sizeof(object));
    _ZNSt3__18ios_base7failureC1ERKNS_12basic_stringIcNS_11char_traitsIcEENS_9allocatorIcEEEERKNS_10error_codeE_nid_postfix(&object, what, code);
}

static void CheckFailure(const LibcException::CategoryObject* iostream, const LibcException::CategoryObject* generic) {
    const auto& type = _ZTINSt3__18ios_base7failureE_nid_postfix;
    REQUIRE(std::strcmp(type.name, "NSt3__18ios_base7failureE") == 0);
    REQUIRE(std::strcmp(KindOf(&type), "N10__cxxabiv120__si_class_type_infoE") == 0);
    REQUIRE(type.base == &_ZTINSt3__112system_errorE_nid_postfix);
    const auto& table = _ZTVNSt3__18ios_base7failureE_nid_postfix;
    const auto& base = _ZTVNSt3__112system_errorE_nid_postfix;
    REQUIRE(table.offset == 0 && table.type == &type);
    REQUIRE(table.destroy == base.destroy && table.deleteObject == base.deleteObject && table.what == base.what);

    const LibcException::LibcxxErrorCode stream {1, 0, iostream};
    LibcException::LibcxxSystemErrorObject object;
    ConstructFailure(object, "stream", &stream);
    REQUIRE(FailureWhat(object) == "stream: unspecified iostream_category error");
    REQUIRE(object.code.value == 1 && object.code.category == iostream);
    _ZNSt3__18ios_base7failureD1Ev_nid_postfix(&object);
    REQUIRE(object.base.message == nullptr && object.code.value == 1);

    ConstructFailure(object, "", &stream);
    REQUIRE(FailureWhat(object) == "unspecified iostream_category error");
    _ZNSt3__18ios_base7failureD2Ev_nid_postfix(&object);
    REQUIRE(object.base.message == nullptr);

    const LibcException::LibcxxErrorCode exists {17, 0, generic};
    ConstructFailure(object, "open", &exists);
    REQUIRE(FailureWhat(object) == "open: File exists" && object.code.category == generic);
    _ZNSt3__18ios_base7failureD1Ev_nid_postfix(&object);

    const LibcException::LibcxxErrorCode none {0, 0, iostream};
    ConstructFailure(object, "nothing failed", &none);
    REQUIRE(FailureWhat(object) == "nothing failed" && object.code.value == 0 && object.code.category == iostream);
    _ZNSt3__18ios_base7failureD1Ev_nid_postfix(&object);

    const LibcxxString shortWhat = MakeString("read");
    ConstructFailure(object, &shortWhat, &stream);
    REQUIRE(FailureWhat(object) == "read: unspecified iostream_category error");
    REQUIRE(object.code.value == 1 && object.code.category == iostream);
    _ZNSt3__18ios_base7failureD1Ev_nid_postfix(&object);

    const char* longText = "a stream message that does not fit inline";
    const LibcxxString longWhat = MakeString(longText);
    const LibcException::LibcxxErrorCode invalid {22, 0, iostream};
    ConstructFailure(object, &longWhat, &invalid);
    REQUIRE(FailureWhat(object) == std::string(longText) + ": Invalid argument" && object.code.value == 22);
    _ZNSt3__18ios_base7failureD2Ev_nid_postfix(&object);
    REQUIRE(object.base.message == nullptr);

    const LibcxxString emptyWhat = MakeString("");
    ConstructFailure(object, &emptyWhat, &stream);
    REQUIRE(FailureWhat(object) == "unspecified iostream_category error");
    _ZNSt3__18ios_base7failureD1Ev_nid_postfix(&object);

    const LibcException::LibcxxErrorCode noCategory {1, 0, nullptr};
    bool rejected = false;
    try { ConstructFailure(object, static_cast<const char*>(nullptr), &stream); }
    catch (const std::invalid_argument&) { rejected = true; }
    REQUIRE(rejected);
    rejected = false;
    try { ConstructFailure(object, "x", nullptr); }
    catch (const std::invalid_argument&) { rejected = true; }
    REQUIRE(rejected);
    rejected = false;
    try { ConstructFailure(object, "x", &noCategory); }
    catch (const std::invalid_argument&) { rejected = true; }
    REQUIRE(rejected);
    rejected = false;
    try { ConstructFailure(object, static_cast<const LibcxxString*>(nullptr), &stream); }
    catch (const std::invalid_argument&) { rejected = true; }
    REQUIRE(rejected);
    rejected = false;
    try { ConstructFailure(object, &shortWhat, nullptr); }
    catch (const std::invalid_argument&) { rejected = true; }
    REQUIRE(rejected);
}

[[noreturn]] static void ThrowFailure(const LibcException::LibcxxErrorCode& code, const char* what) {
    auto* object = static_cast<LibcException::LibcxxSystemErrorObject*>(
        __cxa_allocate_exception_nid_postfix(sizeof(LibcException::LibcxxSystemErrorObject)));
    _ZNSt3__18ios_base7failureC1EPKcRKNS_10error_codeE_nid_postfix(object, what, &code);
    __cxa_throw_nid_postfix(object,
        reinterpret_cast<std::type_info*>(const_cast<LibcException::TypeRecord*>(&_ZTINSt3__18ios_base7failureE_nid_postfix)),
        reinterpret_cast<void (*)(void*)>(&_ZNSt3__18ios_base7failureD1Ev_nid_postfix));
}

static void CheckFailureThrow(const LibcException::CategoryObject* iostream) {
    const LibcException::LibcxxErrorCode stream {1, 0, iostream};
    bool caught = false;
    try { ThrowFailure(stream, "badbit"); }
    catch (const std::runtime_error& error) {
        REQUIRE(__cxa_current_exception_type_nid_postfix() ==
            reinterpret_cast<const std::type_info*>(&_ZTINSt3__18ios_base7failureE_nid_postfix));
        const auto* object = reinterpret_cast<const LibcException::LibcxxSystemErrorObject*>(&error);
        REQUIRE(FailureWhat(*object) == "badbit: unspecified iostream_category error");
        REQUIRE(object->code.value == 1 && object->code.category == iostream);
        REQUIRE(TypeOf(object->base.vtable)->base == &_ZTINSt3__112system_errorE_nid_postfix);
        caught = true;
    }
    REQUIRE(caught);
    caught = false;
    try { ThrowFailure(stream, ""); }
    catch (const std::exception& error) {
        const auto* object = reinterpret_cast<const LibcException::LibcxxSystemErrorObject*>(&error);
        REQUIRE(FailureWhat(*object) == "unspecified iostream_category error");
        caught = true;
    }
    REQUIRE(caught);
}

int main() {
    const std::array<void*, 10> api {};
    ApplicationHeapRegister_nid_no_patch(api.data());
    const LibcException::CategoryObject* generic = _ZNSt3__116generic_categoryEv_nid_postfix();
    CheckGenericCategory(generic);
    CheckTypeRecords();
    CheckConstructors(generic);

    const auto* system = CheckThrow(22, "x", "x: Invalid argument");
    REQUIRE(CheckThrow(0, "no error", "no error") == system);
    REQUIRE(CheckThrow(97, "", "unspecified system_category error") == system);
    CheckSystemCategory(system, generic);
    const LibcException::CategoryObject* iostream = _ZNSt3__117iostream_categoryEv_nid_postfix();
    CheckIostreamCategory(iostream, generic);
    CheckFailure(iostream, generic);
    CheckFailureThrow(iostream);
    bool rejected = false;
    try { _ZNSt3__120__throw_system_errorEiPKc_nid_postfix(5, nullptr); }
    catch (const std::invalid_argument&) { rejected = true; }
    REQUIRE(rejected);

    LibcException::CategoryObject destroyed {&api};
    _ZNSt3__114error_categoryD2Ev_nid_postfix(&destroyed);
    REQUIRE(destroyed.vtable == &api);

    std::puts("libc++ system_error guest ABI checks passed");
    return 0;
}
