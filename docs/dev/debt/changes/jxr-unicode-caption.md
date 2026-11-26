# libSceJxr

## add Functional

- [WMP descriptive metadata captions](../../../core/libs/prx/libSceJxr/src/NativeWmp.c) use a NUL-terminated array of 16-bit units for the caption variant in the pinned [jxrlib record](https://github.com/4creators/jxrlib/blob/f7521879862b9085318e814c6157490dd9dbbdb4/jxrgluelib/JXRMeta.h#L117-L159). The encoder owns a copy and preserves its previous metadata if replacement fails. An empty or one-unit caption requires another descriptive value with out-of-line storage. Otherwise the writer throws before emitting the header, since the reference skips every inline entry in this case. Other wide variants still throw. The console ABI, behavior on allocation failure and inline-only case are unmeasured. Closes when: the console module is measured.
