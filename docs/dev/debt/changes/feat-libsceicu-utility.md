# libSceIcu

## add Functional

- [Platform and trace utility exports](../../../core/libs/prx/libSceIcu/Platform.cpp) (libSceIcu) - twelve exports ported from ICU 68.2's putil.cpp, ustrfmt.cpp, uarrsort.cpp and utrace.cpp: the integer formatter, the absolute-path test, the loose property-name comparison, the stable binary search and insertion sort, and the trace entry, exit, data, function and level registry. The sort calls the guest comparator through an APS5_VABI pointer, the trace forwards its variadic arguments as a a_list the guest handler reads, and uprv_itou rejects a null buffer, a non-positive capacity and a radix outside 2..36 instead of writing through them. uprv_tzname, uprv_tzset, uprv_timezone, utrace_format, utrace_vformat and utrace_functionName stay stubs: the first three need the console's timezone data, and the formatters are a separate port.
