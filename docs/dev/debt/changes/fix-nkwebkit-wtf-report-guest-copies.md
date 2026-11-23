# libSceNKWebKit

## add Functional

- [The WTF assertion and crash reports](../../../core/libs/prx/libSceNKWebKit/src/WtfAssert.cpp) (libSceNKWebKit: `WTFReportAssertionFailure`, `WTFReportAssertionFailureWithMessage`, `WTFReportArgumentAssertionFailure`, `WTFReportFatalError`, `WTFReportError`, `WTFReportNotImplementedYet`, `WTFLogVerbose` and the seven `WTFCrashWithInfoImpl` overloads) copy their file, function, assertion and argument-name strings in with `CopyGuestString_nid_no_patch` (64 KiB limit) and print an unreadable or unterminated one as empty, as they already print a null one. This is a host-safety fallback; the console's behaviour for an inaccessible string is not measured. Their format strings and format arguments are still read in place. Closes when: compared with the console.
