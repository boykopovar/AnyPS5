# libSceNKWebKitRequirements

## change 9 pending exports in

- 7 pending exports in [Export.cpp](../../../core/libs/prx/libSceNKWebKitRequirements/Export.cpp) are present in the [PS5-3.20_Libs](https://github.com/DNNDHH/PS5-3.20_Libs) inventory. Their prototypes and behavior are unverified. They use the no-parameter import-resolution convention and throw through `NotImplemented_nid_no_patch`; these declarations make no ABI or behavior claim. The other two, `wpe_pasteboard_string_initialize` and `wpe_pasteboard_string_free`, are implemented from bundled libwpe in [Wpe.cpp](../../../core/libs/prx/libSceNKWebKitRequirements/Wpe.cpp); see Functional below.

## add Functional

- [WPE pasteboard string lifetime](../../../core/libs/prx/libSceNKWebKitRequirements/Wpe.cpp) - The initialize and free helpers follow the descriptor and ownership rules in [libwpe 1.14.0](https://github.com/WebPlatformForEmbedded/libwpe/blob/b90a8396864c9824a43838a5879f6ab01420c69e/src/pasteboard.c). AnyPS5 reserves one extra zero byte for C-string consumers, including for empty strings. The stored length counts payload bytes. Guest copies preserve `errno`. The console ABI and behavior for invalid pointers or allocation failures are unmeasured. AnyPS5 throws for these failures and for lengths that cannot fit with the terminator. `wpe_pasteboard_get_singleton`, `wpe_pasteboard_get_types`, `wpe_pasteboard_get_string`, `wpe_pasteboard_write` and `wpe_pasteboard_string_vector_free` still throw pending a pasteboard backend and provider loading. Closes when: compared with the console.
