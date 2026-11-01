#ifndef CORE_LIBS_PRX_LIBSCEIME_INCLUDE_IMEINPUT_HPP
#define CORE_LIBS_PRX_LIBSCEIME_INCLUDE_IMEINPUT_HPP

#include <cstddef>
#include <cstdint>

enum class ImeHostInputKind : std::uint32_t {
    Text,
    Backspace,
    Delete,
    Left,
    Right,
    Home,
    End,
    Enter,
    Escape,
};

struct ImeHostInput {
    ImeHostInputKind kind = ImeHostInputKind::Text;
    char text[32] = {};
};

extern "C" void ImePublishInput_nid_postfix(const ImeHostInput& input);
extern "C" bool ImeCapturesKeyboard_nid_postfix();

using ImeHostTextSink = void (*)(ImeHostInputKind kind, const char16_t* text, std::size_t length);

extern "C" void ImeSetHostTextSink_nid_no_patch(ImeHostTextSink sink);

struct ImeHostPad {
    std::uint32_t buttons = 0;
    std::uint8_t leftX = 128;
    std::uint8_t leftY = 128;
    std::uint8_t l2 = 0;
};

using ImeHostPadSink = void (*)(const ImeHostPad& pad);

extern "C" bool ImeCapturesPad_nid_postfix();
extern "C" void ImePublishPad_nid_postfix(const ImeHostPad& pad);
extern "C" void ImeSetHostPadSink_nid_no_patch(ImeHostPadSink sink);

#endif
