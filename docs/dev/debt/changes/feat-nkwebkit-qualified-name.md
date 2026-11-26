# libSceNKWebKit

## add Unknown function info

- [WebCore::QualifiedName interning](../../../core/libs/prx/libSceNKWebKit/src/QualifiedName.cpp) (libSceNKWebKit) - not compared with a console module. The cache is process-wide behind a mutex, not per thread; an entry whose count reached 0 is replaced, not revived. Releasing the last reference to an atom terminates. The 40-byte `QualifiedNameImpl` follows [QualifiedName.h](https://github.com/WebKit/WebKit/blob/c55364d53f971f63a709976f168d932d558b8909/Source/WebCore/dom/QualifiedName.h), since the module exports no `QualifiedNameImpl` constructor and the 11.x source adds fields; behaviour follows [QualifiedNameCache.cpp](https://github.com/WebKit/WebKit/blob/c55364d53f971f63a709976f168d932d558b8909/Source/WebCore/dom/QualifiedNameCache.cpp#L47-L56).

## remove `_ZN7WebCore13QualifiedName17QualifiedNameImplD1Ev`

## remove `_ZN7WebCore13QualifiedName17QualifiedNameImplD2Ev`

## remove `_ZN7WebCore13QualifiedNameC1ERKN3WTF10AtomStringES4_S4_`

## remove `_ZN7WebCore13QualifiedNameC2ERKN3WTF10AtomStringES4_S4_`