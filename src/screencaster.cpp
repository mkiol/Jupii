/* Copyright (C) 2019-2022 Michal Kosciesza <michal@mkiol.net>
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#include "screencaster.h"

#include <QDebug>
#include <QDir>
#include <QGuiApplication>
#include <QScreen>
#include <QStandardPaths>
#include <QUrl>

extern "C" {
#include <libavdevice/avdevice.h>
#include <libavutil/dict.h>
#include <libavutil/error.h>
#include <libavutil/imgutils.h>
#include <libavutil/mathematics.h>
#include <libavutil/time.h>
#include <libavutil/timestamp.h>
}

#ifdef SAILFISH
#include <QPainter>
#include <QTransform>

#include "wayland-lipstick-recorder-client-protocol.h"
#endif

#include "contentserver.h"
#include "contentserverworker.h"
#include "pulseaudiosource.h"
#include "settings.h"
#include "utils.h"

#ifdef DESKTOP
#include <X11/Xlib.h>
#endif

ScreenCaster::ScreenCaster(QObject *parent) : QObject(parent) {
    connect(this, &ScreenCaster::error, ContentServerWorker::instance(),
            &ContentServerWorker::screenErrorHandler);

#ifdef SAILFISH
    frameTimer.setTimerType(Qt::PreciseTimer);
    frameTimer.setInterval(1000 / Settings::instance()->getScreenFramerate());
    connect(&frameTimer, &QTimer::timeout, this, &ScreenCaster::writeVideoData,
            Qt::DirectConnection);
    repaintTimer.setInterval(1000);  // 1s
    repaintTimer.setTimerType(Qt::VeryCoarseTimer);
    connect(&repaintTimer, &QTimer::timeout, this, &ScreenCaster::repaint);
#else
    connect(this, &ScreenCaster::readNextVideoData, this,
            &ScreenCaster::writeVideoData, Qt::QueuedConnection);
#endif
}

ScreenCaster::~ScreenCaster() {
    qDebug() << "ScreenCaster destructor start";

#ifdef SAILFISH
    frameTimer.stop();
    repaintTimer.stop();
#endif

    if (video_sws_ctx) {
        sws_freeContext(video_sws_ctx);
        video_sws_ctx = nullptr;
    }
    if (in_frame_s) {
        av_frame_free(&in_frame_s);
        in_frame_s = nullptr;
    }
    if (video_outbuf) {
        av_free(video_outbuf);
        video_outbuf = nullptr;
    }
    if (out_format_ctx) {
        if (out_format_ctx->pb) {
            if (out_format_ctx->pb->buffer) {
                av_free(out_format_ctx->pb->buffer);
                out_format_ctx->pb->buffer = nullptr;
            }
            avio_context_free(&out_format_ctx->pb);
            out_format_ctx->pb = nullptr;
        }
        avformat_free_context(out_format_ctx);
        out_format_ctx = nullptr;
    }
    if (in_frame) {
        av_frame_free(&in_frame);
        in_frame = nullptr;
    }
    if (in_video_format_ctx) {
        avformat_close_input(&in_video_format_ctx);
        in_video_format_ctx = nullptr;
    }
    if (audio_swr_ctx) {
        swr_free(&audio_swr_ctx);
        audio_swr_ctx = nullptr;
    }
    if (in_audio_codec_ctx) {
        avcodec_free_context(&in_audio_codec_ctx);
        in_audio_codec_ctx = nullptr;
    }

    if (video_out_pkt) {
        av_packet_unref(video_out_pkt);
        video_out_pkt = nullptr;
    }
    if (audio_out_pkt) {
        av_packet_unref(audio_out_pkt);
        audio_out_pkt = nullptr;
    }

    qDebug() << "ScreenCaster destructor end";
}

void ScreenCaster::fixSize(QSize &size, int *xoff, int *yoff) {
    int xoff_delta = size.width() % 2;
    int yoff_delta = size.height() % 2;
    size.setWidth(size.width() - xoff_delta);
    size.setHeight(size.height() - yoff_delta);
    if (xoff) *xoff += xoff_delta;
    if (yoff) *yoff += yoff_delta;
}

void ScreenCaster::initVideoSize() {
#ifdef DESKTOP
    auto *dpy = XOpenDisplay(nullptr);
    auto *s = DefaultScreenOfDisplay(dpy);
    video_size = {s->width, s->height};
    x11_dpy = QString::fromLocal8Bit(DisplayString(dpy));
    qDebug() << "X11 display:" << x11_dpy;
#else
    video_size = QGuiApplication::primaryScreen()->size();
#endif
    qDebug() << "screen video size:" << video_size;

#ifdef SAILFISH
    if (video_size.width() < video_size.height()) {
        qDebug()
            << "portrait orientation detected, so transposing to landscape";
        video_size.transpose();
    }
#endif

    xoff = 0;
    yoff = 0;
    trans_type = Settings::instance()->getScreenCropTo169();

    if (trans_type > 0) {
        bool crop = trans_type > 1;
        qDebug() << "transform to 16:9 ratio";
        int h = static_cast<int>((9.0 / 16.0) * video_size.width());
        if (h <= video_size.height()) {
            if (crop) yoff = (video_size.height() - h) / 2;
            video_size.setHeight(h);
        } else {
            int w = static_cast<int>((16.0 / 9.0) * video_size.height());
            if (crop) xoff = (video_size.width() - w) / 2;
            video_size.setWidth(w);
        }
    }

    fixSize(video_size, &xoff, &yoff);

#ifdef SAILFISH
    if (res_div > 1) {
        video_size.setHeight(video_size.height() / res_div);
        video_size.setWidth(video_size.width() / res_div);
        yoff = yoff / res_div;
        xoff = xoff / res_div;
        fixSize(video_size, &xoff, &yoff);
    }
#endif
}

void ScreenCaster::tuneQuality() {
    auto *s = Settings::instance();
    skipped_frames = 0;
    quality = s->getScreenQuality();
#ifdef SAILFISH
    if (quality < 2) {
        res_div = 4;
        trans_mode = Qt::FastTransformation;
        skipped_frames_max = 10;
    } else if (quality < 3) {
        res_div = 4;
        trans_mode = Qt::SmoothTransformation;
        skipped_frames_max = 10;
    } else if (quality < 4) {
        res_div = 2;
        trans_mode = Qt::SmoothTransformation;
        skipped_frames_max = 10;
    } else if (quality < 5) {
        res_div = 1;
        trans_mode = Qt::SmoothTransformation;
        skipped_frames_max = 10;
    } else {
        res_div = 1;
        trans_mode = Qt::SmoothTransformation;
        skipped_frames_max = 5;
    }

    if (s->getScreenAudio()) skipped_frames_max += 5;
#else
    if (quality < 2) {
        res_div = 4;
    } else if (quality < 3) {
        res_div = 2;
    } else {
        res_div = 1;
    }
#endif
}

bool ScreenCaster::initVideoEncoder() {
    encoder = ENC_UNKNOWN;

    auto enc_name = Settings::instance()->getScreenEncoder();
    qDebug() << "configured encoder name:" << enc_name;

    if (enc_name.isEmpty()) {
        if (!initVideoEncoder("h264_nvenc")) {
            initVideoEncoder("libx264");
        }
    } else {
        initVideoEncoder(enc_name.toLocal8Bit().data());
    }

    if (encoder == ENC_UNKNOWN) {
        qWarning() << "cannot init encoder";
        return false;
    }

    return true;
}

void ScreenCaster::setPixFormats() {
    switch (encoder) {
        case ENC_H264_X264_RGB:
            out_video_codec_ctx->pix_fmt = AV_PIX_FMT_RGB24;
#ifdef SAILFISH
            in_video_codec_ctx->pix_fmt = AV_PIX_FMT_RGB24;
            bgImg = QImage{video_size, QImage::Format_RGB888};
            bgImg.fill(Qt::black);
#else
            in_video_codec_ctx->pix_fmt = AV_PIX_FMT_0RGB32;
#endif
            break;
        case ENC_H264_NVENC:
        case ENC_H264_X264:
        case ENC_H264_OMX:
        default:
            out_video_codec_ctx->pix_fmt = AV_PIX_FMT_YUV420P;
            in_video_codec_ctx->pix_fmt = AV_PIX_FMT_0RGB32;
#ifdef SAILFISH
            bgImg = QImage{video_size, QImage::Format_RGB32};
            bgImg.fill(Qt::black);
#endif
    }
}

bool ScreenCaster::initVideoEncoder(const char *name) {
    qDebug() << "trying encoder:" << name;

    auto *out_video_codec = avcodec_find_encoder_by_name(name);
    if (!out_video_codec) {
        qWarning() << "error in avcodec_find_encoder";
        return false;
    }

    out_video_codec_ctx = avcodec_alloc_context3(out_video_codec);
    if (!out_video_codec_ctx) {
        qWarning() << "error: out_video_codec_ctx is null";
        return false;
    }

    AVDictionary *options = nullptr;

    if (!strcmp(out_video_codec->name, "libx264")) {
        encoder = ENC_H264_X264;
        auto passLogfile =
            QDir{
                QStandardPaths::writableLocation(QStandardPaths::CacheLocation)}
                .filePath(QStringLiteral("passlogfile-%1.log")
                              .arg(Utils::instance()->randString()))
                .toLatin1();
        av_dict_set(&options, "preset", "ultrafast", 0);
        av_dict_set(&options, "tune", "zerolatency", 0);
        av_dict_set(&options, "passlogfile", passLogfile.constData(), 0);
#ifdef SAILFISH
        av_dict_set(&options, "g", "0", 0);
#endif
        av_dict_set(&options, "profile", "high", 0);
    } else if (!strcmp(out_video_codec->name, "libx264rgb")) {
        encoder = ENC_H264_X264_RGB;
        auto passLogfile =
            QDir{
                QStandardPaths::writableLocation(QStandardPaths::CacheLocation)}
                .filePath(QStringLiteral("passlogfile-%1.log")
                              .arg(Utils::instance()->randString()))
                .toLatin1();
        av_dict_set(&options, "preset", "ultrafast", 0);
        av_dict_set(&options, "tune", "zerolatency", 0);
        av_dict_set(&options, "passlogfile", passLogfile.constData(), 0);
#ifdef SAILFISH
        av_dict_set(&options, "g", "0", 0);
#endif
        av_dict_set(&options, "profile", "high444", 0);
    } else if (!strcmp(out_video_codec->name, "h264_nvenc")) {
        encoder = ENC_H264_NVENC;
        av_dict_set(&options, "preset", "llhp", 0);
        av_dict_set(&options, "rc", "cbr", 0);
        av_dict_set(&options, "b", "24M", 0);
        av_dict_set(&options, "profile", "high", 0);
        /*} else if (!strcmp(out_video_codec->name, "h264_omx")) {
            encoder = ENC_H264_OMX;
            av_dict_set(&options, "omx_libname", "/vendor/lib/libOmxCore.so",
           0); av_dict_set(&options, "omx_libprefix", "", 0);
            av_dict_set(&options, "profile", "high", 0);*/
    } else {
        qWarning() << "cannot find valid encoder";
        return false;
    }

