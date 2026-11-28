#include "prx/libc/include/Api.hpp"
#include "prx/libc/include/General.hpp"
#include "prx/libc/include/GuestLocale.hpp"

#include <atomic>
#include <cstdint>
#include <stdexcept>
#include <string>
#include <thread>

namespace LibcException {
struct TypeInfoVtable;
}

extern "C" {
extern LibcException::TypeInfoVtable _ZTVN10__cxxabiv117__class_type_infoE_nid_postfix;
extern LibcException::TypeInfoVtable _ZTVN10__cxxabiv120__si_class_type_infoE_nid_postfix;
}

namespace {

constexpr std::uint64_t OnceUnset = 0;
constexpr std::uint64_t OnceRunning = 1;
constexpr std::uint64_t OnceComplete = ~std::uint64_t{0};

std::atomic<std::uint32_t> nextLocaleId{0};

const void* const* TypeInfoVptr(LibcException::TypeInfoVtable& vtable) {
    return reinterpret_cast<const void* const*>(&vtable) + 2;
}

const LibcException::TypeRecord SharedCountType {
    TypeInfoVptr(_ZTVN10__cxxabiv117__class_type_infoE_nid_postfix), "NSt3__114__shared_countE", nullptr,
};

}

extern "C" {

void APS5_VABI _ZNSt3__16locale5facetD2Ev_nid_postfix(GuestLocale::LibcxxFacet* self) {
    if (self == nullptr) APS5_INVALID_ARG_EX;
}

void APS5_VABI _ZNSt3__16locale5facet16__on_zero_sharedEv_nid_postfix(GuestLocale::LibcxxFacet* self) {
    if (self == nullptr || self->vtable == nullptr || self->vtable->deleteObject == nullptr) APS5_INVALID_ARG_EX;
    self->vtable->deleteObject(self);
}

const LibcException::TypeRecord _ZTINSt3__16locale5facetE_nid_postfix {
    TypeInfoVptr(_ZTVN10__cxxabiv120__si_class_type_infoE_nid_postfix), "NSt3__16locale5facetE", &SharedCountType,
};

std::int64_t APS5_VABI _ZNSt3__16locale2id5__getEv_nid_postfix(GuestLocale::LibcxxLocaleId* self) {
    if (self == nullptr || reinterpret_cast<std::uintptr_t>(self) % alignof(std::uint64_t) != 0) APS5_INVALID_ARG_EX;
    std::atomic_ref<std::uint64_t> flag(self->flag);
    std::uint64_t state = flag.load(std::memory_order_acquire);
    while (state != OnceComplete) {
        if (state == OnceUnset) {
            if (!flag.compare_exchange_weak(state, OnceRunning, std::memory_order_acquire)) continue;
            self->id = static_cast<std::int32_t>(nextLocaleId.fetch_add(1) + 1u);
            flag.store(OnceComplete, std::memory_order_release);
            break;
        }
        if (state != OnceRunning)
            throw std::invalid_argument(std::string(__func__) + ": invalid once_flag state");
        std::this_thread::yield();
        state = flag.load(std::memory_order_acquire);
    }
    return static_cast<std::int64_t>(self->id) - 1;
}

GuestLocale::LibcxxLocaleId _ZNSt3__15ctypeIcE2idE_nid_postfix {};
GuestLocale::LibcxxLocaleId _ZNSt3__17codecvtIcc11__mbstate_tE2idE_nid_postfix {};
GuestLocale::LibcxxLocaleId _ZNSt3__17codecvtIDic11__mbstate_tE2idE_nid_postfix {};
GuestLocale::LibcxxLocaleId _ZNSt3__17collateIcE2idE_nid_postfix {};
GuestLocale::LibcxxLocaleId _ZNSt3__18messagesIcE2idE_nid_postfix {};
GuestLocale::LibcxxLocaleId _ZNSt3__18numpunctIcE2idE_nid_postfix {};
GuestLocale::LibcxxLocaleId _ZNSt3__18numpunctIwE2idE_nid_postfix {};
GuestLocale::LibcxxLocaleId _ZNSt3__18time_putIcNS_19ostreambuf_iteratorIcNS_11char_traitsIcEEEEE2idE_nid_postfix {};

}
