# libSceNKWebKit

## add Functional

- [EventRegion::operator==](../../../core/libs/prx/libSceNKWebKit/src/Region.cpp) (libSceNKWebKit) - not compared with a console module. It assumes `EventRegion` is only its 24-byte `Region` (`static_assert`): the [pinned source](https://github.com/WebKit/WebKit/blob/c55364d53f971f63a709976f168d932d558b8909/Source/WebCore/rendering/EventRegion.cpp#L101-L120) compares other members only under `TOUCH_ACTION_REGIONS`, `WHEEL_EVENT_REGIONS` and `EDITABLE_REGION`, which are off outside Apple platforms, and the 11.00 `EventRegion` constructors jump to the `Region` one, which does not rule out trailing members that need no construction. Bounds and shapes are compared as in [Region.h](https://github.com/WebKit/WebKit/blob/c55364d53f971f63a709976f168d932d558b8909/Source/WebCore/platform/graphics/Region.h#L188-L205); a region that cannot be read throws. Closes when: compared with a console module.

## remove `_ZNK7WebCore11EventRegioneqERKS0_`
