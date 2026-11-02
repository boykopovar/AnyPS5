#ifndef CORE_LIBS_PRX_LIBSCEIMEDIALOG_INCLUDE_PADKEYBOARD_HPP
#define CORE_LIBS_PRX_LIBSCEIMEDIALOG_INCLUDE_PADKEYBOARD_HPP

#include <array>
#include <chrono>
#include <cstdint>
#include <span>
#include <string>

namespace ImeDialog {

inline constexpr std::uint32_t PadButtonL3 = 0x0002;
inline constexpr std::uint32_t PadButtonR3 = 0x0004;
inline constexpr std::uint32_t PadButtonUp = 0x0010;
inline constexpr std::uint32_t PadButtonRight = 0x0020;
inline constexpr std::uint32_t PadButtonDown = 0x0040;
inline constexpr std::uint32_t PadButtonLeft = 0x0080;
inline constexpr std::uint32_t PadButtonL2 = 0x0100;
inline constexpr std::uint32_t PadButtonR2 = 0x0200;
inline constexpr std::uint32_t PadButtonL1 = 0x0400;
inline constexpr std::uint32_t PadButtonR1 = 0x0800;
inline constexpr std::uint32_t PadButtonTriangle = 0x1000;
inline constexpr std::uint32_t PadButtonCircle = 0x2000;
inline constexpr std::uint32_t PadButtonCross = 0x4000;
inline constexpr std::uint32_t PadButtonSquare = 0x8000;

inline constexpr double PadKeyRepeatDelay = 0.275;
inline constexpr double PadKeyRepeatRate = 0.050;
inline constexpr double PadStickDeadzone = 0.24;
inline constexpr double PadStickThreshold = 0.50;
inline constexpr double PadTriggerRelease = 0.20;
inline constexpr double PadEdgeWrapHold = 0.5;

enum class PadKeyAction : std::uint8_t { Character, Shift, Symbols, Specials, Space, Backspace, ArrowLeft, ArrowRight, ArrowUp, ArrowDown, PageNext, PagePrev, Done };
enum class PadKeyGlyph : std::uint8_t { None, Shift, Backspace, ArrowLeft, ArrowRight, ArrowUp, ArrowDown };
enum class PadKeyboardFamily : std::uint8_t { Latin, Symbols, Specials };
enum class PadKeyboardCase : std::uint8_t { Lower, Upper, CapsLock };
enum class PadKeyboardCurrency : std::uint8_t { Euro, Dollar, Pound, Yen, Won };
enum class PadEdit : std::uint8_t { None, Insert, Backspace, Clear, CaretLeft, CaretRight, CaretHome, CaretEnd };
enum class PadFinish : std::uint8_t { None, Done, Cancel };

struct PadKey {
    std::uint8_t row;
    std::uint8_t column;
    std::uint8_t columnSpan;
    std::uint8_t rowSpan;
    const char16_t* label;
    PadKeyAction action;
    PadKeyGlyph glyph;
    const char16_t* badge;
};

struct PadSample {
    std::uint32_t buttons = 0;
    std::uint8_t leftX = 128;
    std::uint8_t leftY = 128;
    std::uint8_t l2 = 0;
};

struct PadKeyboardOptions {
    bool controller = true;
    bool multiline = false;
    PadKeyboardFamily family = PadKeyboardFamily::Latin;
    PadKeyboardCase caseState = PadKeyboardCase::Lower;
    PadKeyboardCurrency currency = PadKeyboardCurrency::Euro;
    std::uint32_t enterLabel = 0;
};

struct PadKeyboardResult {
    PadEdit edit = PadEdit::None;
    std::u16string text;
    PadFinish finish = PadFinish::None;
};

struct PadRepeat {
    bool down = false;
    double next = 0.0;
};

struct PadKeyboardState {
    using Clock = std::chrono::steady_clock;

    PadKeyboardOptions options;
    PadKeyboardFamily family = PadKeyboardFamily::Latin;
    PadKeyboardFamily alphaFamily = PadKeyboardFamily::Latin;
    PadKeyboardCase caseState = PadKeyboardCase::Lower;
    std::uint8_t page = 0;
    int cursorRow = 0;
    int cursorColumn = 0;
    int preferColumn = 0;
    bool started = false;
    bool awaitingInsert = false;
    Clock::time_point origin{};
    std::uint32_t heldAtOpen = 0;
    bool stickHeldAtOpen = false;
    std::uint32_t previous = 0;
    bool crossArmed = false;
    bool l2Armed = true;
    PadRepeat cross;
    PadRepeat left;
    PadRepeat right;
    PadRepeat up;
    PadRepeat down;
    PadRepeat square;
    PadRepeat l1;
    PadRepeat r1;
    double triangleNext = 0.0;
    std::array<bool, 4> previousStick{};
    int stickDirection = 0;
    double stickNext = 0.0;
    bool wrapHold = false;
    int wrapRow = 0;
    int wrapColumn = 0;
    double wrapRelease = 0.0;
};

PadKeyboardState PadKeyboardOpen(const PadKeyboardOptions& options);
PadKeyboardResult PadKeyboardStep(PadKeyboardState& state, const PadSample& sample, PadKeyboardState::Clock::time_point now);
void PadKeyboardNoteInsert(PadKeyboardState& state, bool inserted);
std::span<const PadKey> PadKeyboardKeys(const PadKeyboardState& state);
std::uint8_t PadKeyboardRows(const PadKeyboardState& state);
std::uint8_t PadKeyboardColumns();
const PadKey* PadKeyboardSelected(const PadKeyboardState& state);
std::u16string PadKeyboardLabel(const PadKeyboardState& state, const PadKey& key);
PadKeyboardCurrency PadKeyboardCurrencyFor(std::uint64_t supportedLanguages);

}

#endif
