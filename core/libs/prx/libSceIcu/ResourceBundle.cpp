// © 2016 and later: Unicode, Inc. and others.
// License & terms of use: http://www.unicode.org/copyright.html
/*
******************************************************************************
* Copyright (C) 1997-2016, International Business Machines Corporation and
* others. All Rights Reserved.
******************************************************************************
*/
// https://github.com/unicode-org/icu/blob/84e1f26ea77152936e70d53178a816dbfbf69989/icu4c/source/common/uresbund.cpp

#include "prx/libSceIcu/RecoveredSignatures.hpp"
#include "prx/libc/include/General.hpp"
#include <cerrno>
#include <cstddef>
#include <cstdint>
#include <cstring>

#include "prx/libSceIcu/ResourceBundle.hpp"
#include "prx/libSceIcu/ResourceBundleBackend.hpp"
#include "prx/libSceIcu/Enumeration.hpp"
#include <memory>
#include <mutex>
#include <new>
#include <string>
#include <unordered_map>
#include <utility>

namespace {

constexpr std::size_t Icu68ResourceBundleSize = 200;
constexpr std::size_t Icu68ResourceDataEntryOffset = 8;
constexpr std::size_t Icu68ResourceOffset = 172;
constexpr std::uint32_t IntegerType = 7;
constexpr std::uint32_t TypeShift = 28;
constexpr std::uint32_t ValueMask = 0x0fffffff;
constexpr std::int32_t IllegalArgument = 1;
constexpr std::int32_t ResourceTypeMismatch = 17;

bool CopyIn(void* destination, const void* source, std::size_t bytes, int savedErrno, const char* caller) {
    const bool copied = GuestCopyIn_nid_no_patch(destination, source, bytes);
    errno = savedErrno;
    if (!copied) NotImplemented_nid_no_patch(caller);
    return copied;
}

void CopyOut(void* destination, const void* source, std::size_t bytes, int savedErrno, const char* caller) {
    const bool copied = GuestCopyOut_nid_no_patch(destination, source, bytes);
    errno = savedErrno;
    if (!copied) NotImplemented_nid_no_patch(caller);
}


using Native = std::unique_ptr<void, decltype(&IcuResourceBackend::Close_nid_no_patch)>;
struct Entry {
    Native native;
    std::mutex mutex;
    explicit Entry(Native value) : native(std::move(value)) {}
};
struct Status {
    std::int32_t* pointer;
    std::int32_t before = 0;
    explicit Status(std::int32_t* value) : pointer(value) {
        if (pointer && !GuestCopyIn_nid_no_patch(&before, pointer, sizeof(before))) NotImplemented_nid_no_patch(__func__);
    }
    bool Blocked() const { return !pointer || before > 0; }
    void Write(std::int32_t value) const {
        if (value != before && !GuestCopyOut_nid_no_patch(pointer, &value, sizeof(value))) NotImplemented_nid_no_patch(__func__);
    }
};
std::mutex RegistryMutex;
struct Slot {
    std::unique_ptr<UEnumeration> token;
    std::shared_ptr<Entry> owner;
};
std::unordered_map<const UResourceBundle*, Slot> Handles;

std::shared_ptr<Entry> Optional(const UResourceBundle* handle, bool* known = nullptr) {
    const std::lock_guard lock(RegistryMutex);
    const auto found = Handles.find(handle);
    if (known) *known = found != Handles.end();
    return found == Handles.end() ? nullptr : found->second.owner;
}
std::shared_ptr<Entry> Find(const UResourceBundle* handle) {
    auto result = Optional(handle);
    if (!result) NotImplemented_nid_no_patch(__func__);
    return result;
}
void APS5_VABI RefuseClose_nid_no_patch(UEnumeration*) { NotImplemented_nid_no_patch(__func__); }
std::int32_t APS5_VABI RefuseCount_nid_no_patch(UEnumeration*, std::int32_t*) { NotImplemented_nid_no_patch(__func__); }
const char* APS5_VABI RefuseNext_nid_no_patch(UEnumeration*, std::int32_t*, std::int32_t*) { NotImplemented_nid_no_patch(__func__); }
const char16_t* APS5_VABI RefuseUNext_nid_no_patch(UEnumeration*, std::int32_t*, std::int32_t*) { NotImplemented_nid_no_patch(__func__); }
void APS5_VABI RefuseReset_nid_no_patch(UEnumeration*, std::int32_t*) { NotImplemented_nid_no_patch(__func__); }

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

UResourceBundle* Publish(Native native, const Status& status, std::int32_t after) {
    if (!native) {
        status.Write(after);
        return nullptr;
    }
    auto owner = std::make_shared<Entry>(std::move(native));
    auto token = std::make_unique<UEnumeration>(UEnumeration{nullptr, nullptr, RefuseClose_nid_no_patch,
        RefuseCount_nid_no_patch, RefuseUNext_nid_no_patch, RefuseNext_nid_no_patch, RefuseReset_nid_no_patch});
    auto* handle = reinterpret_cast<UResourceBundle*>(token.get());
    status.Write(after);
    const std::lock_guard lock(RegistryMutex);
    Handles.emplace(handle, Slot{std::move(token), std::move(owner)});
    return handle;
}

UResourceBundle* Open(const char* path, const char* name, bool direct, std::int32_t* error) {
    const Status status(error);
    if (status.Blocked()) return nullptr;
    try {
        const auto input = ReadName(name);
        const auto package = path ? ReadName(path) : std::string();
        auto after = status.before;
        Native native(IcuResourceBackend::Open_nid_no_patch(path ? package.c_str() : nullptr, input.c_str(), direct, &after),
            IcuResourceBackend::Close_nid_no_patch);
        return Publish(std::move(native), status, after);
    } catch (const std::bad_alloc&) {
        status.Write(7);
        return nullptr;
    }
}

UResourceBundle* ChildLocked(const std::shared_ptr<Entry>& parent, const char* key, std::int32_t index, bool byKey,
                             UResourceBundle* fillIn, const std::shared_ptr<Entry>& receiver, const Status& status) {
    if (byKey && IcuResourceBackend::Type_nid_no_patch(parent->native.get()) != 2) {
        status.Write(17);
        return fillIn;
    }
    if (!byKey && (index < 0 || index >= IcuResourceBackend::Size_nid_no_patch(parent->native.get()))) {
        status.Write(2);
        return fillIn;
    }
    if (fillIn && !receiver) NotImplemented_nid_no_patch(__func__);
    const auto input = byKey ? ReadName(key) : std::string();
    auto after = status.before;
    auto* result = IcuResourceBackend::Child_nid_no_patch(parent->native.get(), byKey ? input.c_str() : nullptr,
        index, byKey, receiver ? receiver->native.get() : nullptr, &after);
    if (receiver) {
        if (result && result != receiver->native.get()) NotImplemented_nid_no_patch(__func__);
        status.Write(after);
        return result ? fillIn : nullptr;
    }
    return Publish(Native(result, IcuResourceBackend::Close_nid_no_patch), status, after);
}

UResourceBundle* Child(const UResourceBundle* handle, const char* key, std::int32_t index, bool byKey,
                       UResourceBundle* fillIn, std::int32_t* error) {
    const Status status(error);
    if (status.Blocked()) return fillIn;
    if (!handle) {
        status.Write(1);
        return fillIn;
    }
    try {
        const auto parent = Find(handle);
        const auto receiver = fillIn ? Optional(fillIn) : nullptr;
        if (receiver && receiver != parent) {
            const std::scoped_lock locks(parent->mutex, receiver->mutex);
            return ChildLocked(parent, key, index, byKey, fillIn, receiver, status);
        }
        const std::lock_guard lock(parent->mutex);
        return ChildLocked(parent, key, index, byKey, fillIn, receiver, status);
    } catch (const std::bad_alloc&) {
        status.Write(7);
        return fillIn;
    }
}

template<class Character, class Function>
const Character* Extract(std::int32_t* length, const Status& status, Function&& function) {
    auto after = status.before;
    std::int32_t count = -1;
    const auto* result = function(&count, &after);
    if (length && count >= 0 && !GuestCopyOut_nid_no_patch(length, &count, sizeof(count))) NotImplemented_nid_no_patch(__func__);
    status.Write(after);
    return result;
}

const char16_t* String(const UResourceBundle* handle, int mode, const char* key, std::int32_t index,
                       std::int32_t* length, std::int32_t* error) {
    const Status status(error);
    if (status.Blocked()) return nullptr;
    if (!handle) {
        status.Write(1);
        return nullptr;
    }
    try {
        const auto owner = Find(handle);
        const std::lock_guard lock(owner->mutex);
        if (mode == 1 && IcuResourceBackend::Type_nid_no_patch(owner->native.get()) != 2) {
            status.Write(17);
            return nullptr;
        }
        const auto input = mode == 1 ? ReadName(key) : std::string();
        return Extract<char16_t>(length, status, [&](std::int32_t* count, std::int32_t* after) {
            return IcuResourceBackend::String_nid_no_patch(owner->native.get(), mode, mode == 1 ? input.c_str() : nullptr, index, count, after);
        });
    } catch (const std::bad_alloc&) {
        status.Write(7);
        return nullptr;
    }
}

template<class Character, class Function>
const Character* Data(const UResourceBundle* handle, std::int32_t* length, std::int32_t* error, Function&& function) {
    const Status status(error);
    if (status.Blocked()) return nullptr;
    if (!handle) {
        status.Write(1);
        return nullptr;
    }
    const auto owner = Find(handle);
    const std::lock_guard lock(owner->mutex);
    return Extract<Character>(length, status, [&](std::int32_t* count, std::int32_t* after) {
        return function(owner->native.get(), count, after);
    });
}

} // namespace

