#include "Runtime.hpp"
#include <limits>
#include <regex>

namespace LibcException {
struct TypeRecord { const void* const* vtable; const char* name; const TypeRecord* base; };
struct ExceptionObject { const void* vtable; const char* message; };
struct ExceptionVtable {
    std::ptrdiff_t offset;
    const TypeRecord* type;
    void (*destroy)(ExceptionObject*);
    void (*deleteObject)(ExceptionObject*);
    const char* (*what)(const ExceptionObject*);
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
void DeleteMessage(ExceptionObject* object) { DestroyMessage(object); std::free(object); }
const char* PlainWhat(const ExceptionObject* object) {
    auto* table = static_cast<const ExceptionVtable*>(object->vtable) - 0;
    auto* complete = reinterpret_cast<const ExceptionVtable*>(reinterpret_cast<const unsigned char*>(table) - offsetof(ExceptionVtable, destroy));
    return complete->type->name;
}
const char* MessageWhat(const ExceptionObject* object) { return object->message ? object->message : ""; }
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
    __cxa_throw_nid_postfix(object, reinterpret_cast<std::type_info*>(const_cast<TypeRecord*>(table.type)), [](void* p) { DestroyMessage(static_cast<ExceptionObject*>(p)); });
}
[[noreturn]] void ThrowPlain(const ExceptionVtable& table) {
    auto* object = static_cast<ExceptionObject*>(__cxa_allocate_exception_nid_postfix(sizeof(void*)));
    object->vtable = &table.destroy;
    __cxa_throw_nid_postfix(object, reinterpret_cast<std::type_info*>(const_cast<TypeRecord*>(table.type)), [](void* p) { DestroyPlain(static_cast<ExceptionObject*>(p)); });
}
}

