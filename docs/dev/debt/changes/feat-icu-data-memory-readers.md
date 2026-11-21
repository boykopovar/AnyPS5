# libSceIcu

## add Functional

- [ICU data memory accessors](../../../core/libs/prx/libSceIcu/DataMemory.cpp) - `udata_getRawMemory`, `udata_getMemory` and `udata_getLength` read existing guest handles. Their x86-64 [layout](https://github.com/unicode-org/icu/blob/84e1f26ea77152936e70d53178a816dbfbf69989/icu4c/source/common/udatamem.h), [accessor semantics](https://github.com/unicode-org/icu/blob/84e1f26ea77152936e70d53178a816dbfbf69989/icu4c/source/common/udatamem.cpp) and [header byte order](https://github.com/unicode-org/icu/blob/84e1f26ea77152936e70d53178a816dbfbf69989/icu4c/source/common/ucmndata.cpp) are based on ICU 68.2, not console measurements. A required unreadable field or overflowing address throws. Opening and closing a data item are separate exports and are not implemented by these readers.
