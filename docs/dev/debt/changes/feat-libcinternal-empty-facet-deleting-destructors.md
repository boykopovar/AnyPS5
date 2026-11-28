# libSceLibcInternal

## add Additional callable exports

- [Facet deleting destructors](../../../core/libs/prx/libSceLibcInternal/InventoryExports.cpp) (libSceLibcInternal) - the `D0` destructors of `collate<wchar_t>`, `num_get<char>`, `num_get<wchar_t>`, `num_put<char>`, `num_put<wchar_t>`, `money_get<char>`, `money_get<wchar_t>` and `money_put<char>` (NIDs `ig4VDIRc21Q`, `-1G1iE3KyGI`, `aHDdLa7jA1U`, `aOK5ucXO-5g`, `jyXTVnmlJD4`, `b9QSruV4nnc`, `RB3ratfpZDc` and `s2zG12AYKTg`) only release the object through `_ZdlPv`. Under the [Itanium C++ ABI](https://itanium-cxx-abi.github.io/cxx-abi/abi.html#definitions) `D0` is `D1` followed by `operator delete`, and their `D1` is the bare `ret` noted under Empty facet and exception members, as for the existing `money_put<wchar_t>` `D0`. The console bodies of these `D0` exports were not read and no title was seen calling them
