#include "prx/libSceLibreSsl/include/Api.hpp"
#include "prx/libSceLibreSsl/include/Error.hpp"
#include "prx/libc/include/Api.hpp"
#include <cerrno>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <stdexcept>
#include <string>
#include "prx/libc/include/General.hpp"

static_assert(sizeof(LibreSslUiMethod) == 56);
static_assert(sizeof(LibreSslUi) == 40);
static_assert(offsetof(LibreSslUi, flags) == 32);
static_assert(sizeof(LibreSslUiString) == 64);
static_assert(offsetof(LibreSslUiString, resultBuffer) == 24);
static_assert(offsetof(LibreSslUiString, data) == 32);
static_assert(offsetof(LibreSslUiString, flags) == 56);

namespace {
constexpr int UiLibrary = 40;
constexpr int AnyFunction = 0xfff;
constexpr int ResultTooLarge = 100;
constexpr int ResultTooSmall = 101;
constexpr int NoResultBuffer = 105;
constexpr std::int32_t Redoable = 0x0001;
constexpr std::int32_t PromptString = 1;
constexpr std::int32_t VerifyString = 2;
constexpr std::int32_t BooleanString = 3;

void PutError(int reason) {
    LibreSslPutError_nid_no_patch(UiLibrary, AnyFunction, reason);
}

const char* Characters(const char* characters, const char* function) {
    if (!characters) throw std::invalid_argument(std::string(function) + ": null boolean characters");
    return characters;
}

template <typename T>
bool ReadField(const T* field, T& value) {
    return GuestCopyIn_nid_no_patch(&value, field, sizeof(T));
}

template <typename T>
bool WriteField(T* field, const T& value) {
    return GuestCopyOut_nid_no_patch(field, &value, sizeof(T));
}

std::string GuestText(const char* result, const char* function, const char* what) {
    constexpr std::uintptr_t Page = 4096;
    std::string text;
    auto address = reinterpret_cast<std::uintptr_t>(result);
    char chunk[Page];
    for (;;) {
        const std::size_t size = Page - address % Page;
        const bool copied = GuestCopyIn_nid_no_patch(chunk, reinterpret_cast<const void*>(address), size);
        if (!copied) throw std::invalid_argument(std::string(function) + ": unreadable " + what);
        const auto* end = static_cast<const char*>(std::memchr(chunk, '\0', size));
        text.append(chunk, end ? static_cast<std::size_t>(end - chunk) : size);
        if (end) return text;
        address += size;
    }
}

void WriteResult(char* buffer, const char* bytes, std::size_t size, const char* function) {
    const bool copied = GuestCopyOut_nid_no_patch(buffer, bytes, size);
    if (!copied) throw std::invalid_argument(std::string(function) + ": unwritable result buffer");
}

char DefaultUiMethodName[] = "OpenSSL default user interface";

int APS5_VABI OpenConsole(void*) {
    NotImplemented_nid_no_patch("UI_OpenSSL open_console");
}

int APS5_VABI WriteString(void*, void*) {
    NotImplemented_nid_no_patch("UI_OpenSSL write_string");
}

int APS5_VABI ReadString(void*, void*) {
    NotImplemented_nid_no_patch("UI_OpenSSL read_string");
}

int APS5_VABI CloseConsole(void*) {
    NotImplemented_nid_no_patch("UI_OpenSSL close_console");
}

LibreSslUiMethod DefaultUiMethod {
    DefaultUiMethodName,
    OpenConsole,
    WriteString,
    nullptr,
    ReadString,
    CloseConsole,
    nullptr,
};
}

