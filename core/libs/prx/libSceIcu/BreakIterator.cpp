#include "prx/libSceIcu/BreakIterator.hpp"
#include "prx/libSceIcu/DefaultLocaleName.hpp"
#include "prx/libc/include/General.hpp"
#include "unicode/ubrk.h"
#include "unicode/uenum.h"
#include "unicode/uloc.h"
#include <cerrno>
#include <cstddef>
#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>
#include <utility>

#undef ubrk_open
#undef ubrk_close
#undef ubrk_setText
#undef ubrk_first
#undef ubrk_last
#undef ubrk_next
#undef ubrk_previous
#undef ubrk_current
#undef ubrk_following
#undef ubrk_preceding
#undef ubrk_isBoundary
#undef ubrk_getRuleStatus
#undef ubrk_countAvailable
#undef ubrk_getAvailable

namespace {

constexpr std::size_t LocaleBytes = 4096;

struct Registry {
    std::mutex mutex;
    struct Recipe {
        std::int32_t type;
        std::string locale;
    };
    struct Entry {
        Recipe recipe;
        std::shared_ptr<UBreakIterator> iterator;
    };
    std::unordered_map<const UBreakIterator*, Entry> iterators;
};

Registry& Iterators() {
    static Registry registry;
    return registry;
}

void Register(UBreakIterator* iterator, std::int32_t type, std::string locale) {
    std::shared_ptr<UBreakIterator> owned(iterator, ubrk_close_78);
    auto& registry = Iterators();
    const std::lock_guard lock(registry.mutex);
    registry.iterators.emplace(iterator, Registry::Entry{{type, std::move(locale)}, std::move(owned)});
}

std::shared_ptr<UBreakIterator> Unregister(const UBreakIterator* iterator) {
    auto& registry = Iterators();
    std::shared_ptr<UBreakIterator> owned;
    {
        const std::lock_guard lock(registry.mutex);
        const auto found = registry.iterators.find(iterator);
        if (found == registry.iterators.end()) return {};
        owned = std::move(found->second.iterator);
        registry.iterators.erase(found);
    }
    return owned;
}

std::shared_ptr<UBreakIterator> Registered(const void* iterator, const char* caller) {
    if (!iterator) APS5_INVALID_ARG_EX;
    auto& registry = Iterators();
    const std::lock_guard lock(registry.mutex);
    const auto found = registry.iterators.find(static_cast<const UBreakIterator*>(iterator));
    if (found == registry.iterators.end()) NotImplemented_nid_no_patch(caller);
    return found->second.iterator;
}

std::int32_t ReadStatus(const std::int32_t* status, const char* caller) {
    if (!status) APS5_INVALID_ARG_EX;
    std::int32_t value = 0;
    const int savedErrno = errno;
    const bool copied = GuestCopyIn_nid_no_patch(&value, status, sizeof(value));
    errno = savedErrno;
    if (!copied) NotImplemented_nid_no_patch(caller);
    return value;
}

void WriteStatus(std::int32_t* status, std::int32_t incoming, std::int32_t outgoing, const char* caller) {
    if (incoming == outgoing) return;
    const int savedErrno = errno;
    const bool copied = GuestCopyOut_nid_no_patch(status, &outgoing, sizeof(outgoing));
    errno = savedErrno;
    if (!copied) NotImplemented_nid_no_patch(caller);
}

std::string CopyLocale(const char* locale, const char* caller) {
    if (!locale) return Icu::DefaultLocaleName();
    std::string result;
    const int savedErrno = errno;
    const int copied = CopyGuestString_nid_no_patch(locale, LocaleBytes, result);
    errno = savedErrno;
    if (copied != 0) NotImplemented_nid_no_patch(caller);
    return result;
}

}

namespace Icu::BreakIterator {

void* CloneRegistered(const void* iterator, std::int32_t& status, const char* caller) {
    if (!iterator) return nullptr;
    Registry::Recipe recipe;
    auto& registry = Iterators();
    {
        const std::lock_guard lock(registry.mutex);
        const auto found = registry.iterators.find(static_cast<const UBreakIterator*>(iterator));
        if (found == registry.iterators.end()) NotImplemented_nid_no_patch(caller);
        recipe = found->second.recipe;
    }
    const auto incoming = status;
    auto code = static_cast<UErrorCode>(incoming);
    auto* clone = ubrk_open_78(static_cast<UBreakIteratorType>(recipe.type), recipe.locale.c_str(), u"", 0, &code);
    if (!clone || code > 0) status = code;
    return clone;
}

void CloseClone(void* iterator) {
    ubrk_close_78(static_cast<UBreakIterator*>(iterator));
}

}