#ifdef SAILFISH
    out_video_codec_ctx->width = video_size.width();
    out_video_codec_ctx->height = video_size.height();
#else
    if (res_div > 1) {
        auto size =
            QSize{video_size.width() / res_div, video_size.height() / res_div};
        fixSize(size);
        out_video_codec_ctx->width = size.width();
        out_video_codec_ctx->height = size.height();
    } else {
        out_video_codec_ctx->width = video_size.width();
        out_video_codec_ctx->height = video_size.height();
    }
#endif
    out_video_codec_ctx->flags = AVFMT_FLAG_NOBUFFER | AVFMT_FLAG_FLUSH_PACKETS;
    out_video_codec_ctx->time_base.num = 1;
    out_video_codec_ctx->time_base.den = video_framerate;

    setPixFormats();

    char errbuf[50];
    int ret = avcodec_open2(out_video_codec_ctx, out_video_codec, &options);
    if (ret < 0) {
        qWarning() << "error in avcodec_open2 for out_video_codec_ctx:"
                   << av_make_error_string(errbuf, 50, ret);
        av_dict_free(&options);
        return false;
    }
    av_dict_free(&options);

    auto *out_video_stream = avformat_new_stream(out_format_ctx, nullptr);
    if (!out_video_stream) {
        qWarning() << "error in avformat_new_stream for video";
        return false;
    }
    out_video_stream->id = 0;
    if (avcodec_parameters_from_context(out_video_stream->codecpar,
                                        out_video_codec_ctx) < 0) {
        qWarning() << "error in avcodec_parameters_from_context";
        return false;
    }

    qDebug() << "successfully inited encoder:" << out_video_codec->name;

    return true;
}

