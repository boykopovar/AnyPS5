# libSceSystemGesture technical debt

### Silent stubs

- [System gesture recognizers](../../../core/libs/prx/libSceSystemGesture/Export.cpp) (libSceSystemGesture) - `sceSystemGestureGetTouchEvents`, `GetTouchEventsCount`, `GetTouchEventByIndex`, `GetTouchEventByEventID` and the recognizer append, remove, reset and update calls report that no recognizer ever matches, so a title receives primitive touch events but no tap, drag, hold, pinch, rotation or flick event. Handle and pointer arguments are still checked across all query entry points. `sceSystemGestureGetTouchRecognizerInformation` returns the gesture type and rectangle given to `sceSystemGestureCreateTouchRecognizer` or `UpdateTouchRecognizerRectangle`, stored in the title's recognizer buffer in a layout private to this file, and always reports `updated_time` 0 because no recognizer is ever updated with input. It throws for a recognizer that was not created by `sceSystemGestureCreateTouchRecognizer` or has no rectangle, since the console's result in those cases is not known

### Unknown function info

- [Primitive touch events](../../../core/libs/prx/libSceSystemGesture/Export.cpp): pad handles are not validated against native controller information. Begin, active and end states are confirmed, but cancellation state 4, motion thresholds and SDK-dependent duplicate-report handling remain unimplemented. Timing, coordinate conversion and update flags still use the host approximation. Recognizer allocation and native descriptor layouts remain incomplete.
- [sceSystemGestureDebugGetVersion](../../../core/libs/prx/libSceSystemGesture/Export.cpp) (libSceSystemGesture) - its name and NID are in the [PS5 3.20 export inventory](https://github.com/DNNDHH/PS5-3.20_Libs/blob/b2a77c09cd71c01f34c790cfbe98c751a13262ad/libSceSystemGesture.c), but no credible prototype was found. It is declared without parameters solely to resolve known imports; this is not an ABI claim
