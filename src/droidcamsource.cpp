/* Copyright (C) 2023 Michal Kosciesza <michal@mkiol.net>
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#include "droidcamsource.hpp"

#include <glib/gtestutils.h>
#include <gst/app/gstappsink.h>
#include <gst/app/gstappsrc.h>
#include <gst/gstinfo.h>
#include <gst/gstsample.h>

#include "logger.hpp"

DroidCamSource::DroidCamSource(bool compressed, int dev,
                               DataReadyHandler dataReadyHandler,
                               ErrorHandler errorHandler)
    : m_dev{dev},
      m_dataReadyHandler{std::move(dataReadyHandler)},
      m_errorHandler{std::move(errorHandler)},
      m_props(compressed ? properties().second : properties().first) {
    LOGD("creating droidcam source");
    try {
        initGstLib();
        init();
    } catch (...) {
        cleanGst();
        throw;
    }
}

std::pair<DroidCamSource::Props, DroidCamSource::Props>
DroidCamSource::properties() {
    return {{1280, 720, 15, AV_CODEC_ID_RAWVIDEO, AV_PIX_FMT_NV21},
            {1280, 720, 30, AV_CODEC_ID_H264, AV_PIX_FMT_YUVJ420P}};
}

void DroidCamSource::start() {
    if (m_terminating) return;

    LOGD("starting droidcam source");

    if (auto ret = gst_element_set_state(m_gstPipe.pipeline, GST_STATE_PLAYING);
        ret == GST_STATE_CHANGE_FAILURE) {
        LOGE("unable to set the pipeline to the playing state");
        m_terminating = true;
        if (m_errorHandler) m_errorHandler();
        return;
    }

    startGstThread();

    LOGD("droidcam source started");
}

std::pair<bool, bool> DroidCamSource::camsAvailable(GstElement *source) {
    std::pair<bool, bool> cams{true, true};

    if (!setDroidCamProp(source, "camera-device", 0)) {
        LOGW("no droid camera-device 0");
        cams.first = false;
    }
    if (!setDroidCamProp(source, "camera-device", 1)) {
        LOGW("no droid camera-device 1");
        cams.second = false;
    }

    return cams;
}

bool DroidCamSource::supported() noexcept {
    try {
        initGstLib();
    } catch (const std::runtime_error &e) {
        LOGE(e.what());
        return false;
    }

    auto *source_factory = gst_element_factory_find("droidcamsrc");
    if (source_factory == nullptr) {
        LOGE("no droidcamsrc");
        return false;
    }

    auto *source =
        gst_element_factory_create(source_factory, "app_camera_source");
    if (source == nullptr) {
        g_object_unref(source_factory);
        LOGE("failed to create droidcamsrc");
        return false;
    }

    auto cams = camsAvailable(source);

    g_object_unref(source);
    g_object_unref(source_factory);

    return cams.first || cams.second;
}

DroidCamSource::~DroidCamSource() {
    LOGD("droidcam source termination started");
    m_terminating = true;
    if (m_gstThread.joinable()) m_gstThread.join();
    LOGD("gst thread joined");
    cleanGst();
    LOGD("droidcam source termination completed");
}

int DroidCamSource::droidCamProp(GstElement *source, const char *name) {
    int val = -1;
    g_object_get(source, name, &val, NULL);
    return val;
}

bool DroidCamSource::setDroidCamProp(GstElement *source, const char *name,
                                     int value) {
    g_object_set(source, name, value, NULL);
    return droidCamProp(source, name) == value;
}

void DroidCamSource::initGstLib() {
    if (GError *err = nullptr;
        gst_init_check(nullptr, nullptr, &err) == FALSE) {
        throw std::runtime_error("gst_init error: " +
                                 std::to_string(err->code));
    }

    gst_debug_set_active(Logger::match(Logger::LogType::Debug) ? TRUE : FALSE);
}

void DroidCamSource::init() {
    LOGD("droidcam source init started");

    bool raw = m_props.codecId == AV_CODEC_ID_RAWVIDEO;

    auto *source_factory = gst_element_factory_find("droidcamsrc");
    if (source_factory == nullptr) {
        throw std::runtime_error("no droidcamsrc");
    }
    m_gstPipe.source =
        gst_element_factory_create(source_factory, "app_camera_source");
    if (m_gstPipe.source == nullptr) {
        g_object_unref(source_factory);
        throw std::runtime_error("failed to create droidcamsrc");
    }
    g_object_unref(source_factory);

    auto *sink_factory = gst_element_factory_find("appsink");
    if (sink_factory == nullptr) {
        g_object_unref(source_factory);
        throw std::runtime_error("no appsink");
    }
    m_gstPipe.sink = gst_element_factory_create(sink_factory, "app_sink");
    if (m_gstPipe.sink == nullptr) {
        g_object_unref(sink_factory);
        g_object_unref(source_factory);
        throw std::runtime_error("failed to create droidcamsrc");
    }
    gst_base_sink_set_async_enabled(GST_BASE_SINK(m_gstPipe.sink), FALSE);
    g_object_set(m_gstPipe.sink, "sync", TRUE, NULL);
    g_object_set(m_gstPipe.sink, "emit-signals", TRUE, NULL);
    g_signal_connect(m_gstPipe.sink, "new-sample",
                     G_CALLBACK(gstNewSampleCallbackStatic), this);
    g_object_unref(sink_factory);

    m_gstPipe.pipeline = gst_pipeline_new("app_bin");
    if (m_gstPipe.pipeline == nullptr) {
        g_object_unref(sink_factory);
        g_object_unref(source_factory);
        throw std::runtime_error("failed to create pipeline");
    }

    setDroidCamProp(m_gstPipe.source, "mode", /*video recording*/ 2);

    setDroidCamProp(m_gstPipe.source, "camera-device", m_dev);

    auto *fake_vid_sink =
        gst_element_factory_make("fakesink", "app_fake_vid_sink");
    g_object_set(fake_vid_sink, "sync", FALSE, NULL);
    gst_base_sink_set_async_enabled(GST_BASE_SINK(fake_vid_sink), FALSE);

    auto *queue = gst_element_factory_make("queue", "app_queue");

    auto *h264parse = gst_element_factory_make("h264parse", "app_h264parse");
    g_object_set(h264parse, "disable-passthrough", TRUE, NULL);

    auto *capsfilter = gst_element_factory_make("capsfilter", "app_capsfilter");

    auto *mpeg_caps =
        raw ? gst_caps_new_simple("video/x-raw", "format", G_TYPE_STRING,
                                  "NV21", "width", G_TYPE_INT, m_props.width,
                                  "height", G_TYPE_INT, m_props.height, NULL)
            : gst_caps_new_simple(
                  "video/x-h264", "stream-format", G_TYPE_STRING, "byte-stream",
                  "alignment", G_TYPE_STRING, "au", "width", G_TYPE_INT,
                  m_props.width, "height", G_TYPE_INT, m_props.height, NULL);

    g_object_set(GST_OBJECT(capsfilter), "caps", mpeg_caps, NULL);
    gst_caps_unref(mpeg_caps);

    try {
        if (raw) {
            gst_bin_add_many(GST_BIN(m_gstPipe.pipeline), m_gstPipe.source,
                             capsfilter, queue, m_gstPipe.sink, fake_vid_sink,
                             NULL);
            if (!gst_element_link_pads(m_gstPipe.source, "vidsrc",
                                       fake_vid_sink, "sink")) {
                throw std::runtime_error("unable to link vidsrc pad");
            }

            if (!gst_element_link_pads(m_gstPipe.source, "vfsrc", capsfilter,
                                       "sink")) {
                throw std::runtime_error("unable to link vfsrc pad");
            }

            if (!gst_element_link_many(capsfilter, queue, m_gstPipe.sink,
                                       NULL)) {
                throw std::runtime_error("unable to link many");
            }
        } else {
            gst_bin_add_many(GST_BIN(m_gstPipe.pipeline), m_gstPipe.source,
                             capsfilter, h264parse, queue, m_gstPipe.sink,
                             fake_vid_sink, NULL);

            if (!gst_element_link_pads(m_gstPipe.source, "vidsrc", h264parse,
                                       "sink")) {
                throw std::runtime_error("unable to link vidsrc pad");
            }

            if (!gst_element_link_pads(m_gstPipe.source, "vfsrc", fake_vid_sink,
                                       "sink")) {
                throw std::runtime_error("unable to link vfsrc pad");
            }

            if (!gst_element_link_many(h264parse, capsfilter, queue,
                                       m_gstPipe.sink, NULL)) {
                throw std::runtime_error("unable to link many");
            }
        }
    } catch (const std::runtime_error &err) {
        gst_object_unref(capsfilter);
        gst_object_unref(h264parse);
        gst_object_unref(queue);
        gst_object_unref(fake_vid_sink);
        throw;
    }

    LOGD("droidcam source init completed");
}

