# libSceNKWebKit

## change [JavaScriptCore C API]

- [JavaScriptCore C API](../../../core/libs/prx/libSceNKWebKit/src/JscContext.cpp) (libSceNKWebKit) - `JS*` runs on QuickJS-NG (`3rdparty/quickjs`), not JavaScriptCore: there is no JIT and no conservative stack scan, and error messages and property order come from QuickJS. An unprotected `JSObjectRef` held only in a native variable or heap structure across `JSGarbageCollect` is collected, where JavaScriptCore keeps it through its stack scan; `JSValueProtect`, a marking constraint, a JavaScript reference or an active callback frame keep it alive. `JSStringRef` values are `libSceJsc` strings.

## add Functional

- [Error line, column, sourceURL and stack](../../../core/libs/prx/libSceNKWebKit/src/JscErrorInfo.cpp) (libSceNKWebKit) - not checked on a console. Errors get the `line`, `column`, `sourceURL` and `stack` of [ErrorInstance.cpp](https://github.com/neonmodder123/PS5-WebKit-11.x/blob/80f77e94c63ee7202eec3d4b5b1a021dd89011ba/Source/JavaScriptCore/runtime/ErrorInstance.cpp#L247-L278), built from QuickJS frames. Gaps: columns are QuickJS columns; at most 64 frames, where `Error.stackTraceLimit` allows 100; a function made through the API is not a frame; a `JSON.parse` error reports the line of its text, not of the caller; the source URL is not stripped as `URL::strippedForUseAsReport` does; `Error.captureStackTrace` and the `stack` accessor of `Error.prototype` remain. Closes when: compared with a console.
