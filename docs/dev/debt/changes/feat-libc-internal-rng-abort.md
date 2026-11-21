# libSceLibcInternal

## remove | `_ZSt10_Rng_abortPKc` | `VrWUXy1gqn8` |

## add Unknown function info

- [std::_Rng_abort](../../../core/libs/prx/libSceLibcInternal/InventoryExports.cpp) (libSceLibcInternal, NID `VrWUXy1gqn8`) - the console module was not read. The `const char*` argument comes from the mangled name; the behaviour follows the Dinkumware `_Rng_abort` of the first MSVC STL release ([xrngabort.cpp](https://github.com/microsoft/STL/blob/219514876ea86491de191ceaa88632616bbc0f19/stl/src/xrngabort.cpp)) and this library's `_Thrd_abort`, which it calls: the message and a newline are written to `stderr` and the process aborts. A null message throws instead of passing null to `fputs`. Closes when: compared with the console module.
