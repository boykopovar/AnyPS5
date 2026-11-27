#include "prx/libSceIcu/CollatorHost.hpp"
#include "prx/libSceIcu/DefaultLocaleName.hpp"
#include "prx/libSceIcu/RecoveredSignatures.hpp"
#include "prx/libSceIcu/Status.hpp"
#include "prx/libc/include/General.hpp"
#include <algorithm>
#include <array>
#include <cerrno>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <mutex>
#include <shared_mutex>
#include <string>
#include <unordered_map>
#include <utility>

namespace {

constexpr std::int32_t UcolDefault = -1;
constexpr std::size_t LocaleBytes = 4096;
constexpr std::size_t PageBytes = 4096;

struct Registry {
    std::shared_mutex mutex;
    std::unordered_map<const void*, void*> collators;
    std::uintptr_t nextHandle{1};
};

struct RegisteredCollator {
    std::shared_lock<std::shared_mutex> lock;
    void* handle;
};

Registry& Collators() {
    static auto* registry = new Registry;
    return *registry;
}

bool CopyIn(void* destination, const void* source, std::size_t bytes) {
    const int savedErrno = errno;
    const bool ok = GuestCopyIn_nid_no_patch(destination, source, bytes);
    errno = savedErrno;
    return ok;
}

std::int32_t ReadStatus(const std::int32_t* status, const char* caller) {
    Icu::RequireStatus(status);
    std::int32_t value = 0;
    if (!CopyIn(&value, status, sizeof(value))) NotImplemented_nid_no_patch(caller);
    return value;
}

bool Failure(const std::int32_t* status, const char* caller) {
    return ReadStatus(status, caller) > 0;
}

void WriteStatus(std::int32_t* status, std::int32_t value, const char* caller) {
    const int savedErrno = errno;
    const bool ok = GuestCopyOut_nid_no_patch(status, &value, sizeof(value));
    errno = savedErrno;
    if (!ok) NotImplemented_nid_no_patch(caller);
}

void UpdateStatus(std::int32_t* status, std::int32_t incoming, std::int32_t outgoing, const char* caller) {
    if (outgoing != incoming) WriteStatus(status, outgoing, caller);
}

bool CollationKeyword(const char* keyword, const char* caller) {
    constexpr char expected[] = "collation";
    for (std::size_t index = 0; index < sizeof(expected); ++index) {
        char unit = 0;
        if (!CopyIn(&unit, keyword + index, sizeof(unit))) NotImplemented_nid_no_patch(caller);
        if (unit != expected[index]) return false;
    }
    return true;
}

RegisteredCollator Registered(const UCollator* coll, const char* caller) {
    auto& registry = Collators();
    std::shared_lock lock(registry.mutex);
    const auto found = registry.collators.find(coll);
    if (found == registry.collators.end()) NotImplemented_nid_no_patch(caller);
    return {std::move(lock), found->second};
}

void* Register(void* collator) {
    auto& registry = Collators();
    const std::unique_lock lock(registry.mutex);
    if (registry.nextHandle > std::numeric_limits<std::uintptr_t>::max() - 2) NotImplemented_nid_no_patch(__func__);
    void* key = reinterpret_cast<void*>(registry.nextHandle);
    registry.nextHandle += 2;
    registry.collators.emplace(key, collator);
    return key;
}

bool Unregister(const void* collator) {
    auto& registry = Collators();
    const std::unique_lock lock(registry.mutex);
    return registry.collators.erase(collator) != 0;
}

void CloseRegistered(void* collator, const char* caller) {
    auto& registry = Collators();
    const std::unique_lock lock(registry.mutex);
    const auto found = registry.collators.find(collator);
    if (found == registry.collators.end()) NotImplemented_nid_no_patch(caller);
    void* native = found->second;
    registry.collators.erase(found);
    Icu::CollatorHost::Close(native);
}

template<class TUnit>
std::basic_string<TUnit> CopyUnits(const TUnit* text, std::int32_t length, const char* caller) {
    std::basic_string<TUnit> units;
    std::array<TUnit, PageBytes / sizeof(TUnit)> scratch{};
    auto address = reinterpret_cast<std::uintptr_t>(text);
    if (length >= 0) {
        auto remaining = static_cast<std::size_t>(length);
        while (remaining > 0) {
            const std::size_t page = std::max<std::size_t>((PageBytes - address % PageBytes) / sizeof(TUnit), 1);
            const std::size_t count = std::min({remaining, page, scratch.size()});
            if (!CopyIn(scratch.data(), reinterpret_cast<const void*>(address), count * sizeof(TUnit))) {
                NotImplemented_nid_no_patch(caller);
            }
            units.append(scratch.data(), count);
            remaining -= count;
            address += count * sizeof(TUnit);
        }
        return units;
    }
    while (true) {
        const std::size_t page = std::max<std::size_t>((PageBytes - address % PageBytes) / sizeof(TUnit), 1);
        const std::size_t count = std::min(page, scratch.size());
        if (!CopyIn(scratch.data(), reinterpret_cast<const void*>(address), count * sizeof(TUnit))) {
            NotImplemented_nid_no_patch(caller);
        }
        const auto end = std::find(scratch.begin(), scratch.begin() + static_cast<std::ptrdiff_t>(count), TUnit{});
        units.append(scratch.begin(), end);
        if (end != scratch.begin() + static_cast<std::ptrdiff_t>(count)) return units;
        Icu::Checked(static_cast<std::int64_t>(units.size()));
        address += count * sizeof(TUnit);
    }
}

template<class TUnit>
struct Operand {
    std::basic_string<TUnit> units;
    bool present;
    std::int32_t length;

