# libSceNKWebKit

## add Functional

- [Inspector::InspectorTarget::resume](../../../core/libs/prx/libSceNKWebKit/src/InspectorTarget.cpp) (libSceNKWebKit) - not compared with a console module; uses the pinned layouts, unverified on the console: `InspectorTarget` as vtable pointer, `WTF::Function<void()> m_resumeCallback` (one `std::unique_ptr` to `CallableWrapperBase`) at 8 and `bool m_isPaused` at 16, and the `CallableWrapperBase` vtable with the deleting destructor in slot 1 and `call()` in slot 2; clears the flag, calls the callback, then nulls the field and deletes the wrapper as upstream's release build does, skipping its `ASSERT(m_isPaused)`; a null `this` or a callback without a vtable throws `std::invalid_argument` where upstream would crash. Closes when: compared with a console module.

## remove | `_ZN9Inspector15InspectorTarget6resumeEv` | `XiMnw9g4tsI` |
