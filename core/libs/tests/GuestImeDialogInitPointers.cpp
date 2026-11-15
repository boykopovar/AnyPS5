#include "prx/libSceImeDialog/include/Api.hpp"
#include "SceTypes.hpp"
#include "prx/libSceIme/include/ImeInput.hpp"

#ifdef __linux__
#include <sys/mman.h>
#endif

#include <cerrno>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <initializer_list>
#include <cstring>

namespace {

constexpr int StatusNone = 0;
constexpr int StatusRunning = 1;
constexpr int StatusFinished = 2;
constexpr std::uint32_t EndStatusOk = 0;
constexpr std::uint32_t EndStatusAborted = 2;
constexpr int ErrorInvalidAddress = static_cast<int>(0x80bc0031u);
constexpr int ErrorInvalidInputTextBuffer = static_cast<int>(0x80bc0017u);
constexpr int ErrorNotOpened = static_cast<int>(0x80bc0002u);

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
        Require(sceImeDialogGetStatus() == StatusRunning, "the opened dialog reports Running");
        PositionAndForm form{};
        errno = 71;
        Require(sceImeDialogGetPanelPositionAndForm(nullptr) == ErrorInvalidAddress, "an open dialog rejects a null position and form pointer with ErrorInvalidAddress");
        Require(errno == 71, "null open position and form check preserves errno");
        for (const std::uintptr_t address : {std::uintptr_t{1}, ~std::uintptr_t{0xfff}}) {
            auto* badForm = reinterpret_cast<PositionAndForm*>(address);
            errno = 71;
            Require(sceImeDialogGetPanelPositionAndForm(badForm) == ErrorInvalidAddress, "an open dialog rejects an unmapped position and form pointer with ErrorInvalidAddress");
            Require(errno == 71, "unmapped open position and form check preserves errno");
        }
        errno = 71;
        Require(sceImeDialogGetPanelPositionAndForm(reinterpret_cast<PositionAndForm*>(readOnly)) == ErrorInvalidAddress, "an open dialog rejects a read-only position and form page with ErrorInvalidAddress");
        Require(errno == 71, "read-only open position and form check preserves errno");
        Require(sceImeDialogGetPanelPositionAndForm(&form) == 0, "a running dialog reports its panel position and form");
        Require(form.width == 1920 && form.height == 1080, "the running dialog dimensions match defaults");
        std::uint32_t runningWidth = 0;
        std::uint32_t runningHeight = 0;
        Require(sceImeDialogGetPanelSize(nullptr, &runningWidth, &runningHeight) == 0, "a running dialog reports its panel size");
        Require(runningWidth == 1920 && runningHeight == 1080, "the running panel size matches defaults");
        std::uint32_t readOnlyWidth = 0xdeadbeef;
        errno = 71;
        Require(sceImeDialogGetPanelSize(nullptr, &readOnlyWidth, reinterpret_cast<uint32_t*>(readOnly)) == ErrorInvalidAddress, "a read-only height page returns ErrorInvalidAddress");
        Require(errno == 71, "read-only panel height check preserves errno");
        Require(readOnlyWidth == 0xdeadbeef, "the failed height write leaves the width untouched");
        ImeHostInput enter{};
        enter.kind = ImeHostInputKind::Enter;
        ImePublishInput_nid_postfix(enter);
        Require(sceImeDialogGetStatus() == StatusFinished, "Enter finishes the dialog");
        PositionAndForm finishedForm{};
        Require(sceImeDialogGetPanelPositionAndForm(&finishedForm) == 0, "a finished dialog reports its panel position and form");
        Require(finishedForm.width == 1920 && finishedForm.height == 1080, "the finished dialog dimensions match defaults");
        Result result{};
        Require(sceImeDialogGetResult(&result) == 0 && result.endstatus == EndStatusAborted, "an unwritable buffer ends the dialog as aborted");
        Require(readOnly[0] == u'\0', "nothing is written to a read-only buffer");
        Require(sceImeDialogTerm() == 0, "the finished dialog closes");
        Require(munmap(page, 4096) == 0, "unmap the buffer page");

    }
#endif
    Require(sceImeDialogGetPanelPositionAndForm(nullptr) == ErrorNotOpened, "a closed dialog rejects a null position and form pointer with ErrorNotOpened");
    for (const std::uintptr_t address : {std::uintptr_t{1}, ~std::uintptr_t{0xfff}}) {
        auto* badForm = reinterpret_cast<PositionAndForm*>(address);
        Require(sceImeDialogGetPanelPositionAndForm(badForm) == ErrorNotOpened, "a closed dialog rejects an unmapped position and form pointer with ErrorNotOpened");
    }
    PositionAndForm closedForm{};
    closedForm.width = 0xdeadbeefu;
    closedForm.height = 0xcafebabeu;
    Require(sceImeDialogGetPanelPositionAndForm(&closedForm) == ErrorNotOpened, "a closed dialog rejects a valid position and form pointer with ErrorNotOpened");
    Require(closedForm.width == 0xdeadbeefu && closedForm.height == 0xcafebabeu, "a closed dialog leaves the position and form output untouched");
    std::uint32_t width = 0xdeadbeef;
    std::uint32_t height = 0xdeadbeef;
    Require(sceImeDialogGetPanelSize(nullptr, nullptr, nullptr) == ErrorInvalidAddress, "null panel size pointers return ErrorInvalidAddress");
    for (const std::uintptr_t address : {std::uintptr_t{1}, ~std::uintptr_t{0xfff}}) {
        auto* badWidth = reinterpret_cast<uint32_t*>(address);
        errno = 71;
        Require(sceImeDialogGetPanelSize(nullptr, badWidth, &height) == ErrorInvalidAddress, "an unmapped panel width pointer returns ErrorInvalidAddress");
        Require(errno == 71, "unmapped panel width check preserves errno");
        auto* badHeight = reinterpret_cast<uint32_t*>(address);
        errno = 71;
        Require(sceImeDialogGetPanelSize(nullptr, &width, badHeight) == ErrorInvalidAddress, "an unmapped panel height pointer returns ErrorInvalidAddress");
        Require(errno == 71, "unmapped panel height check preserves errno");
    }
    Require(width == 0xdeadbeef && height == 0xdeadbeef, "a failed size query leaves both outputs untouched");
    Require(sceImeDialogGetPanelSize(nullptr, &width, &height) == 0, "valid panel size pointers succeed");
    Require(width == 1920 && height == 1080, "panel size dimensions match defaults");
    return 0;
}
