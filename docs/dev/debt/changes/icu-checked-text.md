# libSceIcu

## change [ICU UText exports]

- [ICU UText exports](../../../core/libs/prx/libSceIcu/UText.cpp) have no console ABI or behavior verification. `utext_setup`, `utext_openUChars`, `utext_close` and the UTF-16 provider callbacks follow [ICU 78.1 utext.cpp](https://github.com/unicode-org/icu/blob/049e0d6a420629ac7db77256987d083a563287b5/icu4c/source/common/utext.cpp), with the 144-byte public layout and guest-ABI callbacks. Self-clones, non-144-byte clone layouts and inaccessible memory throw `std::invalid_argument`; overlong NUL-terminated text also throws. Deep clones redirect the current chunk to their owned copy, whereas the pinned source leaves it referencing the original text. The other text providers and generic UText access exports remain unimplemented. Closes when: compared with the console.

## add Functional

- [Unicode conversion guest access](../../../core/libs/prx/libSceIcu/Utf32.cpp) and [Java modified UTF-8 guest access](../../../core/libs/prx/libSceIcu/JavaUtf8.cpp) throw `std::invalid_argument` for inaccessible inputs, outputs and status pointers instead of reproducing a console memory fault. Console fault behavior is unverified. Completed prefix writes are retained when a later access fails. The checked accesses preserve errno; conversion rules still follow the pinned ICU 78.1 sources described above.

- [Case-mapping guest access](../../../core/libs/prx/libSceIcu/CaseMapping.cpp) is unverified on a console. Source and locale strings are copied into host storage, and mapping results are staged before checked writes. A non-overlapping short output must be readable as well as writable so ICU's unwritten trailing units can be retained. Inaccessible memory throws `std::invalid_argument`; host allocation failure can throw `std::bad_alloc`. Concurrent changes to source strings during a call are not observed. The existing missing-default-locale limitation remains.
