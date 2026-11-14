#include "prx/libSceIcu/ResourceBundle.hpp"
#include "prx/libSceIcu/ResourceBundleBackend.hpp"
#include "prx/libSceIcu/Enumeration.hpp"
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
std::unordered_map<const UResourceBundle*, std::shared_ptr<Entry>> Handles;
std::vector<std::unique_ptr<UEnumeration>> Tokens;

std::shared_ptr<Entry> Optional(const UResourceBundle* handle) {
    const std::lock_guard lock(RegistryMutex);
    const auto found = Handles.find(handle);
    return found == Handles.end() ? nullptr : found->second;
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
    Tokens.push_back(std::move(token));
    Handles.emplace(handle, std::move(owner));
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
        if (found == Handles.end()) NotImplemented_nid_no_patch(__func__);
        closing = std::move(found->second);
        Handles.erase(found);
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

}
