# libSceJsc

## add Silent stubs

- [JSC::sanitizeStackForVM](../../../core/libs/prx/libSceJsc/src/SanitizeStack.cpp) (libSceJsc) - does not zero the stack. Pinned [VM.cpp](https://github.com/WebKit/WebKit/blob/c55364d53f971f63a709976f168d932d558b8909/Source/JavaScriptCore/runtime/VM.cpp#L1348-L1361) clears the dead stack below the caller so that the garbage collector's conservative stack scan does not keep stale cells alive. This module has no garbage collector that scans guest stacks, so it returns without work; a null VM throws. Closes when: libSceJsc scans guest stacks for cells.
