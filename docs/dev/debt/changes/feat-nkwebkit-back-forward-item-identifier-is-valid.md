# libSceNKWebKit

## add Functional

- [WebCore back-forward item identifier validity](../../../core/libs/prx/libSceNKWebKit/src/BackForwardItemIdentifier.cpp) (libSceNKWebKit) - not compared with a console module: the device's WebKit revision is unverified. `BackForwardItemIdentifier::isValid() const` follows [BackForwardItemIdentifier.cpp](https://github.com/WebKit/WebKit/blob/73a35b43be7ae44c084885bd3fa4b403e59723c8/Source/WebCore/history/BackForwardItemIdentifier.cpp#L31-L34) of August 2020, whose struct of two 64-bit identifiers matches the export name; the 11.x source makes the type a `ProcessQualified` alias with no such member. The pair is invalid when both identifiers are 0 or the process identifier is `UINT64_MAX`, the hash table's deleted value. Closes when: compared with a console module.