bool ScreenCaster::initInAudio() {
    char errbuf[50];
    int ret = 0;

    auto in_audio_codec = avcodec_find_decoder(AV_CODEC_ID_PCM_S16LE);
    if (!in_audio_codec) {
        qWarning() << "error in avcodec_find_decoder for audio";
        return false;
    }

    in_audio_codec_ctx = avcodec_alloc_context3(in_audio_codec);
    if (!in_audio_codec_ctx) {
        qWarning() << "error: avcodec_alloc_context3 is null";
        return false;
    }

    in_audio_codec_ctx->channels = PulseAudioSource::sampleSpec.channels;
    in_audio_codec_ctx->channel_layout =
        uint64_t(av_get_default_channel_layout(in_audio_codec_ctx->channels));
    in_audio_codec_ctx->sample_rate = int(PulseAudioSource::sampleSpec.rate);
    in_audio_codec_ctx->sample_fmt = AV_SAMPLE_FMT_S16;
    in_audio_codec_ctx->time_base.num = 1;
    in_audio_codec_ctx->time_base.den = in_audio_codec_ctx->sample_rate;

    AVDictionary *options = nullptr;
    ret = avcodec_open2(in_audio_codec_ctx, in_audio_codec, &options);
    if (ret < 0) {
        qWarning() << "error in avcodec_open2 for in_audio_codec_ctx:"
                   << av_make_error_string(errbuf, 50, ret);
        return false;
    }
    av_dict_free(&options);

    /*qDebug() << "in audio codec params:";
    qDebug() << " channels:" << in_audio_codec_ctx->channels;
    qDebug() << " channel_layout:" << in_audio_codec_ctx->channel_layout;
    qDebug() << " sample_rate:" << in_audio_codec_ctx->sample_rate;
    qDebug() << " codec_id:" << in_audio_codec_ctx->codec_id;
    qDebug() << " codec_type:" << in_audio_codec_ctx->codec_type;
    qDebug() << " sample_fmt:" << in_audio_codec_ctx->sample_fmt;
    qDebug() << " frame_size:" << in_audio_codec_ctx->frame_size;
    qDebug() << " time_base.num:" << in_audio_codec_ctx->time_base.num;
    qDebug() << " time_base.den:" << in_audio_codec_ctx->time_base.den;*/

    return true;
}

bool ScreenCaster::initOutAudio() {
    char errbuf[50];
    int ret = 0;

    auto *out_audio_codec = avcodec_find_encoder(AV_CODEC_ID_MP3);
    if (!out_audio_codec) {
        qWarning() << "error in avcodec_find_encoder";
        return false;
    }

    out_audio_codec_ctx = avcodec_alloc_context3(out_audio_codec);
    if (!out_audio_codec_ctx) {
        qWarning() << "error: out_audio_codec_ctx is null";
        return false;
    }

    out_audio_codec_ctx->channels = in_audio_codec_ctx->channels;
    out_audio_codec_ctx->sample_rate = in_audio_codec_ctx->sample_rate;
    out_audio_codec_ctx->sample_fmt = AV_SAMPLE_FMT_FLTP;
    out_audio_codec_ctx->flags = AVFMT_FLAG_NOBUFFER | AVFMT_FLAG_FLUSH_PACKETS;
    out_audio_codec_ctx->channel_layout = in_audio_codec_ctx->channel_layout;
    out_audio_codec_ctx->time_base.num = 1;
    out_audio_codec_ctx->time_base.den = in_audio_codec_ctx->sample_rate;

    auto *out_audio_stream = avformat_new_stream(out_format_ctx, nullptr);
    if (!out_audio_stream) {
        qWarning() << "error in avformat_new_stream for audio";
        return false;
    }
    out_audio_stream->id = 1;
    if (avcodec_parameters_from_context(out_audio_stream->codecpar,
                                        out_audio_codec_ctx) < 0) {
        qWarning() << "error in avcodec_parameters_from_context";
        return false;
    }

    AVDictionary *options = nullptr;
    av_dict_set(&options, "b", "128k", 0);
    av_dict_set(&options, "q", "9", 0);
    ret = avcodec_open2(out_audio_codec_ctx, out_audio_codec, &options);
    if (ret < 0) {
        qWarning() << "error in avcodec_open2 for out_audio_codec_ctx:"
                   << av_make_error_string(errbuf, 50, ret);
        av_dict_free(&options);
        return false;
    }
    av_dict_free(&options);

    in_audio_codec_ctx->frame_size = out_audio_codec_ctx->frame_size;
    int buff_size = av_samples_get_buffer_size(
        nullptr, in_audio_codec_ctx->channels, out_audio_codec_ctx->frame_size,
        in_audio_codec_ctx->sample_fmt, 0);
    if (buff_size <= 0) {
        qWarning() << "error in av_samples_get_buffer_size:" << buff_size;
        return false;
    }
    audio_frame_size = uint64_t(buff_size);
    audio_pkt_duration =
        out_audio_codec_ctx
            ->frame_size;  // time_base is 1/rate, so duration of 1 sample is 1
    video_audio_ratio =
        int(av_rescale_q(video_pkt_duration, AVRational{1, 1000000},
                         AVRational{1, out_audio_codec_ctx->sample_rate}) /
            audio_pkt_duration);
#ifdef QT_DEBUG
    qDebug() << "out audio codec params:";
    qDebug() << " codec_id:" << out_audio_codec_ctx->codec_id;
    qDebug() << " codec_type:" << out_audio_codec_ctx->codec_type;
    qDebug() << " bit_rate:" << out_audio_codec_ctx->bit_rate;
    qDebug() << " channels:" << out_audio_codec_ctx->channels;
    qDebug() << " channel_layout:" << out_audio_codec_ctx->channel_layout;
    qDebug() << " sample_rate:" << out_audio_codec_ctx->sample_rate;
    qDebug() << " sample_fmt:" << out_audio_codec_ctx->sample_fmt;
    qDebug() << " time_base.num:" << out_audio_codec_ctx->time_base.num;
    qDebug() << " time_base.den:" << out_audio_codec_ctx->time_base.den;
    qDebug() << " frame_size:" << out_audio_codec_ctx->frame_size;
    qDebug() << " audio_frame_size:" << audio_frame_size;
    qDebug() << " audio_pkt_duration:" << audio_pkt_duration;
    qDebug() << " video_audio_ratio:" << video_audio_ratio;
#endif
    audio_swr_ctx = swr_alloc();
    av_opt_set_int(audio_swr_ctx, "in_channel_layout",
                   static_cast<int>(in_audio_codec_ctx->channel_layout), 0);
    av_opt_set_int(audio_swr_ctx, "out_channel_layout",
                   static_cast<int>(out_audio_codec_ctx->channel_layout), 0);
    av_opt_set_int(audio_swr_ctx, "in_sample_rate",
                   in_audio_codec_ctx->sample_rate, 0);
    av_opt_set_int(audio_swr_ctx, "out_sample_rate",
                   out_audio_codec_ctx->sample_rate, 0);
    av_opt_set_sample_fmt(audio_swr_ctx, "in_sample_fmt",
                          in_audio_codec_ctx->sample_fmt, 0);
    av_opt_set_sample_fmt(audio_swr_ctx, "out_sample_fmt",
                          out_audio_codec_ctx->sample_fmt, 0);
    swr_init(audio_swr_ctx);

    return true;
}

