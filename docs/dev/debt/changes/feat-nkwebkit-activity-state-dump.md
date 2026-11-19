# libSceNKWebKit

## add Functional

- [WebCore activity state dump](../../../core/libs/prx/libSceNKWebKit/src/ActivityState.cpp) (libSceNKWebKit) - not compared with a console module: the device's WebKit revision is unverified. The 3.20 `operator<<(TextStream&, OptionSet<ActivityState::Flag>)` follows [ActivityState.cpp](https://github.com/WebKit/WebKit/blob/73a35b43be7ae44c084885bd3fa4b403e59723c8/Source/WebCore/page/ActivityState.cpp#L33-L56) of August 2020, whose nested `Flag` enum matches the export name and has nine flags; the 11.x source adds `IsConnectedToHardwareConsole` as bit 9, which this revision ignores like any other undeclared bit. The set is passed as a 32-bit value, the unsigned underlying type of the unscoped enum per [OptionSet.h](https://github.com/WebKit/WebKit/blob/73a35b43be7ae44c084885bd3fa4b403e59723c8/Source/WTF/wtf/OptionSet.h#L108). Closes when: compared with a console module.
