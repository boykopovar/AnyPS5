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

int sceSystemGestureCreateTouchRecognizer(int32_t gesture_handle, SystemGestureTouchRecognizer* recognizer, int32_t type, const SystemGestureRectangle* rectangle, const void* param) {
 (void)gesture_handle;
 (void)recognizer;
 (void)type;
 (void)rectangle;
 (void)param;
 return 0;
}

int sceSystemGestureFinalizePrimitiveTouchRecognizer(void) {
 return 0;
}

int sceSystemGestureGetPrimitiveTouchEventByIndex(int32_t gesture_handle, uint32_t index, SystemGesturePrimitiveTouchEvent* event) {
 (void)gesture_handle;
 (void)index;
 (void)event;
 return 0;
}

int sceSystemGestureGetPrimitiveTouchEventByPrimitiveID(int32_t gesture_handle, uint16_t primitiveId, SystemGesturePrimitiveTouchEvent* event) {
 (void)gesture_handle;
 (void)primitiveId;
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

int sceSystemGestureGetTouchEventByEventID(int32_t gesture_handle, const SystemGestureTouchRecognizer* recognizer, uint32_t eventId, SystemGestureTouchEvent* event) {
 (void)gesture_handle;
 (void)recognizer;
 (void)eventId;
 (void)event;
 return 0;
}

int sceSystemGestureGetTouchEventByIndex(int32_t gesture_handle, const SystemGestureTouchRecognizer* recognizer, uint32_t index, SystemGestureTouchEvent* event) {
 (void)gesture_handle;
 (void)recognizer;
 (void)index;
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

int sceSystemGestureInitializePrimitiveTouchRecognizer(const void* param) {
 (void)param;
 return 0;
}

int32_t sceSystemGestureOpen(int32_t input_type, const void* param) {
 (void)input_type;
 (void)param;
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

int sceSystemGestureUpdatePrimitiveTouchRecognizer(int32_t gesture_handle, const void* param) {
 (void)gesture_handle;
 (void)param;
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
