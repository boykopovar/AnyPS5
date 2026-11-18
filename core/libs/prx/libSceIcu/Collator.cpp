#include "prx/libSceIcu/CollatorHost.hpp"
#include "prx/libSceIcu/RecoveredSignatures.hpp"
#include "prx/libSceIcu/Status.hpp"
#include "prx/libc/include/General.hpp"
#include <algorithm>
#include <cerrno>
#include <cstddef>
#include <cstdint>
#include <mutex>
#include <string>
#include <unordered_set>
#include <utility>

namespace {

constexpr std::int32_t UcolDefault = -1;
constexpr std::size_t LocaleBytes = 4096;
constexpr std::size_t PageBytes = 4096;

struct Registry {
    std::mutex mutex;
    std::unordered_set<const void*> collators;
};

Registry& Collators() {
    static Registry registry;
    return registry;
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

void WriteStatus(std::int32_t* status, std::int32_t incoming, std::int32_t outgoing, const char* caller) {
    if (outgoing == incoming) return;
    const int savedErrno = errno;
    const bool ok = GuestCopyOut_nid_no_patch(status, &outgoing, sizeof(outgoing));
    errno = savedErrno;
    if (!ok) NotImplemented_nid_no_patch(caller);
}

void* Registered(const UCollator* coll, const char* caller) {
    auto& registry = Collators();
    const std::lock_guard lock(registry.mutex);
    if (!registry.collators.contains(coll)) NotImplemented_nid_no_patch(caller);
    return const_cast<UCollator*>(coll);
}

void Register(void* collator) {
    auto& registry = Collators();
    const std::lock_guard lock(registry.mutex);
    registry.collators.insert(collator);
}

bool Unregister(const void* collator) {
    auto& registry = Collators();
    const std::lock_guard lock(registry.mutex);
    return registry.collators.erase(collator) != 0;
}

template<class TUnit>
std::basic_string<TUnit> CopyUnits(const TUnit* text, std::int32_t length, const char* caller) {
    std::basic_string<TUnit> units;
    if (length >= 0) {
        units.resize(static_cast<std::size_t>(length));
        if (length > 0 && !CopyIn(units.data(), text, units.size() * sizeof(TUnit))) NotImplemented_nid_no_patch(caller);
        return units;
    }
    auto address = reinterpret_cast<std::uintptr_t>(text);
    while (true) {
        const std::size_t count = std::max<std::size_t>((PageBytes - address % PageBytes) / sizeof(TUnit), 1);
        const std::size_t offset = units.size();
        units.resize(offset + count);
        if (!CopyIn(units.data() + offset, reinterpret_cast<const void*>(address), count * sizeof(TUnit))) {
            NotImplemented_nid_no_patch(caller);
        }
        const auto end = std::find(units.begin() + static_cast<std::ptrdiff_t>(offset), units.end(), TUnit{});
        if (end != units.end()) {
            units.erase(end, units.end());
            return units;
        }
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
    if (!Unregister(coll)) NotImplemented_nid_no_patch(__func__);
    Icu::CollatorHost::Close(coll);
}

std::int32_t APS5_VABI ucol_getAttribute_nid_postfix(const UCollator* coll, std::int32_t attr, std::int32_t* status) {
    const auto incoming = ReadStatus(status, __func__);
    if (incoming > 0 || !coll) return UcolDefault;
    auto outgoing = incoming;
    const auto value = Icu::CollatorHost::GetAttribute(Registered(coll, __func__), attr, outgoing);
    WriteStatus(status, incoming, outgoing, __func__);
    return value;
}

std::int32_t APS5_VABI ucol_getReorderCodes_nid_postfix(const UCollator*, std::int32_t*, std::int32_t, std::int32_t* status) {
    if (Failure(status, __func__)) {
        return 0;
    }
    NotImplemented_nid_no_patch(__func__);
}

UCollationStrength APS5_VABI ucol_getStrength_nid_postfix(const UCollator* coll) {
    if (!coll) return UCOL_DEFAULT;
    std::int32_t status = 0;
    return static_cast<UCollationStrength>(Icu::CollatorHost::GetAttribute(Registered(coll, __func__), UCOL_STRENGTH, status));
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
    void* collator = Icu::CollatorHost::Open(locale ? name.c_str() : nullptr, outgoing);
    if (collator) {
        try {
            Register(collator);
            WriteStatus(status, incoming, outgoing, __func__);
        } catch (...) {
            Unregister(collator);
            Icu::CollatorHost::Close(collator);
            throw;
        }
        return static_cast<UCollator*>(collator);
    }
    WriteStatus(status, incoming, outgoing, __func__);
    return nullptr;
}

void APS5_VABI ucol_setAttribute_nid_postfix(UCollator* coll, UColAttribute attr, UColAttributeValue value, std::int32_t* status) {
    const auto incoming = ReadStatus(status, __func__);
    if (incoming > 0 || !coll) return;
    auto outgoing = incoming;
    Icu::CollatorHost::SetAttribute(Registered(coll, __func__), attr, value, outgoing);
    WriteStatus(status, incoming, outgoing, __func__);
}

void APS5_VABI ucol_setStrength_nid_postfix(UCollator* coll, UCollationStrength strength) {
    if (!coll) return;
    std::int32_t status = 0;
    Icu::CollatorHost::SetAttribute(Registered(coll, __func__), UCOL_STRENGTH, strength, status);
}

UCollationResult APS5_VABI ucol_strcoll_nid_postfix(const UCollator* coll, const char16_t* source, std::int32_t sourceLength,
                                                   const char16_t* target, std::int32_t targetLength) {
    const void* handle = Registered(coll, __func__);
    const auto left = CopyOperand(source, sourceLength, __func__);
    const auto right = CopyOperand(target, targetLength, __func__);
    return static_cast<UCollationResult>(Icu::CollatorHost::Compare(handle, left.Data(), left.length, right.Data(), right.length));
}

UCollationResult APS5_VABI ucol_strcollUTF8_nid_postfix(const UCollator* coll, const char* source, std::int32_t sourceLength,
                                                       const char* target, std::int32_t targetLength, std::int32_t* status) {
    const auto incoming = ReadStatus(status, __func__);
    if (incoming > 0) return UCOL_EQUAL;
    const void* handle = Registered(coll, __func__);
    const auto left = CopyOperand(source, sourceLength, __func__);
    const auto right = CopyOperand(target, targetLength, __func__);
    auto outgoing = incoming;
    const auto result = Icu::CollatorHost::CompareUtf8(handle, left.Data(), left.length, right.Data(), right.length, outgoing);
    WriteStatus(status, incoming, outgoing, __func__);
    return static_cast<UCollationResult>(result);
}

}
