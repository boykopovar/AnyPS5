# libSceJsc

## change [JSC::Yarr::parseFlags]

- [JSC::Yarr::parseFlags](../../../core/libs/prx/libSceJsc/src/YarrFlags.cpp) (libSceJsc) - not compared with a console module; the console JavaScriptCore version is unverified. On a PS5 (FW 11.60, payload process) an 8-bit `StringView` gave: `d g i m s u y` are bits 0 to 6, an input is the OR of its letters in any order, the empty string is an empty set, and `v` or an unknown, upper-case or repeated letter gives no value; the flag set is byte 0 and the engaged flag byte 1 of the 4-byte return register. This library does the same and applies the table to a 16-bit `StringView`. Assumed: the `StringView` is a pointer and a length with the 8-bit flag as bit 32, and the result is read as `eax`; the 16-bit form is not measured. A null character pointer with a nonzero length faults on the console; this library throws. Closes when: compared with a console module.
