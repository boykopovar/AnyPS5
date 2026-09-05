#include "Runtime.hpp"
#include <limits>

namespace LibcException {
inline std::atomic<void(*)()> terminateHandler {std::abort};
[[noreturn]] void Terminate() {
    auto handler = terminateHandler.load(std::memory_order_acquire);
    if (handler) handler();
    std::abort();
}

void Release(void* object) {
    if (!object) return;
    auto* header = FromObject(object);
    auto* allocation = AllocationOf(header);
    if (allocation->references.fetch_sub(1, std::memory_order_acq_rel) == 1) {
        if (header->destructor) header->destructor(object);
        allocation->~Allocation();
        std::free(allocation);
    }
}

void Cleanup(_Unwind_Reason_Code, _Unwind_Exception* exception) {
    auto* header = FromUnwind(exception);
    if (exception->exception_class == DependentClass) {
        Release(reinterpret_cast<void*>(header->type));
        header->~Header();
        std::free(header);
    } else Release(header + 1);
}

bool Equal(const std::type_info* a, const std::type_info* b) {
    return a == b || (a && b && std::strcmp(a->name(), b->name()) == 0);
}

const char* Kind(const std::type_info* type) {
    auto table = *reinterpret_cast<const std::type_info* const* const*>(type);
    return table[-1]->name();
}

struct Search {
    const std::type_info* wanted;
    void* found {};
    unsigned count {};
};

void Bases(const std::type_info* type, void* object, Search& search, unsigned depth = 0) {
    if (depth > 128 || !type) return;
    if (Equal(type, search.wanted)) {
        if (!search.count) { search.found = object; search.count = 1; }
        else if (search.found != object) search.count = 2;
        return;
    }
    const char* kind = Kind(type);
    if (std::strcmp(kind, "N10__cxxabiv120__si_class_type_infoE") == 0) {
        auto* si = static_cast<const __cxxabiv1::__si_class_type_info*>(type);
        Bases(si->__base_type, object, search, depth + 1);
    } else if (std::strcmp(kind, "N10__cxxabiv121__vmi_class_type_infoE") == 0) {
        auto* vmi = static_cast<const __cxxabiv1::__vmi_class_type_info*>(type);
        for (unsigned i = 0; i < vmi->__base_count; ++i) {
            const auto& base = vmi->__base_info[i];
            if (!(base.__offset_flags & 2)) continue;
            auto offset = base.__offset_flags >> 8;
            if (base.__offset_flags & 1) {
                if (!object) continue;
                auto* vtable = *static_cast<const unsigned char* const*>(object);
                std::memcpy(&offset, vtable + offset, sizeof(offset));
            }
            void* adjusted = object ? static_cast<unsigned char*>(object) + offset : nullptr;
            Bases(base.__base_type, adjusted, search, depth + 1);
        }
    }
}

bool Match(const std::type_info* caught, const std::type_info* thrown, void*& object) {
    if (!caught || Equal(caught, thrown)) return true;
    if (!thrown) return false;
    const char* caughtKind = Kind(caught);
    const char* thrownKind = Kind(thrown);
    if (std::strcmp(caughtKind, "N10__cxxabiv119__pointer_type_infoE") == 0 &&
        std::strcmp(thrownKind, "N10__cxxabiv119__pointer_type_infoE") == 0) {
        auto* c = static_cast<const __cxxabiv1::__pointer_type_info*>(caught);
        auto* t = static_cast<const __cxxabiv1::__pointer_type_info*>(thrown);
        if ((t->__flags & 7) & ~(c->__flags & 7)) return false;
        if (std::strcmp(c->__pointee->name(), "v") == 0 && std::strcmp(Kind(t->__pointee), "N10__cxxabiv120__function_type_infoE") != 0) return true;
        Search search {c->__pointee};
        Bases(t->__pointee, object, search);
        if (search.count == 1) { object = search.found; return true; }
        return false;
    }
    Search search {caught};
    Bases(thrown, object, search);
    if (search.count == 1) { object = search.found; return true; }
    return false;
}
}

