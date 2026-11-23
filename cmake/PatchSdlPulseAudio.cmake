set(sdlPulsePatch "${CMAKE_CURRENT_BINARY_DIR}/sdl-pulseaudio-close.patch")
set(sdlPulseSourceDir "${CMAKE_CURRENT_SOURCE_DIR}/3rdparty/SDL2")
set(sdlPulsePatchDir "${CMAKE_CURRENT_BINARY_DIR}/sdl-pulseaudio-patch")
set(sdlPulsePatchedDir "${CMAKE_CURRENT_BINARY_DIR}/sdl-pulseaudio")
set(sdlPulsePatchedSources src/audio/pulseaudio/SDL_pulseaudio.c)
file(CONFIGURE OUTPUT "${sdlPulsePatch}" CONTENT [=[
--- a/src/audio/pulseaudio/SDL_pulseaudio.c
+++ b/src/audio/pulseaudio/SDL_pulseaudio.c
@@ -530,6 +530,7 @@
     PULSEAUDIO_pa_threaded_mainloop_lock(pulseaudio_threaded_mainloop);

     if (this->hidden->stream) {
+        PULSEAUDIO_pa_stream_set_write_callback(this->hidden->stream, NULL, NULL);
         if (this->hidden->capturebuf) {
             PULSEAUDIO_pa_stream_drop(this->hidden->stream);
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
        message(FATAL_ERROR "SDL2-static no longer builds ${source}; review the PulseAudio close patch")
    endif()
    configure_file("${sdlPulsePatchDir}/${source}" "${patched}" COPYONLY)
    get_filename_component(sourceDir "${original}" DIRECTORY)
    set_property(SOURCE "${patched}" TARGET_DIRECTORY SDL2-static APPEND PROPERTY INCLUDE_DIRECTORIES "${sourceDir}")
    list(REMOVE_ITEM sdlPulseSources "${original}")
    list(APPEND sdlPulseSources "${patched}")
endforeach()
set_property(TARGET SDL2-static PROPERTY SOURCES "${sdlPulseSources}")