bool ScreenCaster::initScaler() {
    // TODO: ScreenCaster: use HW scaler
    auto nbytes = av_image_get_buffer_size(out_video_codec_ctx->pix_fmt,
                                           out_video_codec_ctx->width,
                                           out_video_codec_ctx->height, 32);

    video_outbuf = static_cast<uint8_t *>(av_malloc(nbytes));
    if (!video_outbuf) {
        qWarning() << "unable to allocate memory";
        return false;
    }

    in_frame_s = av_frame_alloc();
    if (!in_frame_s) {
        qWarning() << "error in av_frame_alloc";
        return false;
    }

    if (av_image_fill_arrays(in_frame_s->data, in_frame_s->linesize,
                             video_outbuf, out_video_codec_ctx->pix_fmt,
                             out_video_codec_ctx->width,
                             out_video_codec_ctx->height, 1) < 0) {
        qWarning() << "error in av_image_fill_arrays";
        return false;
    }

    video_sws_ctx = sws_getContext(
        in_video_codec_ctx->width, in_video_codec_ctx->height,
        in_video_codec_ctx->pix_fmt, out_video_codec_ctx->width,
        out_video_codec_ctx->height, out_video_codec_ctx->pix_fmt, SWS_POINT,
        nullptr, nullptr, nullptr);
    if (!video_sws_ctx) {
        qWarning() << "error in sws_getContext";
        return false;
    }

    return true;
}

