# libSceNKWebKit

## add Functional

- [OpenPanelParameters capture and media capture type](../../../core/libs/prx/libSceNKWebKit/src/OpenPanelParameters.cpp) (libSceNKWebKit) - `WKOpenPanelParametersCopyCapture` and `WKOpenPanelParametersGetMediaCaptureType` follow [WKOpenPanelParameters.cpp](https://github.com/neonmodder123/PS5-WebKit-11.x/blob/80f77e94c63ee7202eec3d4b5b1a021dd89011ba/Source/WebKit/UIProcess/API/C/WKOpenPanelParameters.cpp) and [APIOpenPanelParameters.h](https://github.com/neonmodder123/PS5-WebKit-11.x/blob/80f77e94c63ee7202eec3d4b5b1a021dd89011ba/Source/WebKit/UIProcess/API/APIOpenPanelParameters.h). Accessors validate `parametersRef` and throw `std::invalid_argument` on null or mismatched type. Unset or null capture returns `nullptr`; media capture type defaults to `kWKMediaCaptureTypeNone` (`0`). Console execution is unverified.
