set(sdlPatch "${CMAKE_CURRENT_BINARY_DIR}/sdl-wayland-window.patch")
set(sdlSourceDir "${CMAKE_CURRENT_SOURCE_DIR}/3rdparty/SDL2")
set(sdlPatchDir "${CMAKE_CURRENT_BINARY_DIR}/sdl-wayland-patch")
set(sdlPatchedDir "${CMAKE_CURRENT_BINARY_DIR}/sdl-wayland")
set(sdlPatchedSources src/video/SDL_video.c src/video/wayland/SDL_waylandwindow.c)
file(CONFIGURE OUTPUT "${sdlPatch}" CONTENT [=[
--- a/src/video/SDL_video.c
+++ b/src/video/SDL_video.c
@@ -1619,9 +1619,16 @@
         }
     }
 }

-static void SDL_FinishWindowCreation(SDL_Window *window, Uint32 flags)
+#if SDL_VIDEO_DRIVER_WAYLAND
+extern void Wayland_ShowWindow(_THIS, SDL_Window *window);
+extern int Wayland_ShowWindowChecked(_THIS, SDL_Window *window);
+#endif
+
+static int SDL_ShowWindowInternal(SDL_Window *window);
+
+static int SDL_FinishWindowCreation(SDL_Window *window, Uint32 flags)
 {
     PrepareDragAndDropSupport(window);

     if (flags & SDL_WINDOW_MAXIMIZED) {
@@ -1644,10 +1651,11 @@
     if (flags & SDL_WINDOW_KEYBOARD_GRABBED) {
         SDL_SetWindowKeyboardGrab(window, SDL_TRUE);
     }
     if (!(flags & SDL_WINDOW_HIDDEN)) {
-        SDL_ShowWindow(window);
-    }
+        return SDL_ShowWindowInternal(window);
+    }
+    return 0;
 }

 static int SDL_ContextNotSupported(const char *name)
 {
@@ -1858,9 +1866,15 @@

     if (title) {
         SDL_SetWindowTitle(window, title);
     }
-    SDL_FinishWindowCreation(window, flags);
+    if (SDL_FinishWindowCreation(window, flags) < 0) {
+        char error[1024];
+        SDL_strlcpy(error, SDL_GetError(), sizeof(error));
+        SDL_DestroyWindow(window);
+        SDL_SetError("%s", error);
+        return NULL;
+    }

     /* If the window was created fullscreen, make sure the mode code matches */
     SDL_UpdateFullscreenMode(window, FULLSCREEN_VISIBLE(window));

@@ -2066,11 +2080,9 @@
     if (window->hit_test) {
         _this->SetWindowHitTest(window, SDL_TRUE);
     }

-    SDL_FinishWindowCreation(window, flags);
-
-    return 0;
+    return SDL_FinishWindowCreation(window, flags);
 }

 SDL_bool SDL_HasWindows(void)
 {
@@ -2542,23 +2554,36 @@
         *max_h = window->max_h;
     }
 }