void DroidCamSource::cleanGst() {
    if (m_gstPipe.pipeline != nullptr) {
        gst_element_set_state(m_gstPipe.pipeline, GST_STATE_NULL);
        gst_object_unref(m_gstPipe.pipeline);
        m_gstPipe.pipeline = nullptr;
        m_gstPipe.source = nullptr;
        m_gstPipe.sink = nullptr;
    } else {
        if (m_gstPipe.source != nullptr) {
            gst_object_unref(m_gstPipe.source);
            m_gstPipe.source = nullptr;
        }
        if (m_gstPipe.sink != nullptr) {
            gst_object_unref(m_gstPipe.sink);
            m_gstPipe.sink = nullptr;
        }
    }
}

void DroidCamSource::startDroidCamCapture() const {
    LOGD("starting compressed video capture");

    if (droidCamProp(m_gstPipe.source, "ready-for-capture") == 0) {
        LOGW("already capturing");
        return;
    }

    g_signal_emit_by_name(m_gstPipe.source, "start-capture", NULL);
}

void DroidCamSource::stopDroidCamCapture() const {
    LOGD("stopping compressed video capture");

    if (droidCamProp(m_gstPipe.source, "ready-for-capture") == 1) {
        LOGW("not capturing");
        return;
    }

    g_signal_emit_by_name(m_gstPipe.source, "stop-capture", NULL);
}

