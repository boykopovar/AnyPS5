# libSceAgcDriver

## remove [sceAgcDriverInitResourceRegistration, sceAgcDriverQueryResourceRegistrationUserMemoryRequirements]

## add Functional

- [sceAgcDriverInitResourceRegistration, sceAgcDriverQueryResourceRegistrationUserMemoryRequirements](../../../core/libs/prx/libSceAgcDriver/Resource/src/Lifecycle.cpp) have unmeasured console initialization and null-query results. Their `0x8A6C9018` results follow [OpenAGC](https://github.com/OpenAGC/OpenAGC/blob/3c7e8072f0d7684e930454794fd5c02056946d73/src/game_compat.c#L300-L314), a reference implementation, not a console measurement. OpenAGC leaves query output untouched. Unwritable non-null query output returns `0x80020065` through safe copy-out with errno preserved as host policy; console invalid-pointer and errno behaviour is unmeasured.
- [Registration query output](../../../core/libs/prx/libSceAgcDriver/Resource/src/Lifecycle.cpp) zeroes a writable eight-byte value. [obSCEne's probe](https://github.com/project-oops/obSCEne/blob/de5efc406319ade18df00654025494804405a7fb/src/probe/sections/agc.c#L22240-L22259) initializes a `uint64_t` to `0xbeefcafebeefcafe`; its [PS5 12.40 report](https://github.com/project-oops/obSCEne/blob/de5efc406319ade18df00654025494804405a7fb/reports/hardware/report-20261013-172400.log#L23428-L23439) records output zero and result `0x8A6C9018`. This supports the eight-byte change but does not measure surrounding bytes, null or invalid pointers, unaligned output, errno, or initialization. Local regressions check AnyPS5 behaviour, not console equivalence.
