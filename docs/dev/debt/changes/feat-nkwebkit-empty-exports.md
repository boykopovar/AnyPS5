# libSceNKWebKit

## add Empty exports

- [Empty exports](../../../core/libs/prx/libSceNKWebKit/src/NoOpExports.cpp) (libSceNKWebKit) - 128 exports whose body in the 11.00 module reads no argument and writes no return value are empty `void` functions instead of throwing. They were moved out of `Export.cpp` and their declarations changed from `int` to `void`; the return type is not part of the mangled name, so the exports themselves are unchanged. Read from disassembly of the 11.00 module, not measured on a console. Exports whose 11.00 body is a bare `ret` but that a later firmware implements with arguments are left to that implementation.