# libSceNKWebKit

## add Functional

- [WTF time and ASCIILiteral dump guest access](../../../core/libs/prx/libSceNKWebKit/src/WtfTimeDump.cpp) (libSceNKWebKit) - returning an exception for unreadable non-null objects, strings, stream records or vtables, null callbacks and a null stream returned by `begin` is host-safety behavior, not measured on a console. Objects and callback slots use checked guest copies; ASCIILiteral characters are copied after `begin`. A failing later read can leave earlier callback output visible. MediaTime is read again between callback groups, preserving callback changes to its value. The console signature, layouts and callback slot order remain unverified.
