set(sdlPulsePatch "${CMAKE_CURRENT_BINARY_DIR}/sdl-pulseaudio.patch")
set(sdlPulseSourceDir "${CMAKE_CURRENT_SOURCE_DIR}/3rdparty/SDL2")
set(sdlPulsePatchDir "${CMAKE_CURRENT_BINARY_DIR}/sdl-pulseaudio-patch")
set(sdlPulsePatchedDir "${CMAKE_CURRENT_BINARY_DIR}/sdl-pulseaudio")
set(sdlPulsePatchedSources src/audio/SDL_audio.c src/audio/pulseaudio/SDL_pulseaudio.c)
file(CONFIGURE OUTPUT "${sdlPulsePatch}" CONTENT [=[
--- a/src/audio/SDL_audio.c
+++ b/src/audio/SDL_audio.c
@@ -1532,9 +1532,9 @@
     }

     if (id == SDL_arraysize(open_devices)) {
+        SDL_UnlockMutex(current_audio.detectionLock);
         close_audio_device(device);
         SDL_SetError("Too many open audio devices");
-        SDL_UnlockMutex(current_audio.detectionLock);
         return 0;
     }

--- a/src/audio/pulseaudio/SDL_pulseaudio.c
+++ b/src/audio/pulseaudio/SDL_pulseaudio.c
@@ -31,6 +31,7 @@
 #endif
 #include <unistd.h>
 #include <sys/types.h>
+#include <sys/time.h>

 #include "SDL_timer.h"
 #include "SDL_audio.h"
@@ -530,6 +531,7 @@
     PULSEAUDIO_pa_threaded_mainloop_lock(pulseaudio_threaded_mainloop);

     if (this->hidden->stream) {
+        PULSEAUDIO_pa_stream_set_write_callback(this->hidden->stream, NULL, NULL);
         if (this->hidden->capturebuf) {
             PULSEAUDIO_pa_stream_drop(this->hidden->stream);
         }
@@ -584,6 +586,12 @@
     PULSEAUDIO_pa_threaded_mainloop_signal(pulseaudio_threaded_mainloop, 0);  /* just signal any waiting code, it can look up the details. */
 }

+static void PulseStreamReadyTimeoutCallback(pa_mainloop_api *api, pa_time_event *event, const struct timeval *tv, void *userdata)
+{
+    *(SDL_bool *)userdata = SDL_TRUE;
+    PULSEAUDIO_pa_threaded_mainloop_signal(pulseaudio_threaded_mainloop, 0);
+}
+
 static int PULSEAUDIO_OpenDevice(_THIS, const char *devname)
 {
     struct SDL_PrivateAudioData *h = NULL;
@@ -709,14 +717,26 @@
             if (rc < 0) {
                 retval = SDL_SetError("Could not connect PulseAudio stream");
             } else {
+                pa_mainloop_api *api = PULSEAUDIO_pa_threaded_mainloop_get_api(pulseaudio_threaded_mainloop);
+                SDL_bool timedOut = SDL_FALSE;
+                struct timeval deadline;
+                pa_time_event *timeout;
                 int state = PULSEAUDIO_pa_stream_get_state(h->stream);
-                while (PA_STREAM_IS_GOOD(state) && (state != PA_STREAM_READY)) {
+                gettimeofday(&deadline, NULL);
+                deadline.tv_sec += 2;
+                timeout = api->time_new(api, &deadline, PulseStreamReadyTimeoutCallback, &timedOut);
+                while (PA_STREAM_IS_GOOD(state) && (state != PA_STREAM_READY) && !timedOut) {
                     PULSEAUDIO_pa_threaded_mainloop_wait(pulseaudio_threaded_mainloop);
                     state = PULSEAUDIO_pa_stream_get_state(h->stream);
                 }
+                if (timeout) {
+                    api->time_free(timeout);
+                }

                 if (!PA_STREAM_IS_GOOD(state)) {
                     retval = SDL_SetError("Could not connect PulseAudio stream");
+                } else if (state != PA_STREAM_READY) {
+                    retval = SDL_SetError("PulseAudio stream was not ready after 2 seconds");
                 }
             }
         }
]=] @ONLY)

foreach(source IN LISTS sdlPulsePatchedSources)
    configure_file("${sdlPulseSourceDir}/${source}" "${sdlPulsePatchDir}/${source}" COPYONLY)
endforeach()
execute_process(
    COMMAND "${CMAKE_COMMAND}" -E env "GIT_CEILING_DIRECTORIES=${CMAKE_CURRENT_BINARY_DIR}"
        git apply --no-index --whitespace=nowarn "${sdlPulsePatch}"
    WORKING_DIRECTORY "${sdlPulsePatchDir}"
    COMMAND_ERROR_IS_FATAL ANY
)

get_target_property(sdlPulseSources SDL2-static SOURCES)
foreach(source IN LISTS sdlPulsePatchedSources)
    set(original "${sdlPulseSourceDir}/${source}")
    set(patched "${sdlPulsePatchedDir}/${source}")
    if(NOT original IN_LIST sdlPulseSources)
        message(FATAL_ERROR "SDL2-static no longer builds ${source}; review the PulseAudio patch")
    endif()
    configure_file("${sdlPulsePatchDir}/${source}" "${patched}" COPYONLY)
    get_filename_component(sourceDir "${original}" DIRECTORY)
    set_property(SOURCE "${patched}" TARGET_DIRECTORY SDL2-static APPEND PROPERTY INCLUDE_DIRECTORIES "${sourceDir}")
    list(REMOVE_ITEM sdlPulseSources "${original}")
    list(APPEND sdlPulseSources "${patched}")
endforeach()
set_property(TARGET SDL2-static PROPERTY SOURCES "${sdlPulseSources}")
