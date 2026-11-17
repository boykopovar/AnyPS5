# libSceNKWebKit

## remove `WKContextGetNotificationManager`

## remove `WKNotificationManagerProviderDidClickNotification`

## remove `WKNotificationManagerProviderDidCloseNotifications`

## remove `WKNotificationManagerProviderDidRemoveNotificationPolicies`

## remove `WKNotificationManagerProviderDidShowNotification`

## remove `WKNotificationManagerSetProvider`

## add Functional

- [WKNotificationManager provider registration and notification event dispatch](../../../core/libs/prx/libSceNKWebKit/src/NotificationManager.cpp) (libSceNKWebKit) - `WKContextGetNotificationManager`, `WKNotificationManagerSetProvider`, `WKNotificationManagerProviderDidShowNotification`, `WKNotificationManagerProviderDidCloseNotifications`, `WKNotificationManagerProviderDidClickNotification`, `WKNotificationManagerProviderDidUpdateNotificationPolicy` and `WKNotificationManagerProviderDidRemoveNotificationPolicies` manage notification provider registration and record notification and origin policy events in host memory. Upstream WebProcess IPC notifications and client policy updates are unmodeled.