bool ScreenCaster::init() {
    qDebug() << "ScreenCaster init";

    char errbuf[50];
    int ret = 0;

    // in video

    auto s = Settings::instance();
    video_framerate = s->getScreenFramerate();
    video_pkt_duration =
        av_rescale_q(1, AVRational{1, video_framerate}, AVRational{1, 1000000});

    tuneQuality();
    initVideoSize();

    qDebug() << "screen quality:" << quality;
    qDebug() << "screen video_framerate:" << video_framerate;
    qDebug() << "screen video_pkt_duration:" << video_pkt_duration;
    qDebug() << "screen video_size:" << video_size;
    qDebug() << "screen skip frames:" << skipped_frames_max;
    qDebug() << "screen res_div:" << res_div;
    qDebug() << "screen trans_mode:" << trans_mode;

    AVDictionary *options = nullptr;

#ifdef SAILFISH
    auto in_video_codec = avcodec_find_decoder(AV_CODEC_ID_RAWVIDEO);
    if (!in_video_codec) {
        qWarning() << "error in avcodec_find_decoder for video";
        return false;
    }
#else
    auto x11grabUrl =
        QStringLiteral("%1+%2,%3").arg(x11_dpy).arg(xoff).arg(yoff).toLatin1();
    auto video_ssize = QStringLiteral("%1x%2")
                           .arg(video_size.width())
                           .arg(video_size.height())
                           .toLatin1();
    qDebug() << "video size:" << video_ssize;
    qDebug() << "video x11grabUrl:" << x11grabUrl;
    if (QGuiApplication::screens().size() > 1) {
        qWarning() << "more that one screen but capturing only first screen";
    }

    auto in_video_format = av_find_input_format("x11grab");

    if (av_dict_set_int(&options, "framerate", video_framerate, 0) < 0 ||
        av_dict_set(&options, "video_size", video_ssize.data(), 0) < 0 ||
        av_dict_set(&options, "draw_mouse", "1", 0) < 0) {
        qWarning() << "Error in av_dict_set";
        return false;
    }

    in_video_format_ctx = nullptr;
    if (avformat_open_input(&in_video_format_ctx, x11grabUrl.data(),
                            in_video_format, &options) < 0) {
        qWarning() << "error in avformat_open_input for video";
        av_dict_free(&options);
        return false;
    }
    av_dict_free(&options);

    int in_video_stream_idx = -1;
    // qDebug() << "x11grab streams count:" << in_video_format_ctx->nb_streams;
    for (int i = 0; i < static_cast<int>(in_video_format_ctx->nb_streams);
         ++i) {
        if (in_video_format_ctx->streams[i]->codecpar &&
            in_video_format_ctx->streams[i]->codecpar->codec_type ==
                AVMEDIA_TYPE_VIDEO) {
            in_video_stream_idx = i;
        }
    }
    if (in_video_stream_idx < 0) {
        qWarning() << "no video stream in x11grab";
        return false;
    }

#ifdef QT_DEBUG
    qDebug() << "x11grab video stream:";
    qDebug() << " id:" << in_video_format_ctx->streams[in_video_stream_idx]->id;
    qDebug()
        << " height:"
        << in_video_format_ctx->streams[in_video_stream_idx]->codecpar->height;
    qDebug()
        << " width:"
        << in_video_format_ctx->streams[in_video_stream_idx]->codecpar->width;
    qDebug() << " codec_id:"
             << in_video_format_ctx->streams[in_video_stream_idx]
                    ->codecpar->codec_id;
    qDebug() << " codec_type:"
             << in_video_format_ctx->streams[in_video_stream_idx]
                    ->codecpar->codec_type;
#endif

    auto in_video_codec = avcodec_find_decoder(
        in_video_format_ctx->streams[in_video_stream_idx]->codecpar->codec_id);
    if (!in_video_codec) {
        qWarning() << "error in avcodec_find_decoder for video";
        return false;
    }
#endif

    in_video_codec_ctx = avcodec_alloc_context3(in_video_codec);
    if (!in_video_codec_ctx) {
        qWarning() << "error: in_video_codec_ctx is null";
        return false;
    }

    if (avformat_alloc_output_context2(&out_format_ctx, nullptr, "mpegts",
                                       nullptr) < 0) {
        qWarning() << "error in avformat_alloc_output_context2";
        return false;
    }

    if (!initVideoEncoder()) {
        qWarning() << "cannot init video encoder";
        return false;
    }

    in_video_codec_ctx->width = video_size.width();
    in_video_codec_ctx->height = video_size.height();
    in_video_codec_ctx->time_base.num = 1;
    in_video_codec_ctx->time_base.den = 1000000;

    ret = avcodec_open2(in_video_codec_ctx, in_video_codec, &options);
    if (ret < 0) {
        qWarning() << "error in avcodec_open2 for in_video_codec_ctx:"
                   << av_make_error_string(errbuf, 50, ret);
        return false;
    }

    in_frame = av_frame_alloc();
    if (!in_frame) {
        qWarning() << "error in av_frame_alloc";
        return false;
    }

    if (s->getScreenAudio()) {
        if (!initInAudio()) {
            qWarning() << "error in in-audio init";
            return false;
        }
    }

    // out
#ifdef QT_DEBUG
    qDebug() << "out video codec params:" << out_video_codec_ctx->codec_id;
    qDebug() << " codec_id:" << out_video_codec_ctx->codec_id;
    qDebug() << " codec_type:" << out_video_codec_ctx->codec_type;
    qDebug() << " pix_fmt:" << out_video_codec_ctx->pix_fmt;
    qDebug() << " bit_rate:" << out_video_codec_ctx->bit_rate;
    qDebug() << " width:" << out_video_codec_ctx->width;
    qDebug() << " height:" << out_video_codec_ctx->height;
    qDebug() << " gop_size:" << out_video_codec_ctx->gop_size;
    qDebug() << " max_b_frames:" << out_video_codec_ctx->max_b_frames;
    qDebug() << " time_base.num:" << out_video_codec_ctx->time_base.num;
    qDebug() << " time_base.den:" << out_video_codec_ctx->time_base.den;
#endif
    initScaler();  // TODO: ScreenCaster: Skip if scaling is not needed

    size_t outbuf_size = 500000;
    auto *outbuf = static_cast<uint8_t *>(av_malloc(outbuf_size));
    if (!outbuf) {
        qWarning() << "unable to allocate memory";
        return false;
    }

    out_format_ctx->pb =
        avio_alloc_context(outbuf, outbuf_size, 1, nullptr, nullptr,
                           write_packet_callback, nullptr);
    if (s->getScreenAudio()) {
        if (!initOutAudio()) {
            qWarning() << "error in out-audio init";
            return false;
        }
    }

    // start stream

    ret = avformat_write_header(out_format_ctx, nullptr);
    if (ret != AVSTREAM_INIT_IN_WRITE_HEADER &&
        ret != AVSTREAM_INIT_IN_INIT_OUTPUT) {
        qWarning() << "error in avformat_write_header:"
                   << av_make_error_string(errbuf, 50, ret);
        return false;
    }

    return true;
}

void ScreenCaster::start() {
#ifdef SAILFISH
    recorder = std::unique_ptr<Recorder>(new Recorder(this, 1));
    frameTimer.start();
    repaintTimer.start();
#else
    emit readNextVideoData();
#endif
}

bool ScreenCaster::audioEnabled() const { return audio_frame_size > 0; }

int ScreenCaster::write_packet_callback(void *, uint8_t *buf, int buf_size) {
    auto worker = ContentServerWorker::instance();
    // qDebug() << "write_packet_callback:" << buf_size;
    worker->sendScreenCaptureData(static_cast<void *>(buf), buf_size);
    return buf_size;
}

void ScreenCaster::writeAudioData(const QByteArray &data) {
    if (video_pkt_start_time == 0) {
        // qDebug() << "discarding audio because video didn't start";
        return;
    }

    audio_outbuf.append(data);

    if (!writeAudioData2()) {
        errorHandler();
    }
}

