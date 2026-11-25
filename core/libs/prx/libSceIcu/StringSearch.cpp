#include "prx/libSceIcu/RecoveredSignatures.hpp"
#include "prx/libSceIcu/Status.hpp"
#include "prx/libSceIcu/StringSearchHost.hpp"
#include "prx/libc/include/General.hpp"
#include <algorithm>
#include <cerrno>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

namespace {

template <typename T>
T CopyIn(const void* guest, const char* caller) {
    T value{};
    const int savedErrno = errno;
    const bool copied = GuestCopyIn_nid_no_patch(&value, guest, sizeof(value));
    errno = savedErrno;
    if (!copied) NotImplemented_nid_no_patch(caller);
    return value;
}

template <typename T>
void CopyOut(void* guest, const T& value, const char* caller) {
    const int savedErrno = errno;
    const bool copied = GuestCopyOut_nid_no_patch(guest, &value, sizeof(value));
    errno = savedErrno;
    if (!copied) NotImplemented_nid_no_patch(caller);
}

constexpr std::int32_t Done = -1;
constexpr std::int32_t AttributeCount = 3;
constexpr std::int32_t AttributeValueCount = 5;
constexpr std::size_t PageBytes = 4096;
constexpr std::size_t LocaleBytes = 4096;

struct SearchState {
    std::mutex mutex;
    UStringSearch* search{};
    std::vector<char16_t> pattern;
    std::vector<char16_t> text;

    ~SearchState() {
        Icu::StringSearchHost::Close(search);
    }
};

struct Registry {
    std::mutex mutex;
    std::unordered_map<const UStringSearch*, std::shared_ptr<SearchState>> searches;
};

Registry& Searches() {
    static Registry registry;
    return registry;
}

bool CopyBytes(void* destination, const void* source, std::size_t bytes) {
    const int savedErrno = errno;
    const bool copied = GuestCopyIn_nid_no_patch(destination, source, bytes);
    errno = savedErrno;
    return copied;
}

std::vector<char16_t> CopyUnits(const char16_t* source, std::int32_t length, const char* caller) {
    std::vector<char16_t> result;
    if (length >= 0) {
        result.resize(static_cast<std::size_t>(length));
        if (length > 0 && !CopyBytes(result.data(), source, result.size() * sizeof(char16_t))) NotImplemented_nid_no_patch(caller);
        return result;
    }
    auto address = reinterpret_cast<std::uintptr_t>(source);
    while (true) {
        const std::size_t count = std::max<std::size_t>((PageBytes - address % PageBytes) / sizeof(char16_t), 1);
        if (result.size() > static_cast<std::size_t>(std::numeric_limits<std::int32_t>::max()) - count) NotImplemented_nid_no_patch(caller);
        const std::size_t offset = result.size();
        result.resize(offset + count);
        if (!CopyBytes(result.data() + offset, reinterpret_cast<const void*>(address), count * sizeof(char16_t))) NotImplemented_nid_no_patch(caller);
        const auto end = std::find(result.begin() + static_cast<std::ptrdiff_t>(offset), result.end(), char16_t{});
        if (end != result.end()) {
            result.erase(end, result.end());
            return result;
        }
        address += count * sizeof(char16_t);
    }
}

void WriteChangedStatus(std::int32_t* status, std::int32_t incoming, std::int32_t outgoing, const char* caller) {
    if (incoming != outgoing) CopyOut(status, outgoing, caller);
}

void Register(const std::shared_ptr<SearchState>& state) {
    auto& registry = Searches();
    const std::lock_guard lock(registry.mutex);
    registry.searches.emplace(state->search, state);
}

bool Succeeded(const std::int32_t* status, const char* caller) {
    Icu::RequireStatus(status);
    return CopyIn<std::int32_t>(status, caller) <= 0;
}

}

