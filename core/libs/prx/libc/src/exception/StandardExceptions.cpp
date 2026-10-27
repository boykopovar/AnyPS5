#include "prx/libc/include/Api.hpp"
#include "prx/libc/include/exceptions/Runtime.hpp"
#include "prx/libc/include/GuestLocale.hpp"
#include <cstdio>
#include <iterator>
#include <limits>
#include <regex>
#include <stdexcept>
#include <string>

namespace LibcException {
struct TypeRecord { const void* const* vtable; const char* name; const TypeRecord* base; };

struct ClassTypeRecord { const void* const* vtable; const char* name; };

struct ExceptionVtable {
    std::ptrdiff_t offset;
    const TypeRecord* type;
    void (APS5_VABI *destroy)(ExceptionObject*);
    void (APS5_VABI *deleteObject)(ExceptionObject*);
    const char* (APS5_VABI *what)(const ExceptionObject*);
};

struct FutureErrorVtable {
    std::ptrdiff_t offset;
    const TypeRecord* type;
    void (APS5_VABI *destroy)(FutureErrorObject*);
    void (APS5_VABI *deleteObject)(FutureErrorObject*);
    const char* (APS5_VABI *what)(const FutureErrorObject*);
    void (APS5_VABI *doraise)(const FutureErrorObject*);
};
static_assert(sizeof(ClassTypeRecord) == 0x10);
static_assert(sizeof(FutureErrorVtable) == 0x30);
struct Message {
    std::size_t length;
    std::size_t capacity;
    std::atomic<std::ptrdiff_t> references;
};

void DestroyType(TypeRecord*) {}
void DeleteType(TypeRecord* type) { std::free(type); }
bool NotPointer(const TypeRecord*) { return false; }
bool TypeCatch(const TypeRecord* self, const TypeRecord* thrown, void** object, unsigned) {
    return Match(reinterpret_cast<const std::type_info*>(self), reinterpret_cast<const std::type_info*>(thrown), *object);
}
bool TypeUpcast(const TypeRecord* self, const TypeRecord* base, void** object) {
    Search search {reinterpret_cast<const std::type_info*>(base)};
    Bases(reinterpret_cast<const std::type_info*>(self), *object, search);
    if (search.count != 1) return false;
    *object = search.found; return true;
}

const TypeRecord ClassCategory {nullptr, "N10__cxxabiv117__class_type_infoE", nullptr};
const TypeRecord SingleCategory {nullptr, "N10__cxxabiv120__si_class_type_infoE", nullptr};
const void* const ClassTypeVtable[] {
    nullptr, &ClassCategory,
    reinterpret_cast<const void*>(DestroyType), reinterpret_cast<const void*>(DeleteType),
    reinterpret_cast<const void*>(NotPointer), reinterpret_cast<const void*>(NotPointer),
    reinterpret_cast<const void*>(TypeCatch), reinterpret_cast<const void*>(TypeUpcast)
};
const void* const SingleTypeVtable[] {
    nullptr, &SingleCategory,
    reinterpret_cast<const void*>(DestroyType), reinterpret_cast<const void*>(DeleteType),
    reinterpret_cast<const void*>(NotPointer), reinterpret_cast<const void*>(NotPointer),
    reinterpret_cast<const void*>(TypeCatch), reinterpret_cast<const void*>(TypeUpcast)
};

const char* CopyMessage(const char* message) {
    if (!message) message = "";
    auto size = std::strlen(message);
    if (size > std::numeric_limits<std::size_t>::max() - sizeof(Message) - 1) Terminate();
    void* storage = std::malloc(sizeof(Message) + size + 1);
    if (!storage) Terminate();
    auto* header = new (storage) Message {size, size, 0};
    char* result = reinterpret_cast<char*>(header + 1);
    std::memcpy(result, message, size + 1);
    return result;
}
void RetainMessage(const char* message) {
    if (message) (reinterpret_cast<Message*>(const_cast<char*>(message)) - 1)->references.fetch_add(1, std::memory_order_relaxed);
}
void ReleaseMessage(const char* message) {
    if (!message) return;
    auto* header = reinterpret_cast<Message*>(const_cast<char*>(message)) - 1;
    if (header->references.fetch_sub(1, std::memory_order_acq_rel) == 0) {
        header->~Message(); std::free(header);
    }
}
void DestroyPlain(ExceptionObject*) {}
void DeletePlain(ExceptionObject* object) { std::free(object); }
void DestroyMessage(ExceptionObject* object) { ReleaseMessage(object->message); object->message = nullptr; }
static void APS5_VABI DestroyGuestMessage_nid_no_patch(void* object) {
    DestroyMessage(static_cast<ExceptionObject*>(object));
}
void DeleteMessage(ExceptionObject* object) { DestroyMessage(object); std::free(object); }
const char* PlainWhat(const ExceptionObject* object) {
    auto* table = static_cast<const ExceptionVtable*>(object->vtable);
    auto* complete = reinterpret_cast<const ExceptionVtable*>(reinterpret_cast<const unsigned char*>(table) - offsetof(ExceptionVtable, destroy));
    const char* name = complete->type->name;
    if (std::strcmp(name, "St9exception") == 0) return "std::exception";
    if (std::strcmp(name, "St8bad_cast") == 0) return "std::bad_cast";
    if (std::strcmp(name, "St10bad_typeid") == 0) return "std::bad_typeid";
    if (std::strcmp(name, "St9bad_alloc") == 0) return "std::bad_alloc";
    if (std::strcmp(name, "St20bad_array_new_length") == 0) return "std::bad_array_new_length";
    if (std::strcmp(name, "St17bad_function_call") == 0) return "std::bad_function_call";
    if (std::strcmp(name, "St12bad_weak_ptr") == 0) return "bad_weak_ptr";
    if (std::strcmp(name, "St18bad_variant_access") == 0) return "bad_variant_access";
    if (std::strcmp(name, "St19bad_optional_access") == 0) return "bad_optional_access";
    return name;
}
const char* MessageWhat(const ExceptionObject* object) { return object->message ? object->message : ""; }
static void APS5_VABI GuestDestroyPlain_nid_no_patch(ExceptionObject* object) { DestroyPlain(object); }
static void APS5_VABI GuestDeletePlain_nid_no_patch(ExceptionObject* object) { DeletePlain(object); }
static const char* APS5_VABI GuestPlainWhat_nid_no_patch(const ExceptionObject* object) { return PlainWhat(object); }
static void APS5_VABI GuestDestroyMessage_nid_no_patch(ExceptionObject* object) { DestroyMessage(object); }
static void APS5_VABI GuestDeleteMessage_nid_no_patch(ExceptionObject* object) { DeleteMessage(object); }
static const char* APS5_VABI GuestMessageWhat_nid_no_patch(const ExceptionObject* object) { return MessageWhat(object); }
void Construct(ExceptionObject* object, const ExceptionVtable& table, const char* message) {
    object->vtable = &table.destroy;
    object->message = CopyMessage(message);
}
void Copy(ExceptionObject* object, const ExceptionObject* source, const ExceptionVtable& table) {
    object->vtable = &table.destroy;
    object->message = source->message;
    RetainMessage(object->message);
}
ExceptionObject* Assign(ExceptionObject* object, const ExceptionObject* source) {
    if (object != source) { RetainMessage(source->message); ReleaseMessage(object->message); object->message = source->message; }
    return object;
}
[[noreturn]] void ThrowMessage(const ExceptionVtable& table, const char* message) {
    auto* object = static_cast<ExceptionObject*>(__cxa_allocate_exception_nid_postfix(sizeof(ExceptionObject)));
    Construct(object, table, message);
    __cxa_throw_nid_postfix(object, reinterpret_cast<std::type_info*>(const_cast<TypeRecord*>(table.type)),
        reinterpret_cast<decltype(Header::destructor)>(DestroyGuestMessage_nid_no_patch));
}
[[noreturn]] void ThrowPlain(const ExceptionVtable& table) {
    auto* object = static_cast<ExceptionObject*>(__cxa_allocate_exception_nid_postfix(sizeof(void*)));
    object->vtable = &table.destroy;
    __cxa_throw_nid_postfix(object, reinterpret_cast<std::type_info*>(const_cast<TypeRecord*>(table.type)), [](void* p) { DestroyPlain(static_cast<ExceptionObject*>(p)); });
}

static_assert(sizeof(ErrorCode) == 16 && offsetof(ErrorCode, value) == 4 && offsetof(ErrorCode, category) == 8);
static_assert(sizeof(SystemErrorObject) == 32 && offsetof(SystemErrorObject, code) == 16);
static_assert(sizeof(FutureErrorObject) == 0x40 && offsetof(FutureErrorObject, message) == 0x8 && offsetof(FutureErrorObject, code) == 0x30);
struct CppError { int value; const char* message; };

static const char* GenericErrorMessage(int value) {
    static constexpr CppError errors[] {
        {1, "operation not permitted"},
        {2, "no such file or directory"},
        {3, "no such process"},
        {4, "interrupted"},
        {5, "io error"},
        {6, "no such device or address"},
        {7, "argument list too long"},
        {8, "executable format error"},
        {9, "bad file descriptor"},
        {10, "no child process"},
        {11, "resource deadlock would occur"},
        {12, "not enough memory"},
        {13, "permission denied"},
        {14, "bad address"},
        {16, "device or resource busy"},
        {17, "file exists"},
        {18, "cross device link"},
        {19, "no such device"},
        {20, "not a directory"},
        {21, "is a directory"},
        {22, "invalid argument"},
        {23, "too many files open in system"},
        {24, "too many files open"},
        {25, "inappropriate io control operation"},
        {26, "text file busy"},
        {27, "file too large"},
        {28, "no space on device"},
        {29, "invalid seek"},
        {30, "read only file system"},
        {31, "too many links"},
        {32, "broken pipe"},
        {33, "argument out of domain"},
        {34, "result out of range"},
        {35, "operation would block"},
        {36, "operation in progress"},
        {37, "connection already in progress"},
        {38, "not a socket"},
        {39, "destination address required"},
        {40, "message size"},
        {41, "wrong protocol type"},
        {42, "no protocol option"},
        {43, "protocol not supported"},
        {45, "not supported"},
        {47, "address family not supported"},
        {48, "address in use"},
        {49, "address not available"},
        {50, "network down"},
        {51, "network unreachable"},
        {52, "network reset"},
        {53, "connection aborted"},
        {54, "connection reset"},
        {55, "no buffer space"},
        {56, "already connected"},
        {57, "not connected"},
        {60, "timed out"},
        {61, "connection refused"},
        {62, "too many symbolic link levels"},
        {63, "filename too long"},
        {65, "host unreachable"},
        {66, "directory not empty"},
        {77, "no lock available"},
        {78, "function not supported"},
        {82, "identifier removed"},
        {83, "no message"},
        {84, "value too large"},
        {85, "operation canceled"},
        {86, "illegal byte sequence"},
        {89, "bad message"},
        {91, "no link"},
        {92, "protocol error"},
        {95, "state not recoverable"},
        {96, "owner dead"}
    };
    for (const auto& error : errors)
        if (error.value == value) return error.message;
    return nullptr;
}

const TypeRecord ErrorCategoryType {ClassTypeVtable + 2, "St14error_category", nullptr};
const TypeRecord GenericCategoryType {SingleTypeVtable + 2, "St23_Generic_error_category", &ErrorCategoryType};
const TypeRecord SystemCategoryType {SingleTypeVtable + 2, "St22_System_error_category", &ErrorCategoryType};
static void APS5_VABI CategoryDestroy_nid_no_patch(CategoryObject*) {}
static void APS5_VABI CategoryDelete_nid_no_patch(CategoryObject*) {
    throw std::invalid_argument("std::error_category: the generic and system category objects are not heap allocated");
}
static const char* APS5_VABI GenericCategoryName_nid_no_patch(const CategoryObject*) { return "generic"; }
static const char* APS5_VABI SystemCategoryName_nid_no_patch(const CategoryObject*) { return "system"; }
static GuestLocale::String* FillMessage(GuestLocale::String* result, const char* message) {
    if (!result) throw std::invalid_argument("std::error_category::message: null result");
    const auto length = std::strlen(message);
    const auto capacity = length | std::size_t{15};
    char* storage = length <= 15 ? result->buffer : static_cast<char*>(_Znwm_nid_postfix(capacity + 1));
    std::memcpy(storage, message, length + 1);
    result->reserved = 0;
    if (length > 15) result->pointer = storage;
    result->size = length;
    result->capacity = capacity;
    return result;
}
static const char* FutureErrorMessage(int value) {
    static constexpr const char* messages[] {"broken promise", "future already retrieved", "promise already satisfied", "no state"};
    if (value < 1 || value > 4) return nullptr;
    return messages[value - 1];
}
static GuestLocale::String* APS5_VABI CategoryMessage_nid_no_patch(GuestLocale::String* result, const CategoryObject*, int value) {
    const char* message = GenericErrorMessage(value);
    if (!message) message = "unknown error";
    return FillMessage(result, message);
}
static GuestLocale::String* APS5_VABI FutureCategoryMessage_nid_no_patch(GuestLocale::String* result, const CategoryObject*, int value) {
    const char* message = FutureErrorMessage(value);
    if (!message) message = GenericErrorMessage(value);
    if (!message) message = "unknown error";
    return FillMessage(result, message);
}
static const char* APS5_VABI FutureCategoryName_nid_no_patch(const CategoryObject*) { return "future"; }
static ErrorCode APS5_VABI CategoryCondition_nid_no_patch(const CategoryObject* self, int value) { return {0, value, self}; }
static bool APS5_VABI CategoryEquivalentCondition_nid_no_patch(const CategoryObject* self, int value, const ErrorCode* condition) {
    using Condition = ErrorCode (APS5_VABI *)(const CategoryObject*, int);
#ifdef __APPLE__
    const auto mapped = reinterpret_cast<Condition>(const_cast<void*>(static_cast<const void* const*>(self->vtable)[4]))(self, value);
#else
    const auto mapped = reinterpret_cast<Condition>(static_cast<const void* const*>(self->vtable)[4])(self, value);
#endif
    return condition->category == mapped.category && condition->value == mapped.value;
}
static bool APS5_VABI CategoryEquivalentCode_nid_no_patch(const CategoryObject* self, const ErrorCode* code, int value) {
    return code->category == self && code->value == value;
}
const void* const GenericCategoryVtable[] {
    nullptr, &GenericCategoryType,
    reinterpret_cast<const void*>(CategoryDestroy_nid_no_patch), reinterpret_cast<const void*>(CategoryDelete_nid_no_patch),
    reinterpret_cast<const void*>(GenericCategoryName_nid_no_patch), reinterpret_cast<const void*>(CategoryMessage_nid_no_patch),
    reinterpret_cast<const void*>(CategoryCondition_nid_no_patch),
    reinterpret_cast<const void*>(CategoryEquivalentCondition_nid_no_patch),
    reinterpret_cast<const void*>(CategoryEquivalentCode_nid_no_patch)
};
const CategoryObject GenericCategory {GenericCategoryVtable + 2};
static ErrorCode APS5_VABI SystemCategoryCondition_nid_no_patch(const CategoryObject* self, int value) {
    return {0, value, GenericErrorMessage(value) ? &GenericCategory : self};
}
const void* const SystemCategoryVtable[] {
    nullptr, &SystemCategoryType,
    reinterpret_cast<const void*>(CategoryDestroy_nid_no_patch), reinterpret_cast<const void*>(CategoryDelete_nid_no_patch),
    reinterpret_cast<const void*>(SystemCategoryName_nid_no_patch), reinterpret_cast<const void*>(CategoryMessage_nid_no_patch),
    reinterpret_cast<const void*>(SystemCategoryCondition_nid_no_patch),
    reinterpret_cast<const void*>(CategoryEquivalentCondition_nid_no_patch),
    reinterpret_cast<const void*>(CategoryEquivalentCode_nid_no_patch)
};
const CategoryObject SystemCategory {SystemCategoryVtable + 2};
[[noreturn]] void ThrowCppError(int code, const ExceptionVtable& table) {
    static constexpr CppError errors[] {
        {16, "device or resource busy"},
        {22, "invalid argument"},
        {3, "no such process"},
        {12, "not enough memory"},
        {1, "operation not permitted"},
        {11, "resource deadlock would occur"},
        {35, "resource unavailable try again"}
    };
    if (code < 0 || code >= static_cast<int>(std::size(errors)))
        throw std::out_of_range("_Throw_Cpp_error: " + std::to_string(code) + " is not a _Cpp_error code");
    const auto& error = errors[code];
    char message[128];
    std::snprintf(message, sizeof(message), "%s: %s", error.message, GenericErrorMessage(error.value));
    auto* object = static_cast<SystemErrorObject*>(__cxa_allocate_exception_nid_postfix(sizeof(SystemErrorObject)));
    Construct(&object->base, table, message);
    object->code = {0, error.value, &GenericCategory};
    __cxa_throw_nid_postfix(object, reinterpret_cast<std::type_info*>(const_cast<TypeRecord*>(table.type)),
        reinterpret_cast<decltype(Header::destructor)>(DestroyGuestMessage_nid_no_patch));
}
}

