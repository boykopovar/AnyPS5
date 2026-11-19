# libSceIcu

## change [ICU UText exports]

- [ICU UText exports](../../../core/libs/prx/libSceIcu/UText.cpp) implement `utext_close` as ICU 68.2 does ([utext.cpp](https://github.com/unicode-org/icu/blob/84e1f26ea77152936e70d53178a816dbfbf69989/icu4c/source/common/utext.cpp) 664-705), with ICU's 64-bit `UText` and `UTextFuncs` offsets: a handle that is null, not open or without the magic comes back unchanged; an open one runs its provider's `close` and frees heap-allocated extra storage and a heap-allocated `UText` through the ICU allocator. Every path that needs a `UText` provider throws `NotImplemented_nid_no_patch`. Unverified on a console. Closes when: those are implemented and compared with the console.