static auto gstStateChangeFromMsg(GstMessage *msg) {
    GstState os, ns, ps;
    gst_message_parse_state_changed(msg, &os, &ns, &ps);

    auto *name = gst_element_get_name(GST_MESSAGE_SRC(msg));

    LOGD("gst state changed ("
         << name << "): " << gst_element_state_get_name(os) << " -> "
         << gst_element_state_get_name(ns) << " ("
         << gst_element_state_get_name(ps) << ")");

    g_free(name);

    return std::array{os, ns, ps};
}

void DroidCamSource::doGstIteration() {
    auto *msg = gst_bus_timed_pop_filtered(
        gst_element_get_bus(m_gstPipe.pipeline), m_gsPipelineTickTime,
        GstMessageType(GST_MESSAGE_STATE_CHANGED | GST_MESSAGE_ERROR |
                       GST_MESSAGE_EOS));

    if (msg == nullptr) return;

    switch (GST_MESSAGE_TYPE(msg)) {
        case GST_MESSAGE_ERROR: {
            GError *err = nullptr;
            gchar *debug_info = nullptr;
            gst_message_parse_error(msg, &err, &debug_info);

            std::ostringstream os;
            os << "error received from element " << GST_OBJECT_NAME(msg->src)
               << " " << err->message;

            g_clear_error(&err);
            g_free(debug_info);
            gst_message_unref(msg);

            throw std::runtime_error(os.str());
        }
        case GST_MESSAGE_EOS:
            gst_message_unref(msg);
            throw std::runtime_error("end-of-stream reached");
        case GST_MESSAGE_STATE_CHANGED:
            if (auto states = gstStateChangeFromMsg(msg);
                GST_MESSAGE_SRC(msg) == GST_OBJECT(m_gstPipe.pipeline)) {
                if (m_props.codecId != AV_CODEC_ID_RAWVIDEO) {
                    if (!m_terminating && states[1] == GST_STATE_PAUSED &&
                        states[2] == GST_STATE_PLAYING) {
                        startDroidCamCapture();
                    }
                }
            }
            break;
        default:
            LOGW("unexpected gst message received");
            break;
    }

    gst_message_unref(msg);
}