extern "C" {
const char* APS5_VABI _ZSt13_Syserror_mapi_nid_postfix(int value) {
    return LibcException::GenericErrorMessage(value);
}
const LibcException::CategoryObject* APS5_VABI _ZSt16generic_categoryv_nid_postfix() {
    return &LibcException::GenericCategory;
}
const LibcException::CategoryObject* APS5_VABI _ZSt15system_categoryv_nid_postfix() {
    return &LibcException::SystemCategory;
}
GuestLocale::String* APS5_VABI _ZNKSt23_Generic_error_category7messageEi_nid_postfix(
    GuestLocale::String* result, const LibcException::CategoryObject* self, int value) {
    return LibcException::CategoryMessage_nid_no_patch(result, self, value);
}
LibcException::TypeRecord _ZTISt8ios_base_nid_postfix {LibcException::ClassTypeVtable + 2, "St8ios_base", nullptr};
LibcException::TypeRecord _ZTISt9exception_nid_postfix {LibcException::ClassTypeVtable + 2, "St9exception", nullptr};
LibcException::ExceptionVtable _ZTVSt9exception_nid_postfix {0, &_ZTISt9exception_nid_postfix, LibcException::GuestDestroyPlain_nid_no_patch, LibcException::GuestDeletePlain_nid_no_patch, LibcException::GuestPlainWhat_nid_no_patch};
void APS5_VABI _ZNSt9exceptionD1Ev_nid_postfix(LibcException::ExceptionObject* self) { LibcException::DestroyPlain(self); }
void APS5_VABI _ZNSt9exceptionD2Ev_nid_postfix(LibcException::ExceptionObject* self) { LibcException::DestroyPlain(self); }
void APS5_VABI _ZNSt9exceptionD0Ev_nid_postfix(LibcException::ExceptionObject* self) { LibcException::DeletePlain(self); }
const char* APS5_VABI _ZNKSt9exception4whatEv_nid_postfix(const LibcException::ExceptionObject* self) { return LibcException::PlainWhat(self); }
[[noreturn]] void APS5_VABI _ZNKSt9exception8_DoraiseEv_nid_postfix(const LibcException::ExceptionObject* self) {
    if (!self) throw std::invalid_argument("exception object is null");
    LibcException::ThrowPlain(_ZTVSt9exception_nid_postfix);
}

[[noreturn]] void APS5_VABI _ZNKSt9exception6_RaiseEv_nid_postfix(const LibcException::ExceptionObject* self) {
    if (!self) throw std::invalid_argument("exception object is null");
    LibcException::ThrowPlain(_ZTVSt9exception_nid_postfix);
}
void APS5_VABI _ZNSt9exceptionC1Ev_nid_postfix(LibcException::ExceptionObject* self) { self->vtable = &_ZTVSt9exception_nid_postfix.destroy; }
void APS5_VABI _ZNSt9exceptionC2Ev_nid_postfix(LibcException::ExceptionObject* self) { self->vtable = &_ZTVSt9exception_nid_postfix.destroy; }

LibcException::TypeRecord _ZTISt8bad_cast_nid_postfix {LibcException::SingleTypeVtable + 2, "St8bad_cast", &_ZTISt9exception_nid_postfix};
LibcException::ExceptionVtable _ZTVSt8bad_cast_nid_postfix {0, &_ZTISt8bad_cast_nid_postfix, LibcException::GuestDestroyPlain_nid_no_patch, LibcException::GuestDeletePlain_nid_no_patch, LibcException::GuestPlainWhat_nid_no_patch};
void APS5_VABI _ZNSt8bad_castD1Ev_nid_postfix(LibcException::ExceptionObject* self) { LibcException::DestroyPlain(self); }
void APS5_VABI _ZNSt8bad_castD2Ev_nid_postfix(LibcException::ExceptionObject* self) { LibcException::DestroyPlain(self); }
void APS5_VABI _ZNSt8bad_castD0Ev_nid_postfix(LibcException::ExceptionObject* self) { LibcException::DeletePlain(self); }
const char* APS5_VABI _ZNKSt8bad_cast4whatEv_nid_postfix(const LibcException::ExceptionObject* self) { return LibcException::PlainWhat(self); }
void APS5_VABI _ZNSt8bad_castC1Ev_nid_postfix(LibcException::ExceptionObject* self) { self->vtable = &_ZTVSt8bad_cast_nid_postfix.destroy; }
void APS5_VABI _ZNSt8bad_castC2Ev_nid_postfix(LibcException::ExceptionObject* self) { self->vtable = &_ZTVSt8bad_cast_nid_postfix.destroy; }
[[noreturn]] void APS5_VABI _ZNKSt8bad_cast8_DoraiseEv_nid_postfix(const LibcException::ExceptionObject* self) {
    if (!self) throw std::invalid_argument("exception object is null");
    LibcException::ThrowPlain(_ZTVSt8bad_cast_nid_postfix);
}

LibcException::TypeRecord _ZTISt10bad_typeid_nid_postfix {LibcException::SingleTypeVtable + 2, "St10bad_typeid", &_ZTISt9exception_nid_postfix};
LibcException::ExceptionVtable _ZTVSt10bad_typeid_nid_postfix {0, &_ZTISt10bad_typeid_nid_postfix, LibcException::GuestDestroyPlain_nid_no_patch, LibcException::GuestDeletePlain_nid_no_patch, LibcException::GuestPlainWhat_nid_no_patch};
void APS5_VABI _ZNSt10bad_typeidD1Ev_nid_postfix(LibcException::ExceptionObject* self) { LibcException::DestroyPlain(self); }
void APS5_VABI _ZNSt10bad_typeidD2Ev_nid_postfix(LibcException::ExceptionObject* self) { LibcException::DestroyPlain(self); }
void APS5_VABI _ZNSt10bad_typeidD0Ev_nid_postfix(LibcException::ExceptionObject* self) { LibcException::DeletePlain(self); }
const char* APS5_VABI _ZNKSt10bad_typeid4whatEv_nid_postfix(const LibcException::ExceptionObject* self) { return LibcException::PlainWhat(self); }
void APS5_VABI _ZNSt10bad_typeidC1Ev_nid_postfix(LibcException::ExceptionObject* self) { self->vtable = &_ZTVSt10bad_typeid_nid_postfix.destroy; }
void APS5_VABI _ZNSt10bad_typeidC2Ev_nid_postfix(LibcException::ExceptionObject* self) { self->vtable = &_ZTVSt10bad_typeid_nid_postfix.destroy; }

LibcException::TypeRecord _ZTISt9bad_alloc_nid_postfix {LibcException::SingleTypeVtable + 2, "St9bad_alloc", &_ZTISt9exception_nid_postfix};
LibcException::ExceptionVtable _ZTVSt9bad_alloc_nid_postfix {0, &_ZTISt9bad_alloc_nid_postfix, LibcException::GuestDestroyPlain_nid_no_patch, LibcException::GuestDeletePlain_nid_no_patch, LibcException::GuestPlainWhat_nid_no_patch};
void APS5_VABI _ZNSt9bad_allocD1Ev_nid_postfix(LibcException::ExceptionObject* self) { LibcException::DestroyPlain(self); }
void APS5_VABI _ZNSt9bad_allocD2Ev_nid_postfix(LibcException::ExceptionObject* self) { LibcException::DestroyPlain(self); }
void APS5_VABI _ZNSt9bad_allocD0Ev_nid_postfix(LibcException::ExceptionObject* self) { LibcException::DeletePlain(self); }
const char* APS5_VABI _ZNKSt9bad_alloc4whatEv_nid_postfix(const LibcException::ExceptionObject* self) { return LibcException::PlainWhat(self); }
void APS5_VABI _ZNSt9bad_allocC1Ev_nid_postfix(LibcException::ExceptionObject* self) { self->vtable = &_ZTVSt9bad_alloc_nid_postfix.destroy; }
void APS5_VABI _ZNSt9bad_allocC2Ev_nid_postfix(LibcException::ExceptionObject* self) { self->vtable = &_ZTVSt9bad_alloc_nid_postfix.destroy; }

LibcException::TypeRecord _ZTISt20bad_array_new_length_nid_postfix {LibcException::SingleTypeVtable + 2, "St20bad_array_new_length", &_ZTISt9bad_alloc_nid_postfix};
LibcException::ExceptionVtable _ZTVSt20bad_array_new_length_nid_postfix {0, &_ZTISt20bad_array_new_length_nid_postfix, LibcException::GuestDestroyPlain_nid_no_patch, LibcException::GuestDeletePlain_nid_no_patch, LibcException::GuestPlainWhat_nid_no_patch};
void APS5_VABI _ZNSt20bad_array_new_lengthD1Ev_nid_postfix(LibcException::ExceptionObject* self) { LibcException::DestroyPlain(self); }
void APS5_VABI _ZNSt20bad_array_new_lengthD2Ev_nid_postfix(LibcException::ExceptionObject* self) { LibcException::DestroyPlain(self); }
void APS5_VABI _ZNSt20bad_array_new_lengthD0Ev_nid_postfix(LibcException::ExceptionObject* self) { LibcException::DeletePlain(self); }
const char* APS5_VABI _ZNKSt20bad_array_new_length4whatEv_nid_postfix(const LibcException::ExceptionObject* self) { return LibcException::PlainWhat(self); }
void APS5_VABI _ZNSt20bad_array_new_lengthC1Ev_nid_postfix(LibcException::ExceptionObject* self) { self->vtable = &_ZTVSt20bad_array_new_length_nid_postfix.destroy; }
void APS5_VABI _ZNSt20bad_array_new_lengthC2Ev_nid_postfix(LibcException::ExceptionObject* self) { self->vtable = &_ZTVSt20bad_array_new_length_nid_postfix.destroy; }

LibcException::TypeRecord _ZTISt17bad_function_call_nid_postfix {LibcException::SingleTypeVtable + 2, "St17bad_function_call", &_ZTISt9exception_nid_postfix};
LibcException::ExceptionVtable _ZTVSt17bad_function_call_nid_postfix {0, &_ZTISt17bad_function_call_nid_postfix, LibcException::GuestDestroyPlain_nid_no_patch, LibcException::GuestDeletePlain_nid_no_patch, LibcException::GuestPlainWhat_nid_no_patch};
void APS5_VABI _ZNSt17bad_function_callD1Ev_nid_postfix(LibcException::ExceptionObject* self) { LibcException::DestroyPlain(self); }
void APS5_VABI _ZNSt17bad_function_callD2Ev_nid_postfix(LibcException::ExceptionObject* self) { LibcException::DestroyPlain(self); }
void APS5_VABI _ZNSt17bad_function_callD0Ev_nid_postfix(LibcException::ExceptionObject* self) { LibcException::DeletePlain(self); }
const char* APS5_VABI _ZNKSt17bad_function_call4whatEv_nid_postfix(const LibcException::ExceptionObject* self) { return LibcException::PlainWhat(self); }
void APS5_VABI _ZNSt17bad_function_callC1Ev_nid_postfix(LibcException::ExceptionObject* self) { self->vtable = &_ZTVSt17bad_function_call_nid_postfix.destroy; }
void APS5_VABI _ZNSt17bad_function_callC2Ev_nid_postfix(LibcException::ExceptionObject* self) { self->vtable = &_ZTVSt17bad_function_call_nid_postfix.destroy; }

LibcException::TypeRecord _ZTISt12bad_weak_ptr_nid_postfix {LibcException::SingleTypeVtable + 2, "St12bad_weak_ptr", &_ZTISt9exception_nid_postfix};
LibcException::ExceptionVtable _ZTVSt12bad_weak_ptr_nid_postfix {0, &_ZTISt12bad_weak_ptr_nid_postfix, LibcException::GuestDestroyPlain_nid_no_patch, LibcException::GuestDeletePlain_nid_no_patch, LibcException::GuestPlainWhat_nid_no_patch};
void APS5_VABI _ZNSt12bad_weak_ptrD1Ev_nid_postfix(LibcException::ExceptionObject* self) { LibcException::DestroyPlain(self); }
void APS5_VABI _ZNSt12bad_weak_ptrD2Ev_nid_postfix(LibcException::ExceptionObject* self) { LibcException::DestroyPlain(self); }
void APS5_VABI _ZNSt12bad_weak_ptrD0Ev_nid_postfix(LibcException::ExceptionObject* self) { LibcException::DeletePlain(self); }
const char* APS5_VABI _ZNKSt12bad_weak_ptr4whatEv_nid_postfix(const LibcException::ExceptionObject* self) { return LibcException::PlainWhat(self); }

LibcException::TypeRecord _ZTISt18bad_variant_access_nid_postfix {LibcException::SingleTypeVtable + 2, "St18bad_variant_access", &_ZTISt9exception_nid_postfix};
LibcException::ExceptionVtable _ZTVSt18bad_variant_access_nid_postfix {0, &_ZTISt18bad_variant_access_nid_postfix, LibcException::GuestDestroyPlain_nid_no_patch, LibcException::GuestDeletePlain_nid_no_patch, LibcException::GuestPlainWhat_nid_no_patch};

LibcException::TypeRecord _ZTISt19bad_optional_access_nid_postfix {LibcException::SingleTypeVtable + 2, "St19bad_optional_access", &_ZTISt9exception_nid_postfix};
LibcException::ExceptionVtable _ZTVSt19bad_optional_access_nid_postfix {0, &_ZTISt19bad_optional_access_nid_postfix, LibcException::GuestDestroyPlain_nid_no_patch, LibcException::GuestDeletePlain_nid_no_patch, LibcException::GuestPlainWhat_nid_no_patch};
void APS5_VABI _ZNSt19bad_optional_accessD1Ev_nid_postfix(LibcException::ExceptionObject* self) { LibcException::DestroyPlain(self); }
void APS5_VABI _ZNSt19bad_optional_accessD2Ev_nid_postfix(LibcException::ExceptionObject* self) { LibcException::DestroyPlain(self); }
void APS5_VABI _ZNSt19bad_optional_accessD0Ev_nid_postfix(LibcException::ExceptionObject* self) { LibcException::DeletePlain(self); }
const char* APS5_VABI _ZNKSt19bad_optional_access4whatEv_nid_postfix(const LibcException::ExceptionObject* self) { return LibcException::PlainWhat(self); }
void APS5_VABI _ZNSt19bad_optional_accessC1Ev_nid_postfix(LibcException::ExceptionObject* self) { self->vtable = &_ZTVSt19bad_optional_access_nid_postfix.destroy; }
void APS5_VABI _ZNSt19bad_optional_accessC2Ev_nid_postfix(LibcException::ExceptionObject* self) { self->vtable = &_ZTVSt19bad_optional_access_nid_postfix.destroy; }

LibcException::TypeRecord _ZTISt11logic_error_nid_postfix {LibcException::SingleTypeVtable + 2, "St11logic_error", &_ZTISt9exception_nid_postfix};
LibcException::ExceptionVtable _ZTVSt11logic_error_nid_postfix {0, &_ZTISt11logic_error_nid_postfix, LibcException::GuestDestroyMessage_nid_no_patch, LibcException::GuestDeleteMessage_nid_no_patch, LibcException::GuestMessageWhat_nid_no_patch};
void APS5_VABI _ZNSt11logic_errorD1Ev_nid_postfix(LibcException::ExceptionObject* self) { LibcException::DestroyMessage(self); }
void APS5_VABI _ZNSt11logic_errorD2Ev_nid_postfix(LibcException::ExceptionObject* self) { LibcException::DestroyMessage(self); }
void APS5_VABI _ZNSt11logic_errorD0Ev_nid_postfix(LibcException::ExceptionObject* self) { LibcException::DeleteMessage(self); }
const char* APS5_VABI _ZNKSt11logic_error4whatEv_nid_postfix(const LibcException::ExceptionObject* self) { return LibcException::MessageWhat(self); }
void APS5_VABI _ZNSt11logic_errorC1EPKc_nid_postfix(LibcException::ExceptionObject* self, const char* message) { LibcException::Construct(self, _ZTVSt11logic_error_nid_postfix, message); }
void APS5_VABI _ZNSt11logic_errorC2EPKc_nid_postfix(LibcException::ExceptionObject* self, const char* message) { LibcException::Construct(self, _ZTVSt11logic_error_nid_postfix, message); }
void APS5_VABI _ZNSt11logic_errorC1ERKS__nid_postfix(LibcException::ExceptionObject* self, const LibcException::ExceptionObject* source) { LibcException::Copy(self, source, _ZTVSt11logic_error_nid_postfix); }
void APS5_VABI _ZNSt11logic_errorC2ERKS__nid_postfix(LibcException::ExceptionObject* self, const LibcException::ExceptionObject* source) { LibcException::Copy(self, source, _ZTVSt11logic_error_nid_postfix); }
LibcException::ExceptionObject* APS5_VABI _ZNSt11logic_erroraSERKS__nid_postfix(LibcException::ExceptionObject* self, const LibcException::ExceptionObject* source) { return LibcException::Assign(self, source); }
[[noreturn]] void APS5_VABI _ZNKSt11logic_error8_DoraiseEv_nid_postfix(const LibcException::ExceptionObject* self) {
    if (!self) throw std::invalid_argument("exception object is null");
    LibcException::ThrowMessage(_ZTVSt11logic_error_nid_postfix, LibcException::MessageWhat(self));
}

LibcException::TypeRecord _ZTISt13runtime_error_nid_postfix {LibcException::SingleTypeVtable + 2, "St13runtime_error", &_ZTISt9exception_nid_postfix};
LibcException::ExceptionVtable _ZTVSt13runtime_error_nid_postfix {0, &_ZTISt13runtime_error_nid_postfix, LibcException::GuestDestroyMessage_nid_no_patch, LibcException::GuestDeleteMessage_nid_no_patch, LibcException::GuestMessageWhat_nid_no_patch};
void APS5_VABI _ZNSt13runtime_errorD1Ev_nid_postfix(LibcException::ExceptionObject* self) { LibcException::DestroyMessage(self); }
void APS5_VABI _ZNSt13runtime_errorD2Ev_nid_postfix(LibcException::ExceptionObject* self) { LibcException::DestroyMessage(self); }
void APS5_VABI _ZNSt13runtime_errorD0Ev_nid_postfix(LibcException::ExceptionObject* self) { LibcException::DeleteMessage(self); }
const char* APS5_VABI _ZNKSt13runtime_error4whatEv_nid_postfix(const LibcException::ExceptionObject* self) { return LibcException::MessageWhat(self); }
void APS5_VABI _ZNSt13runtime_errorC1EPKc_nid_postfix(LibcException::ExceptionObject* self, const char* message) { LibcException::Construct(self, _ZTVSt13runtime_error_nid_postfix, message); }
void APS5_VABI _ZNSt13runtime_errorC2EPKc_nid_postfix(LibcException::ExceptionObject* self, const char* message) { LibcException::Construct(self, _ZTVSt13runtime_error_nid_postfix, message); }
void APS5_VABI _ZNSt13runtime_errorC1ERKS__nid_postfix(LibcException::ExceptionObject* self, const LibcException::ExceptionObject* source) { LibcException::Copy(self, source, _ZTVSt13runtime_error_nid_postfix); }
void APS5_VABI _ZNSt13runtime_errorC2ERKS__nid_postfix(LibcException::ExceptionObject* self, const LibcException::ExceptionObject* source) { LibcException::Copy(self, source, _ZTVSt13runtime_error_nid_postfix); }
LibcException::ExceptionObject* APS5_VABI _ZNSt13runtime_erroraSERKS__nid_postfix(LibcException::ExceptionObject* self, const LibcException::ExceptionObject* source) { return LibcException::Assign(self, source); }

LibcException::TypeRecord _ZTISt16invalid_argument_nid_postfix {LibcException::SingleTypeVtable + 2, "St16invalid_argument", &_ZTISt11logic_error_nid_postfix};
LibcException::ExceptionVtable _ZTVSt16invalid_argument_nid_postfix {0, &_ZTISt16invalid_argument_nid_postfix, LibcException::GuestDestroyMessage_nid_no_patch, LibcException::GuestDeleteMessage_nid_no_patch, LibcException::GuestMessageWhat_nid_no_patch};
void APS5_VABI _ZNSt16invalid_argumentD1Ev_nid_postfix(LibcException::ExceptionObject* self) { LibcException::DestroyMessage(self); }
void APS5_VABI _ZNSt16invalid_argumentD2Ev_nid_postfix(LibcException::ExceptionObject* self) { LibcException::DestroyMessage(self); }
void APS5_VABI _ZNSt16invalid_argumentD0Ev_nid_postfix(LibcException::ExceptionObject* self) { LibcException::DeleteMessage(self); }
const char* APS5_VABI _ZNKSt16invalid_argument4whatEv_nid_postfix(const LibcException::ExceptionObject* self) { return LibcException::MessageWhat(self); }
void APS5_VABI _ZNSt16invalid_argumentC1EPKc_nid_postfix(LibcException::ExceptionObject* self, const char* message) { LibcException::Construct(self, _ZTVSt16invalid_argument_nid_postfix, message); }
void APS5_VABI _ZNSt16invalid_argumentC2EPKc_nid_postfix(LibcException::ExceptionObject* self, const char* message) { LibcException::Construct(self, _ZTVSt16invalid_argument_nid_postfix, message); }
void APS5_VABI _ZNSt16invalid_argumentC1ERKS__nid_postfix(LibcException::ExceptionObject* self, const LibcException::ExceptionObject* source) { LibcException::Copy(self, source, _ZTVSt16invalid_argument_nid_postfix); }
void APS5_VABI _ZNSt16invalid_argumentC2ERKS__nid_postfix(LibcException::ExceptionObject* self, const LibcException::ExceptionObject* source) { LibcException::Copy(self, source, _ZTVSt16invalid_argument_nid_postfix); }
LibcException::ExceptionObject* APS5_VABI _ZNSt16invalid_argumentaSERKS__nid_postfix(LibcException::ExceptionObject* self, const LibcException::ExceptionObject* source) { return LibcException::Assign(self, source); }

LibcException::TypeRecord _ZTISt12out_of_range_nid_postfix {LibcException::SingleTypeVtable + 2, "St12out_of_range", &_ZTISt11logic_error_nid_postfix};
LibcException::ExceptionVtable _ZTVSt12out_of_range_nid_postfix {0, &_ZTISt12out_of_range_nid_postfix, LibcException::GuestDestroyMessage_nid_no_patch, LibcException::GuestDeleteMessage_nid_no_patch, LibcException::GuestMessageWhat_nid_no_patch};
void APS5_VABI _ZNSt12out_of_rangeD1Ev_nid_postfix(LibcException::ExceptionObject* self) { LibcException::DestroyMessage(self); }
void APS5_VABI _ZNSt12out_of_rangeD2Ev_nid_postfix(LibcException::ExceptionObject* self) { LibcException::DestroyMessage(self); }
void APS5_VABI _ZNSt12out_of_rangeD0Ev_nid_postfix(LibcException::ExceptionObject* self) { LibcException::DeleteMessage(self); }
const char* APS5_VABI _ZNKSt12out_of_range4whatEv_nid_postfix(const LibcException::ExceptionObject* self) { return LibcException::MessageWhat(self); }
void APS5_VABI _ZNSt12out_of_rangeC1EPKc_nid_postfix(LibcException::ExceptionObject* self, const char* message) { LibcException::Construct(self, _ZTVSt12out_of_range_nid_postfix, message); }
void APS5_VABI _ZNSt12out_of_rangeC2EPKc_nid_postfix(LibcException::ExceptionObject* self, const char* message) { LibcException::Construct(self, _ZTVSt12out_of_range_nid_postfix, message); }
void APS5_VABI _ZNSt12out_of_rangeC1ERKS__nid_postfix(LibcException::ExceptionObject* self, const LibcException::ExceptionObject* source) { LibcException::Copy(self, source, _ZTVSt12out_of_range_nid_postfix); }
void APS5_VABI _ZNSt12out_of_rangeC2ERKS__nid_postfix(LibcException::ExceptionObject* self, const LibcException::ExceptionObject* source) { LibcException::Copy(self, source, _ZTVSt12out_of_range_nid_postfix); }
LibcException::ExceptionObject* APS5_VABI _ZNSt12out_of_rangeaSERKS__nid_postfix(LibcException::ExceptionObject* self, const LibcException::ExceptionObject* source) { return LibcException::Assign(self, source); }

LibcException::TypeRecord _ZTISt12domain_error_nid_postfix {LibcException::SingleTypeVtable + 2, "St12domain_error", &_ZTISt11logic_error_nid_postfix};
LibcException::ExceptionVtable _ZTVSt12domain_error_nid_postfix {0, &_ZTISt12domain_error_nid_postfix, LibcException::GuestDestroyMessage_nid_no_patch, LibcException::GuestDeleteMessage_nid_no_patch, LibcException::GuestMessageWhat_nid_no_patch};
void APS5_VABI _ZNSt12domain_errorD1Ev_nid_postfix(LibcException::ExceptionObject* self) { LibcException::DestroyMessage(self); }
void APS5_VABI _ZNSt12domain_errorD2Ev_nid_postfix(LibcException::ExceptionObject* self) { LibcException::DestroyMessage(self); }
void APS5_VABI _ZNSt12domain_errorD0Ev_nid_postfix(LibcException::ExceptionObject* self) { LibcException::DeleteMessage(self); }
const char* APS5_VABI _ZNKSt12domain_error4whatEv_nid_postfix(const LibcException::ExceptionObject* self) { return LibcException::MessageWhat(self); }
void APS5_VABI _ZNSt12domain_errorC1EPKc_nid_postfix(LibcException::ExceptionObject* self, const char* message) { LibcException::Construct(self, _ZTVSt12domain_error_nid_postfix, message); }
void APS5_VABI _ZNSt12domain_errorC2EPKc_nid_postfix(LibcException::ExceptionObject* self, const char* message) { LibcException::Construct(self, _ZTVSt12domain_error_nid_postfix, message); }
void APS5_VABI _ZNSt12domain_errorC1ERKS__nid_postfix(LibcException::ExceptionObject* self, const LibcException::ExceptionObject* source) { LibcException::Copy(self, source, _ZTVSt12domain_error_nid_postfix); }
void APS5_VABI _ZNSt12domain_errorC2ERKS__nid_postfix(LibcException::ExceptionObject* self, const LibcException::ExceptionObject* source) { LibcException::Copy(self, source, _ZTVSt12domain_error_nid_postfix); }
LibcException::ExceptionObject* APS5_VABI _ZNSt12domain_erroraSERKS__nid_postfix(LibcException::ExceptionObject* self, const LibcException::ExceptionObject* source) { return LibcException::Assign(self, source); }

LibcException::TypeRecord _ZTISt12length_error_nid_postfix {LibcException::SingleTypeVtable + 2, "St12length_error", &_ZTISt11logic_error_nid_postfix};
LibcException::ExceptionVtable _ZTVSt12length_error_nid_postfix {0, &_ZTISt12length_error_nid_postfix, LibcException::GuestDestroyMessage_nid_no_patch, LibcException::GuestDeleteMessage_nid_no_patch, LibcException::GuestMessageWhat_nid_no_patch};
void APS5_VABI _ZNSt12length_errorD1Ev_nid_postfix(LibcException::ExceptionObject* self) { LibcException::DestroyMessage(self); }
void APS5_VABI _ZNSt12length_errorD2Ev_nid_postfix(LibcException::ExceptionObject* self) { LibcException::DestroyMessage(self); }
void APS5_VABI _ZNSt12length_errorD0Ev_nid_postfix(LibcException::ExceptionObject* self) { LibcException::DeleteMessage(self); }
const char* APS5_VABI _ZNKSt12length_error4whatEv_nid_postfix(const LibcException::ExceptionObject* self) { return LibcException::MessageWhat(self); }
void APS5_VABI _ZNSt12length_errorC1EPKc_nid_postfix(LibcException::ExceptionObject* self, const char* message) { LibcException::Construct(self, _ZTVSt12length_error_nid_postfix, message); }
void APS5_VABI _ZNSt12length_errorC2EPKc_nid_postfix(LibcException::ExceptionObject* self, const char* message) { LibcException::Construct(self, _ZTVSt12length_error_nid_postfix, message); }
void APS5_VABI _ZNSt12length_errorC1ERKS__nid_postfix(LibcException::ExceptionObject* self, const LibcException::ExceptionObject* source) { LibcException::Copy(self, source, _ZTVSt12length_error_nid_postfix); }
void APS5_VABI _ZNSt12length_errorC2ERKS__nid_postfix(LibcException::ExceptionObject* self, const LibcException::ExceptionObject* source) { LibcException::Copy(self, source, _ZTVSt12length_error_nid_postfix); }
LibcException::ExceptionObject* APS5_VABI _ZNSt12length_erroraSERKS__nid_postfix(LibcException::ExceptionObject* self, const LibcException::ExceptionObject* source) { return LibcException::Assign(self, source); }

LibcException::TypeRecord _ZTISt12system_error_nid_postfix {LibcException::SingleTypeVtable + 2, "St12system_error", &_ZTISt13runtime_error_nid_postfix};
LibcException::ExceptionVtable _ZTVSt12system_error_nid_postfix {0, &_ZTISt12system_error_nid_postfix, LibcException::GuestDestroyMessage_nid_no_patch, LibcException::GuestDeleteMessage_nid_no_patch, LibcException::GuestMessageWhat_nid_no_patch};
void APS5_VABI _ZNSt12system_errorD1Ev_nid_postfix(LibcException::ExceptionObject* self) { LibcException::DestroyMessage(self); }
void APS5_VABI _ZNSt12system_errorD2Ev_nid_postfix(LibcException::ExceptionObject* self) { LibcException::DestroyMessage(self); }
void APS5_VABI _ZNSt12system_errorD0Ev_nid_postfix(LibcException::ExceptionObject* self) { LibcException::DeleteMessage(self); }
const char* APS5_VABI _ZNKSt12system_error4whatEv_nid_postfix(const LibcException::ExceptionObject* self) { return LibcException::MessageWhat(self); }
void APS5_VABI _ZNSt12system_errorC1ERKS__nid_postfix(LibcException::SystemErrorObject* self, const LibcException::SystemErrorObject* source) {
    LibcException::Copy(&self->base, &source->base, _ZTVSt12system_error_nid_postfix);
    self->code = source->code;
}
void APS5_VABI _ZNSt12system_errorC2ERKS__nid_postfix(LibcException::SystemErrorObject* self, const LibcException::SystemErrorObject* source) {
    LibcException::Copy(&self->base, &source->base, _ZTVSt12system_error_nid_postfix);
    self->code = source->code;
}
LibcException::SystemErrorObject* APS5_VABI _ZNSt12system_erroraSERKS__nid_postfix(LibcException::SystemErrorObject* self, const LibcException::SystemErrorObject* source) {
    LibcException::Assign(&self->base, &source->base);
    self->code = source->code;
    return self;
}

LibcException::TypeRecord _ZTISt11regex_error_nid_postfix {LibcException::SingleTypeVtable + 2, "St11regex_error", &_ZTISt13runtime_error_nid_postfix};
LibcException::ExceptionVtable _ZTVSt11regex_error_nid_postfix {0, &_ZTISt11regex_error_nid_postfix, LibcException::GuestDestroyMessage_nid_no_patch, LibcException::GuestDeleteMessage_nid_no_patch, LibcException::GuestMessageWhat_nid_no_patch};
void APS5_VABI _ZNSt11regex_errorD1Ev_nid_postfix(LibcException::ExceptionObject* self) { LibcException::DestroyMessage(self); }
void APS5_VABI _ZNSt11regex_errorD2Ev_nid_postfix(LibcException::ExceptionObject* self) { LibcException::DestroyMessage(self); }
void APS5_VABI _ZNSt11regex_errorD0Ev_nid_postfix(LibcException::ExceptionObject* self) { LibcException::DeleteMessage(self); }
const char* APS5_VABI _ZNKSt11regex_error4whatEv_nid_postfix(const LibcException::ExceptionObject* self) { return LibcException::MessageWhat(self); }
void APS5_VABI _ZNSt11regex_errorC1ERKS__nid_postfix(LibcException::ExceptionObject* self, const LibcException::ExceptionObject* source) { LibcException::Copy(self, source, _ZTVSt11regex_error_nid_postfix); }
void APS5_VABI _ZNSt11regex_errorC2ERKS__nid_postfix(LibcException::ExceptionObject* self, const LibcException::ExceptionObject* source) { LibcException::Copy(self, source, _ZTVSt11regex_error_nid_postfix); }
LibcException::ExceptionObject* APS5_VABI _ZNSt11regex_erroraSERKS__nid_postfix(LibcException::ExceptionObject* self, const LibcException::ExceptionObject* source) { return LibcException::Assign(self, source); }

LibcException::TypeRecord _ZTISt11range_error_nid_postfix {LibcException::SingleTypeVtable + 2, "St11range_error", &_ZTISt13runtime_error_nid_postfix};
LibcException::ExceptionVtable _ZTVSt11range_error_nid_postfix {0, &_ZTISt11range_error_nid_postfix, LibcException::GuestDestroyMessage_nid_no_patch, LibcException::GuestDeleteMessage_nid_no_patch, LibcException::GuestMessageWhat_nid_no_patch};
void APS5_VABI _ZNSt11range_errorD1Ev_nid_postfix(LibcException::ExceptionObject* self) { LibcException::DestroyMessage(self); }
void APS5_VABI _ZNSt11range_errorD2Ev_nid_postfix(LibcException::ExceptionObject* self) { LibcException::DestroyMessage(self); }
void APS5_VABI _ZNSt11range_errorD0Ev_nid_postfix(LibcException::ExceptionObject* self) { LibcException::DeleteMessage(self); }

LibcException::TypeRecord _ZTISt14overflow_error_nid_postfix {LibcException::SingleTypeVtable + 2, "St14overflow_error", &_ZTISt13runtime_error_nid_postfix};
LibcException::ExceptionVtable _ZTVSt14overflow_error_nid_postfix {0, &_ZTISt14overflow_error_nid_postfix, LibcException::GuestDestroyMessage_nid_no_patch, LibcException::GuestDeleteMessage_nid_no_patch, LibcException::GuestMessageWhat_nid_no_patch};
void APS5_VABI _ZNSt14overflow_errorD1Ev_nid_postfix(LibcException::ExceptionObject* self) { LibcException::DestroyMessage(self); }

LibcException::TypeRecord _ZTISt15underflow_error_nid_postfix {LibcException::SingleTypeVtable + 2, "St15underflow_error", &_ZTISt13runtime_error_nid_postfix};
LibcException::ExceptionVtable _ZTVSt15underflow_error_nid_postfix {0, &_ZTISt15underflow_error_nid_postfix, LibcException::GuestDestroyMessage_nid_no_patch, LibcException::GuestDeleteMessage_nid_no_patch, LibcException::GuestMessageWhat_nid_no_patch};
void APS5_VABI _ZNSt15underflow_errorD1Ev_nid_postfix(LibcException::ExceptionObject* self) { LibcException::DestroyMessage(self); }

LibcException::TypeRecord _ZTINSt8ios_base7failureE_nid_postfix {LibcException::SingleTypeVtable + 2, "NSt8ios_base7failureE", &_ZTISt12system_error_nid_postfix};
LibcException::ExceptionVtable _ZTVNSt8ios_base7failureE_nid_postfix {0, &_ZTINSt8ios_base7failureE_nid_postfix, LibcException::GuestDestroyMessage_nid_no_patch, LibcException::GuestDeleteMessage_nid_no_patch, LibcException::GuestMessageWhat_nid_no_patch};
void APS5_VABI _ZNSt8ios_base7failureD1Ev_nid_postfix(LibcException::ExceptionObject* self) { LibcException::DestroyMessage(self); }
void APS5_VABI _ZNSt8ios_base7failureD2Ev_nid_postfix(LibcException::ExceptionObject* self) { LibcException::DestroyMessage(self); }
void APS5_VABI _ZNSt8ios_base7failureD0Ev_nid_postfix(LibcException::ExceptionObject* self) { LibcException::DeleteMessage(self); }
const char* APS5_VABI _ZNKSt8ios_base7failure4whatEv_nid_postfix(const LibcException::ExceptionObject* self) { return LibcException::MessageWhat(self); }

[[noreturn]] void APS5_VABI __cxa_bad_cast_nid_postfix() { LibcException::ThrowPlain(_ZTVSt8bad_cast_nid_postfix); }
[[noreturn]] void APS5_VABI __cxa_bad_typeid_nid_postfix() { LibcException::ThrowPlain(_ZTVSt10bad_typeid_nid_postfix); }
[[noreturn]] void APS5_VABI __cxa_throw_bad_array_new_length_nid_postfix() { LibcException::ThrowPlain(_ZTVSt20bad_array_new_length_nid_postfix); }
[[noreturn]] void APS5_VABI _ZSt11_Xbad_allocv_nid_postfix() { LibcException::ThrowPlain(_ZTVSt9bad_alloc_nid_postfix); }
[[noreturn]] void APS5_VABI _ZSt14_Xout_of_rangePKc_nid_postfix(const char* message) { LibcException::ThrowMessage(_ZTVSt12out_of_range_nid_postfix, message); }
[[noreturn]] void APS5_VABI _ZSt14_Xlength_errorPKc_nid_postfix(const char* message) { LibcException::ThrowMessage(_ZTVSt12length_error_nid_postfix, message); }
[[noreturn]] void APS5_VABI _ZSt18_Xinvalid_argumentPKc_nid_postfix(const char* message) { LibcException::ThrowMessage(_ZTVSt16invalid_argument_nid_postfix, message); }
[[noreturn]] void APS5_VABI _ZSt15_Xruntime_errorPKc_nid_postfix(const char* message) { LibcException::ThrowMessage(_ZTVSt13runtime_error_nid_postfix, message); }
[[noreturn]] void APS5_VABI _ZSt13_Xrange_errorPKc_nid_postfix(const char* message) { LibcException::ThrowMessage(_ZTVSt11range_error_nid_postfix, message); }
[[noreturn]] void APS5_VABI _ZSt19_Xbad_function_callv_nid_postfix() { LibcException::ThrowPlain(_ZTVSt17bad_function_call_nid_postfix); }
[[noreturn]] void APS5_VABI _ZSt19_Throw_bad_weak_ptrv_nid_postfix() { LibcException::ThrowPlain(_ZTVSt12bad_weak_ptr_nid_postfix); }
[[noreturn]] void APS5_VABI _ZSt26__throw_bad_variant_accessv_nid_postfix() { LibcException::ThrowPlain(_ZTVSt18bad_variant_access_nid_postfix); }
[[noreturn]] void APS5_VABI _ZSt26_Throw_bad_optional_accessv_nid_postfix() { LibcException::ThrowPlain(_ZTVSt19bad_optional_access_nid_postfix); }
[[noreturn]] void APS5_VABI _ZSt13_Xregex_errorNSt15regex_constants10error_typeE_nid_postfix(std::regex_constants::error_type code) {
    struct RegexObject { LibcException::ExceptionObject base; std::regex_constants::error_type code; };
    auto* object = static_cast<RegexObject*>(__cxa_allocate_exception_nid_postfix(sizeof(RegexObject)));
    LibcException::Construct(&object->base, _ZTVSt11regex_error_nid_postfix, "regular expression error");
    object->code = code;
    __cxa_throw_nid_postfix(object, reinterpret_cast<std::type_info*>(&_ZTISt11regex_error_nid_postfix),
        reinterpret_cast<decltype(LibcException::Header::destructor)>(LibcException::DestroyGuestMessage_nid_no_patch));
}
[[noreturn]] void ThrowCppError_nid_no_patch(int code) {
    LibcException::ThrowCppError(code, _ZTVSt12system_error_nid_postfix);
}
}

