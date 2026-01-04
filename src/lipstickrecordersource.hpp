/* Copyright (C) 2022 Michal Kosciesza <michal@mkiol.net>
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#ifndef LIPSTICKRECORDERSOURCE_H
#define LIPSTICKRECORDERSOURCE_H

#include <wayland-client.h>

#include <cstdint>
#include <functional>
#include <optional>
#include <sstream>
#include <thread>
#include <utility>

extern "C" {
#include <libavutil/pixfmt.h>
}

#include "wayland-lipstick-recorder-client-protocol.h"

class LipstickRecorderSource {
   public:
    using DataReadyHandler = std::function<void(const uint8_t *, size_t)>;
    using ErrorHandler = std::function<void(void)>;

    enum class Transform { Normal = 0, Rot90 = 1, Rot180 = 2, Rot270 = 3 };
    friend std::ostream &operator<<(std::ostream &os, Transform transform);

    struct Props {
        uint32_t width = 0;
        uint32_t height = 0;
        uint32_t stride = 0;
        uint32_t framerate = 20;
        AVPixelFormat pixfmt = AV_PIX_FMT_NONE;
    };

    explicit LipstickRecorderSource(DataReadyHandler dataReadyHandler,
                                    ErrorHandler errorHandler,
                                    uint32_t framerate = 30);
    ~LipstickRecorderSource();
    void start();
    static bool supported() noexcept;
    static Props properties();
    inline Transform transform() const { return m_transform; }
    inline bool yinverted() const { return m_buf ? m_buf->yinverted : false; }

   private:
    struct Globals {
        wl_display *display = nullptr;
        wl_registry *registry = nullptr;
        wl_shm *shm = nullptr;
        wl_output *output = nullptr;
        lipstick_recorder_manager *lrManager = nullptr;
        lipstick_recorder *lrRec = nullptr;
        LipstickRecorderSource *source = nullptr;
        Props props;
        inline auto ok() const {
            return display && registry && shm && output && lrManager && lrRec &&
                   props.stride > 0 && props.width > 0 && props.height > 0;
        }
    };

    struct WlBufferWrapper {
        wl_buffer *buffer = nullptr;
        uint8_t *data = nullptr;
        size_t size = 0;
        bool yinverted = false;
        std::chrono::steady_clock::time_point tp =
            std::chrono::steady_clock::now();
        explicit WlBufferWrapper(wl_shm *shm, uint32_t width, uint32_t height,
                                 uint32_t stride);
    };

    DataReadyHandler m_dataReadyHandler;
    ErrorHandler m_errorHandler;
    Globals m_globals;
    std::optional<WlBufferWrapper> m_buf;
    std::thread m_wlThread;
    Transform m_transform = Transform::Normal;
    bool m_terminating = false;

    void clean();
    void recordFrame();
    static Globals makeGlobals(LipstickRecorderSource *wrapper);
    static bool checkCredentials();
    void flipBufVertically();
    void repaintIfNeeded(std::chrono::microseconds maxDur);
    static void wlRegister(Globals *globals);
    static void wlGlobalCallback(void *data, wl_registry *registry, uint32_t id,
                                 const char *interface, uint32_t version);
    static void wlGlobalRemoveCallback(void *data, wl_registry *registry,
                                       uint32_t id);
    static void wlCallbackCallback(void *data, wl_callback *cb, uint32_t time);
    static void wlLrFrameCallback(void *data, lipstick_recorder *recorder,
                                  wl_buffer *buffer, uint32_t timestamp,
                                  int transform);
    static void wlLrFailedCallback(void *data, lipstick_recorder *recorder,
                                   int result, wl_buffer *buffer);
    static void wlLrCancelCallback(void *data, lipstick_recorder *recorder,
                                   wl_buffer *buffer);
    static void wlLrSetupCallback(void *data, lipstick_recorder *recorder,
                                  int width, int height, int stride,
                                  int format);
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

    static AVPixelFormat wl2FfPixfmt(wl_shm_format wlFormat);
    static wl_shm_format ff2WlPixfmt(AVPixelFormat ffFormat);

    inline static const wl_registry_listener wlGlobalListener{
        wlGlobalCallback, wlGlobalRemoveCallback};
    inline static const wl_callback_listener wlCallback{wlCallbackCallback};
    inline static const wl_output_listener wlOutputListener{
        wlOutputGeometryCallback, wlOutputModeCallback,
        wlOutputDoneCallback,     wlOutputScaleCallback,
        wlOutputNameCallback,     wlOutputDescriptionCallback};
    inline static const lipstick_recorder_listener wlLrListener{
        wlLrSetupCallback, wlLrFrameCallback, wlLrFailedCallback,
        wlLrCancelCallback};
};

#endif  // LIPSTICKRECORDERSOURCE_H