void DroidCamSource::startGstThread() {
    if (m_gstThread.joinable()) {
        m_gstThread.detach();
    }

    m_gstThread = std::thread([this] {
        LOGD("staring gst pipeline");

        try {
            while (!m_terminating) {
                doGstIteration();
            }
        } catch (const std::runtime_error &e) {
            LOGE("error in gst pipeline thread: " << e.what());
        }

        if (m_props.codecId != AV_CODEC_ID_RAWVIDEO) stopDroidCamCapture();

        cleanGst();

        if (!m_terminating) {
            m_terminating = true;
            if (m_errorHandler) m_errorHandler();
        }

        LOGD("gst pipeline ended");
    });
}
GstFlowReturn DroidCamSource::gstNewSampleCallbackStatic(GstElement *element,
                                                         gpointer udata) {
    return static_cast<DroidCamSource *>(udata)->gstNewSampleCallback(element);
}

GstFlowReturn DroidCamSource::gstNewSampleCallback(GstElement *element) {
    GstSample *sample =
        gst_app_sink_pull_sample(reinterpret_cast<GstAppSink *>(element));
    if (sample == nullptr) {
        LOGW("gst sample is null");
        return GST_FLOW_OK;
    }

    GstBuffer *sample_buf = gst_sample_get_buffer(sample);
    if (sample_buf == nullptr) {
        LOGW("gst sample buf is null");
        return GST_FLOW_OK;
    }

    GstMapInfo info;
    if (!gst_buffer_map(sample_buf, &info, GST_MAP_READ)) {
        LOGW("gst buffer map error");
        return GST_FLOW_OK;
    }

    LOGT("new gst video sample");

    if (info.size == 0) {
        gst_buffer_unmap(sample_buf, &info);
        gst_sample_unref(sample);
        LOGW("gst sample size is zero");
        return GST_FLOW_ERROR;
    }

    GstFlowReturn ret = GST_FLOW_OK;

    if (m_terminating)
        ret = GST_FLOW_EOS;
    else if (m_dataReadyHandler)
        m_dataReadyHandler(info.data, info.size);

    LOGT("gst sample written: ret=" << ret);

    gst_buffer_unmap(sample_buf, &info);
    gst_sample_unref(sample);

    return ret;
}

[[maybe_unused]] GHashTable *DroidCamSource::getDroidCamDevTable() const {
    GHashTable *table = nullptr;

    g_object_get(m_gstPipe.source, "device-parameters", &table, NULL);

    if (table == nullptr) LOGW("failed to get device parameters table");

    return table;
}

[[maybe_unused]] std::optional<std::string>
DroidCamSource::readDroidCamDevParam(const std::string &key) const {
    auto *params = getDroidCamDevTable();
    if (params == nullptr) return std::nullopt;

    auto *value = static_cast<char *>(g_hash_table_lookup(params, key.data()));

    if (value == nullptr) return std::nullopt;

    return std::make_optional<std::string>(value);
}
