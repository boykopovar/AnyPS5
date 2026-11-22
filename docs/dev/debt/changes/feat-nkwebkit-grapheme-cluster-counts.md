# libSceNKWebKit

## add Functional

- [WTF grapheme cluster counts](../../../core/libs/prx/libSceNKWebKit/src/GraphemeClusters.cpp) (libSceNKWebKit) - a 16-bit string with a code unit above U+00FF throws through `NotImplemented_nid_no_patch`: it needs the ICU character break iterator, and the bundled ICU has no break rule data. Not compared with a console module. `numGraphemeClusters` and `numCodeUnitsInGraphemeClusters` follow [PS5 WebKit 11.x TextBreakIterator.cpp](https://github.com/neonmodder123/PS5-WebKit-11.x/blob/80f77e94c63ee7202eec3d4b5b1a021dd89011ba/Source/WTF/wtf/text/TextBreakIterator.cpp#L178-L233). Its Latin-1 path also serves 16-bit strings with every code unit at most U+00FF, which [UAX #29](https://www.unicode.org/reports/tr29/#Grapheme_Cluster_Boundary_Rules) splits the same way. A null buffer the function must read throws `std::invalid_argument`.

## remove | `_ZN3WTF19numGraphemeClustersENS_10StringViewE` | `K7qAbz7P3rs` |

## remove | `_ZN3WTF30numCodeUnitsInGraphemeClustersENS_10StringViewEj` | `3PZOiy2oZ9w` |
