# libSceNKWebKit

## add Unknown function info

- [JSC::ConfigFile](../../../core/libs/prx/libSceNKWebKit/src/JscConfigFile.cpp) (libSceNKWebKit) - console layout and calling convention are unmeasured. Follows [ConfigFile.cpp](https://github.com/neonmodder123/PS5-WebKit-11.x/blob/80f77e94c63ee7202eec3d4b5b1a021dd89011ba/Source/JavaScriptCore/runtime/ConfigFile.cpp) and its header: two 1025-byte name arrays (`PATH_MAX` 1024), `strncpy` truncation at 1024, 1024-byte lines and `build` reporting `Release`. Scanning, `jscOptions`, `logFile` and the `processName`, `parentProcessName` and `build` predicates are parsed, but a `jscOptions` block and a `logFile` path throw because `JSC::Options::setOptions` and `WTF::setDataFile(const char*)` are stubs, and `=~` uses `std::regex` extended grammar, as `HAVE(REGEX_H)` on the console is unverified. Null objects, null names and unterminated name arrays throw where the source faults. Closes when: compared with a console module.
