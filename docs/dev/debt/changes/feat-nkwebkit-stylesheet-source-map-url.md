# libSceNKWebKit

## add Functional

- [Inspector::ContentSearchUtilities::findStylesheetSourceMapURL](../../../core/libs/prx/libSceNKWebKit/src/StylesheetSourceMap.cpp) (libSceNKWebKit) - not compared with a console module. It follows the pinned [ContentSearchUtilities.cpp](https://github.com/neonmodder123/PS5-WebKit-11.x/blob/80f77e94c63ee7202eec3d4b5b1a021dd89011ba/Source/JavaScriptCore/inspector/ContentSearchUtilities.cpp#L178-L205) by scanning for the first match of its fixed Yarr pattern directly instead of compiling it: `/*#` or `/*@`, one space or tab, `sourceMappingURL=`, the longest run of non-space, non-quote code units followed by optional spaces or tabs and `*/`, with `\s` as in the pinned Yarr tables. Closes when: compared with a console module.

## remove | `_ZN9Inspector22ContentSearchUtilities26findStylesheetSourceMapURLERKN3WTF6StringE` | `VwaGH4DPYxg` |
