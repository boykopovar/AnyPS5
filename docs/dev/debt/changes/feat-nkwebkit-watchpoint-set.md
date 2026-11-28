# libSceNKWebKit

## add Unknown function info

- [JSC::WatchpointSet construction, destruction and deferred firing](../../../core/libs/prx/libSceNKWebKit/src/JscWatchpoint.cpp) (libSceNKWebKit) - behavior is not compared with a console. The 20-byte layout (count 0, state 4, not-empty 5, sentinel 6, 4-aligned) with packed 6-byte sentinel links (next at 6, prev at 12) matches the module-translated `JSC::InlineWatchpointSet::inflateSlow` of [#23758](https://github.com/boykopovar/AnyPS5/pull/23758), which allocates 0x14 bytes and self-links the sentinel at +0x6 and +0xc, and the pinned headers: `PackedRawSentinelNode` links are `Packed` 6-byte values, alignment 1 ([Watchpoint.h](https://github.com/neonmodder123/PS5-WebKit-11.x/blob/80f77e94c63ee7202eec3d4b5b1a021dd89011ba/Source/JavaScriptCore/bytecode/Watchpoint.h#L266-L278), [Packed.h](https://github.com/neonmodder123/PS5-WebKit-11.x/blob/80f77e94c63ee7202eec3d4b5b1a021dd89011ba/Source/WTF/wtf/Packed.h#L107-L175)). Follows [Watchpoint.cpp](https://github.com/neonmodder123/PS5-WebKit-11.x/blob/80f77e94c63ee7202eec3d4b5b1a021dd89011ba/Source/JavaScriptCore/bytecode/Watchpoint.cpp#L88-L178): constructor writes count, state, self-linked sentinel; destructor unlinks without firing; deferred `fireAllSlow` moves list, state, not-empty flag into `DeferredWatchpointFire`, then invalidates the set. Null objects throw. Closes when: verified on a console.

- [JSC::WatchpointSet firing and InlineWatchpointSet exports](../../../core/libs/prx/libSceNKWebKit/Export.cpp) (libSceNKWebKit) - still throw: both remaining `fireAllSlow` overloads reach `fireAllWatchpoints`, which fires each listed `Watchpoint` through `runWithDowncast` into the derived types of `JSC_WATCHPOINT_TYPES` and vtables this library does not model, and constructs `DeferGCForAWhile`, changing the unmodelled `vm.heap` deferral depth ([Watchpoint.cpp](https://github.com/neonmodder123/PS5-WebKit-11.x/blob/80f77e94c63ee7202eec3d4b5b1a021dd89011ba/Source/JavaScriptCore/bytecode/Watchpoint.cpp#L145-L156)). The three `InlineWatchpointSet` exports stay stubs because no export can give the guest an `InlineWatchpointSet` object: its constructor is inline upstream and nothing returns or stores one. Closes when: the watchpoint types and `VM` heap depth are modelled and a guest `InlineWatchpointSet` source exists.

## remove | `_ZN3JSC13WatchpointSet11fireAllSlowERNS_2VMEPNS_22DeferredWatchpointFireE` | `98kInJrpGRo` |

## remove | `_ZN3JSC13WatchpointSetC1ENS_15WatchpointStateE` | `hIhXV8DgVnI` |

## remove | `_ZN3JSC13WatchpointSetC2ENS_15WatchpointStateE` | `zF4GnUv7KPw` |

## remove | `_ZN3JSC13WatchpointSetD1Ev` | `9mP+epi+9zU` |

## remove | `_ZN3JSC13WatchpointSetD2Ev` | `Wztwg+APnsg` |
