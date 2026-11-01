#include "prx/libSceLibreSsl/include/Api.hpp"
#include "prx/libSceLibreSsl/include/Error.hpp"
#include "prx/libc/include/Api.hpp"
#include <cstddef>
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
}

extern "C" {
LibreSslUiMethod* APS5_VABI UI_create_method_nid_postfix(char* name) {
    auto* method = static_cast<LibreSslUiMethod*>(malloc_nid_postfix(sizeof(LibreSslUiMethod)));
    if (!method) return nullptr;
    std::memset(method, 0, sizeof(LibreSslUiMethod));
    if (name) {
        const std::size_t size = std::strlen(name) + 1;
        method->name = static_cast<char*>(malloc_nid_postfix(size));
        if (method->name) std::memcpy(method->name, name, size);
    }
    return method;
}

void APS5_VABI UI_destroy_method_nid_postfix(LibreSslUiMethod* method) {
    if (!method) throw std::invalid_argument(std::string(__func__) + ": null method");
    free_nid_postfix(method->name);
    method->name = nullptr;
    free_nid_postfix(method);
}

int APS5_VABI UI_method_set_opener_nid_postfix(LibreSslUiMethod* method, UiSessionFunction opener) {
    if (!method) return -1;
    method->openSession = opener;
    return 0;
}

UiSessionFunction APS5_VABI UI_method_get_opener_nid_postfix(const LibreSslUiMethod* method) {
    return method ? method->openSession : nullptr;
}

int APS5_VABI UI_method_set_writer_nid_postfix(LibreSslUiMethod* method, UiStringFunction writer) {
    if (!method) return -1;
    method->writeString = writer;
    return 0;
}

UiStringFunction APS5_VABI UI_method_get_writer_nid_postfix(const LibreSslUiMethod* method) {
    return method ? method->writeString : nullptr;
}

int APS5_VABI UI_method_set_reader_nid_postfix(LibreSslUiMethod* method, UiStringFunction reader) {
    if (!method) return -1;
    method->readString = reader;
    return 0;
}

UiStringFunction APS5_VABI UI_method_get_reader_nid_postfix(const LibreSslUiMethod* method) {
    return method ? method->readString : nullptr;
}

int APS5_VABI UI_method_set_closer_nid_postfix(LibreSslUiMethod* method, UiSessionFunction closer) {
    if (!method) return -1;
    method->closeSession = closer;
    return 0;
}

UiSessionFunction APS5_VABI UI_method_get_closer_nid_postfix(const LibreSslUiMethod* method) {
    return method ? method->closeSession : nullptr;
}

int APS5_VABI UI_set_result_nid_postfix(LibreSslUi* ui, LibreSslUiString* string, const char* result) {
    if (!result) throw std::invalid_argument(std::string(__func__) + ": null result");
    if (!ui) throw std::invalid_argument(std::string(__func__) + ": null UI");
    const auto length = static_cast<int>(std::strlen(result));
    ui->flags &= ~Redoable;
    if (!string) return -1;
    switch (string->type) {
    case PromptString:
    case VerifyString:
        if (length < string->data.stringData.resultMinimum) {
            ui->flags |= Redoable;
            PutError(ResultTooSmall);
            return -1;
        }
        if (length > string->data.stringData.resultMaximum) {
            ui->flags |= Redoable;
            PutError(ResultTooLarge);
            return -1;
        }
        if (!string->resultBuffer) {
            PutError(NoResultBuffer);
            return -1;
        }
        std::memcpy(string->resultBuffer, result, static_cast<std::size_t>(length) + 1);
        break;
    case BooleanString: {
        if (!string->resultBuffer) {
            PutError(NoResultBuffer);
            return -1;
        }
        string->resultBuffer[0] = '\0';
        for (const char* character = result; *character; ++character) {
            const char* const accepted = Characters(string->data.booleanData.okCharacters, __func__);
            if (std::strchr(accepted, *character)) {
                string->resultBuffer[0] = accepted[0];
                break;
            }
            const char* const cancelled = Characters(string->data.booleanData.cancelCharacters, __func__);
            if (std::strchr(cancelled, *character)) {
                string->resultBuffer[0] = cancelled[0];
                break;
            }
        }
        break;
    }
    default:
        break;
    }
    return 0;
}

}
