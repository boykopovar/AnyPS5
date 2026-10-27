#include "prx/libSceImeDialog/include/PadKeyboard.hpp"

#include <chrono>
#include <cstdio>
#include <functional>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace {

using namespace ImeDialog;
using Clock = PadKeyboardState::Clock;

void check(bool condition, const std::string& reason) {
    if (!condition) throw std::runtime_error(reason);
}

struct Driver {
    explicit Driver(PadKeyboardOptions options = {}) : state(PadKeyboardOpen(options)) {}

    PadKeyboardResult Step(std::uint32_t buttons, std::uint8_t leftX = 128, std::uint8_t leftY = 128, std::uint8_t l2 = 0, double seconds = 0.016) {
        now += std::chrono::duration_cast<Clock::duration>(std::chrono::duration<double>(seconds));
        return PadKeyboardStep(state, {buttons, leftX, leftY, l2}, now);
    }

    PadKeyboardResult Press(std::uint32_t button) {
        auto result = Step(button);
        Step(0);
        return result;
    }

    std::u16string SelectedLabel() const {
        const PadKey* key = PadKeyboardSelected(state);
        check(key != nullptr, "a key is selected");
        return PadKeyboardLabel(state, *key);
    }

    std::u16string FirstLabel() const {
        return PadKeyboardLabel(state, PadKeyboardKeys(state).front());
    }

    std::vector<PadKeyboardResult> Hold(std::uint32_t buttons, double seconds) {
        std::vector<PadKeyboardResult> results;
        for (double elapsed = 0.0; elapsed < seconds; elapsed += 0.01) results.push_back(Step(buttons, 128, 128, (buttons & PadButtonL2) != 0 ? 255 : 0, 0.01));
        return results;
    }

    PadKeyboardState state;
    Clock::time_point now{};
};

std::size_t count(const std::vector<PadKeyboardResult>& results, PadEdit edit) {
    std::size_t total = 0;
    for (const auto& result : results) total += result.edit == edit ? 1 : 0;
    return total;
}

void startsOnG() {
    Driver pad;
    pad.Step(0);
    check(pad.SelectedLabel() == u"g", "the Latin keyboard opens on g");
    const auto typed = pad.Press(PadButtonCross);
    check(typed.edit == PadEdit::Insert && typed.text == u"g", "Cross types the selected key");
}

void heldAtOpenDoesNothing() {
    for (const std::uint32_t button : {PadButtonCross, PadButtonSquare, PadButtonRight, PadButtonTriangle, PadButtonL1, PadButtonR2, PadButtonCircle}) {
        Driver pad;
        const auto held = pad.Hold(button, 1.0);
        for (const auto& result : held) check(result.edit == PadEdit::None && result.finish == PadFinish::None, "a button held when the dialog opens does nothing while held");
        check(pad.SelectedLabel() == u"g", "a direction held at open does not move the selection");
        pad.Step(0);
        const auto again = pad.Step(button);
        check(again.edit != PadEdit::None || again.finish != PadFinish::None || pad.SelectedLabel() != u"g", "the button acts once released and pressed again");
    }
    Driver pad;
    for (int i = 0; i < 60; ++i) pad.Step(0, 255, 128);
    check(pad.SelectedLabel() == u"g", "a stick held at open does not move the selection");
    pad.Step(0);
    pad.Step(0, 255, 128);
    check(pad.SelectedLabel() == u"h", "the stick moves once centred and pushed again");
}

void finishes() {
    Driver done;
    done.Step(0);
    check(done.Press(PadButtonR2).finish == PadFinish::Done, "R2 is Done");
    Driver cancel;
    cancel.Step(0);
    check(cancel.Press(PadButtonCircle).finish == PadFinish::Cancel, "Circle cancels");
    Driver both;
    both.Step(0);
    check(both.Press(PadButtonR2 | PadButtonCircle).finish == PadFinish::Done, "Done wins over Cancel in one step");
    Driver key;
    key.Step(0);
    key.state.cursorRow = 5;
    key.state.cursorColumn = 8;
    check(key.SelectedLabel() == u"Done", "the Done key carries the default enter label");
    check(key.Press(PadButtonCross).finish == PadFinish::Done, "Cross on the Done key finishes");
    Driver go({true, false, PadKeyboardFamily::Latin, PadKeyboardCase::Lower, PadKeyboardCurrency::Euro, 3});
    go.state.cursorRow = 5;
    go.state.cursorColumn = 9;
    check(go.SelectedLabel() == u"Go", "the enter label names the Done key");
}