-void SDL_ShowWindow(SDL_Window *window)
-{
-    CHECK_WINDOW_MAGIC(window, );
+static int SDL_ShowWindowInternal(SDL_Window *window)
+{
+    CHECK_WINDOW_MAGIC(window, -1);

     if (window->flags & SDL_WINDOW_SHOWN) {
-        return;
-    }
-
+        return 0;
+    }
+
+#if SDL_VIDEO_DRIVER_WAYLAND
+    if (_this->ShowWindow == Wayland_ShowWindow) {
+        if (Wayland_ShowWindowChecked(_this, window) < 0) {
+            return -1;
+        }
+    } else
+#endif
     if (_this->ShowWindow) {
         _this->ShowWindow(_this, window);
     } else {
         SDL_SetMouseFocus(window);
         SDL_SetKeyboardFocus(window);
     }
     SDL_SendWindowEvent(window, SDL_WINDOWEVENT_SHOWN, 0, 0);
+    return 0;
+}
+
+void SDL_ShowWindow(SDL_Window *window)
+{
+    SDL_ShowWindowInternal(window);
 }

 void SDL_HideWindow(SDL_Window *window)
 {
--- a/src/video/wayland/SDL_waylandwindow.c
+++ b/src/video/wayland/SDL_waylandwindow.c
@@ -34,8 +34,9 @@
 #include "SDL_waylandshmbuffer.h"
 #include "SDL_hints.h"
 #include "../../SDL_hints_c.h"
 #include "SDL_events.h"
+#include "SDL_timer.h"

 #include "xdg-shell-client-protocol.h"
 #include "xdg-decoration-unstable-v1-client-protocol.h"
 #include "idle-inhibit-unstable-v1-client-protocol.h"
@@ -698,34 +699,11 @@
 static void handle_configure_zxdg_decoration(void *data,
                                              struct zxdg_toplevel_decoration_v1 *zxdg_toplevel_decoration_v1,
                                              uint32_t mode)
 {
-    SDL_Window *window = (SDL_Window *)data;
-    SDL_WindowData *driverdata = (SDL_WindowData *)window->driverdata;
-    SDL_VideoDevice *device = SDL_GetVideoDevice();
-
-    /* If the compositor tries to force CSD anyway, bail on direct XDG support
-     * and fall back to libdecor, it will handle these events from then on.
-     *
-     * To do this we have to fully unmap, then map with libdecor loaded.
-     */
-    if (mode == ZXDG_TOPLEVEL_DECORATION_V1_MODE_CLIENT_SIDE) {
-        if (window->flags & SDL_WINDOW_BORDERLESS) {
-            /* borderless windows do request CSD, so we got what we wanted */
-            return;
-        }
-        if (!Wayland_LoadLibdecor(driverdata->waylandData, SDL_TRUE)) {
-            /* libdecor isn't available, so no borders for you... oh well */
-            return;
-        }
-        WAYLAND_wl_display_roundtrip(driverdata->waylandData->display);
-
-        Wayland_HideWindow(device, window);
-        SDL_zero(driverdata->shell_surface);
-        driverdata->shell_surface_type = WAYLAND_SURFACE_LIBDECOR;
-
-        Wayland_ShowWindow(device, window);
-    }
+    (void)data;
+    (void)zxdg_toplevel_decoration_v1;
+    (void)mode;
 }

 static const struct zxdg_toplevel_decoration_v1_listener decoration_listener = {
     handle_configure_zxdg_decoration
@@ -1266,9 +1244,50 @@
     WAYLAND_wl_display_flush(viddata->display);
     return 0;
 }

-void Wayland_ShowWindow(_THIS, SDL_Window *window)
+static void Wayland_ReleaseShellSurface(_THIS, SDL_Window *window);
+
+static int Wayland_WaitForWindowFlag(_THIS, const SDL_bool *ready, const char *operation)
+{
+    const Uint64 deadline = SDL_GetTicks64() + 3000;
+    while (!*ready) {
+        const Uint64 now = SDL_GetTicks64();
+        if (now >= deadline) {
+            return SDL_SetError("Timed out waiting for Wayland %s", operation);
+        }
+        if (Wayland_WaitEventTimeout(_this, (int)(deadline - now)) < 0) {
+            return SDL_SetError("Wayland connection failed while waiting for %s", operation);
+        }
+    }
+    return 0;
+}
+
+static void handle_window_sync_done(void *data, struct wl_callback *callback, uint32_t serial)
+{
+    (void)callback;
+    (void)serial;
+    *(SDL_bool *)data = SDL_TRUE;
+}
+
+static int Wayland_WindowRoundtrip(_THIS)
+{
+    SDL_VideoData *data = _this->driverdata;
+    static const struct wl_callback_listener listener = { handle_window_sync_done };
+    SDL_bool ready = SDL_FALSE;
+    struct wl_callback *callback = wl_display_sync(data->display);
+    int result;
+    if (!callback) {
+        return SDL_SetError("Failed to request Wayland window synchronization");
+    }
+    wl_callback_add_listener(callback, &listener, &ready);
+    result = Wayland_WaitForWindowFlag(_this, &ready, "window synchronization");
+    wl_callback_destroy(callback);
+    return result;
+}
+
+int Wayland_ShowWindowChecked(_THIS, SDL_Window *window);
+int Wayland_ShowWindowChecked(_THIS, SDL_Window *window)
 {
     SDL_VideoData *c = _this->driverdata;
     SDL_WindowData *data = window->driverdata;

@@ -1285,8 +1304,12 @@
      * https://bugs.kde.org/show_bug.cgi?id=448856
      *
      * -flibit
      */
+#ifdef HAVE_LIBDECOR_H
+retry:
+#endif
+    SDL_zero(data->shell_surface);
     wl_surface_attach(data->surface, NULL, 0, 0);
     wl_surface_commit(data->surface);

     /* Create the shell surface and map the toplevel/popup */
@@ -1296,19 +1319,13 @@
                                                                data->surface,
                                                                &libdecor_frame_interface,
                                                                data);
         if (!data->shell_surface.libdecor.frame) {
-            SDL_LogError(SDL_LOG_CATEGORY_VIDEO, "Failed to create libdecor frame!");
-        } else {
-            libdecor_frame_set_app_id(data->shell_surface.libdecor.frame, c->classname);
-            libdecor_frame_map(data->shell_surface.libdecor.frame);
-
-            if (c->xdg_toplevel_icon_manager_v1 && data->xdg_toplevel_icon_v1) {
-                xdg_toplevel_icon_manager_v1_set_icon(c->xdg_toplevel_icon_manager_v1,
-                                                      libdecor_frame_get_xdg_toplevel(data->shell_surface.libdecor.frame),
-                                                      data->xdg_toplevel_icon_v1);
-            }
-        }
+            SDL_SetError("Failed to create libdecor frame");
+            goto libdecor_failed;
+        }
+        libdecor_frame_set_app_id(data->shell_surface.libdecor.frame, c->classname);
+        libdecor_frame_map(data->shell_surface.libdecor.frame);
     } else
 #endif
         if (c->shell.xdg) {
         data->shell_surface.xdg.surface = xdg_wm_base_get_xdg_surface(c->shell.xdg, data->surface);
@@ -1374,33 +1391,35 @@
      * this surface will fail. This is a new rule for xdg_shell.
      */
 #ifdef HAVE_LIBDECOR_H
     if (data->shell_surface_type == WAYLAND_SURFACE_LIBDECOR) {
-        if (data->shell_surface.libdecor.frame) {
-            while (!data->shell_surface.libdecor.initial_configure_seen) {
-                WAYLAND_wl_display_flush(c->display);
-                WAYLAND_wl_display_dispatch(c->display);
-            }
+        libdecor_dispatch(c->shell.libdecor, 0);
+        if (Wayland_WaitForWindowFlag(_this, &data->shell_surface.libdecor.initial_configure_seen, "libdecor initial configure") < 0) {
+            goto libdecor_failed;
+        }
+
+        if (c->xdg_toplevel_icon_manager_v1 && data->xdg_toplevel_icon_v1) {
+            xdg_toplevel_icon_manager_v1_set_icon(c->xdg_toplevel_icon_manager_v1,
+                                                  libdecor_frame_get_xdg_toplevel(data->shell_surface.libdecor.frame),
+                                                  data->xdg_toplevel_icon_v1);
         }
     } else
 #endif
         if (c->shell.xdg) {
         /* Create the window decorations */
         if (data->shell_surface_type != WAYLAND_SURFACE_XDG_POPUP && data->shell_surface.xdg.roleobj.toplevel && c->decoration_manager) {
             data->server_decoration = zxdg_decoration_manager_v1_get_toplevel_decoration(c->decoration_manager, data->shell_surface.xdg.roleobj.toplevel);
-            zxdg_toplevel_decoration_v1_add_listener(data->server_decoration, &decoration_listener, window);
+            zxdg_toplevel_decoration_v1_add_listener(data->server_decoration, &decoration_listener, NULL);
+            Wayland_SetWindowBordered(_this, window, !(window->flags & SDL_WINDOW_BORDERLESS));
         }

         /* Unlike libdecor we need to call this explicitly to prevent a deadlock.
          * libdecor will call this as part of their configure event!
          * -flibit
          */
         wl_surface_commit(data->surface);
-        if (data->shell_surface.xdg.surface) {
-            while (!data->shell_surface.xdg.initial_configure_seen) {
-                WAYLAND_wl_display_flush(c->display);
-                WAYLAND_wl_display_dispatch(c->display);
-            }
+        if (Wayland_WaitForWindowFlag(_this, &data->shell_surface.xdg.initial_configure_seen, "xdg-shell initial configure") < 0) {
+            goto failed;
         }
     } else {
         /* Nothing to see here, just commit. */
         wl_surface_commit(data->surface);
@@ -1431,13 +1450,10 @@
             data->window_width = SDL_max(window->windowed.w, data->system_min_required_width);
             data->window_height = SDL_max(window->windowed.h, data->system_min_required_height);
             CommitLibdecorFrame(window);
         }
-    } else
-#endif
-    {
-        Wayland_SetWindowBordered(_this, window, !(window->flags & SDL_WINDOW_BORDERLESS));
-    }
+    }
+#endif

     /* We're finally done putting the window together, raise if possible */
     if (c->activation_manager) {
         /* Note that we don't check for empty strings, as that is still
@@ -1457,12 +1473,37 @@
     /*
      * Roundtrip required to avoid a possible protocol violation when
      * HideWindow was called immediately before ShowWindow.
      */
-    WAYLAND_wl_display_roundtrip(c->display);
+    if (Wayland_WindowRoundtrip(_this) < 0) {
+        goto failed;
+    }

     /* Send an exposure event to signal that the client should draw. */
     SDL_SendWindowEvent(window, SDL_WINDOWEVENT_EXPOSED, 0, 0);
+    return 0;
+
+#ifdef HAVE_LIBDECOR_H
+libdecor_failed:
+    if (c->shell.xdg && !WAYLAND_wl_display_get_error(c->display)) {
+        SDL_LogWarn(SDL_LOG_CATEGORY_VIDEO, "%s; retrying with xdg-shell", SDL_GetError());
+        Wayland_ReleaseShellSurface(_this, window);
+        data->shell_surface_type = WAYLAND_SURFACE_XDG_TOPLEVEL;
+        if (Wayland_WindowRoundtrip(_this) < 0) {
+            return -1;
+        }
+        SDL_ClearError();
+        goto retry;
+    }
+#endif
+failed:
+    Wayland_ReleaseShellSurface(_this, window);
+    return -1;
+}
+
+void Wayland_ShowWindow(_THIS, SDL_Window *window)
+{
+    Wayland_ShowWindowChecked(_this, window);
 }

 static void Wayland_ReleasePopup(_THIS, SDL_Window *popup)
 {
@@ -1498,9 +1539,9 @@
     popupdata->shell_surface.xdg.roleobj.popup.popup = NULL;
     popupdata->shell_surface.xdg.roleobj.popup.positioner = NULL;
 }

-void Wayland_HideWindow(_THIS, SDL_Window *window)
+static void Wayland_ReleaseShellSurface(_THIS, SDL_Window *window)
 {
     SDL_VideoData *data = _this->driverdata;
     SDL_WindowData *wind = window->driverdata;

@@ -1532,14 +1573,14 @@
             xdg_surface_destroy(wind->shell_surface.xdg.surface);
             wind->shell_surface.xdg.surface = NULL;
         }
     }
