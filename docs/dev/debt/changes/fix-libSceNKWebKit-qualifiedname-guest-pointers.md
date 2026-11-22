# libSceNKWebKit

## add Functional

- [QualifiedName guest pointer validation](../../../core/libs/prx/libSceNKWebKit/src/QualifiedName.cpp) - console behaviour on inaccessible or read-only `QualifiedName`, `AtomString`, `QualifiedNameImpl`, or `WTF::StringImpl` header pointers in `WebCore::QualifiedName::QualifiedName` (`C1`/`C2`) and `WebCore::QualifiedName::QualifiedNameImpl::~QualifiedNameImpl` (`D1`/`D2`) is unmeasured. Closes when: console behaviour on invalid guest pointers in these `WebCore::QualifiedName` exports is measured.
