# libSceNKWebKit

## change [empty exports]

- [empty exports](../../../core/libs/prx/libSceNKWebKit/src/NoOpExports.cpp) (libSceNKWebKit) - 128 exports whose body in the 11.00 module reads no argument and writes no return value are now empty `void` functions instead of throwing. They were moved out of `Export.cpp` and their declarations changed from `int` to `void`; the return type is not part of the mangled name, so the exports themselves are unchanged. Read from disassembly of the 11.00 module, not measured on a console. Exports whose module body in 11.00 is a bare `ret` but that a later firmware implements with arguments are left to that implementation.
