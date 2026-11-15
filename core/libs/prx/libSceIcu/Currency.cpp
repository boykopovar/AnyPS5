#include "prx/libSceIcu/Currency.hpp"
#include "prx/libSceIcu/CurrencyBackend.hpp"
#include "prx/libSceIcu/Enumeration.hpp"
#include "prx/libSceIcu/include/InventoryExports.hpp"
#include "prx/libc/include/General.hpp"
#include "prx/libc/include/GuestArena.hpp"
#include <array>
#include <cerrno>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>
#include <utility>

namespace {

class PreserveErrno {
    int saved = errno;
public:
    ~PreserveErrno() { errno = saved; }
};

std::recursive_mutex CurrencyMutex;

template<class T>
T Read(const T* pointer) {
    T value;
    if (!GuestCopyIn_nid_no_patch(&value, pointer, sizeof(value))) NotImplemented_nid_no_patch("ucurr: unreadable input");
    return value;
}

template<class T>
std::basic_string<T> ReadString(const T* pointer) {
    if (!pointer) NotImplemented_nid_no_patch("ucurr: null string");
    std::basic_string<T> result;
    auto address = reinterpret_cast<std::uintptr_t>(pointer);
    for (;;) {
        const T value = Read(reinterpret_cast<const T*>(address));
        if (value == 0) return result;
        result.push_back(value);
        if (address > UINTPTR_MAX - sizeof(T)) NotImplemented_nid_no_patch("ucurr: string address overflow");
        address += sizeof(T);
    }
}

void Probe(void* pointer, std::size_t bytes) {
    GuestArena::HostWrite access(pointer, bytes);
    if (!access.Open() || !GuestWritable_nid_no_patch(pointer, bytes)) NotImplemented_nid_no_patch("ucurr: unwritable output");
}

void Write(void* pointer, const void* value, std::size_t bytes) {
    if (!GuestCopyOut_nid_no_patch(pointer, value, bytes)) NotImplemented_nid_no_patch("ucurr: output write failed");
}

class Status {
    std::int32_t* pointer;
    std::int32_t original;
public:
    std::int32_t value;
    explicit Status(std::int32_t* output) : pointer(output), original(Read(output)), value(original) {}
    void ProbeOutput() { Probe(pointer, sizeof(value)); }
    void ProbeChanged() { if (value != original) ProbeOutput(); }
    void Publish() {
        if (value == original) return;
        ProbeOutput();
        Write(pointer, &value, sizeof(value));
    }
    template<class T>
    T Finish(T result) {
        Publish();
        return result;
    }
};

std::array<char16_t, 4> Prefix(const char16_t* currency, bool metadata, bool failed = false) {
    std::array<char16_t, 4> result{};
    if (metadata) {
        if (!currency) return result;
        result[0] = Read(currency);
        if (result[0] == 0 || failed) return result;
    }
    if (!GuestCopyIn_nid_no_patch(result.data(), currency, 3 * sizeof(char16_t))) NotImplemented_nid_no_patch("ucurr: unreadable currency prefix");
    return result;
}

std::string Locale(const char* locale, Status& status) {
    const auto defaultLocale = ReadString(uloc_getDefault());
    IcuCurrency::SetDefault_nid_no_patch(defaultLocale.c_str(), status.value);
    return locale ? ReadString(locale) : defaultLocale;
}

std::int32_t PublishCode(const IcuCurrency::CodeResult& result, char16_t* output, Status& status) {
    if (result.length > 3) NotImplemented_nid_no_patch("ucurr: non-ISO currency data");
    if (result.written) {
        const auto bytes = static_cast<std::size_t>(result.written) * sizeof(char16_t);
        Probe(output, bytes);
        status.ProbeChanged();
        Write(output, result.text.data(), bytes);
    }
    return status.Finish(result.length);
}

std::int32_t Digits(const char16_t* currency, std::int32_t usage, std::int32_t* output) {
    const PreserveErrno preserve;
    const std::lock_guard lock(CurrencyMutex);
    Status status(output);
    if (status.value > 0) return 0;
    if (usage < 0 || usage > 1) {
        status.value = 16;
        return status.Finish(0);
    }
    const auto code = Prefix(currency, true);
    const auto result = IcuCurrency::FractionDigits_nid_no_patch(currency ? code.data() : nullptr, usage, status.value);
    return status.Finish(result);
}

double Rounding(const char16_t* currency, std::int32_t usage, std::int32_t* output) {
    const PreserveErrno preserve;
    const std::lock_guard lock(CurrencyMutex);
    Status status(output);
    const auto code = Prefix(currency, true, status.value > 0);
    const auto result = IcuCurrency::RoundingIncrement_nid_no_patch(currency ? code.data() : nullptr, usage, status.value);
    return status.Finish(result);
}

struct NameInput {
    const char16_t* currency;
    const char* plural;
    bool lengthAvailable;
    std::array<char16_t, 4> prefix{};
    std::u16string complete;
    std::string keyword;
};

const char16_t* ReadNameCurrency(void* context, bool terminated) {
    auto& input = *static_cast<NameInput*>(context);
    if (terminated) {
        if (!input.lengthAvailable) NotImplemented_nid_no_patch("ucurr: missing fallback length");
        input.complete = ReadString(input.currency);
        if (input.complete.size() > INT32_MAX) NotImplemented_nid_no_patch("ucurr: currency input exceeds INT32_MAX");
        return input.complete.c_str();
    }
    input.prefix = Prefix(input.currency, false);
    return input.prefix.data();
}

const char* ReadNamePlural(void* context) {
    auto& input = *static_cast<NameInput*>(context);
    if (!input.plural) return nullptr;
    input.keyword = ReadString(input.plural);
    return input.keyword.c_str();
}

const char16_t* Name(const char16_t* currency, const char* locale, std::int32_t style, std::int8_t* choice,
                     const char* plural, bool pluralName, std::int32_t* length, std::int32_t* output) {
    const PreserveErrno preserve;
    const std::lock_guard lock(CurrencyMutex);
    Status status(output);
    if (status.value > 0) return nullptr;
    if (!pluralName && (style < 0 || style > 4)) {
        status.value = 1;
        return status.Finish(static_cast<const char16_t*>(nullptr));
    }
    const auto localeName = Locale(locale, status);
    NameInput input{currency, plural, length != nullptr};
    const auto result = IcuCurrency::Name_nid_no_patch(localeName.c_str(), style, pluralName, choice != nullptr, ReadNameCurrency, ReadNamePlural, &input, status.value);
    if (length && result.length != INT32_MIN) Probe(length, sizeof(*length));
    if (choice && result.choice != -1) Probe(choice, sizeof(*choice));
    status.ProbeChanged();
    if (length && result.length != INT32_MIN) Write(length, &result.length, sizeof(*length));
    if (choice && result.choice != -1) Write(choice, &result.choice, sizeof(*choice));
    return status.Finish(result.inputAlias ? currency : result.text);
}

struct EnumerationBox {
    UEnumeration guest{};
    void* native = nullptr;
    IcuCurrency::NextResult pending;
    std::int32_t pendingStatus = 0;
    bool hasPending = false;
    bool pendingUnicode = false;
    bool pendingStatusChanged = false;
};

std::unordered_map<UEnumeration*, std::unique_ptr<EnumerationBox>> Enumerations;

EnumerationBox& FindEnumeration(UEnumeration* enumeration) {
    const auto found = Enumerations.find(enumeration);
    if (found == Enumerations.end()) NotImplemented_nid_no_patch("ucurr: unknown enumeration");
    return *found->second;
}

void APS5_VABI CloseEnumeration(UEnumeration* enumeration) {
    const PreserveErrno preserve;
    const std::lock_guard lock(CurrencyMutex);
    const auto found = Enumerations.find(enumeration);
    if (found == Enumerations.end()) NotImplemented_nid_no_patch("ucurr: unknown enumeration");
    auto box = std::move(found->second);
    Enumerations.erase(found);
    IcuCurrency::CloseEnumeration_nid_no_patch(box->native);
}

std::int32_t APS5_VABI CountEnumeration(UEnumeration* enumeration, std::int32_t* output) {
    const PreserveErrno preserve;
    const std::lock_guard lock(CurrencyMutex);
    Status status(output);
    if (status.value > 0) return -1;
    auto& box = FindEnumeration(enumeration);
    return status.Finish(IcuCurrency::CountEnumeration_nid_no_patch(box.native, status.value));
}

const void* NextEnumeration(UEnumeration* enumeration, std::int32_t* length, std::int32_t* output, bool unicode) {
    const PreserveErrno preserve;
    const std::lock_guard lock(CurrencyMutex);
    Status status(output);
    if (status.value > 0) return nullptr;
    auto& box = FindEnumeration(enumeration);
    if (box.hasPending) {
        if (box.pendingUnicode != unicode) NotImplemented_nid_no_patch("ucurr: pending enumeration encoding changed");
        if (box.pendingStatusChanged) status.value = box.pendingStatus;
    } else {
        const auto incoming = status.value;
        box.pending = IcuCurrency::NextEnumeration_nid_no_patch(box.native, unicode, status.value);
        box.pendingStatus = status.value;
        box.pendingStatusChanged = status.value != incoming;
        box.pendingUnicode = unicode;
        box.hasPending = true;
    }
    const auto result = box.pending;
    if (length && result.length != INT32_MIN) Probe(length, sizeof(*length));
    status.ProbeChanged();
    if (length && result.length != INT32_MIN) Write(length, &result.length, sizeof(*length));
    status.Publish();
    box.hasPending = false;
    return result.text;
}

const char16_t* APS5_VABI UnicodeNextEnumeration(UEnumeration* enumeration, std::int32_t* length, std::int32_t* status) {
    return static_cast<const char16_t*>(NextEnumeration(enumeration, length, status, true));
}

const char* APS5_VABI CharNextEnumeration(UEnumeration* enumeration, std::int32_t* length, std::int32_t* status) {
    return static_cast<const char*>(NextEnumeration(enumeration, length, status, false));
}

void APS5_VABI ResetEnumeration(UEnumeration* enumeration, std::int32_t* output) {
    const PreserveErrno preserve;
    const std::lock_guard lock(CurrencyMutex);
    Status status(output);
    if (status.value > 0) return;
    auto& box = FindEnumeration(enumeration);
    IcuCurrency::ResetEnumeration_nid_no_patch(box.native, status.value);
    status.Publish();
    box.hasPending = false;
}

template<class T>
UEnumeration* OpenEnumeration(T&& factory, std::int32_t& status) {
    UEnumeration* result = nullptr;
    try {
        auto box = std::make_unique<EnumerationBox>();
        box->guest = {nullptr, nullptr, CloseEnumeration, CountEnumeration, UnicodeNextEnumeration, CharNextEnumeration, ResetEnumeration};
        result = &box->guest;
        Enumerations.emplace(result, std::move(box));
    } catch (const std::bad_alloc&) {
        status = 7;
        return nullptr;
    }
    try {
        auto& box = FindEnumeration(result);
        box.native = factory(status);
        if (box.native) return result;
        Enumerations.erase(result);
        return nullptr;
    } catch (...) {
        Enumerations.erase(result);
        throw;
    }
}

}