extern "C" {
void* __cxa_allocate_exception_nid_postfix(std::size_t size) {
    using namespace LibcException;
    if (size > std::numeric_limits<std::size_t>::max() - sizeof(Allocation)) Terminate();
    void* storage = std::malloc(sizeof(Allocation) + size);
    if (!storage) Terminate();
    auto* allocation = new (storage) Allocation;
    static_assert(offsetof(Allocation, header) + sizeof(Header) == sizeof(Allocation));
    return &allocation->header + 1;
}
void __cxa_free_exception_nid_postfix(void* object) {
    if (!object) return;
    auto* allocation = LibcException::AllocationOf(LibcException::FromObject(object));
    allocation->~Allocation();
    std::free(allocation);
}

[[noreturn]] void __cxa_throw_nid_postfix(void* object, std::type_info* type, void (*destructor)(void*)) {
    using namespace LibcException;
    auto* header = FromObject(object);
    header->type = type;
    header->destructor = destructor;
    header->terminate = terminateHandler.load(std::memory_order_acquire);
    header->adjusted = object;
    header->unwind.exception_class = PrimaryClass;
    header->unwind.exception_cleanup = Cleanup;
    ++globals.uncaught;
    _Unwind_RaiseException_nid_postfix(&header->unwind);
    Terminate();
}

void* __cxa_begin_catch_nid_postfix(void* exception) {
    using namespace LibcException;
    auto* unwind = static_cast<_Unwind_Exception*>(exception);
    if (!unwind || !Native(unwind->exception_class)) Terminate();
    auto* header = FromUnwind(unwind);
    header->handlers = header->handlers < 0 ? -header->handlers + 1 : header->handlers + 1;
    if (globals.uncaught) --globals.uncaught;
    if (globals.caught != header) { header->next = globals.caught; globals.caught = header; }
    return header->adjusted;
}

void __cxa_end_catch_nid_postfix() {
    using namespace LibcException;
    auto* header = globals.caught;
    if (!header) return;
    if (header->handlers < 0) {
        if (++header->handlers == 0) globals.caught = header->next;
    } else if (--header->handlers == 0) {
        globals.caught = header->next;
        _Unwind_DeleteException_nid_postfix(&header->unwind);
    }
}

[[noreturn]] void __cxa_rethrow_nid_postfix() {
    using namespace LibcException;
    auto* header = globals.caught;
    if (!header) Terminate();
    header->handlers = -header->handlers;
    ++globals.uncaught;
    _Unwind_RaiseException_nid_postfix(&header->unwind);
    Terminate();
}

void* __cxa_get_exception_ptr_nid_postfix(void* exception) {
    return LibcException::FromUnwind(static_cast<_Unwind_Exception*>(exception))->adjusted;
}
std::type_info* __cxa_current_exception_type_nid_postfix() {
    using namespace LibcException;
    return globals.caught ? Primary(globals.caught)->type : nullptr;
}
void* __cxa_get_globals_nid_postfix() { return &LibcException::globals; }
void* __cxa_get_globals_fast_nid_postfix() { return &LibcException::globals; }
bool __cxa_uncaught_exception_nid_postfix() { return LibcException::globals.uncaught != 0; }
unsigned __cxa_uncaught_exceptions_nid_postfix() { return LibcException::globals.uncaught; }
bool _ZSt18uncaught_exceptionv_nid_postfix() { return __cxa_uncaught_exception_nid_postfix(); }
int _ZSt19uncaught_exceptionsv_nid_postfix() { return static_cast<int>(__cxa_uncaught_exceptions_nid_postfix()); }
[[noreturn]] void _ZSt9terminatev_nid_postfix() { LibcException::Terminate(); }
using LibcTerminateHandler = void(*)();
LibcTerminateHandler _ZSt13set_terminatePFvvE_nid_postfix(LibcTerminateHandler handler) {
    return LibcException::terminateHandler.exchange(handler ? handler : std::abort);
}
LibcTerminateHandler _ZSt13get_terminatev_nid_postfix() { return LibcException::terminateHandler.load(); }

void __cxa_increment_exception_refcount_nid_postfix(void* object) {
    if (object) LibcException::AllocationOf(LibcException::FromObject(object))->references.fetch_add(1, std::memory_order_relaxed);
}
void __cxa_decrement_exception_refcount_nid_postfix(void* object) { LibcException::Release(object); }
void* __cxa_current_primary_exception_nid_postfix() {
    using namespace LibcException;
    if (!globals.caught) return nullptr;
    void* object = Primary(globals.caught) + 1;
    __cxa_increment_exception_refcount_nid_postfix(object);
    return object;
}
void* __cxa_allocate_dependent_exception_nid_postfix() {
    void* storage = std::malloc(sizeof(LibcException::Header));
    if (!storage) LibcException::Terminate();
    return new (storage) LibcException::Header;
}
void __cxa_free_dependent_exception_nid_postfix(void* storage) {
    if (!storage) return;
    static_cast<LibcException::Header*>(storage)->~Header();
    std::free(storage);
}
void __cxa_rethrow_primary_exception_nid_postfix(void* object) {
    using namespace LibcException;
    if (!object) return;
    auto* header = static_cast<Header*>(__cxa_allocate_dependent_exception_nid_postfix());
    header->type = reinterpret_cast<std::type_info*>(object);
    header->adjusted = object;
    header->terminate = terminateHandler.load();
    header->unwind.exception_class = DependentClass;
    header->unwind.exception_cleanup = Cleanup;
    __cxa_increment_exception_refcount_nid_postfix(object);
    ++globals.uncaught;
    _Unwind_RaiseException_nid_postfix(&header->unwind);
    Terminate();
}

void* __dynamic_cast_nid_postfix(const void* source, const __cxxabiv1::__class_type_info* sourceType,
                               const __cxxabiv1::__class_type_info* destinationType, std::ptrdiff_t) {
    using namespace LibcException;
    if (!source) return nullptr;
    auto* vtable = *static_cast<const std::uintptr_t* const*>(source);
    auto* complete = const_cast<unsigned char*>(static_cast<const unsigned char*>(source)) + static_cast<std::intptr_t>(vtable[-2]);
    auto* dynamicType = reinterpret_cast<const std::type_info*>(vtable[-1]);
    Search target {destinationType};
    Bases(dynamicType, complete, target);
    if (target.count != 1) return nullptr;
    Search downcast {sourceType};
    Bases(destinationType, target.found, downcast);
    if (downcast.count == 1 && downcast.found == source) return target.found;
    Search publicSource {sourceType};
    Bases(dynamicType, complete, publicSource);
    return publicSource.count == 1 && publicSource.found == source ? target.found : nullptr;
}

[[noreturn]] void __cxa_pure_virtual_nid_postfix() { LibcException::Terminate(); }
[[noreturn]] void __cxa_deleted_virtual_nid_postfix() { LibcException::Terminate(); }
}
