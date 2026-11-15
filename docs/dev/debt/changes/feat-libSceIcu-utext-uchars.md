# libSceIcu

## remove | `utext_openUChars` | `o9+yMOok2Zs` |

## remove | `utext_setup` | `sEKpfqARKPI` |

## add Functional

- [UText open and close](../../../core/libs/prx/libSceIcu/NativeUText.cpp) - `utext_setup`, `utext_openUChars` and `utext_close` forward to the bundled ICU [utext.cpp](https://github.com/unicode-org/icu/blob/049e0d6a420629ac7db77256987d083a563287b5/icu4c/source/common/utext.cpp), not to ICU 68.2 as the console module is; the `UText` layout is the same 144 bytes in both. A `UText` allocated here, and its extra storage, come from the bundled ICU heap, which `u_setMemoryFunctions` does not reach. An open `UText` whose `pFuncs` is a guest provider table with a `close` function throws `NotImplemented_nid_no_patch`, as does an open one with a null `pFuncs`: the bundled ICU would call that guest function with the host calling convention. Closes when: guest provider callbacks can be called from these exports.
