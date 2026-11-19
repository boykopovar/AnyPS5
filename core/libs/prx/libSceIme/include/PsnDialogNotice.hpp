#ifndef CORE_LIBS_PRX_LIBSCEIME_INCLUDE_PSNDIALOGNOTICE_HPP
#define CORE_LIBS_PRX_LIBSCEIME_INCLUDE_PSNDIALOGNOTICE_HPP

#include <cstdint>
#include <stdexcept>
#include <string>

#include "prx/libSceIme/include/ModalDialog.hpp"

enum class PsnDialogNoticeMode : std::uint8_t { Silent, Show };

enum class PsnDialogNoticeKind : std::uint8_t { PsnFeature, GenericFeature };

inline constexpr std::uint64_t PsnDialogNoticeIntervalMilliseconds = 2000;

inline PsnDialogNoticeMode ParsePsnDialogNoticeMode(const char* text) {
    if (text == nullptr || *text == '\0') return PsnDialogNoticeMode::Silent;
    const std::string value(text);
    if (value == "silent") return PsnDialogNoticeMode::Silent;
    if (value == "show") return PsnDialogNoticeMode::Show;
    throw std::runtime_error("ANYPS5_PSN_DIALOGS must be show or silent, not '" + value + "'");
}

inline ModalDialogRequest PsnDialogNoticeRequest(PsnDialogNoticeKind kind) {
    ModalDialogRequest request;
    request.owner = ModalDialogOwnerPsnNotice;
    if (kind == PsnDialogNoticeKind::PsnFeature) {
        request.message = u"This feature needs PlayStation Network, which AnyPS5 does not provide, so it is not available.";
    } else {
        request.message = u"This system feature is not available in AnyPS5.";
    }
    request.buttons = {u"OK"};
    request.focus = 0;
    request.cancel = 0;
    return request;
}

extern "C" void PsnDialogNoticeSetMode_nid_postfix(PsnDialogNoticeMode mode);
extern "C" bool PsnDialogNoticeOffer_nid_postfix(PsnDialogNoticeKind kind);
extern "C" bool PsnDialogNoticeOfferAt_nid_postfix(PsnDialogNoticeKind kind, std::uint64_t nowMilliseconds);

#endif