namespace LibcException::SceV2 {
static const char* APS5_VABI BadOptionalAccessWhat_nid_no_patch(const ExceptionObject*) { return "Bad optional access"; }
static const char* APS5_VABI BadWeakPtrWhat_nid_no_patch(const ExceptionObject*) { return "bad_weak_ptr"; }
static const char* APS5_VABI BadVariantAccessWhat_nid_no_patch(const ExceptionObject*) { return "bad variant access"; }
static const char* APS5_VABI BadFunctionCallWhat_nid_no_patch(const ExceptionObject*) { return "bad function call"; }
}

extern "C" {
LibcException::TypeRecord _ZTINSt13__sce_cxxvers16_Error_exceptionE_nid_postfix {LibcException::SingleTypeVtable + 2, "NSt13__sce_cxxvers16_Error_exceptionE", &_ZTISt9exception_nid_postfix};
}

namespace LibcException::SceV2 {
constexpr ExceptionVtable MessageTable(const TypeRecord& type) {
    return {0, &type, GuestDestroyMessage_nid_no_patch, GuestDeleteMessage_nid_no_patch, GuestMessageWhat_nid_no_patch};
}
constexpr ExceptionVtable PlainTable(const TypeRecord& type, const char* (APS5_VABI *what)(const ExceptionObject*)) {
    return {0, &type, GuestDestroyPlain_nid_no_patch, GuestDeletePlain_nid_no_patch, what};
}
const ExceptionVtable ErrorException = MessageTable(_ZTINSt13__sce_cxxvers16_Error_exceptionE_nid_postfix);
const TypeRecord LogicErrorType {SingleTypeVtable + 2, "NSt8__sce_v211logic_errorE", &_ZTINSt13__sce_cxxvers16_Error_exceptionE_nid_postfix};
const TypeRecord LengthErrorType {SingleTypeVtable + 2, "NSt8__sce_v212length_errorE", &LogicErrorType};
const TypeRecord OutOfRangeType {SingleTypeVtable + 2, "NSt8__sce_v212out_of_rangeE", &LogicErrorType};
const TypeRecord InvalidArgumentType {SingleTypeVtable + 2, "NSt8__sce_v216invalid_argumentE", &LogicErrorType};
const TypeRecord RuntimeErrorType {SingleTypeVtable + 2, "NSt8__sce_v213runtime_errorE", &_ZTINSt13__sce_cxxvers16_Error_exceptionE_nid_postfix};
const TypeRecord OverflowErrorType {SingleTypeVtable + 2, "NSt8__sce_v214overflow_errorE", &RuntimeErrorType};
const TypeRecord RangeErrorType {SingleTypeVtable + 2, "NSt8__sce_v211range_errorE", &RuntimeErrorType};
const TypeRecord SystemErrorBaseType {SingleTypeVtable + 2, "NSt8__sce_v213_System_errorE", &RuntimeErrorType};
const TypeRecord SystemErrorType {SingleTypeVtable + 2, "NSt8__sce_v212system_errorE", &SystemErrorBaseType};
const TypeRecord IosFailureType {SingleTypeVtable + 2, "NSt8__sce_v28ios_base7failureE", &SystemErrorType};
const TypeRecord BadOptionalAccessType {SingleTypeVtable + 2, "NSt8__sce_v219bad_optional_accessE", &_ZTISt9exception_nid_postfix};
const TypeRecord BadWeakPtrType {SingleTypeVtable + 2, "NSt8__sce_v212bad_weak_ptrE", &_ZTISt9exception_nid_postfix};
const TypeRecord BadVariantAccessType {SingleTypeVtable + 2, "NSt8__sce_v218bad_variant_accessE", &_ZTISt9exception_nid_postfix};
const TypeRecord BadFunctionCallType {SingleTypeVtable + 2, "NSt8__sce_v217bad_function_callE", &_ZTISt9exception_nid_postfix};
const ExceptionVtable LengthError = MessageTable(LengthErrorType);
const ExceptionVtable OutOfRange = MessageTable(OutOfRangeType);
const ExceptionVtable InvalidArgument = MessageTable(InvalidArgumentType);
const ExceptionVtable RuntimeError = MessageTable(RuntimeErrorType);
const ExceptionVtable OverflowError = MessageTable(OverflowErrorType);
const ExceptionVtable RangeError = MessageTable(RangeErrorType);
const ExceptionVtable SystemError = MessageTable(SystemErrorType);
const ExceptionVtable IosFailure = MessageTable(IosFailureType);
const ExceptionVtable BadOptionalAccess = PlainTable(BadOptionalAccessType, BadOptionalAccessWhat_nid_no_patch);
const ExceptionVtable BadWeakPtr = PlainTable(BadWeakPtrType, BadWeakPtrWhat_nid_no_patch);
const ExceptionVtable BadVariantAccess = PlainTable(BadVariantAccessType, BadVariantAccessWhat_nid_no_patch);
const ExceptionVtable BadFunctionCall = PlainTable(BadFunctionCallType, BadFunctionCallWhat_nid_no_patch);

[[noreturn]] void ThrowCopy(const ExceptionObject* source, const ExceptionVtable& table) {
    auto* object = static_cast<ExceptionObject*>(__cxa_allocate_exception_nid_postfix(sizeof(ExceptionObject)));
    Copy(object, source, table);
    __cxa_throw_nid_postfix(object, reinterpret_cast<std::type_info*>(const_cast<TypeRecord*>(table.type)),
        reinterpret_cast<decltype(Header::destructor)>(DestroyGuestMessage_nid_no_patch));
}
[[noreturn]] void ThrowSystemCopy(const SystemErrorObject* source, const ExceptionVtable& table) {
    auto* object = static_cast<SystemErrorObject*>(__cxa_allocate_exception_nid_postfix(sizeof(SystemErrorObject)));
    Copy(&object->base, &source->base, table);
    object->code = source->code;
    __cxa_throw_nid_postfix(object, reinterpret_cast<std::type_info*>(const_cast<TypeRecord*>(table.type)),
        reinterpret_cast<decltype(Header::destructor)>(DestroyGuestMessage_nid_no_patch));
}
}

