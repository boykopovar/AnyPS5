# libSceNKWebKit

## add Functional

- [DisplayList::SetState](../../../core/libs/prx/libSceNKWebKit/src/DisplayListSetState.cpp) (libSceNKWebKit) - the item is the 120-byte Cairo layout of the 2020 source (`Item`, a 96-byte `GraphicsContextState` and the 4-byte change flags), not measured on a console. The constructors copy the state and flags and the destructor releases the item; both throw `std::invalid_argument` when the state holds a gradient, a pattern or an out-of-line colour, because the byte copy neither takes a reference nor releases one, and those states await `GraphicsContextState`. `apply` still throws through `ApplyWithoutGraphicsContext`, so replay awaits `GraphicsContext::setState`. Closes when: `apply` is implemented, `GraphicsContextState` reference counting is implemented, and the layout is checked against a PS5.