extern "C" {

UCollator* APS5_VABI usearch_getCollator_nid_postfix(const UStringSearch* search) {
    if (search == nullptr) return nullptr;
    NotImplemented_nid_no_patch(__func__);
}

std::int32_t APS5_VABI usearch_getMatchedLength_nid_postfix(const UStringSearch* search) {
    if (search == nullptr) return Done;
    NotImplemented_nid_no_patch(__func__);
}

std::int32_t APS5_VABI usearch_next_nid_postfix(UStringSearch* search, std::int32_t* status) {
    if (!Succeeded(status, __func__) || search == nullptr) return Done;
    NotImplemented_nid_no_patch(__func__);
}

UStringSearch* APS5_VABI usearch_open_nid_postfix(const char16_t* pattern, std::int32_t patternLength, const char16_t* text,
                                                 std::int32_t textLength, const char* locale, UBreakIterator* breakIterator,
                                                 std::int32_t* status) {
    Icu::RequireStatus(status);
    const auto incoming = CopyIn<std::int32_t>(status, __func__);
    if (incoming > 0) return nullptr;
    if (!pattern || !text || !locale || patternLength < -1 || textLength < -1) {
        WriteChangedStatus(status, incoming, Icu::IllegalArgument, __func__);
        return nullptr;
    }
    auto state = std::make_shared<SearchState>();
    state->pattern = CopyUnits(pattern, patternLength, __func__);
    state->text = CopyUnits(text, textLength, __func__);
    if (state->pattern.empty() || state->text.empty()) {
        WriteChangedStatus(status, incoming, Icu::IllegalArgument, __func__);
        return nullptr;
    }
    std::string localeName;
    const int savedErrno = errno;
    const int copied = CopyGuestString_nid_no_patch(locale, LocaleBytes, localeName);
    errno = savedErrno;
    if (copied != 0) NotImplemented_nid_no_patch(__func__);
    auto outgoing = incoming;
    state->search = static_cast<UStringSearch*>(Icu::StringSearchHost::Open(
        state->pattern.data(), static_cast<std::int32_t>(state->pattern.size()), state->text.data(),
        static_cast<std::int32_t>(state->text.size()), localeName.c_str(), breakIterator, outgoing));
    WriteChangedStatus(status, incoming, outgoing, __func__);
    if (!state->search) return nullptr;
    Register(state);
    return state->search;
}

void APS5_VABI usearch_reset_nid_postfix(UStringSearch* search) {
    if (search == nullptr) return;
    NotImplemented_nid_no_patch(__func__);
}

void APS5_VABI usearch_setAttribute_nid_postfix(UStringSearch* search, std::int32_t attribute, std::int32_t value, std::int32_t* status) {
    if (Succeeded(status, __func__) && search != nullptr) {
        if (attribute >= 0 && attribute < AttributeCount) NotImplemented_nid_no_patch(__func__);
        CopyOut(status, Icu::IllegalArgument, __func__);
    }
    if (value == AttributeValueCount) CopyOut(status, Icu::IllegalArgument, __func__);
}

void APS5_VABI usearch_setOffset_nid_postfix(UStringSearch* search, std::int32_t, std::int32_t* status) {
    if (!Succeeded(status, __func__) || search == nullptr) return;
    NotImplemented_nid_no_patch(__func__);
}

void APS5_VABI usearch_setPattern_nid_postfix(UStringSearch* search, const char16_t* pattern, std::int32_t patternLength, std::int32_t* status) {
    if (!Succeeded(status, __func__)) return;
    if (search == nullptr || pattern == nullptr) {
        CopyOut(status, Icu::IllegalArgument, __func__);
        return;
    }
    if (patternLength == -1 ? CopyIn<char16_t>(pattern, __func__) == 0 : patternLength == 0) {
        CopyOut(status, Icu::IllegalArgument, __func__);
        return;
    }
    NotImplemented_nid_no_patch(__func__);
}

void APS5_VABI usearch_setText_nid_postfix(UStringSearch* search, const char16_t* text, std::int32_t textLength, std::int32_t* status) {
    if (!Succeeded(status, __func__)) return;
    if (search == nullptr || text == nullptr || textLength < -1 || textLength == 0) {
        CopyOut(status, Icu::IllegalArgument, __func__);
        return;
    }
    NotImplemented_nid_no_patch(__func__);
}

}