void spaceAndBackspace() {
    Driver pad;
    pad.Step(0);
    const auto space = pad.Press(PadButtonTriangle);
    check(space.edit == PadEdit::Insert && space.text == u" ", "Triangle types a space");
    const auto backspace = pad.Press(PadButtonSquare);
    check(backspace.edit == PadEdit::Backspace, "Square is backspace");
    pad.Step(0);
    const auto held = pad.Hold(PadButtonSquare, 0.5);
    const auto repeats = count(held, PadEdit::Backspace);
    check(repeats >= 4 && repeats <= 7, "Square repeats after 0.275 s every 0.05 s: " + std::to_string(repeats));
    pad.Step(0);
    const auto spaces = count(pad.Hold(PadButtonTriangle, 0.5), PadEdit::Insert);
    check(spaces >= 4 && spaces <= 7, "Triangle repeats like Square: " + std::to_string(spaces));
}

void caret() {
    Driver pad;
    pad.Step(0);
    check(pad.Press(PadButtonL1).edit == PadEdit::CaretLeft, "L1 moves the caret left");
    check(pad.Press(PadButtonR1).edit == PadEdit::CaretRight, "R1 moves the caret right");
    pad.Step(0);
    check(count(pad.Hold(PadButtonR1, 0.5), PadEdit::CaretRight) >= 4, "R1 repeats");
    pad.state.cursorRow = 5;
    pad.state.cursorColumn = 1;
    pad.Step(0);
    check(pad.Press(PadButtonCross).edit == PadEdit::CaretHome, "the up-arrow key moves the caret to the start");
    pad.state.cursorColumn = 0;
    check(pad.Press(PadButtonCross).edit == PadEdit::CaretEnd, "the down-arrow key moves the caret to the end");
    Driver multiline({true, true});
    multiline.Step(0);
    multiline.state.cursorRow = 5;
    multiline.state.cursorColumn = 1;
    check(multiline.Press(PadButtonCross).edit == PadEdit::None, "the up-arrow key does nothing in multiline text");
}

void clearAll() {
    Driver l1First;
    l1First.Step(0);
    check(l1First.Step(PadButtonL1).edit == PadEdit::CaretLeft, "L1 alone moves the caret on its own edge");
    check(l1First.Step(PadButtonL1 | PadButtonSquare).edit == PadEdit::Clear, "L1 then Square clears");
    Driver squareFirst;
    squareFirst.Step(0);
    check(squareFirst.Step(PadButtonSquare).edit == PadEdit::Backspace, "Square alone deletes on its own edge");
    check(squareFirst.Step(PadButtonSquare | PadButtonL1).edit == PadEdit::Clear, "Square then L1 clears");
}

void shiftCycle() {
    Driver pad;
    pad.Step(0);
    pad.Step(PadButtonL2, 128, 128, 255);
    pad.Step(0);
    check(pad.state.caseState == PadKeyboardCase::Upper, "L2 shifts once");
    const auto typed = pad.Press(PadButtonCross);
    check(typed.text == u"G", "a shifted key types upper case");
    PadKeyboardNoteInsert(pad.state, true);
    check(pad.state.caseState == PadKeyboardCase::Lower, "one-shot upper case ends after one typed character");
    pad.Step(PadButtonL2, 128, 128, 255);
    pad.Step(0);
    pad.Step(PadButtonL2, 128, 128, 255);
    pad.Step(0);
    check(pad.state.caseState == PadKeyboardCase::CapsLock, "a second L2 locks upper case");
    pad.Press(PadButtonCross);
    PadKeyboardNoteInsert(pad.state, true);
    check(pad.state.caseState == PadKeyboardCase::CapsLock, "caps lock survives typing");
    pad.Step(PadButtonL2, 128, 128, 255);
    pad.Step(0);
    check(pad.state.caseState == PadKeyboardCase::Lower, "a third L2 returns to lower case");
    pad.Step(PadButtonL2, 128, 128, 255);
    pad.Step(0, 128, 128, 120);
    pad.Step(PadButtonL2, 128, 128, 255);
    check(pad.state.caseState == PadKeyboardCase::Upper, "L2 does not shift again until the trigger is released past 0.20");
    pad.Step(0, 128, 128, 0);
    pad.Step(PadButtonL2, 128, 128, 255);
    check(pad.state.caseState == PadKeyboardCase::CapsLock, "a released trigger shifts again");
    Driver notTyped;
    notTyped.Step(0);
    notTyped.Step(PadButtonL2, 128, 128, 255);
    notTyped.Step(0);
    notTyped.Press(PadButtonCross);
    PadKeyboardNoteInsert(notTyped.state, false);
    check(notTyped.state.caseState == PadKeyboardCase::Upper, "a character that did not fit keeps the one-shot shift");
}

