# libSceAgcDriver

## remove [sceAgcDriverInitResourceRegistration, sceAgcDriverQueryResourceRegistrationUserMemoryRequirements]

## add Functional

- [sceAgcDriverQueryResourceRegistrationUserMemoryRequirements](../../../core/libs/prx/libSceAgcDriver/Resource/src/Lifecycle.cpp) uses safe copy-out and returns `0x80020065` for an unwritable non-null output, preserving errno. Console invalid-pointer and errno behaviour is unmeasured. [obSCEne's public report](https://github.com/project-oops/obSCEne/blob/de5efc406319ade18df00654025494804405a7fb/reports/hardware/report-20261013-172400.log#L23428-L23439) records query result `0x8A6C9018` and output-size value zero. It does not establish a write from a nonzero sentinel, write width, null handling or the initializer's result; it records only the initializer's presence. This implementation clears a writable eight-byte output and returns `0x8A6C9018` for null/writable query outputs and initialization. Those additional behaviours are not verified by this cited report. Local regression tests check AnyPS5 behaviour, not console equivalence.
