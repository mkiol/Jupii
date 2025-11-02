/* Copyright (C) 2023 Michal Kosciesza <michal@mkiol.net>
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#ifndef ORIENTATIONMONITOR_H
#define ORIENTATIONMONITOR_H

#include <wayland-client.h>

#include <cstdint>
#include <thread>
#include <utility>

class OrientationMonitor {
   public:
    enum class Transform { Normal = 0, Rot90 = 1, Rot180 = 2, Rot270 = 3 };
    friend std::ostream &operator<<(std::ostream &os, Transform transform);

    OrientationMonitor();
    ~OrientationMonitor();
    void start();

    inline Transform transform() const { return m_transform; }

   private:
    struct Globals {
        wl_display *display = nullptr;
        wl_registry *registry = nullptr;
        wl_output *output = nullptr;
        OrientationMonitor *source = nullptr;
        inline auto ok() const { return display && registry && output; }
    };

    static const uint32_t framerate = 5;

    Globals m_globals;
    std::thread m_wlThread;
    Transform m_transform = Transform::Normal;
    bool m_terminating = false;

    void clean();

    static void wlRegister(Globals *globals);
    static void wlGlobalCallback(void *data, wl_registry *registry, uint32_t id,
                                 const char *interface, uint32_t version);
    static void wlGlobalRemoveCallback(void *data, wl_registry *registry,
                                       uint32_t id);
    static void wlCallbackCallback(void *data, wl_callback *cb, uint32_t time);
    static void wlOutputGeometryCallback(void *data, wl_output *wl_output,
                                         int32_t x, int32_t y,
                                         int32_t physical_width,
                                         int32_t physical_height,
                                         int32_t subpixel, const char *make,
                                         const char *model, int32_t transform);
    static void wlOutputModeCallback(void *data, wl_output *wl_output,
                                     uint32_t flags, int32_t width,
                                     int32_t height, int32_t refresh);
    static void wlOutputDoneCallback(void *data, wl_output *wl_output);
    static void wlOutputScaleCallback(void *data, wl_output *wl_output,
                                      int32_t factor);
    static void wlOutputNameCallback(void *data, wl_output *wl_output,
                                     const char *name);
    static void wlOutputDescriptionCallback(void *data, wl_output *wl_output,
                                            const char *description);

    inline static const wl_registry_listener wlGlobalListener{
        wlGlobalCallback, wlGlobalRemoveCallback};
    inline static const wl_callback_listener wlCallback{wlCallbackCallback};
    inline static const wl_output_listener wlOutputListener{
        wlOutputGeometryCallback, wlOutputModeCallback,
        wlOutputDoneCallback,     wlOutputScaleCallback,
        wlOutputNameCallback,     wlOutputDescriptionCallback};
};

#endif  // ORIENTATIONMONITOR_H