void symbols() {
    Driver l2First;
    l2First.Step(0);
    l2First.Step(PadButtonL2, 128, 128, 255);
    l2First.Step(PadButtonL2 | PadButtonTriangle, 128, 128, 255);
    check(l2First.state.family == PadKeyboardFamily::Symbols && l2First.FirstLabel() == u"!", "L2 then Triangle opens the symbols");
    l2First.Step(0);
    check(l2First.Press(PadButtonL2).edit == PadEdit::None && l2First.state.family == PadKeyboardFamily::Symbols, "L2 alone does nothing on the symbols");
    l2First.Step(PadButtonL2, 128, 128, 255);
    l2First.Step(PadButtonL2 | PadButtonTriangle, 128, 128, 255);
    check(l2First.state.family == PadKeyboardFamily::Latin, "L2 and Triangle return to the letters");
    Driver triangleFirst;
    triangleFirst.Step(0);
    check(triangleFirst.Step(PadButtonTriangle).text == u" ", "Triangle alone types a space");
    const auto toggled = triangleFirst.Step(PadButtonTriangle | PadButtonL2, 128, 128, 255);
    check(toggled.edit == PadEdit::None && triangleFirst.state.family == PadKeyboardFamily::Symbols, "Triangle then L2 opens the symbols");
    Driver key;
    key.Step(0);
    key.state.cursorRow = 4;
    key.state.cursorColumn = 1;
    check(key.SelectedLabel() == u"@#:", "the symbols key is selected");
    key.Press(PadButtonCross);
    check(key.state.family == PadKeyboardFamily::Symbols && key.SelectedLabel() == u"ABC", "the symbols key opens the symbols and keeps the selection on its key");
    key.state.cursorRow = 2;
    key.state.cursorColumn = 9;
    key.Press(PadButtonCross);
    check(key.FirstLabel() == u"‚", "the next-page key shows the second symbols page");
    key.Press(PadButtonCross);
    check(key.FirstLabel() == u"!", "the previous-page key returns to the first symbols page");
    key.state.cursorRow = 3;
    key.state.cursorColumn = 5;
    check(key.SelectedLabel() == u"€", "the currency key follows the Euro default");
    Driver dollar({true, false, PadKeyboardFamily::Symbols, PadKeyboardCase::Lower, PadKeyboardCurrencyFor(0x4), 0});
    dollar.state.cursorRow = 3;
    dollar.state.cursorColumn = 5;
    check(dollar.SelectedLabel() == u"$", "US English puts the dollar in the currency key");
    check(PadKeyboardCurrencyFor(0x4000 | 0x4) == PadKeyboardCurrency::Won && PadKeyboardCurrencyFor(0x2000) == PadKeyboardCurrency::Yen && PadKeyboardCurrencyFor(0x40000) == PadKeyboardCurrency::Pound, "the currency follows the supported languages");
}

void specials() {
    Driver pad;
    pad.Step(0);
    pad.Press(PadButtonL3);
    check(pad.state.family == PadKeyboardFamily::Specials && pad.FirstLabel() == u"à", "L3 opens the accented letters");
    pad.Step(PadButtonL2, 128, 128, 255);
    pad.Step(0);
    check(pad.FirstLabel() == u"À", "L2 shifts the accented letters");
    pad.Press(PadButtonL3);
    check(pad.state.family == PadKeyboardFamily::Latin, "L3 returns to the letters");
    pad.Step(PadButtonL2 | PadButtonTriangle, 128, 128, 255);
    pad.Step(0);
    check(pad.state.family == PadKeyboardFamily::Symbols, "the symbols are open");
    pad.Press(PadButtonL3);
    check(pad.state.family == PadKeyboardFamily::Symbols, "L3 does nothing on the symbols");
}