extern "C" {
LibreSslUiMethod* APS5_VABI UI_OpenSSL_nid_postfix(void) {
    return &DefaultUiMethod;
}

LibreSslUiMethod* APS5_VABI UI_create_method_nid_postfix(char* name) {
    const std::string text = name ? GuestText(name, __func__, "name") : std::string();
    auto* method = static_cast<LibreSslUiMethod*>(malloc_nid_postfix(sizeof(LibreSslUiMethod)));
    if (!method) return nullptr;
    std::memset(method, 0, sizeof(LibreSslUiMethod));
    if (name) {
        const std::size_t size = text.size() + 1;
        method->name = static_cast<char*>(malloc_nid_postfix(size));
        if (method->name) std::memcpy(method->name, text.c_str(), size);
    }
    return method;
}

void APS5_VABI UI_destroy_method_nid_postfix(LibreSslUiMethod* method) {
    if (!method) throw std::invalid_argument(std::string(__func__) + ": null method");
    char* name = nullptr;
    if (!ReadField(&method->name, name) || !WriteField(&method->name, static_cast<char*>(nullptr)))
        throw std::invalid_argument(std::string(__func__) + ": unreadable method");
    free_nid_postfix(name);
    free_nid_postfix(method);
}

int APS5_VABI UI_method_set_opener_nid_postfix(LibreSslUiMethod* method, UiSessionFunction opener) {
    if (!method || !WriteField(&method->openSession, opener)) return -1;
    return 0;
}

UiSessionFunction APS5_VABI UI_method_get_opener_nid_postfix(const LibreSslUiMethod* method) {
    UiSessionFunction value = nullptr;
    return method && ReadField(&method->openSession, value) ? value : nullptr;
}

int APS5_VABI UI_method_set_writer_nid_postfix(LibreSslUiMethod* method, UiStringFunction writer) {
    if (!method || !WriteField(&method->writeString, writer)) return -1;
    return 0;
}

UiStringFunction APS5_VABI UI_method_get_writer_nid_postfix(const LibreSslUiMethod* method) {
    UiStringFunction value = nullptr;
    return method && ReadField(&method->writeString, value) ? value : nullptr;
}

int APS5_VABI UI_method_set_reader_nid_postfix(LibreSslUiMethod* method, UiStringFunction reader) {
    if (!method || !WriteField(&method->readString, reader)) return -1;
    return 0;
}

UiStringFunction APS5_VABI UI_method_get_reader_nid_postfix(const LibreSslUiMethod* method) {
    UiStringFunction value = nullptr;
    return method && ReadField(&method->readString, value) ? value : nullptr;
}

int APS5_VABI UI_method_set_closer_nid_postfix(LibreSslUiMethod* method, UiSessionFunction closer) {
    if (!method || !WriteField(&method->closeSession, closer)) return -1;
    return 0;
}

UiSessionFunction APS5_VABI UI_method_get_closer_nid_postfix(const LibreSslUiMethod* method) {
    UiSessionFunction value = nullptr;
    return method && ReadField(&method->closeSession, value) ? value : nullptr;
}

int APS5_VABI UI_set_result_nid_postfix(LibreSslUi* ui, LibreSslUiString* string, const char* result) {
    if (!result) throw std::invalid_argument(std::string(__func__) + ": null result");
    if (!ui) throw std::invalid_argument(std::string(__func__) + ": null UI");
    const std::string text = GuestText(result, __func__, "result");
    const auto length = static_cast<int>(text.size());
    std::int32_t uiFlags = 0;
    if (!ReadField(&ui->flags, uiFlags)) throw std::invalid_argument(std::string(__func__) + ": unreadable UI");
    uiFlags &= ~Redoable;
    if (!WriteField(&ui->flags, uiFlags)) throw std::invalid_argument(std::string(__func__) + ": unwritable UI");
    if (!string) return -1;
    LibreSslUiString entry;
    if (!ReadField(string, entry)) throw std::invalid_argument(std::string(__func__) + ": unreadable string");
    switch (entry.type) {
    case PromptString:
    case VerifyString:
        if (length < entry.data.stringData.resultMinimum) {
            WriteField(&ui->flags, uiFlags | Redoable);
            PutError(ResultTooSmall);
            return -1;
        }
        if (length > entry.data.stringData.resultMaximum) {
            WriteField(&ui->flags, uiFlags | Redoable);
            PutError(ResultTooLarge);
            return -1;
        }
        if (!entry.resultBuffer) {
            PutError(NoResultBuffer);
            return -1;
        }
        WriteResult(entry.resultBuffer, text.c_str(), text.size() + 1, __func__);
        break;
    case BooleanString: {
        if (!entry.resultBuffer) {
            PutError(NoResultBuffer);
            return -1;
        }
        char answer = '\0';
        std::string accepted;
        std::string cancelled;
        bool haveAccepted = false;
        bool haveCancelled = false;
        for (const char character : text) {
            if (!haveAccepted) {
                accepted = GuestText(Characters(entry.data.booleanData.okCharacters, __func__), __func__, "characters");
                haveAccepted = true;
            }
            if (accepted.find(character) != std::string::npos) {
                answer = accepted[0];
                break;
            }
            if (!haveCancelled) {
                cancelled = GuestText(Characters(entry.data.booleanData.cancelCharacters, __func__), __func__, "characters");
                haveCancelled = true;
            }
            if (cancelled.find(character) != std::string::npos) {
                answer = cancelled[0];
                break;
            }
        }
        WriteResult(entry.resultBuffer, &answer, 1, __func__);
        break;
    }
    default:
        break;
    }
    return 0;
}

}