extern "C" {

std::int32_t APS5_VABI ucurr_forLocale_nid_postfix(const char* locale, char16_t* output, std::int32_t capacity, std::int32_t* error) {
    const PreserveErrno preserve;
    const std::lock_guard lock(CurrencyMutex);
    Status status(error);
    if (status.value > 0) return 0;
    if (capacity < 0 || (!output && capacity > 0)) {
        status.value = 1;
        return status.Finish(0);
    }
    const auto localeName = Locale(locale, status);
    return PublishCode(IcuCurrency::ForLocale_nid_no_patch(localeName.c_str(), capacity, status.value), output, status);
}

const char16_t* APS5_VABI ucurr_getName_nid_postfix(const char16_t* currency, const char* locale, std::int32_t style, std::int8_t* choice, std::int32_t* length, std::int32_t* status) {
    return Name(currency, locale, style, choice, nullptr, false, length, status);
}

const char16_t* APS5_VABI ucurr_getPluralName_nid_postfix(const char16_t* currency, const char* locale, std::int8_t* choice, const char* plural, std::int32_t* length, std::int32_t* status) {
    return Name(currency, locale, 1, choice, plural, true, length, status);
}

std::int32_t APS5_VABI ucurr_getDefaultFractionDigits_nid_postfix(const char16_t* currency, std::int32_t* status) {
    return Digits(currency, 0, status);
}

std::int32_t APS5_VABI ucurr_getDefaultFractionDigitsForUsage_nid_postfix(const char16_t* currency, std::int32_t usage, std::int32_t* status) {
    return Digits(currency, usage, status);
}

double APS5_VABI ucurr_getRoundingIncrement_nid_postfix(const char16_t* currency, std::int32_t* status) {
    return Rounding(currency, 0, status);
}

double APS5_VABI ucurr_getRoundingIncrementForUsage_nid_postfix(const char16_t* currency, std::int32_t usage, std::int32_t* status) {
    return Rounding(currency, usage, status);
}

std::int32_t APS5_VABI ucurr_getNumericCode_nid_postfix(const char16_t* currency) {
    const PreserveErrno preserve;
    const std::lock_guard lock(CurrencyMutex);
    if (!currency) return 0;
    const auto code = ReadString(currency);
    return IcuCurrency::NumericCode_nid_no_patch(code.c_str());
}

std::int8_t APS5_VABI ucurr_isAvailable_nid_postfix(const char16_t* currency, double from, double to, std::int32_t* output) {
    const PreserveErrno preserve;
    const std::lock_guard lock(CurrencyMutex);
    Status status(output);
    if (status.value > 0) return 0;
    const auto code = currency ? ReadString(currency) : std::u16string{};
    return status.Finish(IcuCurrency::IsAvailable_nid_no_patch(currency ? code.c_str() : nullptr, from, to, status.value));
}

std::int32_t APS5_VABI ucurr_countCurrencies_nid_postfix(const char* locale, double date, std::int32_t* output) {
    const PreserveErrno preserve;
    const std::lock_guard lock(CurrencyMutex);
    if (!output) return 0;
    Status status(output);
    if (status.value > 0) return 0;
    const auto localeName = Locale(locale, status);
    return status.Finish(IcuCurrency::CountCurrencies_nid_no_patch(localeName.c_str(), date, status.value));
}

std::int32_t APS5_VABI ucurr_forLocaleAndDate_nid_postfix(const char* locale, double date, std::int32_t index, char16_t* output, std::int32_t capacity, std::int32_t* error) {
    const PreserveErrno preserve;
    const std::lock_guard lock(CurrencyMutex);
    if (!error) return 0;
    Status status(error);
    if (status.value > 0) return 0;
    if (!output && capacity != 0) {
        status.value = 1;
        return status.Finish(0);
    }
    const auto localeName = Locale(locale, status);
    return PublishCode(IcuCurrency::ForLocaleAndDate_nid_no_patch(localeName.c_str(), date, index, capacity, status.value), output, status);
}

UEnumeration* APS5_VABI ucurr_openISOCurrencies_nid_postfix(std::uint32_t type, std::int32_t* output) {
    const PreserveErrno preserve;
    const std::lock_guard lock(CurrencyMutex);
    std::int32_t status = 0;
    auto* result = OpenEnumeration([&](std::int32_t& error) { return IcuCurrency::OpenISO_nid_no_patch(type, error); }, status);
    if (!result) {
        Probe(output, sizeof(status));
        Write(output, &status, sizeof(status));
    }
    return result;
}

UEnumeration* APS5_VABI ucurr_getKeywordValuesForLocale_nid_postfix(const char*, const char* locale, std::int8_t commonlyUsed, std::int32_t* output) {
    const PreserveErrno preserve;
    const std::lock_guard lock(CurrencyMutex);
    Status status(output);
    if (status.value > 0) return nullptr;
    const auto localeName = Locale(locale, status);
    auto* result = OpenEnumeration([&](std::int32_t& error) { return IcuCurrency::OpenKeywords_nid_no_patch(localeName.c_str(), commonlyUsed, error); }, status.value);
    try {
        return status.Finish(result);
    } catch (...) {
        if (result) CloseEnumeration(result);
        throw;
    }
}

UCurrRegistryKey APS5_VABI ucurr_register_nid_postfix(const char16_t* currency, const char* locale, std::int32_t* output) {
    const PreserveErrno preserve;
    const std::lock_guard lock(CurrencyMutex);
    if (!output) return nullptr;
    Status status(output);
    if (status.value > 0) return nullptr;
    const auto localeName = Locale(locale, status);
    const auto code = currency ? Prefix(currency, false) : std::array<char16_t, 4>{};
    status.ProbeOutput();
    const auto result = IcuCurrency::Register_nid_no_patch(currency ? code.data() : nullptr, localeName.c_str(), status.value);
    try {
        return status.Finish(result);
    } catch (...) {
        if (result) {
            std::int32_t rollback = 0;
            IcuCurrency::Unregister_nid_no_patch(result, rollback);
        }
        throw;
    }
}

std::int8_t APS5_VABI ucurr_unregister_nid_postfix(UCurrRegistryKey key, std::int32_t* output) {
    const PreserveErrno preserve;
    const std::lock_guard lock(CurrencyMutex);
    if (!output) return 0;
    Status status(output);
    if (status.value > 0) return 0;
    return status.Finish(IcuCurrency::Unregister_nid_no_patch(key, status.value));
}

}
