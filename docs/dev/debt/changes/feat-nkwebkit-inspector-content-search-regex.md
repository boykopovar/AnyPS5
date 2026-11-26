# libSceNKWebKit

## add Functional

- [Inspector::ContentSearchUtilities::createRegularExpressionForSearchString and countRegularExpressionMatches](../../../core/libs/prx/libSceNKWebKit/src/InspectorContentSearchRegex.cpp) (libSceNKWebKit) - not compared with a console module; they port the pinned [ContentSearchUtilities.cpp](https://github.com/neonmodder123/PS5-WebKit-11.x/blob/80f77e94c63ee7202eec3d4b5b1a021dd89011ba/Source/JavaScriptCore/inspector/ContentSearchUtilities.cpp#L46-L166) on the libregexp-backed `JSC::Yarr::RegularExpression` model and share its divergences from Yarr. The `SearchStringType` values (0 Regex, 1 ExactString, 2 ContainsString) passed as an `int`, the one-pointer `RegularExpression` returned through the hidden pointer and `WTF::String` are unverified. An unknown `SearchStringType`, a null result, regex or string pointer, or a string with null data and a nonzero length throws `std::invalid_argument` where upstream compiles a null pattern or crashes. Closes when: compared with a console module.

## remove | `_ZN9Inspector22ContentSearchUtilities29countRegularExpressionMatchesERKN3JSC4Yarr17RegularExpressionERKN3WTF6StringE` | `jmHSpqaEYMI` |

## remove | `_ZN9Inspector22ContentSearchUtilities38createRegularExpressionForSearchStringERKN3WTF6StringEbNS0_16SearchStringTypeE` | `UlQSBk8DDBg` |