extern "C" {
void APS5_VABI _ZNSt13__sce_cxxvers16_Error_exceptionC1EPKc_nid_postfix(LibcException::ExceptionObject* self, const char* message) { LibcException::Construct(self, LibcException::SceV2::ErrorException, message); }
void APS5_VABI _ZNSt13__sce_cxxvers16_Error_exceptionC2EPKc_nid_postfix(LibcException::ExceptionObject* self, const char* message) { LibcException::Construct(self, LibcException::SceV2::ErrorException, message); }
void APS5_VABI _ZNSt13__sce_cxxvers16_Error_exceptionC1ERKS0__nid_postfix(LibcException::ExceptionObject* self, const LibcException::ExceptionObject* source) { LibcException::Copy(self, source, LibcException::SceV2::ErrorException); }
void APS5_VABI _ZNSt13__sce_cxxvers16_Error_exceptionC2ERKS0__nid_postfix(LibcException::ExceptionObject* self, const LibcException::ExceptionObject* source) { LibcException::Copy(self, source, LibcException::SceV2::ErrorException); }
void APS5_VABI _ZNSt13__sce_cxxvers16_Error_exceptionD1Ev_nid_postfix(LibcException::ExceptionObject* self) { LibcException::DestroyMessage(self); }
void APS5_VABI _ZNSt13__sce_cxxvers16_Error_exceptionD2Ev_nid_postfix(LibcException::ExceptionObject* self) { LibcException::DestroyMessage(self); }
void APS5_VABI _ZNSt13__sce_cxxvers16_Error_exceptionD0Ev_nid_postfix(LibcException::ExceptionObject* self) { LibcException::DeleteMessage(self); }
const char* APS5_VABI _ZNKSt13__sce_cxxvers16_Error_exception4whatEv_nid_postfix(const LibcException::ExceptionObject* self) { return LibcException::MessageWhat(self); }

[[noreturn]] void APS5_VABI _ZNSt8__sce_v214_Xlength_errorEPKc_nid_postfix(const char* message) { LibcException::ThrowMessage(LibcException::SceV2::LengthError, message); }
[[noreturn]] void APS5_VABI _ZNSt8__sce_v214_Xout_of_rangeEPKc_nid_postfix(const char* message) { LibcException::ThrowMessage(LibcException::SceV2::OutOfRange, message); }
[[noreturn]] void APS5_VABI _ZNSt8__sce_v218_Xinvalid_argumentEPKc_nid_postfix(const char* message) { LibcException::ThrowMessage(LibcException::SceV2::InvalidArgument, message); }
[[noreturn]] void APS5_VABI _ZNSt8__sce_v215_Xruntime_errorEPKc_nid_postfix(const char* message) { LibcException::ThrowMessage(LibcException::SceV2::RuntimeError, message); }
[[noreturn]] void APS5_VABI _ZNSt8__sce_v216_Xoverflow_errorEPKc_nid_postfix(const char* message) { LibcException::ThrowMessage(LibcException::SceV2::OverflowError, message); }
[[noreturn]] void APS5_VABI _ZNSt8__sce_v211_Xbad_allocEv_nid_postfix() { LibcException::ThrowPlain(_ZTVSt9bad_alloc_nid_postfix); }
[[noreturn]] void APS5_VABI _ZNSt8__sce_v219_Xbad_function_callEv_nid_postfix() { LibcException::ThrowPlain(LibcException::SceV2::BadFunctionCall); }
[[noreturn]] void APS5_VABI _ZNSt8__sce_v26_ThrowERKNS_19bad_optional_accessE_nid_postfix(const LibcException::ExceptionObject*) { LibcException::ThrowPlain(LibcException::SceV2::BadOptionalAccess); }
[[noreturn]] void APS5_VABI _ZNSt8__sce_v26_ThrowERKNS_12bad_weak_ptrE_nid_postfix(const LibcException::ExceptionObject*) { LibcException::ThrowPlain(LibcException::SceV2::BadWeakPtr); }
[[noreturn]] void APS5_VABI _ZNSt8__sce_v26_ThrowERKNS_18bad_variant_accessE_nid_postfix(const LibcException::ExceptionObject*) { LibcException::ThrowPlain(LibcException::SceV2::BadVariantAccess); }
[[noreturn]] void APS5_VABI _ZNSt8__sce_v26_ThrowERKNS_11range_errorE_nid_postfix(const LibcException::ExceptionObject* error) { LibcException::SceV2::ThrowCopy(error, LibcException::SceV2::RangeError); }
[[noreturn]] void APS5_VABI _ZNSt8__sce_v26_ThrowERKNS_12system_errorE_nid_postfix(const LibcException::SystemErrorObject* error) { LibcException::SceV2::ThrowSystemCopy(error, LibcException::SceV2::SystemError); }
[[noreturn]] void APS5_VABI _ZNSt8__sce_v28ios_base6_ThrowERKNS0_7failureE_nid_postfix(const LibcException::SystemErrorObject* failure) { LibcException::SceV2::ThrowSystemCopy(failure, LibcException::SceV2::IosFailure); }
[[noreturn]] void APS5_VABI _ZNSt8__sce_v216_Throw_Cpp_errorEi_nid_postfix(int code) { LibcException::ThrowCppError(code, LibcException::SceV2::SystemError); }

std::int64_t APS5_VABI _ZNSt8__sce_v216_Xtime_get_ticksEv_nid_postfix() { return _Xtime_get_ticks_nid_postfix(); }
const char* APS5_VABI _ZNSt8__sce_v213_Syserror_mapEi_nid_postfix(int value) {
    if (value == 0) return "success";
    const char* message = LibcException::GenericErrorMessage(value);
    return message ? message : "unknown error";
}
LibcException::ClassTypeRecord _ZTISt14error_category_nid_postfix {LibcException::ClassTypeVtable + 2, "St14error_category"};
LibcException::TypeRecord _ZTISt22_Future_error_category_nid_postfix {LibcException::SingleTypeVtable + 2, "St22_Future_error_category", &LibcException::GenericCategoryType};
static void APS5_VABI FutureCategoryDelete_nid_no_patch(LibcException::CategoryObject*) {
    throw std::invalid_argument("std::error_category: the future category object is not heap allocated");
}
extern const void* const _ZTVSt22_Future_error_category_nid_postfix[] {
    nullptr, &_ZTISt22_Future_error_category_nid_postfix,
    reinterpret_cast<const void*>(LibcException::CategoryDestroy_nid_no_patch), reinterpret_cast<const void*>(FutureCategoryDelete_nid_no_patch),
    reinterpret_cast<const void*>(LibcException::FutureCategoryName_nid_no_patch), reinterpret_cast<const void*>(LibcException::FutureCategoryMessage_nid_no_patch),
    reinterpret_cast<const void*>(LibcException::CategoryCondition_nid_no_patch),
    reinterpret_cast<const void*>(LibcException::CategoryEquivalentCondition_nid_no_patch),
    reinterpret_cast<const void*>(LibcException::CategoryEquivalentCode_nid_no_patch)
};
static const LibcException::CategoryObject FutureCategory {_ZTVSt22_Future_error_category_nid_postfix + 2};
const LibcException::CategoryObject* APS5_VABI _ZSt15future_categoryv_nid_postfix() {
    return &FutureCategory;
}
LibcException::ErrorCode APS5_VABI _ZNKSt14error_category23default_error_conditionEi_nid_postfix(const LibcException::CategoryObject* self, int value) {
    return LibcException::CategoryCondition_nid_no_patch(self, value);
}
bool APS5_VABI _ZNKSt14error_category10equivalentEiRKSt15error_condition_nid_postfix(const LibcException::CategoryObject* self, int value, const LibcException::ErrorCode* condition) {
    return LibcException::CategoryEquivalentCondition_nid_no_patch(self, value, condition);
}
bool APS5_VABI _ZNKSt14error_category10equivalentERKSt10error_codei_nid_postfix(const LibcException::CategoryObject* self, const LibcException::ErrorCode* code, int value) {
    return LibcException::CategoryEquivalentCode_nid_no_patch(self, code, value);
}
void APS5_VABI _ZNSt14error_categoryD2Ev_nid_postfix(LibcException::CategoryObject*) {}
LibcException::TypeRecord _ZTISt12future_error_nid_postfix {LibcException::SingleTypeVtable + 2, "St12future_error", &_ZTISt11logic_error_nid_postfix};
static void DestroyFutureErrorString(GuestLocale::String* message) {
    if (message->capacity > 15) _ZdlPv_nid_postfix(message->pointer);
    message->reserved = 0;
    message->size = 0;
    message->capacity = 15;
    std::memset(message->buffer, 0, sizeof(message->buffer));
}
static void APS5_VABI DestroyFutureError_nid_no_patch(LibcException::FutureErrorObject* self) {
    DestroyFutureErrorString(&self->message);
}
static void APS5_VABI DeleteFutureError_nid_no_patch(LibcException::FutureErrorObject* self) {
    DestroyFutureErrorString(&self->message);
    std::free(self);
}
static const char* APS5_VABI FutureErrorWhat_nid_no_patch(const LibcException::FutureErrorObject* self) {
    return LibcException::FutureErrorMessage(self->code.value);
}
static void CopyFutureError(LibcException::FutureErrorObject* self, const LibcException::FutureErrorObject* source) {
    *self = *source;
    if (source->message.capacity > 15) {
        const auto capacity = source->message.capacity;
        auto* storage = static_cast<char*>(_Znwm_nid_postfix(capacity + 1));
        std::memcpy(storage, source->message.pointer, source->message.size + 1);
        self->message.pointer = storage;
    }
}
[[noreturn]] static void APS5_VABI RaiseFutureError_nid_no_patch(const LibcException::FutureErrorObject* self) {
    auto* object = static_cast<LibcException::FutureErrorObject*>(__cxa_allocate_exception_nid_postfix(sizeof(LibcException::FutureErrorObject)));
    try {
        CopyFutureError(object, self);
    } catch (...) {
        __cxa_free_exception_nid_postfix(object);
        throw;
    }
    __cxa_throw_nid_postfix(object, reinterpret_cast<std::type_info*>(&_ZTISt12future_error_nid_postfix),
        reinterpret_cast<decltype(LibcException::Header::destructor)>(DestroyFutureError_nid_no_patch));
}
LibcException::FutureErrorVtable _ZTVSt12future_error_nid_postfix {
    0, &_ZTISt12future_error_nid_postfix,
    DestroyFutureError_nid_no_patch, DeleteFutureError_nid_no_patch, FutureErrorWhat_nid_no_patch, RaiseFutureError_nid_no_patch
};
[[noreturn]] void APS5_VABI _ZSt19_Throw_future_errorRKSt10error_code_nid_postfix(const LibcException::ErrorCode* code) {
    auto* object = static_cast<LibcException::FutureErrorObject*>(__cxa_allocate_exception_nid_postfix(sizeof(LibcException::FutureErrorObject)));
    object->vtable = &_ZTVSt12future_error_nid_postfix.destroy;
    object->message = {};
    object->message.capacity = 15;
    object->code = *code;
    __cxa_throw_nid_postfix(object, reinterpret_cast<std::type_info*>(&_ZTISt12future_error_nid_postfix),
        reinterpret_cast<decltype(LibcException::Header::destructor)>(DestroyFutureError_nid_no_patch));
}
[[noreturn]] void APS5_VABI _ZSt25_Rethrow_future_exceptionSt13exception_ptr_nid_postfix(GuestExceptionPointerArgument exception) {
    _ZSt17rethrow_exceptionNSt15__exception_ptr13exception_ptrE_nid_postfix(exception);
}
void APS5_VABI _ZNSt12future_errorD1Ev_nid_postfix(LibcException::FutureErrorObject* self) {
    DestroyFutureErrorString(&self->message);
}

}
