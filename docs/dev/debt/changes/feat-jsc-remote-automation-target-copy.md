# libSceJsc

## add Functional

- [RemoteAutomationTarget base-object copy constructor](../../../core/libs/prx/libSceJsc/src/InspectorAccessors.cpp) (libSceJsc) - not compared with a console module; the console JavaScriptCore version and layout are unverified. It follows the implicit copy constructor of pinned [RemoteAutomationTarget.h](https://github.com/WebKit/WebKit/blob/c55364d53f971f63a709976f168d932d558b8909/Source/JavaScriptCore/inspector/remote/RemoteAutomationTarget.h) on the layout already used by `remoteControlAllowed`: it copies the 32-bit target identifier at offset 8 and `m_paired` at offset 12. The vtable slot is left to the derived copy constructor, which stores its own vtable right after; the HLE has no guest vtable for the abstract class. A null `this` or source throws. Closes when: compared with a console module.
