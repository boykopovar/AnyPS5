# libSceLibcInternal

## remove [_ZNSbIwSt11char_traitsIwESaIwEE5_CopyEmm]

## add Functional

- [basic_string<wchar_t>::_Copy](../../../core/libs/prx/libSceLibcInternal/BasicString.cpp) (libSceLibcInternal, NID `Ti86LmOKvr0`): the console's `libSceLibcInternal.sprx` was not read and no title was seen calling it. It follows `_Copy` of Dinkumware's [VC12 xstring](https://github.com/ganboing/crt.vc12/blob/f1d47126bf13d1c9dccaa6c53b72217fd0474968/src/xstring#L2200-L2233): a buffer of the wide growth capacity the other wide modifiers use, the first `_Oldlen` units copied, the old buffer released, the string ended at `_Oldlen`; a failed allocation is retried at `_Newsize`, and a second failure leaves an empty inline string and rethrows. Where VC12 settles for `max_size()` after a geometric overflow this reports `bad_alloc`, and a size above `max_size()` reports `length_error`, as the other wide growth paths do. A kept length above the size or the new size, and a new size that fits the inline buffer, throw `std::invalid_argument`; no VC12 caller passes them.
