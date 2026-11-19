# libSceNKWebKit

## add Functional

- [WTF::PrintStream::printfVariableFormat](../../../core/libs/prx/libSceNKWebKit/src/WtfPrintfVariableFormat.cpp) (libSceNKWebKit) - not compared with a console module. The body follows [PrintStream.cpp](https://github.com/neonmodder123/PS5-WebKit-11.x/blob/80f77e94c63ee7202eec3d4b5b1a021dd89011ba/Source/WTF/wtf/PrintStream.cpp#L46-L56): it starts a System V `va_list` and passes the format and that list to the stream's virtual `vprintf`, read from vtable slot 2 as in the `Jsc::WtfAbi::PrintStream` model. A null stream, vtable or `vprintf` slot throws, and exceptions from `vprintf` propagate. Closes when: compared with a console module.

## remove `_ZN3WTF11PrintStream20printfVariableFormatEPKcz`
