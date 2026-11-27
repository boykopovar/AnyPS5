#include "prx/libc/include/Api.hpp"
#include <array>
#include <atomic>
#include <cerrno>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <stdexcept>
#include <thread>
#include <typeinfo>
#include <vector>

static void Require(bool condition) { if (!condition) std::abort(); }

namespace {

using ClassTypeInfo = __cxxabiv1::__class_type_info;
using Upcast = bool (APS5_VABI*)(const std::type_info*, const ClassTypeInfo*, void**);

GuestLocale::LibcxxLocaleId* const exportedIds[] {
    &_ZNSt3__15ctypeIcE2idE_nid_postfix,
    &_ZNSt3__17codecvtIcc11__mbstate_tE2idE_nid_postfix,
    &_ZNSt3__17codecvtIDic11__mbstate_tE2idE_nid_postfix,
    &_ZNSt3__17collateIcE2idE_nid_postfix,
    &_ZNSt3__18messagesIcE2idE_nid_postfix,
    &_ZNSt3__18numpunctIcE2idE_nid_postfix,
    &_ZNSt3__18numpunctIwE2idE_nid_postfix,
    &_ZNSt3__18time_putIcNS_19ostreambuf_iteratorIcNS_11char_traitsIcEEEEE2idE_nid_postfix,
};

int deleteCalls = 0;
GuestLocale::LibcxxFacet* deletedFacet = nullptr;

void APS5_VABI FailFacetCall(GuestLocale::LibcxxFacet*) { std::abort(); }

void APS5_VABI CountFacetDelete(GuestLocale::LibcxxFacet* self) {
    ++deleteCalls;
    deletedFacet = self;
}

const GuestLocale::LibcxxFacetVtable countingVtable {FailFacetCall, CountFacetDelete, FailFacetCall};
const GuestLocale::LibcxxFacetVtable noDeleteVtable {FailFacetCall, nullptr, FailFacetCall};

template<typename TCall>
void RequireInvalid(TCall call) {
    try {
        call();
    } catch (const std::invalid_argument&) {
        return;
    }
    std::abort();
}

const char* KindOf(const void* const* vtable) {
    return static_cast<const LibcException::TypeRecord*>(vtable[-1])->name;
}

void CheckLocaleIds() {
    const std::array<unsigned char, 16> zero{};
    for (const auto* id : exportedIds) Require(std::memcmp(id, zero.data(), zero.size()) == 0);
    for (std::size_t index = 0; index < std::size(exportedIds); ++index)
        Require(_ZNSt3__16locale2id5__getEv_nid_postfix(exportedIds[index]) == static_cast<std::int64_t>(index));
    for (std::size_t index = std::size(exportedIds); index-- > 0;)
        Require(_ZNSt3__16locale2id5__getEv_nid_postfix(exportedIds[index]) == static_cast<std::int64_t>(index));
    Require(exportedIds[3]->flag == ~std::uint64_t{0} && exportedIds[3]->id == 4);

    GuestLocale::LibcxxLocaleId raced{};
    std::atomic<bool> start{false};
    std::array<std::int64_t, 8> results{};
    std::vector<std::thread> threads;
    for (std::size_t index = 0; index < results.size(); ++index) {
        threads.emplace_back([&, index] {
            while (!start.load(std::memory_order_acquire)) std::this_thread::yield();
            results[index] = _ZNSt3__16locale2id5__getEv_nid_postfix(&raced);
        });
    }
    start.store(true, std::memory_order_release);
    for (auto& thread : threads) thread.join();
    for (const auto result : results) Require(result == 8);
    GuestLocale::LibcxxLocaleId next{};
    Require(_ZNSt3__16locale2id5__getEv_nid_postfix(&next) == 9);
    Require(_ZNSt3__16locale2id5__getEv_nid_postfix(&raced) == 8);

    RequireInvalid([] { _ZNSt3__16locale2id5__getEv_nid_postfix(nullptr); });
    alignas(8) std::array<unsigned char, 24> storage{};
    RequireInvalid([&] {
        _ZNSt3__16locale2id5__getEv_nid_postfix(reinterpret_cast<GuestLocale::LibcxxLocaleId*>(storage.data() + 4));
    });
    GuestLocale::LibcxxLocaleId corrupt{5, 7};
    RequireInvalid([&] { _ZNSt3__16locale2id5__getEv_nid_postfix(&corrupt); });
    Require(corrupt.flag == 5 && corrupt.id == 7);
    GuestLocale::LibcxxLocaleId after{};
    Require(_ZNSt3__16locale2id5__getEv_nid_postfix(&after) == 10);
}

void CheckFacetLifetime() {
    GuestLocale::LibcxxFacet facet {&countingVtable, -1};
    _ZNSt3__16locale5facet16__on_zero_sharedEv_nid_postfix(&facet);
    Require(deleteCalls == 1 && deletedFacet == &facet);
    Require(facet.vtable == &countingVtable && facet.sharedOwners == -1);

    const GuestLocale::LibcxxFacet before = facet;
    _ZNSt3__16locale5facetD2Ev_nid_postfix(&facet);
    Require(std::memcmp(&before, &facet, sizeof(facet)) == 0);
    _ZNSt3__17codecvtIcc11__mbstate_tED2Ev_nid_postfix(&facet);
    Require(std::memcmp(&before, &facet, sizeof(facet)) == 0 && deleteCalls == 1);

    RequireInvalid([] { _ZNSt3__16locale5facetD2Ev_nid_postfix(nullptr); });
    RequireInvalid([] { _ZNSt3__17codecvtIcc11__mbstate_tED2Ev_nid_postfix(nullptr); });
    RequireInvalid([] { _ZNSt3__16locale5facet16__on_zero_sharedEv_nid_postfix(nullptr); });
    GuestLocale::LibcxxFacet noVtable {nullptr, 0};
    RequireInvalid([&] { _ZNSt3__16locale5facet16__on_zero_sharedEv_nid_postfix(&noVtable); });
    GuestLocale::LibcxxFacet noDelete {&noDeleteVtable, 0};
    RequireInvalid([&] { _ZNSt3__16locale5facet16__on_zero_sharedEv_nid_postfix(&noDelete); });
    Require(deleteCalls == 1);
}

void CheckTypeInfo() {
    const auto& codecvtRecord = _ZTINSt3__17codecvtIcc11__mbstate_tEE_nid_postfix;
    const auto& facetRecord = _ZTINSt3__16locale5facetE_nid_postfix;
    const auto* codecvtType = reinterpret_cast<const std::type_info*>(&codecvtRecord);
    const auto* facetType = reinterpret_cast<const std::type_info*>(&facetRecord);
    Require(std::strcmp(KindOf(codecvtRecord.vtable), "N10__cxxabiv121__vmi_class_type_infoE") == 0);
    Require(std::strcmp(KindOf(facetRecord.vtable), "N10__cxxabiv120__si_class_type_infoE") == 0);

    const auto* codecvt = static_cast<const __cxxabiv1::__vmi_class_type_info*>(codecvtType);
    Require(std::strcmp(codecvt->name(), "NSt3__17codecvtIcc11__mbstate_tEE") == 0);
    Require(codecvt->__flags == 0 && codecvt->__base_count == 2);
    const auto& facetBase = codecvt->__base_info[0];
    const auto& codecvtBase = codecvt->__base_info[1];
    Require(facetBase.__base_type == facetType && facetBase.__offset_flags == 2);
    Require(std::strcmp(codecvtBase.__base_type->name(), "NSt3__112codecvt_baseE") == 0 && codecvtBase.__offset_flags == 2);
    Require(std::strcmp(KindOf(codecvtRecord.bases[1].type->vtable), "N10__cxxabiv117__class_type_infoE") == 0);

    const auto* facet = static_cast<const __cxxabiv1::__si_class_type_info*>(facetType);
    Require(std::strcmp(facet->name(), "NSt3__16locale5facetE") == 0);
    Require(std::strcmp(facet->__base_type->name(), "NSt3__114__shared_countE") == 0);
    Require(std::strcmp(KindOf(facetRecord.base->vtable), "N10__cxxabiv117__class_type_infoE") == 0);

    const LibcException::TypeRecord guestType {
        facetRecord.vtable, "9GuestCvt", reinterpret_cast<const LibcException::TypeRecord*>(&codecvtRecord),
    };
    const auto* guest = reinterpret_cast<const std::type_info*>(&guestType);
    const auto* unrelated = reinterpret_cast<const ClassTypeInfo*>(&_ZTINSt3__112bad_weak_ptrE_nid_postfix);
    const void* objectVtable[] {nullptr, &guestType, nullptr};
    struct GuestObject { const void* const* vtable; std::int64_t sharedOwners; } object {objectVtable + 2, 0};

    const auto upcastCodecvt = reinterpret_cast<Upcast>(codecvtRecord.vtable[5]);
    const auto upcastGuest = reinterpret_cast<Upcast>(guestType.vtable[5]);
    for (const auto* target : {static_cast<const ClassTypeInfo*>(facetType), codecvtBase.__base_type, facet->__base_type}) {
        void* adjusted = &object;
        Require(upcastCodecvt(codecvtType, target, &adjusted) && adjusted == &object);
        adjusted = &object;
        Require(upcastGuest(guest, target, &adjusted) && adjusted == &object);
    }
    void* adjusted = &object;
    Require(!upcastCodecvt(codecvtType, unrelated, &adjusted) && adjusted == &object);

    const auto* facetClass = static_cast<const ClassTypeInfo*>(facetType);
    Require(__dynamic_cast_nid_postfix(&object, facetClass, static_cast<const ClassTypeInfo*>(codecvtType), -1) == &object);
    Require(__dynamic_cast_nid_postfix(&object, facetClass, static_cast<const ClassTypeInfo*>(guest), -1) == &object);
    Require(__dynamic_cast_nid_postfix(&object, facetClass, codecvtBase.__base_type, -1) == &object);
    Require(__dynamic_cast_nid_postfix(&object, facetClass, unrelated, -1) == nullptr);
}

}