void navigation() {
    Driver pad;
    pad.Step(0);
    pad.Press(PadButtonRight);
    check(pad.SelectedLabel() == u"h", "right moves one key");
    pad.Press(PadButtonDown);
    check(pad.SelectedLabel() == u"n", "down moves one row");
    pad.state.cursorRow = 2;
    pad.state.cursorColumn = 0;
    pad.Press(PadButtonLeft);
    check(pad.SelectedLabel() == u"'", "a single press wraps to the far edge at once");
    pad.state.cursorRow = 2;
    pad.state.cursorColumn = 1;
    pad.Step(0);
    pad.Hold(PadButtonLeft, 0.4);
    check(pad.SelectedLabel() == u"a", "a held direction stops at the edge before wrapping");
    pad.Hold(PadButtonLeft, 0.6);
    check(pad.SelectedLabel() != u"a", "the held direction wraps after half a second");
    pad.Step(0);
    pad.state.cursorRow = 4;
    pad.state.cursorColumn = 3;
    pad.Press(PadButtonRight);
    check(pad.SelectedLabel() == u"" && pad.state.cursorColumn == 8, "a spanned key is one step and empty cells are skipped");
    pad.state.cursorRow = 0;
    pad.state.cursorColumn = 2;
    pad.Press(PadButtonUp);
    check(pad.state.cursorRow == 5 && pad.state.cursorColumn == 2, "up from the top row wraps to the bottom row");
    Driver stick;
    stick.Step(0);
    stick.Step(0, 150, 128);
    check(stick.SelectedLabel() == u"g", "a stick inside the dead zone does not move");
    stick.Step(0, 255, 128);
    check(stick.SelectedLabel() == u"h", "the stick moves once past the threshold");
    for (int i = 0; i < 20; ++i) stick.Step(0, 255, 128);
    check(stick.SelectedLabel() != u"h", "a held stick repeats");
}

void repeatsOnlyEditingKeys() {
    Driver typed;
    typed.Step(0);
    const auto characters = count(typed.Hold(PadButtonCross, 0.5), PadEdit::Insert);
    check(characters >= 4, "Cross repeats a character key: " + std::to_string(characters));
    Driver shift;
    shift.Step(0);
    shift.state.cursorRow = 4;
    shift.state.cursorColumn = 0;
    shift.Hold(PadButtonCross, 0.5);
    check(shift.state.caseState == PadKeyboardCase::Upper, "Cross does not repeat the shift key");
}

void priority() {
    Driver pad;
    pad.Step(0);
    const auto both = pad.Step(PadButtonTriangle | PadButtonR2);
    check(both.edit == PadEdit::Insert && both.finish == PadFinish::None, "the space outranks Done in one step");
    pad.Step(0);
    pad.state.cursorRow = 2;
    pad.state.cursorColumn = 4;
    const auto grid = pad.Step(PadButtonCross | PadButtonSquare);
    check(grid.edit == PadEdit::Insert && grid.text == u"g", "a key the grid activates outranks the shortcuts");
}

void controllerDisabled() {
    Driver pad({false});
    pad.Step(0);
    for (const std::uint32_t button : {PadButtonCross, PadButtonR2, PadButtonCircle, PadButtonSquare, PadButtonRight}) {
        const auto result = pad.Press(button);
        check(result.edit == PadEdit::None && result.finish == PadFinish::None, "a disabled controller drives nothing");
    }
    check(pad.SelectedLabel() == u"g", "a disabled controller does not move the selection");
}

void initialPages() {
    Driver symbols({true, false, PadKeyboardFamily::Symbols});
    check(symbols.SelectedLabel() == u"!", "the symbols open on their first key");
    Driver locked({true, false, PadKeyboardFamily::Latin, PadKeyboardCase::CapsLock});
    check(locked.SelectedLabel() == u"1" && locked.state.caseState == PadKeyboardCase::CapsLock, "a shift-locked start uses the first key");
}

}

int main() {
    const std::vector<std::pair<const char*, std::function<void()>>> tests{
        {"starts on g", startsOnG},
        {"held at open", heldAtOpenDoesNothing},
        {"finishes", finishes},
        {"space and backspace", spaceAndBackspace},
        {"caret", caret},
        {"clear all", clearAll},
        {"shift cycle", shiftCycle},
        {"symbols", symbols},
        {"specials", specials},
        {"navigation", navigation},
        {"repeats only editing keys", repeatsOnlyEditingKeys},
        {"priority", priority},
        {"controller disabled", controllerDisabled},
        {"initial pages", initialPages},
    };
    int failed = 0;
    for (const auto& [name, test] : tests) {
        try {
            test();
        } catch (const std::exception& error) {
            std::fprintf(stderr, "FAIL %s: %s\n", name, error.what());
            ++failed;
        }
    }
    if (failed != 0) return 1;
    std::printf("pad keyboard tests passed\n");
    return 0;
}
