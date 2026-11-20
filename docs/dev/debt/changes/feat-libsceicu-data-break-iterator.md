# libSceIcu

## remove -Hc6XkAq-eU

## remove bZT8JIrrg3I

## remove fsgqunLD2b8

## remove cL3dlqTcRqk

## remove dC+0Pz5afr0

## remove +Nd4bYkEgRc

## remove av8J5NBkbRE

## remove ypOz3lP3VhE

## remove n6SEUE4Txvg

## remove 4Uj1DZGlHTw

## remove NfGO57hqUEw

## remove 9hkT1aMC0Rs

## remove tGeOQLpMGgA

## remove NtjY-lZG4ss

## remove wHZwCpZVcGI

## remove mCyTdtewGpk

## change [Cleanup]

- [Cleanup](../../../core/libs/prx/libSceIcu/Configuration.cpp) is unverified on a console. `u_cleanup` follows ICU 78.1 [ucln_cmn.cpp](https://github.com/unicode-org/icu/blob/049e0d6a420629ac7db77256987d083a563287b5/icu4c/source/common/ucln_cmn.cpp) for the state this module keeps: it clears the functions stored by `u_setMemoryFunctions` and resets the data and time zone files directories. Pointers returned earlier by the directory getters are invalidated, as in ICU. It first calls the native ICU 78.1 `u_cleanup`, whose cleanup chains release the backend's shared caches (see Allocator integration). The NID comes from the 3.20 inventory and its presence on current firmware is unproven (see ICU export availability). Closes when: compared with the console.

## change [Memory function configuration]

- [Memory function configuration](../../../core/libs/prx/libSceIcu/Configuration.cpp) is unverified on a console. `u_setMemoryFunctions` follows ICU 78.1 [cmemory.cpp](https://github.com/unicode-org/icu/blob/049e0d6a420629ac7db77256987d083a563287b5/icu4c/source/common/cmemory.cpp): it stores the context and callbacks under a lock and rejects a null callback with `U_ILLEGAL_ARGUMENT_ERROR`, and native thunks route the ICU 78.1 backend's allocations through them (see Allocator integration); their guest calling convention is unverified on the console. Firmware 13.42 is reported to export no `u_setMemoryFunctions` ([#11132](https://github.com/boykopovar/AnyPS5/pull/11132)); the NID comes from the 3.20 inventory (see ICU export availability). Closes when: compared with the console.

## add Data and C break iteration

- [Source-built data](../../../cmake/IcuData.cmake) for [libSceIcu](../../../core/libs/prx/libSceIcu/Availability.cpp) is not compared with console data. Tools from ICU 78.1's pinned submodule run its `BUILDRULES.py` through `icutools.databuilder`, then `icupkg` and `genccode` link a common package through ICU's standard `icudt78_dat` entry point. The [additive filter](../../../cmake/IcuDataFilter.json) retains all break rules, dictionaries, break locales, locale availability and parent/alias resources, converter aliases, normalization, layout, emoji and miscellaneous dependencies. Optional LSTM and AdaBoost models are excluded: ICU falls back to dictionary segmentation; console model selection is unverified. Other locale resources and API families are excluded; widen the filter for follow-ups. Guest file I/O remains disabled as in #13578; `udata_setFileAccess` stores the policy without validating it or consulting status, as ICU's setter does. Closes when: compared with console data.

- [Native host tools](../../../cmake/icu-host/CMakeLists.txt) for [libSceIcu](../../../core/libs/prx/libSceIcu/CMakeLists.txt) use a separate CMake build even for cross builds. They run on Linux (GCC 15) and Windows (MinGW GCC 15.2, Python 3.12); macOS execution remains unverified. Closes when: tested on macOS.

- [C break iterators](../../../core/libs/prx/libSceIcu/BreakIterator.cpp) have unverified console ABI and behavior. The 14 `ubrk_*` inventory exports other than `ubrk_setUText` forward to pinned ICU 78.1 [ubrk.cpp](https://github.com/unicode-org/icu/blob/049e0d6a420629ac7db77256987d083a563287b5/icu4c/source/common/ubrk.cpp); guest `UChar` is 16-bit, `UBool` 8-bit and status 32-bit. ICU retains the caller's UTF-16 buffer without copying; it must remain alive and unchanged until replacement or close. Handles belong to this backend and cannot be mixed with ICU 68 objects. Null handles or status pointers throw where ICU dereferences them; negative locale indices and index equal to the count throw where `locavailable.cpp` reads outside its allocated array; other arguments and status values pass through ICU. UText callbacks, rule construction, cloning and locale-query exports remain unavailable. Closes when: compared with the console.

- [Allocator integration](../../../core/libs/prx/libSceIcu/Configuration.cpp) is unverified on the console. `u_setMemoryFunctions` and `u_cleanup` keep the validation and state described above and also forward to the native ICU 78.1 functions. Native callback thunks invoke the configured guest-ABI allocator for ICU allocation, reallocation and freeing. Close releases per-handle allocations; shared caches survive until native `u_cleanup`, which runs before clearing the callbacks. Reconfiguration and cleanup require no concurrent use or outstanding handles, as in ICU. Hand-ported objects retain their own allocation paths. Firmware availability remains as recorded under ICU export availability; no ICU 68 C++ objects are changed. Closes when: compared with the console, including the guest callbacks on Windows.
