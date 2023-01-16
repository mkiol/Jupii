/* Copyright (C) 2023 Michal Kosciesza <michal@mkiol.net>
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#ifndef DROIDCAMSOURCE_HPP
#define DROIDCAMSOURCE_HPP

#include <gst/gst.h>

#include <cstdint>
#include <functional>
#include <thread>
#include <utility>

extern "C" {
#include <libavcodec/codec_id.h>
#include <libavutil/pixfmt.h>
}

#include "databuffer.hpp"

class DroidCamSource {
   public:
    using DataReadyHandler = std::function<void(const uint8_t *, size_t)>;
    using ErrorHandler = std::function<void(void)>;

    struct Props {
        uint32_t width = 0;
        uint32_t height = 0;
        uint32_t framerate = 0;
        AVCodecID codecId = AV_CODEC_ID_NONE;
        AVPixelFormat pixfmt = AV_PIX_FMT_NONE;
    };

    explicit DroidCamSource(bool compressed, int cam,
                            DataReadyHandler dataReadyHandler,
                            ErrorHandler errorHandler);
    ~DroidCamSource();
    void start();
    static bool supported() noexcept;
    static std::pair<Props, Props> properties();

   private:
    struct GstPipeline {
        GstElement *pipeline = nullptr;
        GstElement *source = nullptr;
        GstElement *sink = nullptr;
    };

    static constexpr const unsigned int m_videoBufSize = 0x100000;
    static constexpr const GstClockTime m_gsPipelineTickTime =
        400000000;  // nano s

    int m_dev = 0;
    DataReadyHandler m_dataReadyHandler;
    ErrorHandler m_errorHandler;
    Props m_props;
    std::thread m_gstThread;
    GstPipeline m_gstPipe;
    bool m_terminating = false;

    static GstFlowReturn gstNewSampleCallbackStatic(GstElement *element,
                                                    gpointer udata);
    GstFlowReturn gstNewSampleCallback(GstElement *element);
    GHashTable *getDroidCamDevTable() const;
    void doGstIteration();
    void init();
    void cleanGst();
    void startGstThread();
    void startDroidCamCapture() const;
    void stopDroidCamCapture() const;
    static int droidCamProp(GstElement *source, const char *name);
    static bool setDroidCamProp(GstElement *source, const char *name,
                                int value);
    static void initGstLib();
    std::optional<std::string> readDroidCamDevParam(
        const std::string &key) const;
    static std::pair<bool, bool> camsAvailable(GstElement *source);
};

#endif  // DROIDCAMSOURCE_HPP