bool ScreenCaster::writeAudioData2() {
    while (static_cast<uint64_t>(audio_outbuf.size()) >= audio_frame_size) {
        const char *d = audio_outbuf.data();

        int ndelay = 0;
        bool start = false;
        if (audio_pkt_time == 0) {
            qDebug() << "first audio samples";
            audio_pkt_time =
                av_rescale_q(video_pkt_start_time, AVRational{1, 1000000},
                             AVRational{1, out_audio_codec_ctx->sample_rate}) -
                3 * audio_pkt_duration;
            ndelay = 3;
            start = true;
        }

        auto *audio_in_pkt = av_packet_alloc();
        if (!audio_in_pkt) {
            qDebug() << "audio_in_pkt is null";
            return false;
        }

        if (av_new_packet(audio_in_pkt, int(audio_frame_size)) < 0) {
            qDebug() << "error in av_new_packet";
            return false;
        }

        int samples_to_remove = 1;
        for (int i = 0; i < ndelay + 1; ++i) {
            // qDebug() << "encoding new audio data";
            audio_in_pkt->dts = audio_pkt_time;
            audio_in_pkt->pts = audio_pkt_time;
            audio_in_pkt->duration = audio_pkt_duration;

            memcpy(audio_in_pkt->data, d, size_t(audio_frame_size));

            int ret = avcodec_send_packet(in_audio_codec_ctx, audio_in_pkt);
            if (ret == 0) {
                ret = avcodec_receive_frame(in_audio_codec_ctx, in_frame);
                if (ret == 0 || ret == AVERROR(EAGAIN)) {
                    // qDebug() << "audio frame decoded";
                    //  resampling
                    AVFrame *frame = av_frame_alloc();
                    frame->channel_layout = out_audio_codec_ctx->channel_layout;
                    frame->format = out_audio_codec_ctx->sample_fmt;
                    frame->sample_rate = out_audio_codec_ctx->sample_rate;
                    swr_convert_frame(audio_swr_ctx, frame, in_frame);
                    frame->pts = in_frame->pts;
                    if (!audio_out_pkt) audio_out_pkt = av_packet_alloc();
                    ret = avcodec_send_frame(out_audio_codec_ctx, frame);
                    av_frame_free(&frame);
                    if (ret == 0 || ret == AVERROR(EAGAIN)) {
                        ret = avcodec_receive_packet(out_audio_codec_ctx,
                                                     audio_out_pkt);
                        if (ret == 0) {
                            // qDebug() << "audio packet encoded";
                            start = false;
                            if (!writeAudioData3()) {
                                av_packet_unref(audio_in_pkt);
                                return false;
                            }
                        } else if (ret == AVERROR(EAGAIN)) {
                            // qDebug() << "packet not ready";
                        } else {
                            qWarning()
                                << "error in avcodec_receive_packet for audio";
                            av_packet_unref(audio_in_pkt);
                            return false;
                        }
                    } else {
                        qWarning() << "error in avcodec_send_frame for audio";
                        av_packet_unref(audio_in_pkt);
                        return false;
                    }
                } else if (ret == AVERROR(EAGAIN)) {
                    // qDebug() << "Audio frame not ready";
                } else {
                    qWarning() << "error in avcodec_receive_frame for audio";
                    av_packet_unref(audio_in_pkt);
                    return false;
                }
            } else {
                qWarning() << "error in avcodec_send_packet for audio";
                av_packet_unref(audio_in_pkt);
                return false;
            }

            if (start) {
                audio_pkt_time += audio_pkt_duration;
            }
        }

        int audio_to_remove = samples_to_remove * int(audio_frame_size);
        if (audio_to_remove >= audio_outbuf.size())
            audio_outbuf.clear();
        else
            audio_outbuf.remove(0, audio_to_remove);

        av_packet_unref(audio_in_pkt);
    }

    return true;
}

bool ScreenCaster::writeAudioData3() {
    auto in_tb = in_audio_codec_ctx->time_base;
    auto out_tb = out_format_ctx->streams[1]->time_base;

    audio_out_pkt->stream_index = 1;  // audio output stream
    audio_out_pkt->pos = -1;
    audio_out_pkt->pts = av_rescale_q_rnd(
        audio_pkt_time, in_tb, out_tb,
        static_cast<AVRounding>(AV_ROUND_NEAR_INF | AV_ROUND_PASS_MINMAX));
    audio_out_pkt->dts = av_rescale_q_rnd(
        audio_pkt_time, in_tb, out_tb,
        static_cast<AVRounding>(AV_ROUND_NEAR_INF | AV_ROUND_PASS_MINMAX));
    audio_out_pkt->duration =
        av_rescale_q(audio_out_pkt->duration, in_tb, out_tb);

    int ret = av_interleaved_write_frame(out_format_ctx, audio_out_pkt);
    if (ret < 0) {
        qWarning() << "error in av_write_frame for audio";
        return false;
    }

    audio_pkt_time += audio_pkt_duration;

    return true;
}

void ScreenCaster::errorHandler() {
    qDebug() << "screen capture error";
#ifdef SAILFISH
    frameTimer.stop();
    repaintTimer.stop();
#endif
    emit error();
}

#ifdef SAILFISH
void ScreenCaster::repaint() {
    if (recorder) recorder->repaint();
}

