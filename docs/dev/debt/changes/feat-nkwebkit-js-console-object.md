# libSceNKWebKit

## add Functional

- [console object](../../../core/libs/prx/libSceNKWebKit/src/JscConsole.cpp) (libSceNKWebKit) - not checked on a console. Each JS global object has the `console` of [ConsoleObject.cpp](https://github.com/neonmodder123/PS5-WebKit-11.x/blob/80f77e94c63ee7202eec3d4b5b1a021dd89011ba/Source/JavaScriptCore/runtime/ConsoleObject.cpp#L84-L121): its 26 functions and their argument conversions. The messages go nowhere. JavaScriptCore gives them to the inspector console agent when a frontend is attached, and to the system log when [`logToSystemConsole`](https://github.com/neonmodder123/PS5-WebKit-11.x/blob/80f77e94c63ee7202eec3d4b5b1a021dd89011ba/Source/JavaScriptCore/inspector/JSGlobalObjectConsoleClient.cpp#L39-L48) is on, in which case it also converts every argument to a string; the port has neither, and `JSRemoteInspectorStart` and `JSRemoteInspectorSetLogToSystemConsole` throw. Closes when: the remote inspector exports are implemented.
