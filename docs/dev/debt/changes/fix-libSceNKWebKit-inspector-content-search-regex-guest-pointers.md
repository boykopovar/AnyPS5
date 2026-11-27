# libSceNKWebKit

## add Functional

- [InspectorContentSearchRegex guest pointer fault behavior](../../../core/libs/prx/libSceNKWebKit/src/InspectorContentSearchRegex.cpp) (libSceNKWebKit) - `Inspector::ContentSearchUtilities::createRegularExpressionForSearchString` and `countRegularExpressionMatches` validate guest `RegularExpression`, `RegularExpressionPrivate`, `WTF::String`, `WTF::StringImpl`, and character buffers with checked guest reads/writes and call `NotImplementedAt_nid_no_patch` on unmapped or unreadable/unwritable guest memory. Console fault behavior is unmeasured. Closes when: console behavior for invalid guest pointers in `Inspector::ContentSearchUtilities` regex exports is verified on hardware.