namespace IcuResourceBundle {

std::optional<bool> OwnedHasNext_nid_no_patch(const UResourceBundle* handle) {
    bool known;
    const auto owner = Optional(handle, &known);
    if (!known) return std::nullopt;
    if (!owner) NotImplemented_nid_no_patch(__func__);
    const std::lock_guard lock(owner->mutex);
    return IcuResourceBackend::HasNext_nid_no_patch(owner->native.get());
}

bool CopyOwnedString_nid_no_patch(const UResourceBundle* handle, StringLookup lookup, const char* key,
                                 std::int32_t index, std::vector<char16_t>& units, std::int32_t& status) {
    bool known;
    const auto owner = Optional(handle, &known);
    if (!known) return false;
    if (!owner) NotImplemented_nid_no_patch(__func__);
    const std::lock_guard lock(owner->mutex);
    try {
        std::string input;
        if (lookup == StringLookup::Key) {
            if (IcuResourceBackend::Type_nid_no_patch(owner->native.get()) != 2) {
                status = ResourceTypeMismatch;
                return true;
            }
            if (!key) {
                status = 2;
                return true;
            }
            const int savedErrno = errno;
            const int error = CopyGuestString_nid_no_patch(key, 0x10000, input);
            errno = savedErrno;
            if (error != 0) NotImplemented_nid_no_patch(__func__);
        }
        std::int32_t size = 0;
        const auto* text = IcuResourceBackend::String_nid_no_patch(owner->native.get(), static_cast<int>(lookup),
            lookup == StringLookup::Key ? input.c_str() : nullptr, index, &size, &status);
        if (text && size > 0) units.assign(text, text + size);
    } catch (const std::bad_alloc&) {
        status = 7;
    }
    return true;
}

} // namespace IcuResourceBundle

