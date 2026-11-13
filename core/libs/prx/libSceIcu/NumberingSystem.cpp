#include "prx/libSceIcu/RecoveredSignatures.hpp"
#include "prx/libSceIcu/Enumeration.hpp"
#include "prx/libSceIcu/NumberingSystemBackend.hpp"
#include "prx/libc/include/General.hpp"
#include <cstdint>
#include <memory>
#include <mutex>
#include <new>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

namespace {

using Native = std::unique_ptr<void, void (*)(void*)>;
struct Entry {
    Native native;
    std::mutex mutex;
    explicit Entry(Native value) : native(std::move(value)) {}
};
using Token = UEnumeration;
using Registry = std::unordered_map<const void*, std::shared_ptr<Entry>>;
std::mutex RegistryMutex;
Registry Systems;
Registry Enumerations;
std::vector<std::unique_ptr<Token>> SystemTokens;
std::vector<std::unique_ptr<UEnumeration>> EnumerationTokens;

std::int32_t ReadStatus(const std::int32_t* pointer) {
    std::int32_t value;
    if (!GuestCopyIn_nid_no_patch(&value, pointer, sizeof(value))) NotImplemented_nid_no_patch(__func__);
    return value;
}

void WriteStatus(std::int32_t* pointer, std::int32_t before, std::int32_t after) {
    if (before != after && !GuestCopyOut_nid_no_patch(pointer, &after, sizeof(after))) NotImplemented_nid_no_patch(__func__);
}

std::string ReadName(const char* pointer) {
    if (!pointer) NotImplemented_nid_no_patch(__func__);
    auto address = reinterpret_cast<std::uintptr_t>(pointer);
    std::string result;
    for (;;) {
        char value;
        if (!GuestCopyIn_nid_no_patch(&value, reinterpret_cast<const void*>(address), sizeof(value))) NotImplemented_nid_no_patch(__func__);
        if (!value) return result;
        if (address == UINTPTR_MAX) APS5_INVALID_ARG_EX;
        result.push_back(value);
        ++address;
    }
}

std::shared_ptr<Entry> Find(const Registry& registry, const void* token) {
    const std::lock_guard lock(RegistryMutex);
    const auto found = registry.find(token);
    if (found == registry.end()) NotImplemented_nid_no_patch(__func__);
    return found->second;
}

void Remove(Registry& registry, const void* token) {
    std::shared_ptr<Entry> closing;
    {
        const std::lock_guard lock(RegistryMutex);
        const auto found = registry.find(token);
        if (found == registry.end()) NotImplemented_nid_no_patch(__func__);
        closing = std::move(found->second);
        registry.erase(found);
    }
}

void APS5_VABI CloseEnumeration_nid_no_patch(UEnumeration* token) {
    Remove(Enumerations, token);
}

std::int32_t APS5_VABI CountEnumeration_nid_no_patch(UEnumeration* token, std::int32_t* error) {
    const auto before = ReadStatus(error);
    if (before > 0) return -1;
    const auto owner = Find(Enumerations, token);
    const std::lock_guard lock(owner->mutex);
    auto status = before;
    const auto count = IcuNumberingBackend::Count_nid_no_patch(owner->native.get(), &status);
    WriteStatus(error, before, status);
    return count;
}

template<class Character, class Function>
const Character* Next(UEnumeration* token, std::int32_t* length, std::int32_t* error, Function&& function) {
    const auto before = ReadStatus(error);
    if (before > 0) return nullptr;
    const auto owner = Find(Enumerations, token);
    const std::lock_guard lock(owner->mutex);
    auto status = before;
    std::int32_t count = -1;
    const auto* result = function(owner->native.get(), &count, &status);
    if (length && count >= 0 && !GuestCopyOut_nid_no_patch(length, &count, sizeof(count))) NotImplemented_nid_no_patch(__func__);
    WriteStatus(error, before, status);
    return result;
}

const char* APS5_VABI NextEnumeration_nid_no_patch(UEnumeration* token, std::int32_t* length, std::int32_t* error) {
    return Next<char>(token, length, error, IcuNumberingBackend::Next_nid_no_patch);
}

const char16_t* APS5_VABI UNextEnumeration_nid_no_patch(UEnumeration* token, std::int32_t* length, std::int32_t* error) {
    return Next<char16_t>(token, length, error, IcuNumberingBackend::UNext_nid_no_patch);
}

void APS5_VABI ResetEnumeration_nid_no_patch(UEnumeration* token, std::int32_t* error) {
    const auto before = ReadStatus(error);
    if (before > 0) return;
    const auto owner = Find(Enumerations, token);
    const std::lock_guard lock(owner->mutex);
    auto status = before;
    IcuNumberingBackend::Reset_nid_no_patch(owner->native.get(), &status);
    WriteStatus(error, before, status);
}

UNumberingSystem* Open(const char* name, bool byName, std::int32_t* error) {
    const auto before = ReadStatus(error);
    if (before > 0) return nullptr;
    try {
        const auto input = ReadName(name);
        auto status = before;
        Native native(IcuNumberingBackend::Open_nid_no_patch(input.c_str(), byName, &status), IcuNumberingBackend::Close_nid_no_patch);
        if (!native) {
            WriteStatus(error, before, status);
            return nullptr;
        }
        auto owner = std::make_shared<Entry>(std::move(native));
        auto token = std::make_unique<Token>(Token{nullptr, nullptr,
            CloseEnumeration_nid_no_patch, CountEnumeration_nid_no_patch, UNextEnumeration_nid_no_patch,
            NextEnumeration_nid_no_patch, ResetEnumeration_nid_no_patch});
        auto* handle = reinterpret_cast<UNumberingSystem*>(token.get());
        WriteStatus(error, before, status);
        const std::lock_guard lock(RegistryMutex);
        SystemTokens.push_back(std::move(token));
        Systems.emplace(handle, std::move(owner));
        return handle;
    } catch (const std::bad_alloc&) {
        WriteStatus(error, before, 7);
        return nullptr;
    }
}

} // namespace

