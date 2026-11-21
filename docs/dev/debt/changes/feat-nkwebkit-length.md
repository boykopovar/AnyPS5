# libSceNKWebKit

## add Unknown function info

- [WebCore::Length calculated values](../../../core/libs/prx/libSceNKWebKit/src/WebCoreLength.cpp) - unmeasured on a console. Constructor, ref, deref and nonNanCalculatedValue follow [Length.cpp:162-244](https://github.com/neonmodder123/PS5-WebKit-11.x/blob/80f77e94c63ee7202eec3d4b5b1a021dd89011ba/Source/WebCore/platform/Length.cpp#L162-L244): handles grow monotonically from 1, skipping the invalid keys 0 and UINT_MAX; the map owns the leaked reference and copies share it by handle. nonNanCalculatedValue takes int (upstream float) and evaluates the expression node's vtable slot 2 with evaluate()'s clamping. The last deref runs the expression's deleting-destructor slot and frees the value with _ZN3WTF8fastFreeEPv, the upstream pairing: CalculationValue is WTF_MAKE_FAST_ALLOCATED, whose operator delete calls WTF::fastFree (pinned FastMalloc.h:321-323). Null self, value or expression, non-calculated lengths and unknown handles throw. Closes when: compared with a console module.

## remove [Calculated length ref-counting]

## remove | `_ZNK7WebCore6Length3refEv` | `8OHZsn1lLv0` |

## remove | `_ZNK7WebCore6Length5derefEv` | `CKQm3LRNxFQ` |

## change [WebCore layout helpers]

- [WebCore layout helpers](../../../core/libs/prx/libSceNKWebKit/src/WebCoreLayout.cpp) (libSceNKWebKit) - `valueForLength`, `floatValueForLength`, `GridPosition::setSpanPosition`, `GridPosition::setExplicitPosition`, `GridPosition::setMaxPositionForTesting` and `unitedBoundingBoxes` follow the pinned [LengthFunctions.cpp](https://github.com/neonmodder123/PS5-WebKit-11.x/blob/80f77e94c63ee7202eec3d4b5b1a021dd89011ba/Source/WebCore/platform/LengthFunctions.cpp) and that revision's GridPosition.cpp and FloatQuad.cpp. A calculated length throws; upstream computes it through `Length::nonNanCalculatedValue`, implemented here, but the console's int parameter and LayoutUnit conversion are unmeasured. The span clamp is signed ([-max, max]) as in WebKit's `setSpanPosition`; positive-only spans are enforced by the CSS parser. Percent lengths use WebKit's float math with the truncating `LayoutUnit(float)` conversion. Closes when: compared with the console.
