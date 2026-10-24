#pragma once

#include "SceTypes.hpp"

extern "C" {

int APS5_VABI sceSystemGestureAppendTouchRecognizer(int32_t gestureHandle, SystemGestureTouchRecognizer* recognizer);
int APS5_VABI sceSystemGestureClose(int32_t gestureHandle);
int APS5_VABI sceSystemGestureCreateTouchRecognizer(int32_t gestureHandle, SystemGestureTouchRecognizer* recognizer, int32_t type, const SystemGestureRectangle* rectangle, const void* param);
int APS5_VABI sceSystemGestureFinalizePrimitiveTouchRecognizer(void);
int APS5_VABI sceSystemGestureGetPrimitiveTouchEventByIndex(int32_t gestureHandle, uint32_t index, SystemGesturePrimitiveTouchEvent* event);
int APS5_VABI sceSystemGestureGetPrimitiveTouchEventByPrimitiveID(int32_t gestureHandle, uint16_t primitiveId, SystemGesturePrimitiveTouchEvent* event);
int APS5_VABI sceSystemGestureGetPrimitiveTouchEvents(int32_t gestureHandle, SystemGesturePrimitiveTouchEvent* eventBuffer, uint32_t capacityOfBuffer, uint32_t* numberOfEvent);
int APS5_VABI sceSystemGestureGetPrimitiveTouchEventsCount(int32_t gestureHandle);
int APS5_VABI sceSystemGestureGetTouchEventByEventID(int32_t gestureHandle, const SystemGestureTouchRecognizer* recognizer, uint32_t eventId, SystemGestureTouchEvent* event);
int APS5_VABI sceSystemGestureGetTouchEventByIndex(int32_t gestureHandle, const SystemGestureTouchRecognizer* recognizer, uint32_t index, SystemGestureTouchEvent* event);
int APS5_VABI sceSystemGestureGetTouchEvents(int32_t gestureHandle, const SystemGestureTouchRecognizer* recognizer, SystemGestureTouchEvent* eventBuffer, uint32_t capacityOfBuffer, uint32_t* numberOfEvent);
int APS5_VABI sceSystemGestureGetTouchEventsCount(int32_t gestureHandle, const SystemGestureTouchRecognizer* recognizer);
int APS5_VABI sceSystemGestureGetTouchRecognizerInformation(int32_t gestureHandle, const SystemGestureTouchRecognizer* recognizer, SystemGestureTouchRecognizerInformation* information);
int APS5_VABI sceSystemGestureInitializePrimitiveTouchRecognizer();
int32_t APS5_VABI sceSystemGestureOpen(int32_t inputType, const void* param);
int APS5_VABI sceSystemGestureRemoveTouchRecognizer(int32_t gestureHandle, SystemGestureTouchRecognizer* recognizer);
int APS5_VABI sceSystemGestureResetPrimitiveTouchRecognizer(int32_t gestureHandle);
int APS5_VABI sceSystemGestureResetTouchRecognizer(int32_t gestureHandle, SystemGestureTouchRecognizer* recognizer);
int APS5_VABI sceSystemGestureUpdateAllTouchRecognizer(int32_t gestureHandle);
int APS5_VABI sceSystemGestureUpdatePrimitiveTouchRecognizer(int32_t gestureHandle, const SystemGestureTouchPadData* input);
int APS5_VABI sceSystemGestureUpdateTouchRecognizer(int32_t gestureHandle, SystemGestureTouchRecognizer* recognizer);
int APS5_VABI sceSystemGestureUpdateTouchRecognizerRectangle(int32_t gestureHandle, SystemGestureTouchRecognizer* recognizer, const SystemGestureRectangle* rectangle);
int APS5_VABI sceSystemGestureDebugGetVersion();

}