extern "C" {

void APS5_VABI unumsys_close_nid_postfix(UNumberingSystem* handle) {
    if (!handle) return;
    Remove(Systems, handle);
}

UNumberingSystem* APS5_VABI unumsys_open_nid_postfix(const char* locale, std::int32_t* status) {
    return Open(locale, false, status);
}

UNumberingSystem* APS5_VABI unumsys_openByName_nid_postfix(const char* name, std::int32_t* status) {
    return Open(name, true, status);
}

const char* APS5_VABI unumsys_getName_nid_postfix(const UNumberingSystem* handle) {
    const auto owner = Find(Systems, handle);
    const std::lock_guard lock(owner->mutex);
    return IcuNumberingBackend::Name_nid_no_patch(owner->native.get());
}

std::int8_t APS5_VABI unumsys_isAlgorithmic_nid_postfix(const UNumberingSystem* handle) {
    const auto owner = Find(Systems, handle);
    const std::lock_guard lock(owner->mutex);
    return IcuNumberingBackend::Algorithmic_nid_no_patch(owner->native.get());
}

UEnumeration* APS5_VABI unumsys_openAvailableNames_nid_postfix(std::int32_t* error) {
    const auto before = ReadStatus(error);
    if (before > 0) return nullptr;
    try {
        auto status = before;
        Native native(IcuNumberingBackend::Enumeration_nid_no_patch(&status), IcuNumberingBackend::EnumClose_nid_no_patch);
        if (!native) {
            WriteStatus(error, before, status);
            return nullptr;
        }
        auto owner = std::make_shared<Entry>(std::move(native));
        auto token = std::make_unique<UEnumeration>(UEnumeration{nullptr, nullptr,
            CloseEnumeration_nid_no_patch, CountEnumeration_nid_no_patch, UNextEnumeration_nid_no_patch,
            NextEnumeration_nid_no_patch, ResetEnumeration_nid_no_patch});
        auto* handle = token.get();
        WriteStatus(error, before, status);
        const std::lock_guard lock(RegistryMutex);
        EnumerationTokens.push_back(std::move(token));
        Enumerations.emplace(handle, std::move(owner));
        return handle;
    } catch (const std::bad_alloc&) {
        WriteStatus(error, before, 7);
        return nullptr;
    }
}

}
