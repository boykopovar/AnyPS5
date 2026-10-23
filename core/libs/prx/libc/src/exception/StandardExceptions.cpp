#include "prx/libc/include/exceptions/Runtime.hpp"
#include "prx/libc/include/GuestLocale.hpp"
#include <cstdio>
#include <iterator>
#include <limits>
#include <regex>
#include <stdexcept>
#include <string>

extern "C" void* APS5_VABI _Znwm_nid_postfix(std::size_t size);

namespace LibcException {
struct TypeRecord { const void* const* vtable; const char* name; const TypeRecord* base; };
struct ExceptionObject { const void* vtable; const char* message; };
struct ExceptionVtable {
    std::ptrdiff_t offset;
    const TypeRecord* type;
    void (APS5_VABI *destroy)(ExceptionObject*);
    void (APS5_VABI *deleteObject)(ExceptionObject*);
    const char* (APS5_VABI *what)(const ExceptionObject*);
};
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

struct CategoryObject { const void* vtable; };
struct ErrorCode {
    std::int32_t value;
    std::int32_t value_high;
    const CategoryObject* category;
};
struct SystemErrorObject { ExceptionObject base; ErrorCode code; };
static_assert(sizeof(ErrorCode) == 16 && sizeof(SystemErrorObject) == 32 && offsetof(SystemErrorObject, code) == 16);
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
static void APS5_VABI GenericCategoryDestroy_nid_no_patch(CategoryObject*) {}
static void APS5_VABI GenericCategoryDelete_nid_no_patch(CategoryObject*) {
    throw std::invalid_argument("std::_Generic_error_category: the generic category object is not heap allocated");
}
static const char* APS5_VABI GenericCategoryName_nid_no_patch(const CategoryObject*) { return "generic"; }
static GuestLocale::String* APS5_VABI GenericCategoryMessage_nid_no_patch(GuestLocale::String* result, const CategoryObject*, int value) {
    if (!result) throw std::invalid_argument("std::_Generic_error_category::message: null result");
    const char* message = GenericErrorMessage(value);
    if (!message) message = "unknown error";
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
static ErrorCode APS5_VABI GenericCategoryCondition_nid_no_patch(const CategoryObject* self, int value) {
    return {value, value, self};
}
static bool APS5_VABI GenericCategoryEquivalentCondition_nid_no_patch(const CategoryObject* self, int value, const ErrorCode* condition) {
    if (!condition) return false;
    return condition->category == self && (condition->value == value || condition->value_high == value);
}
static bool APS5_VABI GenericCategoryEquivalentCode_nid_no_patch(const CategoryObject* self, const ErrorCode* code, int value) {
    if (!code) return false;
    return code->category == self && (code->value == value || code->value_high == value);
}
const void* const GenericCategoryVtable[] {
    nullptr, &GenericCategoryType,
    reinterpret_cast<const void*>(GenericCategoryDestroy_nid_no_patch), reinterpret_cast<const void*>(GenericCategoryDelete_nid_no_patch),
    reinterpret_cast<const void*>(GenericCategoryName_nid_no_patch), reinterpret_cast<const void*>(GenericCategoryMessage_nid_no_patch),
    reinterpret_cast<const void*>(GenericCategoryCondition_nid_no_patch),
    reinterpret_cast<const void*>(GenericCategoryEquivalentCondition_nid_no_patch),
    reinterpret_cast<const void*>(GenericCategoryEquivalentCode_nid_no_patch)
};
const CategoryObject GenericCategory {GenericCategoryVtable + 2};

const TypeRecord SystemCategoryType {SingleTypeVtable + 2, "St22_System_error_category", &ErrorCategoryType};
static void APS5_VABI SystemCategoryDestroy_nid_no_patch(CategoryObject*) {}
static void APS5_VABI SystemCategoryDelete_nid_no_patch(CategoryObject*) {
    throw std::invalid_argument("std::_System_error_category: the system category object is not heap allocated");
}
static const char* APS5_VABI SystemCategoryName_nid_no_patch(const CategoryObject*) { return "system"; }
static GuestLocale::String* APS5_VABI SystemCategoryMessage_nid_no_patch(GuestLocale::String* result, const CategoryObject* self, int value) {
    return GenericCategoryMessage_nid_no_patch(result, self, value);
}
static ErrorCode APS5_VABI SystemCategoryCondition_nid_no_patch(const CategoryObject* self, int value) {
    if (GenericErrorMessage(value) != nullptr) {
        return {value, value, &GenericCategory};
    }
    return {value, value, self};
}
static bool APS5_VABI SystemCategoryEquivalentCondition_nid_no_patch(const CategoryObject* self, int value, const ErrorCode* condition) {
    if (!condition) return false;
    const CategoryObject* expected_cat = (GenericErrorMessage(value) != nullptr) ? &GenericCategory : self;
    return (condition->category == expected_cat || condition->category == self) &&
           (condition->value == value || condition->value_high == value);
}
static bool APS5_VABI SystemCategoryEquivalentCode_nid_no_patch(const CategoryObject* self, const ErrorCode* code, int value) {
    if (!code) return false;
    return code->category == self && (code->value == value || code->value_high == value);
}
const void* const SystemCategoryVtable[] {
    nullptr, &SystemCategoryType,
    reinterpret_cast<const void*>(SystemCategoryDestroy_nid_no_patch), reinterpret_cast<const void*>(SystemCategoryDelete_nid_no_patch),
    reinterpret_cast<const void*>(SystemCategoryName_nid_no_patch), reinterpret_cast<const void*>(SystemCategoryMessage_nid_no_patch),
    reinterpret_cast<const void*>(SystemCategoryCondition_nid_no_patch),
    reinterpret_cast<const void*>(SystemCategoryEquivalentCondition_nid_no_patch),
    reinterpret_cast<const void*>(SystemCategoryEquivalentCode_nid_no_patch)
};
const CategoryObject SystemCategory {SystemCategoryVtable + 2};

const TypeRecord FutureCategoryType {SingleTypeVtable + 2, "St22_Future_error_category", &ErrorCategoryType};
static void APS5_VABI FutureCategoryDestroy_nid_no_patch(CategoryObject*) {}
static void APS5_VABI FutureCategoryDelete_nid_no_patch(CategoryObject*) {
    throw std::invalid_argument("std::_Future_error_category: the future category object is not heap allocated");
}
static const char* APS5_VABI FutureCategoryName_nid_no_patch(const CategoryObject*) { return "future"; }
static GuestLocale::String* APS5_VABI FutureCategoryMessage_nid_no_patch(GuestLocale::String* result, const CategoryObject*, int value) {
    if (!result) throw std::invalid_argument("std::_Future_error_category::message: null result");
    const char* message = "future error";
    if (value == 1) message = "broken promise";
    else if (value == 2) message = "future already retrieved";
    else if (value == 3) message = "promise already satisfied";
    else if (value == 4) message = "no state";
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
static ErrorCode APS5_VABI FutureCategoryCondition_nid_no_patch(const CategoryObject* self, int value) {
    return {value, value, self};
}
static bool APS5_VABI FutureCategoryEquivalentCondition_nid_no_patch(const CategoryObject* self, int value, const ErrorCode* condition) {
    if (!condition) return false;
    return condition->category == self && (condition->value == value || condition->value_high == value);
}
static bool APS5_VABI FutureCategoryEquivalentCode_nid_no_patch(const CategoryObject* self, const ErrorCode* code, int value) {
    if (!code) return false;
    return code->category == self && (code->value == value || code->value_high == value);
}
const void* const FutureCategoryVtable[] {
    nullptr, &FutureCategoryType,
    reinterpret_cast<const void*>(FutureCategoryDestroy_nid_no_patch), reinterpret_cast<const void*>(FutureCategoryDelete_nid_no_patch),
    reinterpret_cast<const void*>(FutureCategoryName_nid_no_patch), reinterpret_cast<const void*>(FutureCategoryMessage_nid_no_patch),
    reinterpret_cast<const void*>(FutureCategoryCondition_nid_no_patch),
    reinterpret_cast<const void*>(FutureCategoryEquivalentCondition_nid_no_patch),
    reinterpret_cast<const void*>(FutureCategoryEquivalentCode_nid_no_patch)
};
const CategoryObject FutureCategory {FutureCategoryVtable + 2};
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
const LibcException::CategoryObject* APS5_VABI _ZSt15future_categoryv_nid_postfix() {
    return &LibcException::FutureCategory;
}
void APS5_VABI _ZNSt14error_categoryD2Ev_nid_postfix(LibcException::CategoryObject*) {}
void APS5_VABI _ZNSt14error_categoryD1Ev_nid_postfix(LibcException::CategoryObject*) {}
void APS5_VABI _ZNSt14error_categoryD0Ev_nid_postfix(LibcException::CategoryObject*) {}
LibcException::ErrorCode APS5_VABI _ZNKSt14error_category23default_error_conditionEi_nid_postfix(
    const LibcException::CategoryObject* self, int value) {
    return {value, value, self};
}
bool APS5_VABI _ZNKSt14error_category10equivalentEiRKSt15error_condition_nid_postfix(
    const LibcException::CategoryObject* self, int value, const LibcException::ErrorCode* condition) {
    if (!self || !condition) return false;
    auto cond = reinterpret_cast<LibcException::ErrorCode (APS5_VABI *)(const LibcException::CategoryObject*, int)>(
        reinterpret_cast<const void* const*>(self->vtable)[4])(self, value);
    return cond.category == condition->category && (cond.value == condition->value || cond.value_high == condition->value_high);
}
bool APS5_VABI _ZNKSt14error_category10equivalentERKSt10error_codei_nid_postfix(
    const LibcException::CategoryObject* self, const LibcException::ErrorCode* code, int value) {
    if (!code) return false;
    return code->category == self && (code->value == value || code->value_high == value);
}
GuestLocale::String* APS5_VABI _ZNKSt23_Generic_error_category7messageEi_nid_postfix(
    GuestLocale::String* result, const LibcException::CategoryObject* self, int value) {
    return LibcException::GenericCategoryMessage_nid_no_patch(result, self, value);
}
GuestLocale::String* APS5_VABI _ZNKSt22_System_error_category7messageEi_nid_postfix(
    GuestLocale::String* result, const LibcException::CategoryObject* self, int value) {
    return LibcException::SystemCategoryMessage_nid_no_patch(result, self, value);
}
const char* APS5_VABI _ZNKSt22_System_error_category4nameEv_nid_postfix(const LibcException::CategoryObject* self) {
    return LibcException::SystemCategoryName_nid_no_patch(self);
}
LibcException::ErrorCode APS5_VABI _ZNKSt22_System_error_category23default_error_conditionEi_nid_postfix(
    const LibcException::CategoryObject* self, int value) {
    return LibcException::SystemCategoryCondition_nid_no_patch(self, value);
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
[[noreturn]] void APS5_VABI _ZNSt8__sce_v219_Xbad_function_callEv_nid_postfix() { LibcException::ThrowPlain(_ZTVSt17bad_function_call_nid_postfix); }
[[noreturn]] void APS5_VABI _ZSt19_Throw_bad_weak_ptrv_nid_postfix() { LibcException::ThrowPlain(_ZTVSt12bad_weak_ptr_nid_postfix); }
[[noreturn]] void APS5_VABI _ZSt13_Xregex_errorNSt15regex_constants10error_typeE_nid_postfix(std::regex_constants::error_type code) {
    struct RegexObject { LibcException::ExceptionObject base; std::regex_constants::error_type code; };
    auto* object = static_cast<RegexObject*>(__cxa_allocate_exception_nid_postfix(sizeof(RegexObject)));
    LibcException::Construct(&object->base, _ZTVSt11regex_error_nid_postfix, "regular expression error");
    object->code = code;
    __cxa_throw_nid_postfix(object, reinterpret_cast<std::type_info*>(&_ZTISt11regex_error_nid_postfix),
        reinterpret_cast<decltype(LibcException::Header::destructor)>(LibcException::DestroyGuestMessage_nid_no_patch));
}
[[noreturn]] void ThrowCppError_nid_no_patch(int code) {
    static constexpr LibcException::CppError errors[] {
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
    std::snprintf(message, sizeof(message), "%s: %s", error.message, LibcException::GenericErrorMessage(error.value));
    auto* object = static_cast<LibcException::SystemErrorObject*>(__cxa_allocate_exception_nid_postfix(sizeof(LibcException::SystemErrorObject)));
    LibcException::Construct(&object->base, _ZTVSt12system_error_nid_postfix, message);
    object->code = {error.value, error.value, &LibcException::GenericCategory};
    __cxa_throw_nid_postfix(object, reinterpret_cast<std::type_info*>(&_ZTISt12system_error_nid_postfix),
        reinterpret_cast<decltype(LibcException::Header::destructor)>(LibcException::DestroyGuestMessage_nid_no_patch));
}
}
