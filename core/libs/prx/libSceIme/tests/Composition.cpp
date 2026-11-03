#include "prx/libSceIme/include/ImeComposition.hpp"

#include <cstdio>
#include <stdexcept>

namespace {

void check(bool condition, const char* reason) {
    if (!condition) throw std::runtime_error(reason);
}

ImeComposition composing(const std::u16string& text, std::int32_t start = 0, std::int32_t length = 0) {
    ImeComposition composition;
    composition.Set(text, start, length);
    return composition;
}

void sets_cursor_and_selection_in_characters() {
    auto composition = composing(u"\u3053\u3093\u306B\u3061\u306F", 2, 2);
    check(composition.Active() && composition.cursor == 2 && composition.selection == 2, "start and length count characters");
    composition = composing(u"a\U0001F600b", 2, 1);
    check(composition.cursor == 3 && composition.selection == 1, "a surrogate pair is one character");
    composition = composing(u"ab", 9, 4);
    check(composition.cursor == 2 && composition.selection == 0, "a start past the end is clamped");
    composition = composing(u"abc", 1, 99);
    check(composition.cursor == 1 && composition.selection == 2, "a length past the end is clamped");
    composition = composing(u"abc", -3, -1);
    check(composition.cursor == 0 && composition.selection == 0, "negative values are zero");
    composition = composing(u"abc", -1, 3);
    check(composition.cursor == 0 && composition.selection == 3, "a negative start is zero and the length is kept");
    composition = composing(u"abc", 2147483647, 2147483647);
    check(composition.cursor == 3 && composition.selection == 0, "huge values do not overflow");
}

void empty_text_is_not_active() {
    auto composition = composing(u"", 3, 3);
    check(!composition.Active() && composition.cursor == 0 && composition.selection == 0, "an empty composition is inactive");
    composition = composing(u"x");
    composition.Clear();
    check(!composition.Active() && composition.text.empty(), "Clear empties the composition");
}

void commit_clears_the_composition() {
    auto composition = composing(u"ni");
    check(ImeRouteWhileComposing(composition, ImeHostInputKind::Text) == ImeCompositionRoute::Apply, "a commit is applied");
    check(!composition.Active(), "a commit ends the composition");
    check(ImeRouteWhileComposing(composition, ImeHostInputKind::Text) == ImeCompositionRoute::Apply, "text without a composition is applied");
}

void enter_and_escape_end_a_composition_first() {
    for (const auto kind : {ImeHostInputKind::Enter, ImeHostInputKind::Escape}) {
        auto composition = composing(u"ni");
        check(ImeRouteWhileComposing(composition, kind) == ImeCompositionRoute::Swallow, "the first press only discards the composition");
        check(!composition.Active(), "the composition is gone after that press");
        check(ImeRouteWhileComposing(composition, kind) == ImeCompositionRoute::Apply, "the next press reaches the dialog");
    }
}

void edit_keys_belong_to_the_input_method_while_composing() {
    for (const auto kind : {ImeHostInputKind::Backspace, ImeHostInputKind::Delete, ImeHostInputKind::Left, ImeHostInputKind::Right, ImeHostInputKind::Home, ImeHostInputKind::End, ImeHostInputKind::Paste}) {
        auto composition = composing(u"ni");
        check(ImeRouteWhileComposing(composition, kind) == ImeCompositionRoute::Swallow, "an edit key is swallowed while composing");
        check(composition.Active(), "an edit key does not end the composition");
        composition.Clear();
        check(ImeRouteWhileComposing(composition, kind) == ImeCompositionRoute::Apply, "an edit key is applied when not composing");
    }
}

void preedit_is_never_applied() {
    ImeComposition composition;
    check(ImeRouteWhileComposing(composition, ImeHostInputKind::Preedit) == ImeCompositionRoute::Swallow, "a preedit is swallowed");
    composition = composing(u"ni");
    check(ImeRouteWhileComposing(composition, ImeHostInputKind::Preedit) == ImeCompositionRoute::Swallow && composition.Active(), "a preedit leaves the state to the caller");
}

}

int main() {
    try {
        sets_cursor_and_selection_in_characters();
        empty_text_is_not_active();
        commit_clears_the_composition();
        enter_and_escape_end_a_composition_first();
        edit_keys_belong_to_the_input_method_while_composing();
        preedit_is_never_applied();
    } catch (const std::exception& error) {
        std::fprintf(stderr, "ime composition: %s\n", error.what());
        return 1;
    }
    return 0;
}
