#ifndef CORE_LIBS_PRX_LIBSCEIME_INCLUDE_MODALDIALOG_HPP
#define CORE_LIBS_PRX_LIBSCEIME_INCLUDE_MODALDIALOG_HPP

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#include "prx/libSceIme/include/ImeInput.hpp"

inline constexpr std::size_t ModalDialogNoButton = static_cast<std::size_t>(-1);
inline constexpr std::size_t ModalDialogMaxButtons = 3;

inline constexpr std::uint32_t ModalDialogAnyOwner = 0;
inline constexpr std::uint32_t ModalDialogOwnerMsgDialog = 1;
inline constexpr std::uint32_t ModalDialogOwnerErrorDialog = 2;
inline constexpr std::uint32_t ModalDialogOwnerPsnNotice = 3;

struct ModalDialogRequest {
    std::uint32_t owner = ModalDialogAnyOwner;
    std::u16string message;
    std::vector<std::u16string> buttons;
    std::size_t focus = 0;
    std::size_t cancel = ModalDialogNoButton;
};

struct ModalDialogButtonView {
    std::u16string label;
    bool selected = false;
};

struct ModalDialogView {
    std::u16string message;
    std::vector<ModalDialogButtonView> buttons;
};

enum class ModalDialogState : std::uint8_t { Idle, Showing, Answered, Closed, Unshown };

inline constexpr std::uint32_t ModalDialogShowTimeoutMilliseconds = 3000;

enum class ModalDialogInputKind : std::uint8_t { Previous, Next, Confirm, Cancel };

extern "C" void ModalDialogSetPresenter_nid_postfix(bool present);
extern "C" bool ModalDialogHasPresenter_nid_postfix();
extern "C" void ModalDialogSetShowTimeout_nid_postfix(std::uint32_t milliseconds);
extern "C" bool ModalDialogOpen_nid_postfix(const ModalDialogRequest& request);
extern "C" ModalDialogState ModalDialogPoll_nid_postfix(std::size_t& chosen, std::uint32_t owner = ModalDialogAnyOwner);
extern "C" void ModalDialogClose_nid_postfix(std::uint32_t owner = ModalDialogAnyOwner);
extern "C" void ModalDialogReset_nid_postfix(std::uint32_t owner = ModalDialogAnyOwner);
extern "C" bool ModalDialogReadView_nid_postfix(ModalDialogView& view);
extern "C" bool ModalDialogCapturesInput_nid_postfix();
extern "C" void ModalDialogPublishInput_nid_postfix(ModalDialogInputKind kind);
extern "C" void ModalDialogPublishPad_nid_postfix(const ImeHostPad& pad);

#endif
