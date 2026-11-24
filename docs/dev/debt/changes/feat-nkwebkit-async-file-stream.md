# libSceNKWebKit

## add Functional

- [WebCore::AsyncFileStream](../../../core/libs/prx/libSceNKWebKit/src/AsyncFileStream.cpp) (libSceNKWebKit) - the constructors, the destructors, `openForRead`, `close`, `perform`, `read` and `getSize`, not compared with a console module. They follow the 2020 [AsyncFileStream.cpp](https://github.com/WebKit/WebKit/blob/73a35b43be7ae44c084885bd3fa4b403e59723c8/Source/WebCore/fileapi/AsyncFileStream.cpp) and [FileStream.cpp](https://github.com/WebKit/WebKit/blob/73a35b43be7ae44c084885bd3fa4b403e59723c8/Source/WebCore/platform/FileStream.cpp). The `FileStream` handed to a `perform` operation has the layout of the pinned header (handle 0, bytes processed 8, total 16), derived, not measured. The file thread is a kernel thread with default attributes and no name. Size and modification time come from `stat`. No test covers a failed read or a read interrupted by a signal. Closes when: compared with a console module.
- [AsyncFileStream after a failed seek](../../../core/libs/prx/libSceNKWebKit/src/AsyncFileStream.cpp) (libSceNKWebKit) - when the seek of `openForRead` fails, the descriptor is closed and the handle reset, as asked in review; the source returns with the handle still open, so its next `openForRead` reports success without opening anything. The test reaches that path only on a host that refuses a seek to the largest offset (ext4 does, NTFS does not). Closes when: the module's behaviour after a failed seek is measured.
- [AsyncFileStream where the source is undefined](../../../core/libs/prx/libSceNKWebKit/src/AsyncFileStream.cpp) (libSceNKWebKit) - `getSize` with an expected modification time that is NaN throws `std::invalid_argument` (the source converts it to `time_t` with a cast whose result is undefined), a `perform` operation that returns a null `WTF::Function` makes its main thread task throw `std::logic_error` (the source calls through the null pointer), and a call on a destroyed stream throws `std::invalid_argument` (the source dereferences the released pointer). Closes when: the module's result for a NaN time is measured.

## remove `_ZN7WebCore15AsyncFileStream11openForReadERKN3WTF6StringExx`

## remove `_ZN7WebCore15AsyncFileStream5closeEv`

## remove `_ZN7WebCore15AsyncFileStream7performEON3WTF8FunctionIFNS2_IFvRNS_16FileStreamClientEEEERNS_10FileStreamEEEE`

## remove `_ZN7WebCore15AsyncFileStreamC1ERNS_16FileStreamClientE`

## remove `_ZN7WebCore15AsyncFileStreamC2ERNS_16FileStreamClientE`

## remove `_ZN7WebCore15AsyncFileStreamD1Ev`

## remove `_ZN7WebCore15AsyncFileStreamD2Ev`
