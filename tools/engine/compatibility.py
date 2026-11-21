PATCHES = ('''--- a/3rdparty/libusb-cmake/CMakeLists.txt
+++ b/3rdparty/libusb-cmake/CMakeLists.txt
@@ -230,9 +230,10 @@
     COMMAND "${CMAKE_COMMAND}" -E copy_if_different
         "${LIBUSB_VERSION_DESCRIBE_LOCK}"
         "${LIBUSB_VERSION_DESCRIBE_ROOT}/.tarball-version"
-    COMMAND ${LIBUSB_VERSION_DESCRIBE_COMMAND}
-        "${LIBUSB_VERSION_DESCRIBE_ROOT}"
-        "${LIBUSB_VERSION_DESCRIBE_HEADER}"
+    COMMAND "${CMAKE_COMMAND}"
+        "-DVERSION_FILE=${LIBUSB_VERSION_DESCRIBE_LOCK}"
+        "-DOUTPUT=${LIBUSB_VERSION_DESCRIBE_HEADER}"
+        -P "${CMAKE_SOURCE_DIR}/cmake/LibUsbVersion.cmake"
     DEPENDS
         "${LIBUSB_VERSION_DESCRIBE_LOCK}"
         "${LIBUSB_VERSION_DESCRIBE_SCRIPT}"
''',)
