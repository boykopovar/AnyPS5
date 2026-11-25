# libSceNKWebKit

## add Unknown function info

- [WebCore static value helpers](../../../core/libs/prx/libSceNKWebKit/src/WebCoreValueHelpers.cpp) (libSceNKWebKit) - not compared with a console module; the pinned tree is newer than the exports. `reduceTimeResolution` floors to 5 seconds ([ResourceLoadStatistics.cpp](https://github.com/neonmodder123/PS5-WebKit-11.x/blob/80f77e94c63ee7202eec3d4b5b1a021dd89011ba/Source/WebCore/loader/ResourceLoadStatistics.cpp#L566-L569)), `platformDefaultMediaSourceEnabled` is `true` ([SettingsBase.cpp](https://github.com/neonmodder123/PS5-WebKit-11.x/blob/80f77e94c63ee7202eec3d4b5b1a021dd89011ba/Source/WebCore/page/SettingsBase.cpp#L87-L91)) and `scriptNameToCode` uses the table of [LocaleToScriptMapping.cpp](https://github.com/neonmodder123/PS5-WebKit-11.x/blob/80f77e94c63ee7202eec3d4b5b1a021dd89011ba/Source/WebCore/platform/text/LocaleToScriptMapping.cpp#L40-L158) with ICU `uscript.h` values, matching four ASCII characters ignoring case. Closes when: compared with a console module.

## remove `_ZN7WebCore12SettingsBase33platformDefaultMediaSourceEnabledEv`

## remove `_ZN7WebCore22ResourceLoadStatistics20reduceTimeResolutionEN3WTF8WallTimeE`
