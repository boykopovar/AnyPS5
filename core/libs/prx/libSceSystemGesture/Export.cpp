#include <cstdint>
#include <cstddef>
#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"

extern "C" {

int APS5_VABI sceSystemGestureAppendTouchRecognizer(int32_t gesture_handle, SystemGestureTouchRecognizer* recognizer) {
 (void)gesture_handle;
 (void)recognizer;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int APS5_VABI sceSystemGestureClose(int32_t gesture_handle) {
 (void)gesture_handle;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int APS5_VABI sceSystemGestureCreateTouchRecognizer(int32_t gesture_handle, SystemGestureTouchRecognizer* recognizer, int32_t type, const SystemGestureRectangle* rectangle, const void* param) {
 (void)gesture_handle;
 (void)recognizer;
 (void)type;
 (void)rectangle;
 (void)param;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int APS5_VABI sceSystemGestureFinalizePrimitiveTouchRecognizer(void) {
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int APS5_VABI sceSystemGestureGetPrimitiveTouchEventByIndex(int32_t gesture_handle, uint32_t index, SystemGesturePrimitiveTouchEvent* event) {
 (void)gesture_handle;
 (void)index;
 (void)event;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int APS5_VABI sceSystemGestureGetPrimitiveTouchEventByPrimitiveID(int32_t gesture_handle, uint16_t primitiveId, SystemGesturePrimitiveTouchEvent* event) {
 (void)gesture_handle;
 (void)primitiveId;
 (void)event;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int APS5_VABI sceSystemGestureGetPrimitiveTouchEvents(int32_t gesture_handle, SystemGesturePrimitiveTouchEvent* event_buffer, uint32_t capacity_of_buffer, uint32_t* number_of_event) {
 (void)gesture_handle;
 (void)event_buffer;
 (void)capacity_of_buffer;
 (void)number_of_event;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int APS5_VABI sceSystemGestureGetPrimitiveTouchEventsCount(int32_t gesture_handle) {
 (void)gesture_handle;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int APS5_VABI sceSystemGestureGetTouchEventByEventID(int32_t gesture_handle, const SystemGestureTouchRecognizer* recognizer, uint32_t eventId, SystemGestureTouchEvent* event) {
 (void)gesture_handle;
 (void)recognizer;
 (void)eventId;
 (void)event;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int APS5_VABI sceSystemGestureGetTouchEventByIndex(int32_t gesture_handle, const SystemGestureTouchRecognizer* recognizer, uint32_t index, SystemGestureTouchEvent* event) {
 (void)gesture_handle;
 (void)recognizer;
 (void)index;
 (void)event;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int APS5_VABI sceSystemGestureGetTouchEvents(int32_t gesture_handle, const SystemGestureTouchRecognizer* recognizer, SystemGestureTouchEvent* event_buffer, uint32_t capacity_of_buffer, uint32_t* number_of_event) {
 (void)gesture_handle;
 (void)recognizer;
 (void)event_buffer;
 (void)capacity_of_buffer;
 (void)number_of_event;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int APS5_VABI sceSystemGestureGetTouchEventsCount(int32_t gesture_handle, const SystemGestureTouchRecognizer* recognizer) {
 (void)gesture_handle;
 (void)recognizer;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int APS5_VABI sceSystemGestureGetTouchRecognizerInformation(int32_t gesture_handle, const SystemGestureTouchRecognizer* recognizer, SystemGestureTouchRecognizerInformation* information) {
 (void)gesture_handle;
 (void)recognizer;
 (void)information;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int APS5_VABI sceSystemGestureInitializePrimitiveTouchRecognizer(const void* param) {
 (void)param;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int32_t APS5_VABI sceSystemGestureOpen(int32_t input_type, const void* param) {
 (void)input_type;
 (void)param;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int APS5_VABI sceSystemGestureRemoveTouchRecognizer(int32_t gesture_handle, SystemGestureTouchRecognizer* recognizer) {
 (void)gesture_handle;
 (void)recognizer;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int APS5_VABI sceSystemGestureResetPrimitiveTouchRecognizer(int32_t gesture_handle) {
 (void)gesture_handle;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int APS5_VABI sceSystemGestureResetTouchRecognizer(int32_t gesture_handle, SystemGestureTouchRecognizer* recognizer) {
 (void)gesture_handle;
 (void)recognizer;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int APS5_VABI sceSystemGestureUpdateAllTouchRecognizer(int32_t gesture_handle) {
 (void)gesture_handle;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int APS5_VABI sceSystemGestureUpdatePrimitiveTouchRecognizer(int32_t gesture_handle, const void* param) {
 (void)gesture_handle;
 (void)param;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int APS5_VABI sceSystemGestureUpdateTouchRecognizer(int32_t gesture_handle, SystemGestureTouchRecognizer* recognizer) {
 (void)gesture_handle;
 (void)recognizer;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int APS5_VABI sceSystemGestureUpdateTouchRecognizerRectangle(int32_t gesture_handle, SystemGestureTouchRecognizer* recognizer, const SystemGestureRectangle* rectangle) {
 (void)gesture_handle;
 (void)recognizer;
 (void)rectangle;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

}