-
-    /*
-     * Roundtrip required to avoid a possible protocol violation when
-     * ShowWindow is called immediately after HideWindow.
-     */
-    WAYLAND_wl_display_roundtrip(data->display);
+}
+
+void Wayland_HideWindow(_THIS, SDL_Window *window)
+{
+    Wayland_ReleaseShellSurface(_this, window);
+    Wayland_WindowRoundtrip(_this);
 }

 static void handle_xdg_activation_done(void *data,
                                        struct xdg_activation_token_v1 *xdg_activation_token_v1,
@@ -1750,9 +1791,8 @@
                                  SDL_VideoDisplay *_display, SDL_bool fullscreen)
 {
     SDL_WindowData *wind = (SDL_WindowData *)window->driverdata;
     struct wl_output *output = ((SDL_WaylandOutputData *)_display->driverdata)->output;
-    SDL_VideoData *viddata = (SDL_VideoData *)_this->driverdata;

     /* Called from within a configure event or the window is a popup, drop it. */
     if (wind->in_fullscreen_transition || wind->shell_surface_type == WAYLAND_SURFACE_XDG_POPUP) {
         return;
@@ -1768,9 +1808,9 @@
         wind->is_fullscreen = fullscreen;
         SetFullscreen(window, fullscreen ? output : NULL);

         /* Roundtrip required to receive the updated window dimensions */
-        WAYLAND_wl_display_roundtrip(viddata->display);
+        Wayland_WindowRoundtrip(_this);
     } else if (wind->is_fullscreen) {
         /*
          * If the window is already fullscreen, this is likely a request to switch between
          * fullscreen and fullscreen desktop, or to change the video mode. Update the
@@ -1779,9 +1819,9 @@
         ConfigureWindowGeometry(window);
         CommitLibdecorFrame(window);

         /* Roundtrip required to receive the updated window dimensions */
-        WAYLAND_wl_display_roundtrip(viddata->display);
+        Wayland_WindowRoundtrip(_this);
     }
 }

 void Wayland_RestoreWindow(_THIS, SDL_Window *window)
]=] @ONLY)

