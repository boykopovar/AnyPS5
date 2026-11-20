# libSceIcu

## add Functional

- [UText framework and the UChar string provider](../../../core/libs/prx/libSceIcu/UTextAccess.cpp) (libSceIcu) - twenty-two exports ported from ICU 68.2's `utext.cpp` to the guest `UText` layout, whose fields and `UTextFuncs` slots are asserted at compile time against the offsets `UText.cpp` already uses (magic 0, `pFuncs` 56, close at vtable offset 80, 144 bytes). `utext_openUTF8`, `utext_openUnicodeString`, `utext_openConstUnicodeString`, `utext_openReplaceable` and `utext_openCharacterIterator` stay stubs: their providers need the UTF-8 chunker and the guest `UnicodeString`, `Replaceable` and `CharacterIterator` models. A UText the library allocates asks the guest heap for its storage, so it needs the relinked program's process metadata; a guest-provided `UText` does not. The new test covers the supplied-struct path, iteration over a surrogate pair, cloning, extraction and the read-only refusals.