extern "C" {
LibcException::TypeRecord _ZTISt9exception_nid_postfix {LibcException::ClassTypeVtable + 2, "St9exception", nullptr};
LibcException::ExceptionVtable _ZTVSt9exception_nid_postfix {0, &_ZTISt9exception_nid_postfix, LibcException::DestroyPlain, LibcException::DeletePlain, LibcException::PlainWhat};
void _ZNSt9exceptionD1Ev_nid_postfix(LibcException::ExceptionObject* self) { LibcException::DestroyPlain(self); }
void _ZNSt9exceptionD2Ev_nid_postfix(LibcException::ExceptionObject* self) { LibcException::DestroyPlain(self); }
void _ZNSt9exceptionD0Ev_nid_postfix(LibcException::ExceptionObject* self) { LibcException::DeletePlain(self); }
const char* _ZNKSt9exception4whatEv_nid_postfix(const LibcException::ExceptionObject* self) { return LibcException::PlainWhat(self); }
void _ZNSt9exceptionC1Ev_nid_postfix(LibcException::ExceptionObject* self) { self->vtable = &_ZTVSt9exception_nid_postfix.destroy; }
void _ZNSt9exceptionC2Ev_nid_postfix(LibcException::ExceptionObject* self) { self->vtable = &_ZTVSt9exception_nid_postfix.destroy; }

LibcException::TypeRecord _ZTISt8bad_cast_nid_postfix {LibcException::SingleTypeVtable + 2, "St8bad_cast", &_ZTISt9exception_nid_postfix};
LibcException::ExceptionVtable _ZTVSt8bad_cast_nid_postfix {0, &_ZTISt8bad_cast_nid_postfix, LibcException::DestroyPlain, LibcException::DeletePlain, LibcException::PlainWhat};
void _ZNSt8bad_castD1Ev_nid_postfix(LibcException::ExceptionObject* self) { LibcException::DestroyPlain(self); }
void _ZNSt8bad_castD2Ev_nid_postfix(LibcException::ExceptionObject* self) { LibcException::DestroyPlain(self); }
void _ZNSt8bad_castD0Ev_nid_postfix(LibcException::ExceptionObject* self) { LibcException::DeletePlain(self); }
const char* _ZNKSt8bad_cast4whatEv_nid_postfix(const LibcException::ExceptionObject* self) { return LibcException::PlainWhat(self); }
void _ZNSt8bad_castC1Ev_nid_postfix(LibcException::ExceptionObject* self) { self->vtable = &_ZTVSt8bad_cast_nid_postfix.destroy; }
void _ZNSt8bad_castC2Ev_nid_postfix(LibcException::ExceptionObject* self) { self->vtable = &_ZTVSt8bad_cast_nid_postfix.destroy; }

LibcException::TypeRecord _ZTISt10bad_typeid_nid_postfix {LibcException::SingleTypeVtable + 2, "St10bad_typeid", &_ZTISt9exception_nid_postfix};
LibcException::ExceptionVtable _ZTVSt10bad_typeid_nid_postfix {0, &_ZTISt10bad_typeid_nid_postfix, LibcException::DestroyPlain, LibcException::DeletePlain, LibcException::PlainWhat};
void _ZNSt10bad_typeidD1Ev_nid_postfix(LibcException::ExceptionObject* self) { LibcException::DestroyPlain(self); }
void _ZNSt10bad_typeidD2Ev_nid_postfix(LibcException::ExceptionObject* self) { LibcException::DestroyPlain(self); }
void _ZNSt10bad_typeidD0Ev_nid_postfix(LibcException::ExceptionObject* self) { LibcException::DeletePlain(self); }
const char* _ZNKSt10bad_typeid4whatEv_nid_postfix(const LibcException::ExceptionObject* self) { return LibcException::PlainWhat(self); }
void _ZNSt10bad_typeidC1Ev_nid_postfix(LibcException::ExceptionObject* self) { self->vtable = &_ZTVSt10bad_typeid_nid_postfix.destroy; }
void _ZNSt10bad_typeidC2Ev_nid_postfix(LibcException::ExceptionObject* self) { self->vtable = &_ZTVSt10bad_typeid_nid_postfix.destroy; }

LibcException::TypeRecord _ZTISt9bad_alloc_nid_postfix {LibcException::SingleTypeVtable + 2, "St9bad_alloc", &_ZTISt9exception_nid_postfix};
LibcException::ExceptionVtable _ZTVSt9bad_alloc_nid_postfix {0, &_ZTISt9bad_alloc_nid_postfix, LibcException::DestroyPlain, LibcException::DeletePlain, LibcException::PlainWhat};
void _ZNSt9bad_allocD1Ev_nid_postfix(LibcException::ExceptionObject* self) { LibcException::DestroyPlain(self); }
void _ZNSt9bad_allocD2Ev_nid_postfix(LibcException::ExceptionObject* self) { LibcException::DestroyPlain(self); }
void _ZNSt9bad_allocD0Ev_nid_postfix(LibcException::ExceptionObject* self) { LibcException::DeletePlain(self); }
const char* _ZNKSt9bad_alloc4whatEv_nid_postfix(const LibcException::ExceptionObject* self) { return LibcException::PlainWhat(self); }
void _ZNSt9bad_allocC1Ev_nid_postfix(LibcException::ExceptionObject* self) { self->vtable = &_ZTVSt9bad_alloc_nid_postfix.destroy; }
void _ZNSt9bad_allocC2Ev_nid_postfix(LibcException::ExceptionObject* self) { self->vtable = &_ZTVSt9bad_alloc_nid_postfix.destroy; }

LibcException::TypeRecord _ZTISt20bad_array_new_length_nid_postfix {LibcException::SingleTypeVtable + 2, "St20bad_array_new_length", &_ZTISt9bad_alloc_nid_postfix};
LibcException::ExceptionVtable _ZTVSt20bad_array_new_length_nid_postfix {0, &_ZTISt20bad_array_new_length_nid_postfix, LibcException::DestroyPlain, LibcException::DeletePlain, LibcException::PlainWhat};
void _ZNSt20bad_array_new_lengthD1Ev_nid_postfix(LibcException::ExceptionObject* self) { LibcException::DestroyPlain(self); }
void _ZNSt20bad_array_new_lengthD2Ev_nid_postfix(LibcException::ExceptionObject* self) { LibcException::DestroyPlain(self); }
void _ZNSt20bad_array_new_lengthD0Ev_nid_postfix(LibcException::ExceptionObject* self) { LibcException::DeletePlain(self); }
const char* _ZNKSt20bad_array_new_length4whatEv_nid_postfix(const LibcException::ExceptionObject* self) { return LibcException::PlainWhat(self); }
void _ZNSt20bad_array_new_lengthC1Ev_nid_postfix(LibcException::ExceptionObject* self) { self->vtable = &_ZTVSt20bad_array_new_length_nid_postfix.destroy; }
void _ZNSt20bad_array_new_lengthC2Ev_nid_postfix(LibcException::ExceptionObject* self) { self->vtable = &_ZTVSt20bad_array_new_length_nid_postfix.destroy; }

LibcException::TypeRecord _ZTISt17bad_function_call_nid_postfix {LibcException::SingleTypeVtable + 2, "St17bad_function_call", &_ZTISt9exception_nid_postfix};
LibcException::ExceptionVtable _ZTVSt17bad_function_call_nid_postfix {0, &_ZTISt17bad_function_call_nid_postfix, LibcException::DestroyPlain, LibcException::DeletePlain, LibcException::PlainWhat};
void _ZNSt17bad_function_callD1Ev_nid_postfix(LibcException::ExceptionObject* self) { LibcException::DestroyPlain(self); }
void _ZNSt17bad_function_callD2Ev_nid_postfix(LibcException::ExceptionObject* self) { LibcException::DestroyPlain(self); }
void _ZNSt17bad_function_callD0Ev_nid_postfix(LibcException::ExceptionObject* self) { LibcException::DeletePlain(self); }
const char* _ZNKSt17bad_function_call4whatEv_nid_postfix(const LibcException::ExceptionObject* self) { return LibcException::PlainWhat(self); }
void _ZNSt17bad_function_callC1Ev_nid_postfix(LibcException::ExceptionObject* self) { self->vtable = &_ZTVSt17bad_function_call_nid_postfix.destroy; }
void _ZNSt17bad_function_callC2Ev_nid_postfix(LibcException::ExceptionObject* self) { self->vtable = &_ZTVSt17bad_function_call_nid_postfix.destroy; }

LibcException::TypeRecord _ZTISt11logic_error_nid_postfix {LibcException::SingleTypeVtable + 2, "St11logic_error", &_ZTISt9exception_nid_postfix};
LibcException::ExceptionVtable _ZTVSt11logic_error_nid_postfix {0, &_ZTISt11logic_error_nid_postfix, LibcException::DestroyMessage, LibcException::DeleteMessage, LibcException::MessageWhat};
void _ZNSt11logic_errorD1Ev_nid_postfix(LibcException::ExceptionObject* self) { LibcException::DestroyMessage(self); }
void _ZNSt11logic_errorD2Ev_nid_postfix(LibcException::ExceptionObject* self) { LibcException::DestroyMessage(self); }
void _ZNSt11logic_errorD0Ev_nid_postfix(LibcException::ExceptionObject* self) { LibcException::DeleteMessage(self); }
const char* _ZNKSt11logic_error4whatEv_nid_postfix(const LibcException::ExceptionObject* self) { return LibcException::MessageWhat(self); }
void _ZNSt11logic_errorC1EPKc_nid_postfix(LibcException::ExceptionObject* self, const char* message) { LibcException::Construct(self, _ZTVSt11logic_error_nid_postfix, message); }
void _ZNSt11logic_errorC2EPKc_nid_postfix(LibcException::ExceptionObject* self, const char* message) { LibcException::Construct(self, _ZTVSt11logic_error_nid_postfix, message); }
void _ZNSt11logic_errorC1ERKS__nid_postfix(LibcException::ExceptionObject* self, const LibcException::ExceptionObject* source) { LibcException::Copy(self, source, _ZTVSt11logic_error_nid_postfix); }
void _ZNSt11logic_errorC2ERKS__nid_postfix(LibcException::ExceptionObject* self, const LibcException::ExceptionObject* source) { LibcException::Copy(self, source, _ZTVSt11logic_error_nid_postfix); }
LibcException::ExceptionObject* _ZNSt11logic_erroraSERKS__nid_postfix(LibcException::ExceptionObject* self, const LibcException::ExceptionObject* source) { return LibcException::Assign(self, source); }

LibcException::TypeRecord _ZTISt13runtime_error_nid_postfix {LibcException::SingleTypeVtable + 2, "St13runtime_error", &_ZTISt9exception_nid_postfix};
LibcException::ExceptionVtable _ZTVSt13runtime_error_nid_postfix {0, &_ZTISt13runtime_error_nid_postfix, LibcException::DestroyMessage, LibcException::DeleteMessage, LibcException::MessageWhat};
void _ZNSt13runtime_errorD1Ev_nid_postfix(LibcException::ExceptionObject* self) { LibcException::DestroyMessage(self); }
void _ZNSt13runtime_errorD2Ev_nid_postfix(LibcException::ExceptionObject* self) { LibcException::DestroyMessage(self); }
void _ZNSt13runtime_errorD0Ev_nid_postfix(LibcException::ExceptionObject* self) { LibcException::DeleteMessage(self); }
const char* _ZNKSt13runtime_error4whatEv_nid_postfix(const LibcException::ExceptionObject* self) { return LibcException::MessageWhat(self); }
void _ZNSt13runtime_errorC1EPKc_nid_postfix(LibcException::ExceptionObject* self, const char* message) { LibcException::Construct(self, _ZTVSt13runtime_error_nid_postfix, message); }
void _ZNSt13runtime_errorC2EPKc_nid_postfix(LibcException::ExceptionObject* self, const char* message) { LibcException::Construct(self, _ZTVSt13runtime_error_nid_postfix, message); }
void _ZNSt13runtime_errorC1ERKS__nid_postfix(LibcException::ExceptionObject* self, const LibcException::ExceptionObject* source) { LibcException::Copy(self, source, _ZTVSt13runtime_error_nid_postfix); }
void _ZNSt13runtime_errorC2ERKS__nid_postfix(LibcException::ExceptionObject* self, const LibcException::ExceptionObject* source) { LibcException::Copy(self, source, _ZTVSt13runtime_error_nid_postfix); }
LibcException::ExceptionObject* _ZNSt13runtime_erroraSERKS__nid_postfix(LibcException::ExceptionObject* self, const LibcException::ExceptionObject* source) { return LibcException::Assign(self, source); }

LibcException::TypeRecord _ZTISt16invalid_argument_nid_postfix {LibcException::SingleTypeVtable + 2, "St16invalid_argument", &_ZTISt11logic_error_nid_postfix};
LibcException::ExceptionVtable _ZTVSt16invalid_argument_nid_postfix {0, &_ZTISt16invalid_argument_nid_postfix, LibcException::DestroyMessage, LibcException::DeleteMessage, LibcException::MessageWhat};
void _ZNSt16invalid_argumentD1Ev_nid_postfix(LibcException::ExceptionObject* self) { LibcException::DestroyMessage(self); }
void _ZNSt16invalid_argumentD2Ev_nid_postfix(LibcException::ExceptionObject* self) { LibcException::DestroyMessage(self); }
void _ZNSt16invalid_argumentD0Ev_nid_postfix(LibcException::ExceptionObject* self) { LibcException::DeleteMessage(self); }
const char* _ZNKSt16invalid_argument4whatEv_nid_postfix(const LibcException::ExceptionObject* self) { return LibcException::MessageWhat(self); }
void _ZNSt16invalid_argumentC1EPKc_nid_postfix(LibcException::ExceptionObject* self, const char* message) { LibcException::Construct(self, _ZTVSt16invalid_argument_nid_postfix, message); }
void _ZNSt16invalid_argumentC2EPKc_nid_postfix(LibcException::ExceptionObject* self, const char* message) { LibcException::Construct(self, _ZTVSt16invalid_argument_nid_postfix, message); }
void _ZNSt16invalid_argumentC1ERKS__nid_postfix(LibcException::ExceptionObject* self, const LibcException::ExceptionObject* source) { LibcException::Copy(self, source, _ZTVSt16invalid_argument_nid_postfix); }
void _ZNSt16invalid_argumentC2ERKS__nid_postfix(LibcException::ExceptionObject* self, const LibcException::ExceptionObject* source) { LibcException::Copy(self, source, _ZTVSt16invalid_argument_nid_postfix); }
LibcException::ExceptionObject* _ZNSt16invalid_argumentaSERKS__nid_postfix(LibcException::ExceptionObject* self, const LibcException::ExceptionObject* source) { return LibcException::Assign(self, source); }

LibcException::TypeRecord _ZTISt12out_of_range_nid_postfix {LibcException::SingleTypeVtable + 2, "St12out_of_range", &_ZTISt11logic_error_nid_postfix};
LibcException::ExceptionVtable _ZTVSt12out_of_range_nid_postfix {0, &_ZTISt12out_of_range_nid_postfix, LibcException::DestroyMessage, LibcException::DeleteMessage, LibcException::MessageWhat};
void _ZNSt12out_of_rangeD1Ev_nid_postfix(LibcException::ExceptionObject* self) { LibcException::DestroyMessage(self); }
void _ZNSt12out_of_rangeD2Ev_nid_postfix(LibcException::ExceptionObject* self) { LibcException::DestroyMessage(self); }
void _ZNSt12out_of_rangeD0Ev_nid_postfix(LibcException::ExceptionObject* self) { LibcException::DeleteMessage(self); }
const char* _ZNKSt12out_of_range4whatEv_nid_postfix(const LibcException::ExceptionObject* self) { return LibcException::MessageWhat(self); }
void _ZNSt12out_of_rangeC1EPKc_nid_postfix(LibcException::ExceptionObject* self, const char* message) { LibcException::Construct(self, _ZTVSt12out_of_range_nid_postfix, message); }
void _ZNSt12out_of_rangeC2EPKc_nid_postfix(LibcException::ExceptionObject* self, const char* message) { LibcException::Construct(self, _ZTVSt12out_of_range_nid_postfix, message); }
void _ZNSt12out_of_rangeC1ERKS__nid_postfix(LibcException::ExceptionObject* self, const LibcException::ExceptionObject* source) { LibcException::Copy(self, source, _ZTVSt12out_of_range_nid_postfix); }
void _ZNSt12out_of_rangeC2ERKS__nid_postfix(LibcException::ExceptionObject* self, const LibcException::ExceptionObject* source) { LibcException::Copy(self, source, _ZTVSt12out_of_range_nid_postfix); }
LibcException::ExceptionObject* _ZNSt12out_of_rangeaSERKS__nid_postfix(LibcException::ExceptionObject* self, const LibcException::ExceptionObject* source) { return LibcException::Assign(self, source); }

LibcException::TypeRecord _ZTISt12domain_error_nid_postfix {LibcException::SingleTypeVtable + 2, "St12domain_error", &_ZTISt11logic_error_nid_postfix};
LibcException::ExceptionVtable _ZTVSt12domain_error_nid_postfix {0, &_ZTISt12domain_error_nid_postfix, LibcException::DestroyMessage, LibcException::DeleteMessage, LibcException::MessageWhat};
void _ZNSt12domain_errorD1Ev_nid_postfix(LibcException::ExceptionObject* self) { LibcException::DestroyMessage(self); }
void _ZNSt12domain_errorD2Ev_nid_postfix(LibcException::ExceptionObject* self) { LibcException::DestroyMessage(self); }
void _ZNSt12domain_errorD0Ev_nid_postfix(LibcException::ExceptionObject* self) { LibcException::DeleteMessage(self); }
const char* _ZNKSt12domain_error4whatEv_nid_postfix(const LibcException::ExceptionObject* self) { return LibcException::MessageWhat(self); }
void _ZNSt12domain_errorC1EPKc_nid_postfix(LibcException::ExceptionObject* self, const char* message) { LibcException::Construct(self, _ZTVSt12domain_error_nid_postfix, message); }
void _ZNSt12domain_errorC2EPKc_nid_postfix(LibcException::ExceptionObject* self, const char* message) { LibcException::Construct(self, _ZTVSt12domain_error_nid_postfix, message); }
void _ZNSt12domain_errorC1ERKS__nid_postfix(LibcException::ExceptionObject* self, const LibcException::ExceptionObject* source) { LibcException::Copy(self, source, _ZTVSt12domain_error_nid_postfix); }
void _ZNSt12domain_errorC2ERKS__nid_postfix(LibcException::ExceptionObject* self, const LibcException::ExceptionObject* source) { LibcException::Copy(self, source, _ZTVSt12domain_error_nid_postfix); }
LibcException::ExceptionObject* _ZNSt12domain_erroraSERKS__nid_postfix(LibcException::ExceptionObject* self, const LibcException::ExceptionObject* source) { return LibcException::Assign(self, source); }

LibcException::TypeRecord _ZTISt12length_error_nid_postfix {LibcException::SingleTypeVtable + 2, "St12length_error", &_ZTISt11logic_error_nid_postfix};
LibcException::ExceptionVtable _ZTVSt12length_error_nid_postfix {0, &_ZTISt12length_error_nid_postfix, LibcException::DestroyMessage, LibcException::DeleteMessage, LibcException::MessageWhat};
void _ZNSt12length_errorD1Ev_nid_postfix(LibcException::ExceptionObject* self) { LibcException::DestroyMessage(self); }
void _ZNSt12length_errorD2Ev_nid_postfix(LibcException::ExceptionObject* self) { LibcException::DestroyMessage(self); }
void _ZNSt12length_errorD0Ev_nid_postfix(LibcException::ExceptionObject* self) { LibcException::DeleteMessage(self); }
const char* _ZNKSt12length_error4whatEv_nid_postfix(const LibcException::ExceptionObject* self) { return LibcException::MessageWhat(self); }
void _ZNSt12length_errorC1EPKc_nid_postfix(LibcException::ExceptionObject* self, const char* message) { LibcException::Construct(self, _ZTVSt12length_error_nid_postfix, message); }
void _ZNSt12length_errorC2EPKc_nid_postfix(LibcException::ExceptionObject* self, const char* message) { LibcException::Construct(self, _ZTVSt12length_error_nid_postfix, message); }
void _ZNSt12length_errorC1ERKS__nid_postfix(LibcException::ExceptionObject* self, const LibcException::ExceptionObject* source) { LibcException::Copy(self, source, _ZTVSt12length_error_nid_postfix); }
void _ZNSt12length_errorC2ERKS__nid_postfix(LibcException::ExceptionObject* self, const LibcException::ExceptionObject* source) { LibcException::Copy(self, source, _ZTVSt12length_error_nid_postfix); }
LibcException::ExceptionObject* _ZNSt12length_erroraSERKS__nid_postfix(LibcException::ExceptionObject* self, const LibcException::ExceptionObject* source) { return LibcException::Assign(self, source); }

LibcException::TypeRecord _ZTISt12system_error_nid_postfix {LibcException::SingleTypeVtable + 2, "St12system_error", &_ZTISt13runtime_error_nid_postfix};
LibcException::ExceptionVtable _ZTVSt12system_error_nid_postfix {0, &_ZTISt12system_error_nid_postfix, LibcException::DestroyMessage, LibcException::DeleteMessage, LibcException::MessageWhat};
void _ZNSt12system_errorD1Ev_nid_postfix(LibcException::ExceptionObject* self) { LibcException::DestroyMessage(self); }
void _ZNSt12system_errorD2Ev_nid_postfix(LibcException::ExceptionObject* self) { LibcException::DestroyMessage(self); }
void _ZNSt12system_errorD0Ev_nid_postfix(LibcException::ExceptionObject* self) { LibcException::DeleteMessage(self); }
const char* _ZNKSt12system_error4whatEv_nid_postfix(const LibcException::ExceptionObject* self) { return LibcException::MessageWhat(self); }
void _ZNSt12system_errorC1EPKc_nid_postfix(LibcException::ExceptionObject* self, const char* message) { LibcException::Construct(self, _ZTVSt12system_error_nid_postfix, message); }
void _ZNSt12system_errorC2EPKc_nid_postfix(LibcException::ExceptionObject* self, const char* message) { LibcException::Construct(self, _ZTVSt12system_error_nid_postfix, message); }
void _ZNSt12system_errorC1ERKS__nid_postfix(LibcException::ExceptionObject* self, const LibcException::ExceptionObject* source) { LibcException::Copy(self, source, _ZTVSt12system_error_nid_postfix); }
void _ZNSt12system_errorC2ERKS__nid_postfix(LibcException::ExceptionObject* self, const LibcException::ExceptionObject* source) { LibcException::Copy(self, source, _ZTVSt12system_error_nid_postfix); }
LibcException::ExceptionObject* _ZNSt12system_erroraSERKS__nid_postfix(LibcException::ExceptionObject* self, const LibcException::ExceptionObject* source) { return LibcException::Assign(self, source); }

LibcException::TypeRecord _ZTISt11regex_error_nid_postfix {LibcException::SingleTypeVtable + 2, "St11regex_error", &_ZTISt13runtime_error_nid_postfix};
LibcException::ExceptionVtable _ZTVSt11regex_error_nid_postfix {0, &_ZTISt11regex_error_nid_postfix, LibcException::DestroyMessage, LibcException::DeleteMessage, LibcException::MessageWhat};
void _ZNSt11regex_errorD1Ev_nid_postfix(LibcException::ExceptionObject* self) { LibcException::DestroyMessage(self); }
void _ZNSt11regex_errorD2Ev_nid_postfix(LibcException::ExceptionObject* self) { LibcException::DestroyMessage(self); }
void _ZNSt11regex_errorD0Ev_nid_postfix(LibcException::ExceptionObject* self) { LibcException::DeleteMessage(self); }
const char* _ZNKSt11regex_error4whatEv_nid_postfix(const LibcException::ExceptionObject* self) { return LibcException::MessageWhat(self); }
void _ZNSt11regex_errorC1EPKc_nid_postfix(LibcException::ExceptionObject* self, const char* message) { LibcException::Construct(self, _ZTVSt11regex_error_nid_postfix, message); }
void _ZNSt11regex_errorC2EPKc_nid_postfix(LibcException::ExceptionObject* self, const char* message) { LibcException::Construct(self, _ZTVSt11regex_error_nid_postfix, message); }
void _ZNSt11regex_errorC1ERKS__nid_postfix(LibcException::ExceptionObject* self, const LibcException::ExceptionObject* source) { LibcException::Copy(self, source, _ZTVSt11regex_error_nid_postfix); }
void _ZNSt11regex_errorC2ERKS__nid_postfix(LibcException::ExceptionObject* self, const LibcException::ExceptionObject* source) { LibcException::Copy(self, source, _ZTVSt11regex_error_nid_postfix); }
LibcException::ExceptionObject* _ZNSt11regex_erroraSERKS__nid_postfix(LibcException::ExceptionObject* self, const LibcException::ExceptionObject* source) { return LibcException::Assign(self, source); }

LibcException::TypeRecord _ZTINSt8ios_base7failureE_nid_postfix {LibcException::SingleTypeVtable + 2, "NSt8ios_base7failureE", &_ZTISt12system_error_nid_postfix};
LibcException::ExceptionVtable _ZTVNSt8ios_base7failureE_nid_postfix {0, &_ZTINSt8ios_base7failureE_nid_postfix, LibcException::DestroyMessage, LibcException::DeleteMessage, LibcException::MessageWhat};
void _ZNSt8ios_base7failureD1Ev_nid_postfix(LibcException::ExceptionObject* self) { LibcException::DestroyMessage(self); }
void _ZNSt8ios_base7failureD2Ev_nid_postfix(LibcException::ExceptionObject* self) { LibcException::DestroyMessage(self); }
void _ZNSt8ios_base7failureD0Ev_nid_postfix(LibcException::ExceptionObject* self) { LibcException::DeleteMessage(self); }
const char* _ZNKSt8ios_base7failure4whatEv_nid_postfix(const LibcException::ExceptionObject* self) { return LibcException::MessageWhat(self); }
void _ZNSt8ios_base7failureC1EPKc_nid_postfix(LibcException::ExceptionObject* self, const char* message) { LibcException::Construct(self, _ZTVNSt8ios_base7failureE_nid_postfix, message); }
void _ZNSt8ios_base7failureC2EPKc_nid_postfix(LibcException::ExceptionObject* self, const char* message) { LibcException::Construct(self, _ZTVNSt8ios_base7failureE_nid_postfix, message); }

[[noreturn]] void __cxa_bad_cast_nid_postfix() { LibcException::ThrowPlain(_ZTVSt8bad_cast_nid_postfix); }
[[noreturn]] void __cxa_bad_typeid_nid_postfix() { LibcException::ThrowPlain(_ZTVSt10bad_typeid_nid_postfix); }
[[noreturn]] void __cxa_throw_bad_array_new_length_nid_postfix() { LibcException::ThrowPlain(_ZTVSt20bad_array_new_length_nid_postfix); }
[[noreturn]] void _ZSt11_Xbad_allocv_nid_postfix() { LibcException::ThrowPlain(_ZTVSt9bad_alloc_nid_postfix); }
[[noreturn]] void _ZSt14_Xout_of_rangePKc_nid_postfix(const char* message) { LibcException::ThrowMessage(_ZTVSt12out_of_range_nid_postfix, message); }
[[noreturn]] void _ZSt14_Xlength_errorPKc_nid_postfix(const char* message) { LibcException::ThrowMessage(_ZTVSt12length_error_nid_postfix, message); }
[[noreturn]] void _ZSt18_Xinvalid_argumentPKc_nid_postfix(const char* message) { LibcException::ThrowMessage(_ZTVSt16invalid_argument_nid_postfix, message); }
[[noreturn]] void _ZSt19_Xbad_function_callv_nid_postfix() { LibcException::ThrowPlain(_ZTVSt17bad_function_call_nid_postfix); }
[[noreturn]] void _ZSt13_Xregex_errorNSt15regex_constants10error_typeE_nid_postfix(std::regex_constants::error_type code) {
    struct RegexObject { LibcException::ExceptionObject base; std::regex_constants::error_type code; };
    auto* object = static_cast<RegexObject*>(__cxa_allocate_exception_nid_postfix(sizeof(RegexObject)));
    LibcException::Construct(&object->base, _ZTVSt11regex_error_nid_postfix, "regular expression error");
    object->code = code;
    __cxa_throw_nid_postfix(object, reinterpret_cast<std::type_info*>(&_ZTISt11regex_error_nid_postfix), [](void* p) { LibcException::DestroyMessage(static_cast<LibcException::ExceptionObject*>(p)); });
}
}
