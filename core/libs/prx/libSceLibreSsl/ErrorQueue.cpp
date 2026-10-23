#include "ErrorQueue.hpp"
#include "prx/libc/include/General.hpp"
#include <algorithm>
#include <atomic>
#include <cstdio>
#include <cstring>
#include <iterator>

extern "C" int APS5_VABI strerror_r_nid_postfix(int error, char* buffer, std::size_t length);

namespace {
struct ErrorString {
    std::uint32_t code;
    const char* text;
};

#include "ErrorStrings.inc"

enum class StringState { Uninitialized, Loaded, Freed };
std::atomic<StringState> stringState{StringState::Uninitialized};
char errorBuffer[256];

constexpr std::uint64_t Pack(int library, int function, int reason) {
    return ((static_cast<std::uint64_t>(library) & 255) << 24) |
           ((static_cast<std::uint64_t>(function) & 4095) << 12) |
           (static_cast<std::uint64_t>(reason) & 4095);
}

const char* Lookup(std::uint32_t code) {
    const auto entry = std::lower_bound(std::begin(CryptoStrings), std::end(CryptoStrings), code,
        [](const ErrorString& value, std::uint32_t key) { return value.code < key; });
    return entry != std::end(CryptoStrings) && entry->code == code ? entry->text : nullptr;
}
}

extern "C" {
void LibreSslLoadErrorStrings_nid_no_patch() {
    auto expected = StringState::Uninitialized;
    stringState.compare_exchange_strong(expected, StringState::Loaded);
}

char* APS5_VABI ERR_error_string_nid_postfix(std::uint64_t error, char* buffer) {
    if (buffer == nullptr) buffer = errorBuffer;
    ERR_error_string_n_nid_postfix(error, buffer, sizeof(errorBuffer));
    return buffer;
}

void APS5_VABI ERR_error_string_n_nid_postfix(std::uint64_t error, char* buffer, std::size_t length) {
    if (buffer == nullptr && length != 0) NotImplemented_nid_no_patch(__func__);
    const int library = (error >> 24) & 255;
    const int function = (error >> 12) & 4095;
    const int reason = error & 4095;
    LibreSslLoadErrorStrings_nid_no_patch();
    const bool loaded = stringState.load() == StringState::Loaded;
    const char* libraryText = loaded ? Lookup(static_cast<std::uint32_t>(Pack(library, 0, 0))) : nullptr;
    const char* functionText = loaded ? Lookup(static_cast<std::uint32_t>(Pack(library, function, 0))) : nullptr;
    const char* reasonText = loaded ? Lookup(static_cast<std::uint32_t>(Pack(library, 0, reason))) : nullptr;
    char libraryBuffer[30];
    char functionBuffer[30];
    char reasonBuffer[32];
    if (loaded && library == 2 && reason >= 1 && reason <= 127) {
        char message[128];
        strerror_r_nid_postfix(reason, message, sizeof(message));
        std::memcpy(reasonBuffer, message, std::min(std::strlen(message) + 1, sizeof(reasonBuffer)));
        reasonBuffer[sizeof(reasonBuffer) - 1] = '\0';
        reasonText = reasonBuffer;
    }
    if (reasonText == nullptr && loaded) reasonText = Lookup(reason);
    if (libraryText == nullptr) {
        std::snprintf(libraryBuffer, sizeof(libraryBuffer), "lib(%d)", library);
        libraryText = libraryBuffer;
    }
    if (functionText == nullptr) {
        std::snprintf(functionBuffer, sizeof(functionBuffer), "func(%d)", function);
        functionText = functionBuffer;
    }
    if (reasonText == nullptr) {
        std::snprintf(reasonBuffer, sizeof(reasonBuffer), "reason(%d)", reason);
        reasonText = reasonBuffer;
    }
    const int written = std::snprintf(buffer, length, "error:%08llX:%s:%s:%s",
        static_cast<unsigned long long>(error), libraryText, functionText, reasonText);
    if (written >= 0 && static_cast<std::size_t>(written) >= length && length > 4) {
        char* next = buffer;
        for (std::size_t index = 0; index < 4; ++index) {
            char* colon = std::strchr(next, ':');
            char* last = buffer + length - 5 + index;
            if (colon == nullptr || colon > last) {
                colon = last;
                *colon = ':';
            }
            next = colon + 1;
        }
    }
}

void APS5_VABI ERR_free_strings_nid_postfix() {
    LibreSslLoadErrorStrings_nid_no_patch();
    stringState.store(StringState::Freed);
}

}
