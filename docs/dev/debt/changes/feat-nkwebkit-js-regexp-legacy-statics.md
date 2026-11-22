# libSceNKWebKit

## add Functional

- [RegExp legacy static properties](../../../core/libs/prx/libSceNKWebKit/src/JscRegExp.cpp) (libSceNKWebKit) - not checked on a console. `RegExp.$1` to `$9`, `input`, `lastMatch`, `lastParen`, `leftContext`, `rightContext`, `multiline` and their `$` aliases follow [`RegExpConstructor.cpp`](https://github.com/neonmodder123/PS5-WebKit-11.x/blob/80f77e94c63ee7202eec3d4b5b1a021dd89011ba/Source/JavaScriptCore/runtime/RegExpConstructor.cpp). They are fed by a replacement of `RegExp.prototype.exec`, through which the engine runs every match. In `replace` with a global expression and a function, the engine finds every match before it calls the function, so the function sees the last match where JavaScriptCore shows the current one. Closes when: the engine records each match itself, and compared with a console.
