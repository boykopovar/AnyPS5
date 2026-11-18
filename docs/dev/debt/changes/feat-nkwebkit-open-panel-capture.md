# libSceNKWebKit

## add Functional

- [OpenPanelParameters capture and media capture type](../../../core/libs/prx/libSceNKWebKit/src/OpenPanelParameters.cpp) (libSceNKWebKit) - `WKOpenPanelParametersGetMediaCaptureType` returns whether a media capture type is set, as the `bool` in [WKOpenPanelParametersRef.cpp](https://github.com/neonmodder123/PS5-WebKit-11.x/blob/80f77e94c63ee7202eec3d4b5b1a021dd89011ba/Source/WebKit/UIProcess/API/C/WKOpenPanelParametersRef.cpp) does. The deprecated `WKOpenPanelParametersCopyCapture` returns a recorded capture string, or `nullptr` when none is set; the pinned source always returns `nullptr`. Accessors validate `parametersRef` and throw `std::invalid_argument` on null or mismatched type. Console execution is unverified.
