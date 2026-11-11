#include "prx/libSceImeDialog/include/Api.hpp"
#include "SceTypes.hpp"
#include "prx/libSceIme/include/ImeInput.hpp"

#ifdef __linux__
#include <sys/mman.h>
#endif

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <initializer_list>
#include <cstring>

namespace {

constexpr int StatusNone = 0;
constexpr int StatusFinished = 2;
constexpr std::uint32_t EndStatusOk = 0;
constexpr std::uint32_t EndStatusAborted = 2;
constexpr int ErrorInvalidAddress = static_cast<int>(0x80bc0031u);
constexpr int ErrorInvalidInputTextBuffer = static_cast<int>(0x80bc0017u);

void Require(bool value, const char* message) {
    if (value) return;
    std::fprintf(stderr, "%s\n", message);
    std::abort();
}

}

int main() {
    for (const std::uintptr_t address : {std::uintptr_t{1}, ~std::uintptr_t{0xfff}}) {
        const auto* badParam = reinterpret_cast<const Param*>(address);
        Require(sceImeDialogInit(badParam, nullptr) == ErrorInvalidAddress, "an unmapped param returns ErrorInvalidAddress");
        Require(sceImeDialogGetStatus() == StatusNone, "a rejected param leaves no dialog open");
    }
    const Param valid{};
    for (const std::uintptr_t address : {std::uintptr_t{1}, ~std::uintptr_t{0xfff}}) {
        const auto* badExtended = reinterpret_cast<const ExtendedParam*>(address);
        Require(sceImeDialogInit(&valid, badExtended) == ErrorInvalidAddress, "an unmapped extended param returns ErrorInvalidAddress");
        Require(sceImeDialogGetStatus() == StatusNone, "a rejected extended param leaves no dialog open");
    }
    for (const std::uintptr_t address : {std::uintptr_t{1}, ~std::uintptr_t{0xfff}}) {
        Param unmappedText{};
        unmappedText.max_text_length = 8;
        unmappedText.input_text_buffer = reinterpret_cast<char16_t*>(address);
        Require(sceImeDialogInit(&unmappedText, nullptr) == ErrorInvalidInputTextBuffer, "an unmapped input text buffer returns ErrorInvalidInputTextBuffer");
        Require(sceImeDialogGetStatus() == StatusNone, "an unreadable input text buffer leaves no dialog open");
    }
#ifdef __linux__
    {
        void* page = mmap(nullptr, 4096, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
        Require(page != MAP_FAILED, "map a page for the read-only buffer");
        auto* readOnly = static_cast<char16_t*>(page);
        readOnly[0] = u'\0';
        Require(mprotect(page, 4096, PROT_READ) == 0, "protect the buffer page read-only");
        Param param{};
        param.max_text_length = 8;
        param.input_text_buffer = readOnly;
        Require(sceImeDialogInit(&param, nullptr) == 0, "a readable buffer opens the dialog");
        ImeHostInput enter{};
        enter.kind = ImeHostInputKind::Enter;
        ImePublishInput_nid_postfix(enter);
        Require(sceImeDialogGetStatus() == StatusFinished, "Enter finishes the dialog");
        Result result{};
        Require(sceImeDialogGetResult(&result) == 0 && result.endstatus == EndStatusAborted, "an unwritable buffer ends the dialog as aborted");
        Require(readOnly[0] == u'\0', "nothing is written to a read-only buffer");
        Require(sceImeDialogTerm() == 0, "the finished dialog closes");
        Require(munmap(page, 4096) == 0, "unmap the buffer page");

    }
#endif
    return 0;
}
