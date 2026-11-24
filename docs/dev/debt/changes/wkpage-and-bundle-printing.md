# libSceNKWebKit

## add Silent stubs

- [WKPageBeginPrinting, WKPageEndPrinting](../../../core/libs/prx/libSceNKWebKit/src/Printing.cpp) (libSceNKWebKit) - toggle a printing flag on the page that only they read (`WKPageComputePagesForPrinting` sets it too), and store the print info without using it.
- [WKPageComputePagesForPrinting](../../../core/libs/prx/libSceNKWebKit/src/Printing.cpp) (libSceNKWebKit) - calls the callback at once with one page rect equal to the available paper size and the requested scale, as no layout exists to paginate.
- [WKBundlePageCopyRenderTreeExternalRepresentationForPrinting](../../../core/libs/prx/libSceNKWebKit/src/Printing.cpp) (libSceNKWebKit) - returns an empty string, as no render tree exists.

## remove `63tAnrWmYsQ`

## remove `hujPhSbtd4A`

## remove `uuF9vP1RSZ8`

## remove `ffNVq81OG9I`