void ScreenCaster::writeVideoData() {
    int64_t curr_time = av_gettime();

    if (video_pkt_time == 0) {
        video_pkt_time = curr_time;
        video_pkt_start_time = curr_time;
    }

    int64_t video_delay = (curr_time - video_pkt_time) / video_pkt_duration;
    bool img_not_changed =
        havePrevVideoPkt && (!currImgBuff || !currImgBuff->busy);
    bool video_delayed = havePrevVideoPkt ? video_delay > 0 : false;
    bool audio_delayed = false;

    if (audioEnabled() && audio_pkt_time != 0 && havePrevVideoPkt) {
        int64_t video_pkt_time_a =
            av_rescale_q(video_pkt_time, AVRational{1, 1000000},
                         AVRational{1, out_audio_codec_ctx->sample_rate});
        int64_t audio_video_delay =
            (video_pkt_time_a - audio_pkt_time) / audio_pkt_duration;
        audio_delayed = audio_outbuf.size() / int(audio_frame_size) > 0;
        if (audio_video_delay > 2 * video_audio_ratio) {
            qDebug() << "skipping video frame because audio is delayed";
            return;
        }
    }

    if (!img_not_changed && !video_delayed && !audio_delayed &&
        skipped_frames == 0) {
        // qDebug() << "encoding new pkt";

        auto img = makeCurrImg();
        auto size = img.byteCount();
        auto data = img.constBits();

        auto *video_in_pkt = av_packet_alloc();
        if (!video_in_pkt) {
            qDebug() << "video_in_pkt is null";
            errorHandler();
            return;
        }
        if (av_new_packet(video_in_pkt, size) < 0) {
            qDebug() << "error in av_new_packet";
            errorHandler();
            return;
        }

        memcpy(video_in_pkt->data, data, static_cast<size_t>(size));
        int ret = avcodec_send_packet(in_video_codec_ctx, video_in_pkt);
        if (ret != 0 && ret != AVERROR(EAGAIN)) {
            qWarning() << "error in avcodec_send_packet for video";
            errorHandler();
            return;
        }

        ret = avcodec_receive_frame(in_video_codec_ctx, in_frame);
        if (ret == 0) {
            // qDebug() << "video frame decoded";
            if (scalingNeeded()) {
                // int64_t t1 = av_gettime();
                sws_scale(video_sws_ctx, in_frame->data, in_frame->linesize, 0,
                          in_video_codec_ctx->height, in_frame_s->data,
                          in_frame_s->linesize);
                // qDebug() << "sws_scale duration:" << av_gettime() - t1;
                av_frame_copy_props(in_frame_s, in_frame);
                if (!video_out_pkt) video_out_pkt = av_packet_alloc();

                in_frame_s->format = out_video_codec_ctx->pix_fmt;
                in_frame_s->width = out_video_codec_ctx->width;
                in_frame_s->height = out_video_codec_ctx->height;

                ret = avcodec_send_frame(out_video_codec_ctx, in_frame_s);
            } else {
                ret = avcodec_send_frame(out_video_codec_ctx, in_frame);
            }

            if (ret == 0 || ret == AVERROR(EAGAIN)) {
                ret =
                    avcodec_receive_packet(out_video_codec_ctx, video_out_pkt);
                if (ret == 0) {
                    qDebug() << "video packet encoded";
                    havePrevVideoPkt = true;
                    if (!writeVideoData2()) {
                        av_packet_unref(video_in_pkt);
                        errorHandler();
                        return;
                    }
                } else if (ret == AVERROR(EAGAIN)) {
                    // qDebug() << "packet not ready";
                } else {
                    qWarning() << "error in avcodec_receive_packet for video";
                    av_packet_unref(video_in_pkt);
                    errorHandler();
                    return;
                }
            } else {
                qWarning() << "error in avcodec_send_frame for video";
                av_packet_unref(video_in_pkt);
                errorHandler();
                return;
            }
        } else if (ret == AVERROR(EAGAIN)) {
            // qDebug() << "video frame not ready";
        } else {
            qWarning() << "error in avcodec_receive_frame for video";
            av_packet_unref(video_in_pkt);
            errorHandler();
            return;
        }

        av_packet_unref(video_in_pkt);
    } else {
        if (skipped_frames_max > 0) {
            skipped_frames++;
            if (skipped_frames > skipped_frames_max) {
                // qDebug() << "max skipped frames reached";
                skipped_frames = 0;
            }
        }
        // qDebug() << "sending previous pkt instead new";
        if (!writeVideoData2()) {
            errorHandler();
            return;
        }
    }
}

bool ScreenCaster::scalingNeeded() {
    return out_video_codec_ctx->pix_fmt != in_video_codec_ctx->pix_fmt ||
           out_video_codec_ctx->height != in_video_codec_ctx->height ||
           out_video_codec_ctx->width != in_video_codec_ctx->width;
}

QImage ScreenCaster::makeCurrImg() {
    QImage img =
        currImgBuff ? currImgTransform == LIPSTICK_RECORDER_TRANSFORM_Y_INVERTED
                          ? currImgBuff->image.mirrored(false, true)
                          : currImgBuff->image
                    : bgImg;

    if (res_div > 1) {
        img = img.scaledToHeight(img.height() / res_div, trans_mode);
    }

    // rotation
    auto orientation = QGuiApplication::primaryScreen()->orientation();
    if (orientation == Qt::LandscapeOrientation) {
        QTransform t;
        t.rotate(-90);
        img = img.transformed(t);
    } else if (orientation == Qt::InvertedLandscapeOrientation) {
        QTransform t;
        t.rotate(-270);
        img = img.transformed(t);
    } else if (orientation == Qt::InvertedPortraitOrientation) {
        QTransform t;
        t.rotate(-180);
        img = img.transformed(t);
    }

    // scaling
    if (img.width() < img.height()) {  // portrait scaling
        QImage target = bgImg;
        QPainter p(&target);
        p.setCompositionMode(QPainter::CompositionMode_SourceOver);
        img = img.scaledToHeight(target.height(), trans_mode);
        p.drawImage((target.width() - img.width()) / 2, 0, img);
        p.end();
        img = target;
    } else if (img.width() !=
               video_size.width()) {  // landscape scaling only if needed
        if (trans_type == 1) {        // scaling to width
            QImage target = bgImg;
            QPainter p(&target);
            p.setCompositionMode(QPainter::CompositionMode_SourceOver);
            img = img.scaledToWidth(target.width(), trans_mode);
            p.drawImage(0, (target.height() - img.height()) / 2, img);
            p.end();
            img = target;
        } else if (trans_type == 2) {  // cropping to width
            QRect rect(xoff, yoff, video_size.width(), video_size.height());
            img = img.copy(rect);
        }
    }

    if (bgImg.format() !=
        img.format()) {  // converting to pix format accepted by encoder
        img = img.convertToFormat(bgImg.format());
    }

    if (currImgBuff) {
        currImgBuff->busy = false;
        if (recorder && recorder->m_starving) recorder->recordFrame();
    }

    return img;
}

bool ScreenCaster::event(QEvent *e) {
    if (e->type() == FrameEvent::FrameEventType) {
        // qDebug() << "new screen image";
        auto fe = static_cast<FrameEvent *>(e);
        currImgBuff = fe->buffer;
        currImgTransform = fe->transform;
    }
    return true;
}

