# libSceLibcInternal

## remove _ZNSt8messagesIcEC1Em

## remove _ZNSt8messagesIcEC2EPKcm

## remove _ZNSt8messagesIwEC1Em

## remove _ZNSt8messagesIwEC2EPKcm

## remove _ZNSt8messagesIwEC2ERKSt8_Locinfom

## add Functional

- [The remaining messages<char> and messages<wchar_t> constructor forms](../../../core/libs/prx/libSceLibcInternal/Messages.cpp) (libSceLibcInternal, NIDs `6gCBQs1mIi4`, `qF3mHeMAHVk`, `n4+3hznhkU4`, `DlDsyX+XsoA`, `DDQjbwNC31E`): the complete-object `(size_t)` forms and the base-object `(const char*, size_t)` forms of both, and the base-object `(const _Locinfo&, size_t)` form of `messages<wchar_t>`, do what their already implemented sibling forms do, as the [Itanium C++ ABI](https://itanium-cxx-abi.github.io/cxx-abi/abi.html#mangling-special-ctor-dtor) emits identical complete- and base-object constructors for a class without virtual bases. They share the siblings' unverified points; the console's bodies for these NIDs were not read.
