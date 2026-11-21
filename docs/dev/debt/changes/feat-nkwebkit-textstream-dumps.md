# libSceNKWebKit

## add Additional callable exports

- [Editing TextStream dumps](../../../core/libs/prx/libSceNKWebKit/src/WebCoreDump.cpp) (libSceNKWebKit) - console ABI and output are unmeasured. The `VisiblePosition`, `VisibleSelection` and internal `Position` dumps follow the pinned [VisiblePosition.cpp:772-792](https://github.com/neonmodder123/PS5-WebKit-11.x/blob/80f77e94c63ee7202eec3d4b5b1a021dd89011ba/Source/WebCore/editing/VisiblePosition.cpp#L772-L792), [VisibleSelection.cpp:737-747](https://github.com/neonmodder123/PS5-WebKit-11.x/blob/80f77e94c63ee7202eec3d4b5b1a021dd89011ba/Source/WebCore/editing/VisibleSelection.cpp#L737-L747) and [Position.cpp:1540-1569](https://github.com/neonmodder123/PS5-WebKit-11.x/blob/80f77e94c63ee7202eec3d4b5b1a021dd89011ba/Source/WebCore/dom/Position.cpp#L1540-L1569); the `Position`, `VisiblePosition` and `VisibleSelection` layouts are source-derived. Affinity and anchor-type values outside their switches append nothing. Closes when: compared with a console module.

- [WebCore parameter TextStream dump](../../../core/libs/prx/libSceNKWebKit/src/WebCoreDump.cpp) (libSceNKWebKit) - console ABI and output are unmeasured. The `ScrollableAreaParameters` dump follows the pinned [ScrollingCoordinatorTypes.cpp:129-152](https://github.com/neonmodder123/PS5-WebKit-11.x/blob/80f77e94c63ee7202eec3d4b5b1a021dd89011ba/Source/WebCore/page/scrolling/ScrollingCoordinatorTypes.cpp#L129-L152); the 11-byte `ScrollableAreaParameters` layout is source-derived. Closes when: compared with a console module.

- [WebCore object TextStream dumps](../../../core/libs/prx/libSceNKWebKit/src/WebCoreDump.cpp) (libSceNKWebKit) - console ABI and output are unmeasured. The `Vector<unsigned long>` dump follows the pinned [TextStream.h:236-251](https://github.com/neonmodder123/PS5-WebKit-11.x/blob/80f77e94c63ee7202eec3d4b5b1a021dd89011ba/Source/WTF/wtf/text/TextStream.h#L236-L251). `Vector` items are copied and validated through GuestCopyIn before the first append; an unreadable buffer throws. Closes when: compared with a console module.

## change [WTF text builder layouts]

- [WTF text builder layouts](../../../core/libs/prx/libSceNKWebKit/src/WtfTextAbi.hpp) (libSceNKWebKit) - layouts, buffer growth and the `shrinkToFit` threshold are not checked against a console binary. `WTF::StringBuilder` (40 bytes) and `WTF::TextStream` (56 bytes) follow the pinned 11.x [StringBuilder.h](https://github.com/neonmodder123/PS5-WebKit-11.x/blob/80f77e94c63ee7202eec3d4b5b1a021dd89011ba/Source/WTF/wtf/text/StringBuilder.h#L365-L376) and [TextStream.h](https://github.com/neonmodder123/PS5-WebKit-11.x/blob/80f77e94c63ee7202eec3d4b5b1a021dd89011ba/Source/WTF/wtf/text/TextStream.h#L185-L191) without `ASSERT_ENABLED`, pinned with `static_assert`: the `TextStream` holds the builder, then `indent` at 40, one-byte formatting flags at 44, `multiLineMode` at 45 and `containerSizeLimit` at 48; the earlier transcription had flags and indent swapped. Closes when: compared with a console module.

## remove | `_ZN7WebCorelsERN3WTF10TextStreamERKNS_15VisiblePositionE` | `at-9g8ayvfA` |

## remove | `_ZN7WebCorelsERN3WTF10TextStreamERKNS_16VisibleSelectionE` | `WsBJZWjWhmc` |

## remove | `_ZN7WebCorelsERN3WTF10TextStreamENS_24ScrollableAreaParametersE` | `vrqGZr3Opv8` |