foreach(source IN LISTS sdlPatchedSources)
    configure_file("${sdlSourceDir}/${source}" "${sdlPatchDir}/${source}" COPYONLY)
endforeach()
execute_process(
    COMMAND "${CMAKE_COMMAND}" -E env "GIT_CEILING_DIRECTORIES=${CMAKE_CURRENT_BINARY_DIR}"
        git apply --no-index --whitespace=nowarn "${sdlPatch}"
    WORKING_DIRECTORY "${sdlPatchDir}"
    COMMAND_ERROR_IS_FATAL ANY
)

get_target_property(sdlSources SDL2-static SOURCES)
foreach(source IN LISTS sdlPatchedSources)
    set(original "${sdlSourceDir}/${source}")
    set(patched "${sdlPatchedDir}/${source}")
    if(NOT original IN_LIST sdlSources)
        message(FATAL_ERROR "SDL2-static no longer builds ${source}; review the Wayland window patch")
    endif()
    configure_file("${sdlPatchDir}/${source}" "${patched}" COPYONLY)
    get_filename_component(sourceDir "${original}" DIRECTORY)
    set_property(SOURCE "${patched}" TARGET_DIRECTORY SDL2-static APPEND PROPERTY INCLUDE_DIRECTORIES "${sourceDir}")
    list(REMOVE_ITEM sdlSources "${original}")
    list(APPEND sdlSources "${patched}")
endforeach()
set_property(TARGET SDL2-static PROPERTY SOURCES "${sdlSources}")