bool ScreenCaster::writeVideoData2() {
    int64_t ndelay = (av_gettime() - video_pkt_time) / video_pkt_duration;
    // qDebug() << "video ndelay:" << ndelay;
    ndelay = ndelay < 0 ? 0 : ndelay;

    auto in_tb = in_video_codec_ctx->time_base;
    auto out_tb = out_format_ctx->streams[0]->time_base;

    for (int i = 0; i < ndelay + 1; ++i) {
        video_out_pkt->stream_index = 0;  // video output stream
        video_out_pkt->pos = -1;
        video_out_pkt->pts = av_rescale_q_rnd(
            video_pkt_time, in_tb, out_tb,
            static_cast<AVRounding>(AV_ROUND_NEAR_INF | AV_ROUND_PASS_MINMAX));
        video_out_pkt->dts = av_rescale_q_rnd(
            video_pkt_time, in_tb, out_tb,
            static_cast<AVRounding>(AV_ROUND_NEAR_INF | AV_ROUND_PASS_MINMAX));
        video_out_pkt->duration =
            av_rescale_q(video_out_pkt->duration, in_tb, out_tb);

        auto *pkt = av_packet_alloc();
        av_packet_ref(pkt, video_out_pkt);

        // int ret = av_interleaved_write_frame(out_format_ctx, video_out_pkt);
        int ret = av_interleaved_write_frame(out_format_ctx, pkt);
        if (ret < 0) {
            qWarning() << "error in av_write_frame for video";
            return false;
        }

        video_pkt_time += video_pkt_duration;
    }

    return true;
}
#else
void ScreenCaster::writeVideoData() {
    auto curr_time = av_gettime();

    if (video_pkt_time == 0) {
        video_pkt_time = curr_time;
        video_pkt_start_time = curr_time;
    }

    bool audio_delayed = false;
    if (audioEnabled() && audio_pkt_time != 0 && havePrevVideoPkt) {
        int64_t video_pkt_time_a =
            av_rescale_q(video_pkt_time, AVRational{1, 1000000},
                         AVRational{1, out_audio_codec_ctx->sample_rate});
        int64_t audio_video_delay =
            (video_pkt_time_a - audio_pkt_time) / audio_pkt_duration;
        audio_delayed = audio_video_delay > 2;
    }

    if (audio_delayed) {
        emit readNextVideoData();
        return;
    }

    // qDebug() << "encoding new pkt";

    auto *video_in_pkt = av_packet_alloc();

    if (av_read_frame(in_video_format_ctx, video_in_pkt) >= 0) {
        if (video_in_pkt->stream_index == 0) {  // video in stream
            int ret = avcodec_send_packet(in_video_codec_ctx, video_in_pkt);
            if (ret == 0 || ret == AVERROR(EAGAIN)) {
                ret = avcodec_receive_frame(in_video_codec_ctx, in_frame);
                if (ret == 0) {
                    // qDebug() << "video frame decoded:" << av_gettime() -
                    // curr_time; int64_t t = av_gettime();
                    sws_scale(video_sws_ctx, in_frame->data, in_frame->linesize,
                              0, in_video_codec_ctx->height, in_frame_s->data,
                              in_frame_s->linesize);
                    // qDebug() << "sws_scale duration:" << av_gettime() - t;
                    av_frame_copy_props(in_frame_s, in_frame);
                    if (!video_out_pkt) video_out_pkt = av_packet_alloc();

                    in_frame_s->format = out_video_codec_ctx->pix_fmt;
                    in_frame_s->width = out_video_codec_ctx->width;
                    in_frame_s->height = out_video_codec_ctx->height;

                    ret = avcodec_send_frame(out_video_codec_ctx, in_frame_s);

                    if (ret == 0 || ret == AVERROR(EAGAIN)) {
                        ret = avcodec_receive_packet(out_video_codec_ctx,
                                                     video_out_pkt);
                        if (ret == 0) {
                            // qDebug() << "video packet encoded:" <<
                            // av_gettime() - curr_time;
                            havePrevVideoPkt = true;
                            if (!writeVideoData2()) {
                                errorHandler();
                                return;
                            }
                            emit readNextVideoData();
                        } else if (ret == AVERROR(EAGAIN)) {
                            // qDebug() << "packet not ready"
                            emit readNextVideoData();
                        } else {
                            qWarning()
                                << "error in avcodec_receive_packet for video";
                            errorHandler();
                            return;
                        }
                    } else {
                        qWarning() << "error in avcodec_send_frame for video";
                        errorHandler();
                        return;
                    }
                } else if (ret == AVERROR(EAGAIN)) {
                    // qDebug() << "video frame not ready"
                    emit readNextVideoData();
                } else {
                    qWarning() << "error in avcodec_receive_frame for video";
                    errorHandler();
                    return;
                }
            } else {
                qWarning() << "error in avcodec_receive_frame for video";
                errorHandler();
                return;
            }
        } else {
            qDebug() << "stream_index != video_index for video";
            errorHandler();
            return;
        }
    } else {
        qWarning() << "error in av_read_frame for video";
        errorHandler();
        return;
    }

    av_packet_unref(video_in_pkt);
}

bool ScreenCaster::writeVideoData2() {
    auto in_tb = in_video_codec_ctx->time_base;
    auto out_tb = out_format_ctx->streams[0]->time_base;

    video_out_pkt->stream_index = 0;  // video output stream
    video_out_pkt->pos = -1;
    video_out_pkt->pts = av_rescale_q_rnd(
        video_pkt_time, in_tb, out_tb,
        static_cast<AVRounding>(AV_ROUND_NEAR_INF | AV_ROUND_PASS_MINMAX));
    video_out_pkt->dts = av_rescale_q_rnd(
        video_pkt_time, in_tb, out_tb,
        static_cast<AVRounding>(AV_ROUND_NEAR_INF | AV_ROUND_PASS_MINMAX));
    video_out_pkt->duration =
        av_rescale_q(video_out_pkt->duration, in_tb, out_tb);

    int ret = av_interleaved_write_frame(out_format_ctx, video_out_pkt);

    if (ret < 0) {
        qWarning() << "Error in av_write_frame for video";
        return false;
    }

    video_pkt_time += video_pkt_duration;

    return true;
}
#endif
