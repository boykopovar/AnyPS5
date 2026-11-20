# libSceJsc

## add Silent stubs

- [WTF::initializeThreading](../../../core/libs/prx/libSceJsc/src/WtfThreading.cpp) (libSceJsc) - returns without work. Pinned [Threading.cpp](https://github.com/WebKit/WebKit/blob/c55364d53f971f63a709976f168d932d558b8909/Source/WTF/wtf/Threading.cpp#L367-L391) forwards it to `WTF::initialize`, which seeds the random number generator, creates the thread TLS key, sets up the date cache and installs thread-suspension signal handlers once. This module keeps no such state: its random helpers seed themselves on first use, and it has no WTF `Thread` objects, date cache or garbage collector that suspends threads. Embedders call it once at startup, where it threw. Closes when: libSceJsc models WTF threads or dates.

## remove | `_ZN3WTF19initializeThreadingEv` | `9zPBIupchgA` |
