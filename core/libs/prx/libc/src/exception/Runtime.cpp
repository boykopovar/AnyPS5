#include "Runtime.hpp"
#include <limits>

namespace LibcException {
inline std::atomic<void(*)()> terminateHandler {std::abort};
[[noreturn]] void InvokeTerminate(void (*handler)()) {
    try { if (handler) handler(); } catch (...) {}
    std::abort();
}
[[noreturn]] void Terminate() { InvokeTerminate(terminateHandler.load(std::memory_order_acquire)); }

_Unwind_Exception* UnwindOf(Header* header) {
    return Native(header->unwind.exception_class) ? &header->unwind : reinterpret_cast<_Unwind_Exception*>(header->landing);
}
void DeleteForeignHeader(Header* header) { header->~Header(); std::free(header); }

void Release(void* object) {
    if (!object) return;
    auto* header = FromObject(object);
    auto* allocation = AllocationOf(header);
    if (allocation->references.fetch_sub(1, std::memory_order_acq_rel) == 1) {
        try { if (header->destructor) header->destructor(object); }
        catch (...) { Terminate(); }
        allocation->~Allocation();
        std::free(allocation);
    }
}

void Cleanup(_Unwind_Reason_Code reason, _Unwind_Exception* exception) {
    if (reason != _URC_FOREIGN_EXCEPTION_CAUGHT) Terminate();
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

struct BaseLocation {
    void* object;
    std::uintptr_t identity;
};

template<class Visitor>
void VisitBases(const std::type_info* type, BaseLocation location, bool publicPath, Visitor& visitor, unsigned depth = 0) {
    if (!type) return;
    if (depth > 128) Terminate();
    visitor(type, location, publicPath);
    const char* kind = Kind(type);
    if (std::strcmp(kind, "N10__cxxabiv120__si_class_type_infoE") == 0) {
        auto* si = static_cast<const __cxxabiv1::__si_class_type_info*>(type);
        VisitBases(si->__base_type, location, publicPath, visitor, depth + 1);
    } else if (std::strcmp(kind, "N10__cxxabiv121__vmi_class_type_infoE") == 0) {
        auto* vmi = static_cast<const __cxxabiv1::__vmi_class_type_info*>(type);
        for (unsigned i = 0; i < vmi->__base_count; ++i) {
            const auto& base = vmi->__base_info[i];
            auto offset = base.__offset_flags >> 8;
            BaseLocation next = location;
            if (base.__offset_flags & 1) {
                if (location.object) {
                    auto* vtable = *static_cast<const unsigned char* const*>(location.object);
                    std::memcpy(&offset, vtable + offset, sizeof(offset));
                } else {
                    next.identity = reinterpret_cast<std::uintptr_t>(base.__base_type);
                    offset = 0;
                }
            }
            if (location.object) next.object = static_cast<unsigned char*>(location.object) + offset;
            else next.identity += offset;
            VisitBases(base.__base_type, next, publicPath && (base.__offset_flags & 2), visitor, depth + 1);
        }
    }
}

void Bases(const std::type_info* type, void* object, Search& search) {
    std::uintptr_t identity = 0;
    auto visitor = [&](const std::type_info* candidate, BaseLocation location, bool publicPath) {
        if (!publicPath || !Equal(candidate, search.wanted)) return;
        if (!search.count) { search.found = location.object; identity = location.identity; search.count = 1; }
        else if (search.found != location.object || identity != location.identity) search.count = 2;
    };
    VisitBases(type, {object, 0}, true, visitor);
}

bool PointerMatch(const __cxxabiv1::__pbase_type_info* caught, const __cxxabiv1::__pbase_type_info* thrown,
                  void*& object, unsigned depth, bool constPath) {
    if (depth > 64) return false;
    unsigned added = (caught->__flags & 7) & ~(thrown->__flags & 7);
    if (((thrown->__flags & 7) & ~(caught->__flags & 7)) || (depth && added && !constPath)) return false;
    if ((caught->__flags & 0x60) & ~(thrown->__flags & 0x60)) return false;
    if (Equal(caught->__pointee, thrown->__pointee)) return true;
    if (std::strcmp(Kind(caught->__pointee), "N10__cxxabiv119__pointer_type_infoE") == 0 &&
        std::strcmp(Kind(thrown->__pointee), "N10__cxxabiv119__pointer_type_infoE") == 0)
        return PointerMatch(static_cast<const __cxxabiv1::__pbase_type_info*>(caught->__pointee),
                            static_cast<const __cxxabiv1::__pbase_type_info*>(thrown->__pointee), object,
                            depth + 1, constPath && (caught->__flags & 1));
    if (depth) return false;
    if (std::strcmp(caught->__pointee->name(), "v") == 0 &&
        std::strcmp(Kind(thrown->__pointee), "N10__cxxabiv120__function_type_infoE") != 0) return true;
    Search search {caught->__pointee};
    Bases(thrown->__pointee, object, search);
    if (search.count != 1) return false;
    object = search.found;
    return true;
}

bool Match(const std::type_info* caught, const std::type_info* thrown, void*& object) {
    if (!caught || Equal(caught, thrown)) return true;
    if (!thrown) return false;
    const char* caughtKind = Kind(caught);
    const char* thrownKind = Kind(thrown);
    if (std::strcmp(caughtKind, "N10__cxxabiv119__pointer_type_infoE") == 0 &&
        std::strcmp(thrownKind, "N10__cxxabiv119__pointer_type_infoE") == 0) {
        return PointerMatch(static_cast<const __cxxabiv1::__pointer_type_info*>(caught),
                            static_cast<const __cxxabiv1::__pointer_type_info*>(thrown), object, 0, true);
    }
    if (std::strcmp(caughtKind, "N10__cxxabiv119__pointer_type_infoE") == 0 && std::strcmp(thrown->name(), "Dn") == 0) {
        object = nullptr;
        return true;
    }
    if (std::strcmp(caughtKind, "N10__cxxabiv129__pointer_to_member_type_infoE") == 0 &&
        std::strcmp(thrownKind, "N10__cxxabiv129__pointer_to_member_type_infoE") == 0) {
        auto* c = static_cast<const __cxxabiv1::__pointer_to_member_type_info*>(caught);
        auto* t = static_cast<const __cxxabiv1::__pointer_to_member_type_info*>(thrown);
        return Equal(c->__context, t->__context) && PointerMatch(c, t, object, 0, true);
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
    __cxa_begin_catch_nid_postfix(&header->unwind);
    InvokeTerminate(header->terminate);
}

void* __cxa_begin_catch_nid_postfix(void* exception) {
    using namespace LibcException;
    auto* unwind = static_cast<_Unwind_Exception*>(exception);
    if (!unwind) Terminate();
    Header* header;
    if (Native(unwind->exception_class)) {
        header = FromUnwind(unwind);
        if (globals.uncaught) --globals.uncaught;
    } else if (globals.caught && UnwindOf(globals.caught) == unwind) {
        header = globals.caught;
    } else {
        void* storage = std::malloc(sizeof(Header));
        if (!storage) Terminate();
        header = new (storage) Header;
        header->unwind.exception_class = unwind->exception_class;
        header->landing = reinterpret_cast<std::uintptr_t>(unwind);
        header->adjusted = unwind + 1;
    }
    header->handlers = header->handlers < 0 ? -header->handlers + 1 : header->handlers + 1;
    if (globals.caught != header) { header->next = globals.caught; globals.caught = header; }
    return header->adjusted;
}

void __cxa_end_catch_nid_postfix() {
    using namespace LibcException;
    auto* header = globals.caught;
    if (!header) return;
    if (header->handlers < 0) {
        if (++header->handlers == 0) {
            globals.caught = header->next;
            if (!Native(header->unwind.exception_class)) DeleteForeignHeader(header);
        }
    } else if (--header->handlers == 0) {
        globals.caught = header->next;
        bool foreign = !Native(header->unwind.exception_class);
        _Unwind_DeleteException_nid_postfix(UnwindOf(header));
        if (foreign) DeleteForeignHeader(header);
    }
}

[[noreturn]] void __cxa_rethrow_nid_postfix() {
    using namespace LibcException;
    auto* header = globals.caught;
    if (!header) Terminate();
    header->handlers = -header->handlers;
    if (Native(header->unwind.exception_class)) ++globals.uncaught;
    _Unwind_RaiseException_nid_postfix(UnwindOf(header));
    __cxa_begin_catch_nid_postfix(UnwindOf(header));
    InvokeTerminate(header->terminate ? header->terminate : terminateHandler.load());
}

void* __cxa_get_exception_ptr_nid_postfix(void* exception) {
    auto* unwind = static_cast<_Unwind_Exception*>(exception);
    return LibcException::Native(unwind->exception_class) ? LibcException::FromUnwind(unwind)->adjusted : unwind + 1;
}
std::type_info* __cxa_current_exception_type_nid_postfix() {
    using namespace LibcException;
    return globals.caught && Native(globals.caught->unwind.exception_class) ? Primary(globals.caught)->type : nullptr;
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
    if (!globals.caught || !Native(globals.caught->unwind.exception_class)) return nullptr;
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
    __cxa_begin_catch_nid_postfix(&header->unwind);
    InvokeTerminate(header->terminate);
}

void* __dynamic_cast_nid_postfix(const void* source, const __cxxabiv1::__class_type_info* sourceType,
                               const __cxxabiv1::__class_type_info* destinationType, std::ptrdiff_t) {
    using namespace LibcException;
    if (!source) return nullptr;
    auto* vtable = *static_cast<const std::uintptr_t* const*>(source);
    auto* complete = const_cast<unsigned char*>(static_cast<const unsigned char*>(source)) + static_cast<std::intptr_t>(vtable[-2]);
    auto* dynamicType = reinterpret_cast<const std::type_info*>(vtable[-1]);
    Search downcastTarget {destinationType};
    auto visitor = [&](const std::type_info* candidate, BaseLocation location, bool) {
        if (!Equal(candidate, destinationType)) return;
        bool containsSource = false;
        auto sourceVisitor = [&](const std::type_info* base, BaseLocation baseLocation, bool publicPath) {
            if (publicPath && Equal(base, sourceType) && baseLocation.object == source) containsSource = true;
        };
        VisitBases(candidate, location, true, sourceVisitor);
        if (!containsSource) return;
        if (!downcastTarget.count) { downcastTarget.found = location.object; downcastTarget.count = 1; }
        else if (downcastTarget.found != location.object) downcastTarget.count = 2;
    };
    VisitBases(dynamicType, {complete, 0}, true, visitor);
    if (downcastTarget.count == 1) return downcastTarget.found;
    bool sourceIsPublic = false;
    auto sourceVisitor = [&](const std::type_info* candidate, BaseLocation location, bool publicPath) {
        if (publicPath && Equal(candidate, sourceType) && location.object == source) sourceIsPublic = true;
    };
    VisitBases(dynamicType, {complete, 0}, true, sourceVisitor);
    if (!sourceIsPublic) return nullptr;
    Search publicTarget {destinationType};
    Bases(dynamicType, complete, publicTarget);
    return publicTarget.count == 1 ? publicTarget.found : nullptr;
}

[[noreturn]] void __cxa_call_terminate_nid_postfix(void* exception) {
    __cxa_begin_catch_nid_postfix(exception);
    LibcException::Terminate();
}
[[noreturn]] void __cxa_pure_virtual_nid_postfix() { LibcException::Terminate(); }
[[noreturn]] void __cxa_deleted_virtual_nid_postfix() { LibcException::Terminate(); }
}