    const TUnit* Data() const {
        return present ? units.data() : nullptr;
    }
};

template<class TUnit>
Operand<TUnit> CopyOperand(const TUnit* text, std::int32_t length, const char* caller) {
    if (!text) return {{}, false, length};
    auto units = CopyUnits(text, length, caller);
    const auto size = Icu::Checked(static_cast<std::int64_t>(units.size()));
    return {std::move(units), true, size};
}

} // namespace

extern "C" {

void APS5_VABI ucol_close_nid_postfix(UCollator* coll) {
    if (!coll) return;
    CloseRegistered(coll, __func__);
}

std::int32_t APS5_VABI ucol_getAttribute_nid_postfix(const UCollator* coll, std::int32_t attr, std::int32_t* status) {
    const auto incoming = ReadStatus(status, __func__);
    if (incoming > 0 || !coll) return UcolDefault;
    auto outgoing = incoming;
    const auto registered = Registered(coll, __func__);
    const auto value = Icu::CollatorHost::GetAttribute(registered.handle, attr, outgoing);
    UpdateStatus(status, incoming, outgoing, __func__);
    return value;
}

void APS5_VABI ucol_getContractionsAndExpansions_nid_postfix(const UCollator* coll, USet*, USet*, std::int8_t, std::int32_t* status) {
    if (Failure(status, __func__)) return;
    if (!coll) {
        WriteStatus(status, Icu::IllegalArgument, __func__);
        return;
    }
    NotImplemented_nid_no_patch(__func__);
}

UEnumeration* APS5_VABI ucol_getKeywordValues_nid_postfix(const char* keyword, std::int32_t* status) {
    if (Failure(status, __func__)) return nullptr;
    if (!keyword || !CollationKeyword(keyword, __func__)) {
        WriteStatus(status, Icu::IllegalArgument, __func__);
        return nullptr;
    }
    NotImplemented_nid_no_patch(__func__);
}

UEnumeration* APS5_VABI ucol_getKeywordValuesForLocale_nid_postfix(const char*, const char*, std::int8_t, std::int32_t* status) {
    if (Failure(status, __func__)) return nullptr;
    NotImplemented_nid_no_patch(__func__);
}

std::int32_t APS5_VABI ucol_getReorderCodes_nid_postfix(const UCollator*, std::int32_t*, std::int32_t, std::int32_t* status) {
    if (Failure(status, __func__)) {
        return 0;
    }
    NotImplemented_nid_no_patch(__func__);
}

const char16_t* APS5_VABI ucol_getRules_nid_postfix(const UCollator*, std::int32_t*) {
    NotImplemented_nid_no_patch(__func__);
}

UCollationStrength APS5_VABI ucol_getStrength_nid_postfix(const UCollator* coll) {
    if (!coll) return UCOL_DEFAULT;
    std::int32_t status = 0;
    const auto registered = Registered(coll, __func__);
    return static_cast<UCollationStrength>(Icu::CollatorHost::GetAttribute(registered.handle, UCOL_STRENGTH, status));
}

USet* APS5_VABI ucol_getTailoredSet_nid_postfix(const UCollator*, std::int32_t* status) {
    if (Failure(status, __func__)) {
        return nullptr;
    }
    NotImplemented_nid_no_patch(__func__);
}

UCollator* APS5_VABI ucol_open_nid_postfix(const char* locale, std::int32_t* status) {
    const auto incoming = ReadStatus(status, __func__);
    if (incoming > 0) return nullptr;
    std::string name;
    if (locale) {
        const int savedErrno = errno;
        const int copied = CopyGuestString_nid_no_patch(locale, LocaleBytes, name);
        errno = savedErrno;
        if (copied != 0) NotImplemented_nid_no_patch(__func__);
    }
    auto outgoing = incoming;
    void* collator = Icu::CollatorHost::Open(locale ? name.c_str() : Icu::DefaultLocaleName(), outgoing);
    if (collator) {
        void* handle = nullptr;
        try {
            handle = Register(collator);
            UpdateStatus(status, incoming, outgoing, __func__);
        } catch (...) {
            Unregister(handle);
            Icu::CollatorHost::Close(collator);
            throw;
        }
        return static_cast<UCollator*>(handle);
    }
    UpdateStatus(status, incoming, outgoing, __func__);
    return nullptr;
}

void APS5_VABI ucol_setAttribute_nid_postfix(UCollator* coll, UColAttribute attr, UColAttributeValue value, std::int32_t* status) {
    const auto incoming = ReadStatus(status, __func__);
    if (incoming > 0 || !coll) return;
    auto outgoing = incoming;
    const auto registered = Registered(coll, __func__);
    Icu::CollatorHost::SetAttribute(registered.handle, attr, value, outgoing);
    UpdateStatus(status, incoming, outgoing, __func__);
}

void APS5_VABI ucol_setStrength_nid_postfix(UCollator* coll, UCollationStrength strength) {
    if (!coll) return;
    std::int32_t status = 0;
    const auto registered = Registered(coll, __func__);
    Icu::CollatorHost::SetAttribute(registered.handle, UCOL_STRENGTH, strength, status);
}

UCollationResult APS5_VABI ucol_strcoll_nid_postfix(const UCollator* coll, const char16_t* source, std::int32_t sourceLength,
                                                   const char16_t* target, std::int32_t targetLength) {
    const auto registered = Registered(coll, __func__);
    const auto left = CopyOperand(source, sourceLength, __func__);
    const auto right = CopyOperand(target, targetLength, __func__);
    return static_cast<UCollationResult>(
        Icu::CollatorHost::Compare(registered.handle, left.Data(), left.length, right.Data(), right.length));
}

std::int32_t APS5_VABI ucol_strcollIter_nid_postfix(const UCollator* coll, UCharIterator* sIter, UCharIterator* tIter, std::int32_t* status) {
    if (!status || Failure(status, __func__)) return 0;
    if (!sIter || !tIter || !coll) {
        WriteStatus(status, Icu::IllegalArgument, __func__);
        return 0;
    }
    NotImplemented_nid_no_patch(__func__);
}

UCollationResult APS5_VABI ucol_strcollUTF8_nid_postfix(const UCollator* coll, const char* source, std::int32_t sourceLength,
                                                       const char* target, std::int32_t targetLength, std::int32_t* status) {
    const auto incoming = ReadStatus(status, __func__);
    if (incoming > 0) return UCOL_EQUAL;
    const auto registered = Registered(coll, __func__);
    const auto left = CopyOperand(source, sourceLength, __func__);
    const auto right = CopyOperand(target, targetLength, __func__);
    auto outgoing = incoming;
    const auto result =
        Icu::CollatorHost::CompareUtf8(registered.handle, left.Data(), left.length, right.Data(), right.length, outgoing);
    UpdateStatus(status, incoming, outgoing, __func__);
    return static_cast<UCollationResult>(result);
}

}