int main() {
    std::array<char, 8> input{'a', 'b', 'c', 'd', 'e', 'f', 'g', 'h'};
    std::array<char, 8> output{};
    output.fill('!');
    char facet = 0;
    char state = 0;
    const char* fromNext = nullptr;
    char* toNext = nullptr;
    errno = ESRCH;

    Require(_ZNKSt3__17codecvtIcc11__mbstate_tE11do_encodingEv_nid_postfix(&facet) == 1);
    Require(_ZNKSt3__17codecvtIcc11__mbstate_tE13do_max_lengthEv_nid_postfix(&facet) == 1);
    Require(_ZNKSt3__17codecvtIcc11__mbstate_tE5do_inERS1_PKcS5_RS5_PcS7_RS7__nid_postfix(
        &facet, &state, input.data(), input.data() + input.size(), &fromNext,
        output.data(), output.data() + output.size(), &toNext) == 3);
    Require(fromNext == input.data() && toNext == output.data());
    Require(output[0] == '!' && output[7] == '!' && errno == ESRCH);

    fromNext = nullptr;
    toNext = nullptr;
    Require(_ZNKSt3__17codecvtIcc11__mbstate_tE6do_outERS1_PKcS5_RS5_PcS7_RS7__nid_postfix(
        &facet, &state, input.data(), input.data() + input.size(), &fromNext,
        output.data(), output.data() + output.size(), &toNext) == 3);
    Require(fromNext == input.data() && toNext == output.data());
    Require(output[0] == '!' && output[7] == '!' && errno == ESRCH);

    toNext = nullptr;
    Require(_ZNKSt3__17codecvtIcc11__mbstate_tE10do_unshiftERS1_PcS4_RS4__nid_postfix(
        &facet, &state, output.data(), output.data() + output.size(), &toNext) == 3);
    Require(toNext == output.data() && output[0] == '!' && errno == ESRCH);

    Require(_ZNKSt3__17codecvtIcc11__mbstate_tE9do_lengthERS1_PKcS5_m_nid_postfix(
        &facet, &state, input.data(), input.data() + input.size(), 3) == 3);
    Require(_ZNKSt3__17codecvtIcc11__mbstate_tE9do_lengthERS1_PKcS5_m_nid_postfix(
        &facet, &state, input.data(), input.data() + input.size(), 99) == 8);
    Require(_ZNKSt3__17codecvtIcc11__mbstate_tE9do_lengthERS1_PKcS5_m_nid_postfix(
        &facet, &state, input.data(), input.data(), 99) == 0);

    try {
        _ZNKSt3__17codecvtIcc11__mbstate_tE10do_unshiftERS1_PcS4_RS4__nid_postfix(
            &facet, &state, output.data(), output.data() + output.size(), nullptr);
        std::abort();
    } catch (const std::invalid_argument&) { Require(errno == ESRCH); }

    CheckLocaleIds();
    CheckFacetLifetime();
    CheckTypeInfo();
}
