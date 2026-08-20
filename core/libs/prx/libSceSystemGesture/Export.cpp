#include <cstdint>
#include <cstddef>
#include "SceTypes.hpp"

extern "C" {

int sceSystemGestureAppendTouchRecognizer(int32_t gesture_handle, SystemGestureTouchRecognizer* recognizer) {
 (void)gesture_handle;
 (void)recognizer;
 return 0;
}

int sceSystemGestureClose(int32_t gesture_handle) {
 (void)gesture_handle;
 return 0;
}

int sceSystemGestureCreateTouchRecognizer(int32_t gesture_handle, SystemGestureTouchRecognizer* recognizer, int32_t, const SystemGestureRectangle*, const void*) {
 (void)gesture_handle;
 (void)recognizer;
 (void)int32_t;
 (void)SystemGestureRectangle;
 return 0;
}

int sceSystemGestureFinalizePrimitiveTouchRecognizer(void) {
 return 0;
}

int sceSystemGestureGetPrimitiveTouchEventByIndex(int32_t gesture_handle, uint32_t, SystemGesturePrimitiveTouchEvent* event) {
 (void)gesture_handle;
 (void)uint32_t;
 (void)event;
 return 0;
}

int sceSystemGestureGetPrimitiveTouchEventByPrimitiveID(int32_t gesture_handle, uint16_t, SystemGesturePrimitiveTouchEvent* event) {
 (void)gesture_handle;
 (void)uint16_t;
 (void)event;
 return 0;
}

int sceSystemGestureGetPrimitiveTouchEvents(int32_t gesture_handle, SystemGesturePrimitiveTouchEvent* event_buffer, uint32_t capacity_of_buffer, uint32_t* number_of_event) {
 (void)gesture_handle;
 (void)event_buffer;
 (void)capacity_of_buffer;
 (void)number_of_event;
 return 0;
}

int sceSystemGestureGetPrimitiveTouchEventsCount(int32_t gesture_handle) {
 (void)gesture_handle;
 return 0;
}

int sceSystemGestureGetTouchEventByEventID(int32_t gesture_handle, const SystemGestureTouchRecognizer* recognizer, uint32_t, SystemGestureTouchEvent* event) {
 (void)gesture_handle;
 (void)recognizer;
 (void)uint32_t;
 (void)event;
 return 0;
}

int sceSystemGestureGetTouchEventByIndex(int32_t gesture_handle, const SystemGestureTouchRecognizer* recognizer, uint32_t, SystemGestureTouchEvent* event) {
 (void)gesture_handle;
 (void)recognizer;
 (void)uint32_t;
 (void)event;
 return 0;
}

int sceSystemGestureGetTouchEvents(int32_t gesture_handle, const SystemGestureTouchRecognizer* recognizer, SystemGestureTouchEvent* event_buffer, uint32_t capacity_of_buffer, uint32_t* number_of_event) {
 (void)gesture_handle;
 (void)recognizer;
 (void)event_buffer;
 (void)capacity_of_buffer;
 (void)number_of_event;
 return 0;
}

int sceSystemGestureGetTouchEventsCount(int32_t gesture_handle, const SystemGestureTouchRecognizer* recognizer) {
 (void)gesture_handle;
 (void)recognizer;
 return 0;
}

int sceSystemGestureGetTouchRecognizerInformation(int32_t gesture_handle, const SystemGestureTouchRecognizer* recognizer, SystemGestureTouchRecognizerInformation* information) {
 (void)gesture_handle;
 (void)recognizer;
 (void)information;
 return 0;
}

int sceSystemGestureInitializePrimitiveTouchRecognizer(const void*) {
 return 0;
}

int32_t sceSystemGestureOpen(int32_t input_type, const void*) {
 (void)input_type;
 return 0;
}

int sceSystemGestureRemoveTouchRecognizer(int32_t gesture_handle, SystemGestureTouchRecognizer* recognizer) {
 (void)gesture_handle;
 (void)recognizer;
 return 0;
}

int sceSystemGestureResetPrimitiveTouchRecognizer(int32_t gesture_handle) {
 (void)gesture_handle;
 return 0;
}

int sceSystemGestureResetTouchRecognizer(int32_t gesture_handle, SystemGestureTouchRecognizer* recognizer) {
 (void)gesture_handle;
 (void)recognizer;
 return 0;
}

int sceSystemGestureUpdateAllTouchRecognizer(int32_t gesture_handle) {
 (void)gesture_handle;
 return 0;
}

int sceSystemGestureUpdatePrimitiveTouchRecognizer(int32_t gesture_handle, const void*) {
 (void)gesture_handle;
 return 0;
}

int sceSystemGestureUpdateTouchRecognizer(int32_t gesture_handle, SystemGestureTouchRecognizer* recognizer) {
 (void)gesture_handle;
 (void)recognizer;
 return 0;
}

int sceSystemGestureUpdateTouchRecognizerRectangle(int32_t gesture_handle, SystemGestureTouchRecognizer* recognizer, const SystemGestureRectangle* rectangle) {
 (void)gesture_handle;
 (void)recognizer;
 (void)rectangle;
 return 0;
}

}