extern "C" {

UResourceBundle* APS5_VABI ures_open(const char* path, const char* name, std::int32_t* status) {
    return Open(path, name, false, status);
}
UResourceBundle* APS5_VABI ures_openDirect_nid_postfix(const char* path, const char* name, std::int32_t* status) {
    return Open(path, name, true, status);
}
void APS5_VABI ures_close(UResourceBundle* handle) {
    if (!handle) return;
    std::shared_ptr<Entry> closing;
    {
        const std::lock_guard lock(RegistryMutex);
        const auto found = Handles.find(handle);
        if (found == Handles.end() || !found->second.owner) NotImplemented_nid_no_patch(__func__);
        closing = std::move(found->second.owner);
    }
}
UResourceBundle* APS5_VABI ures_getByKey(const UResourceBundle* handle, const char* key, UResourceBundle* fillIn, std::int32_t* status) {
    return Child(handle, key, 0, true, fillIn, status);
}
UResourceBundle* APS5_VABI ures_getByIndex_nid_postfix(const UResourceBundle* handle, std::int32_t index, UResourceBundle* fillIn, std::int32_t* status) {
    return Child(handle, nullptr, index, false, fillIn, status);
}
std::int32_t APS5_VABI ures_getSize_nid_postfix(const UResourceBundle* handle) {
    if (!handle) return 0;
    const auto owner = Find(handle);
    const std::lock_guard lock(owner->mutex);
    return IcuResourceBackend::Size_nid_no_patch(owner->native.get());
}
std::int32_t APS5_VABI ures_getType_nid_postfix(const UResourceBundle* handle) {
    if (!handle) return -1;
    const auto owner = Find(handle);
    const std::lock_guard lock(owner->mutex);
    return IcuResourceBackend::Type_nid_no_patch(owner->native.get());
}
const char* APS5_VABI ures_getKey_nid_postfix(const UResourceBundle* handle) {
    if (!handle) return nullptr;
    const auto owner = Find(handle);
    const std::lock_guard lock(owner->mutex);
    return IcuResourceBackend::Key_nid_no_patch(owner->native.get());
}
const char16_t* APS5_VABI ures_getString_nid_postfix(const UResourceBundle* handle, std::int32_t* length, std::int32_t* status) {
    return String(handle, 0, nullptr, 0, length, status);
}
const char16_t* APS5_VABI ures_getStringByKey(const UResourceBundle* handle, const char* key, std::int32_t* length, std::int32_t* status) {
    return String(handle, 1, key, 0, length, status);
}
const char16_t* APS5_VABI ures_getStringByIndex_nid_postfix(const UResourceBundle* handle, std::int32_t index, std::int32_t* length, std::int32_t* status) {
    return String(handle, 2, nullptr, index, length, status);
}
std::int32_t APS5_VABI ures_getInt_nid_postfix(const UResourceBundle* handle, std::int32_t* error) {
    const Status status(error);
    if (status.Blocked()) return -1;
    if (!handle) {
        status.Write(1);
        return -1;
    }
    const auto owner = Find(handle);
    const std::lock_guard lock(owner->mutex);
    auto after = status.before;
    const auto result = IcuResourceBackend::Int_nid_no_patch(owner->native.get(), &after);
    status.Write(after);
    return result;
}
const std::uint8_t* APS5_VABI ures_getBinary_nid_postfix(const UResourceBundle* handle, std::int32_t* length, std::int32_t* status) {
    return Data<std::uint8_t>(handle, length, status, IcuResourceBackend::Binary_nid_no_patch);
}
const std::int32_t* APS5_VABI ures_getIntVector_nid_postfix(const UResourceBundle* handle, std::int32_t* length, std::int32_t* status) {
    return Data<std::int32_t>(handle, length, status, IcuResourceBackend::Vector_nid_no_patch);
}

void APS5_VABI ures_initStackObject_nid_postfix(UResourceBundle* resB) {
    bool known;
    Optional(resB, &known);
    if (known) NotImplemented_nid_no_patch(__func__);
    std::memset(resB, 0, Icu68ResourceBundleSize);
}

const char* APS5_VABI ures_getLocaleInternal_nid_postfix(const UResourceBundle* bundle, std::int32_t* status) {
    if (!status) return nullptr;
    const int savedErrno = errno;
    std::int32_t currentStatus;
    CopyIn(&currentStatus, status, sizeof(currentStatus), savedErrno, __func__);
    if (currentStatus > 0) return nullptr;
    if (!bundle) {
        currentStatus = IllegalArgument;
        CopyOut(status, &currentStatus, sizeof(currentStatus), savedErrno, __func__);
        return nullptr;
    }

    const auto address = reinterpret_cast<std::uintptr_t>(bundle);
    if (address > UINTPTR_MAX - Icu68ResourceDataEntryOffset) NotImplemented_nid_no_patch(__func__);
    const void* entry;
    CopyIn(&entry, reinterpret_cast<const void*>(address + Icu68ResourceDataEntryOffset), sizeof(entry), savedErrno,
           __func__);
    const char* name;
    CopyIn(&name, entry, sizeof(name), savedErrno, __func__);
    return name;
}

const char* APS5_VABI ures_getLocale_nid_postfix(const UResourceBundle* bundle, std::int32_t* status) {
    return ures_getLocaleInternal_nid_postfix(bundle, status);
}

std::uint32_t APS5_VABI ures_getUInt_nid_postfix(const UResourceBundle* resB, std::int32_t* status) {
    constexpr std::uint32_t failure = UINT32_MAX;
    if (!status) return failure;

    const int savedErrno = errno;
    std::int32_t currentStatus;
    CopyIn(&currentStatus, status, sizeof(currentStatus), savedErrno, __func__);
    if (currentStatus > 0) return failure;
    if (!resB) {
        currentStatus = IllegalArgument;
        CopyOut(status, &currentStatus, sizeof(currentStatus), savedErrno, __func__);
        return failure;
    }

    bool known;
    const auto owner = Optional(resB, &known);
    if (known) {
        if (!owner) NotImplemented_nid_no_patch(__func__);
        const std::lock_guard lock(owner->mutex);
        auto after = currentStatus;
        const auto value = IcuResourceBackend::UInt_nid_no_patch(owner->native.get(), &after);
        errno = savedErrno;
        if (after != currentStatus) CopyOut(status, &after, sizeof(after), savedErrno, __func__);
        return value;
    }

    std::uint32_t resource;
    const auto* resourceAddress = reinterpret_cast<const std::uint8_t*>(resB) + Icu68ResourceOffset;
    CopyIn(&resource, resourceAddress, sizeof(resource), savedErrno, __func__);
    if ((resource >> TypeShift) != IntegerType) {
        currentStatus = ResourceTypeMismatch;
        CopyOut(status, &currentStatus, sizeof(currentStatus), savedErrno, __func__);
        return failure;
    }
    return resource & ValueMask;
}

}