extern "C" {

void* APS5_VABI ubrk_open(std::int32_t type, const char* locale, const char16_t* text, std::int32_t length,
                         std::int32_t* error) {
    const auto incoming = ReadStatus(error, __func__);
    if (incoming > 0) return nullptr;
    if (type < UBRK_CHARACTER || type >= UBRK_COUNT) {
        WriteStatus(error, incoming, U_ILLEGAL_ARGUMENT_ERROR, __func__);
        return nullptr;
    }
    const std::string localeName = CopyLocale(locale, __func__);
    auto status = static_cast<UErrorCode>(incoming);
    auto* result = ubrk_open_78(static_cast<UBreakIteratorType>(type), localeName.c_str(), text, length, &status);
    if (result) {
        try {
            Register(result, type, localeName);
            WriteStatus(error, incoming, status, __func__);
        } catch (...) {
            Unregister(result);
            throw;
        }
    } else {
        WriteStatus(error, incoming, status, __func__);
    }
    return result;
}

void APS5_VABI ubrk_close(void* iterator) {
    if (!iterator) return;
    auto owned = Unregister(static_cast<UBreakIterator*>(iterator));
    if (!owned) NotImplemented_nid_no_patch(__func__);
}

void APS5_VABI ubrk_setText(void* iterator, const char16_t* text, std::int32_t length, std::int32_t* error) {
    if (!iterator || !error) APS5_INVALID_ARG_EX;
    const auto incoming = ReadStatus(error, __func__);
    if (incoming > 0) return;
    const auto registered = Registered(iterator, __func__);
    auto status = static_cast<UErrorCode>(incoming);
    ubrk_setText_78(registered.get(), text, length, &status);
    WriteStatus(error, incoming, status, __func__);
}

std::int32_t APS5_VABI ubrk_first(void* iterator) {
    const auto registered = Registered(iterator, __func__);
    return ubrk_first_78(registered.get());
}

std::int32_t APS5_VABI ubrk_last(void* iterator) {
    const auto registered = Registered(iterator, __func__);
    return ubrk_last_78(registered.get());
}

std::int32_t APS5_VABI ubrk_next(void* iterator) {
    const auto registered = Registered(iterator, __func__);
    return ubrk_next_78(registered.get());
}

std::int32_t APS5_VABI ubrk_previous(void* iterator) {
    const auto registered = Registered(iterator, __func__);
    return ubrk_previous_78(registered.get());
}

std::int32_t APS5_VABI ubrk_current(const void* iterator) {
    const auto registered = Registered(iterator, __func__);
    return ubrk_current_78(registered.get());
}

std::int32_t APS5_VABI ubrk_following(void* iterator, std::int32_t offset) {
    const auto registered = Registered(iterator, __func__);
    return ubrk_following_78(registered.get(), offset);
}

std::int32_t APS5_VABI ubrk_preceding(void* iterator, std::int32_t offset) {
    const auto registered = Registered(iterator, __func__);
    return ubrk_preceding_78(registered.get(), offset);
}

std::int8_t APS5_VABI ubrk_isBoundary(void* iterator, std::int32_t offset) {
    const auto registered = Registered(iterator, __func__);
    return ubrk_isBoundary_78(registered.get(), offset);
}

std::int32_t APS5_VABI ubrk_getRuleStatus(void* iterator) {
    const auto registered = Registered(iterator, __func__);
    return ubrk_getRuleStatus_78(registered.get());
}

std::int32_t APS5_VABI ubrk_countAvailable() {
    return ubrk_countAvailable_78();
}

const char* APS5_VABI ubrk_getAvailable(std::int32_t index) {
    const auto count = ubrk_countAvailable_78();
    if (index < 0 || index == count) {
        if (count > 0) APS5_INVALID_ARG_EX;
        UErrorCode status = U_ZERO_ERROR;
        auto* available = uloc_openAvailableByType(ULOC_AVAILABLE_DEFAULT, &status);
        uenum_close(available);
        if (U_SUCCESS(status)) APS5_INVALID_ARG_EX;
    }
    return ubrk_getAvailable_78(index);
}

}
