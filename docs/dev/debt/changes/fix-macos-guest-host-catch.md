# libc

## add Functional

- [Guest exceptions in macOS host catches](../../../core/libs/prx/libc/src/exception/Runtime.cpp) (libc) - libc++abi treats these exceptions as foreign. The guest catch record survives a bare host rethrow until libc++abi deletes the unwind exception, so guest queries during that unwind can still see the record. Nested foreign catches in libc++abi and rethrows from its unwinder back into guest frames are outside this bridge.
