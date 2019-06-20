/* Copyright (C) 2019 Michal Kosciesza <michal@mkiol.net>
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#include "screencaster.h"

#include <QDebug>
#include <QGuiApplication>
#include <QScreen>
#include <QStandardPaths>
#include <QDir>

extern "C" {
#include <libavutil/dict.h>
#include <libavutil/mathematics.h>
#include <libavdevice/avdevice.h>
#include <libavutil/imgutils.h>
#include <libavutil/timestamp.h>
#include <libavutil/time.h>
}

#ifdef SAILFISH
#include <QTransform>
#include <QPainter>
#include "wayland-lipstick-recorder-client-protocol.h"
#endif

#include "settings.h"
#include "pulseaudiosource.h"
#include "utils.h"
#include "contentserver.h"

ScreenCaster::ScreenCaster(QObject *parent) : QObject(parent)
{
#ifdef DESKTOP
    connect(this, &ScreenCaster::readNextVideoFrame,
            this, &ScreenCaster::readVideoFrame, Qt::QueuedConnection);
#endif
#ifdef SAILFISH
    auto s = Settings::instance();
    frameTimer.setSingleShot(true);
    //frameTimer.setTimerType(Qt::PreciseTimer);
    frameTimer.setInterval(1000/s->getScreenFramerate());
    connect(&frameTimer, &QTimer::timeout, this, &ScreenCaster::writeVideoData);
    repaintTimer.setSingleShot(true);
    repaintTimer.setInterval(1000); // 1s
    connect(&repaintTimer, &QTimer::timeout, this, &ScreenCaster::repaint);
#endif
}

ScreenCaster::~ScreenCaster()
{
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
}

bool ScreenCaster::init()
{
    qDebug() << "ScreenCaster init";

    char errbuf[50];
    int ret = 0;

    // in video

    auto s = Settings::instance();
    auto video_framerate = s->getScreenFramerate();
    video_size = QGuiApplication::primaryScreen()->size();

    if (video_size.width() < video_size.height()) {
        qDebug() << "Portrait orientation detected, so transposing to landscape";
        video_size.transpose();
    }

    int xoff = 0; int yoff = 0;
    if (s->getScreenCropTo169()) {
        qDebug() << "Cropping to 16:9 ratio";
        int h = video_size.width()*(9.0/16.0);
        h = h - h%2;
        if (h <= video_size.height()) {
            yoff = (video_size.height() - h) / 2;
            video_size.setHeight(h);
        } else {
            int w = video_size.height()*(16.0/9.0);
            w = w - w%2;
            xoff = (video_size.width() - w) / 2;
            video_size.setWidth(w);
        }
    }

    AVDictionary* options = nullptr;

#ifdef SAILFISH
    bgImg = QImage(video_size, QImage::Format_RGB32); bgImg.fill(Qt::black);
    currImg = bgImg;

    auto in_video_codec = avcodec_find_decoder(AV_CODEC_ID_RAWVIDEO);
    if (!in_video_codec) {
        qWarning() << "Error in avcodec_find_decoder for video";
        return false;
    }

    in_video_codec_ctx = avcodec_alloc_context3(in_video_codec);
    if (!in_video_codec_ctx) {
        qWarning() << "Error: avcodec_alloc_context3 is null";
        return false;
    }

    /*in_video_codec_ctx->sample_rate = PulseAudioSource::sampleSpec.rate;
    in_audio_codec_ctx->sample_fmt = AV_SAMPLE_FMT_S16;
    in_audio_codec_ctx->time_base.num = 1;
    in_audio_codec_ctx->time_base.den = in_audio_codec_ctx->sample_rate;*/
#else
    auto x11grabUrl = QString(":0.0+%1,%2").arg(xoff).arg(yoff).toLatin1();
    auto video_ssize = QString("%1x%2").arg(video_size.width())
            .arg(video_size.height()).toLatin1();
    qDebug() << "video size:" << video_ssize;
    qDebug() << "video framerate:" << video_framerate;
    qDebug() << "video x11grabUrl:" << x11grabUrl;
    if (QGuiApplication::screens().size() > 1) {
        qWarning() << "More that one screen but capturing only first screen";
    }

    auto in_video_format = av_find_input_format("x11grab");

    if (av_dict_set_int(&options, "framerate", video_framerate, 0) < 0 ||
        av_dict_set(&options, "preset", "fast", 0) < 0 ||
        av_dict_set(&options, "video_size", video_ssize.data(), 0) < 0 ||
        //av_dict_set(&options, "show_region", "1", 0) < 0 ||
        //av_dict_set(&options, "region_border", "1", 0) < 0 ||
        //av_dict_set(&options, "follow_mouse", "centered", 0) < 0 ||
        av_dict_set(&options, "draw_mouse", "1", 0) < 0) {
        qWarning() << "Error in av_dict_set";
        return false;
    }

    in_video_format_ctx = nullptr;
    if (avformat_open_input(&in_video_format_ctx, x11grabUrl.data(), in_video_format,
                            &options) < 0) {
        qWarning() << "Error in avformat_open_input for video";
        av_dict_free(&options);
        return false;
    }
    av_dict_free(&options);

    int in_video_stream_idx = 0;
    if (in_video_format_ctx->streams[in_video_stream_idx]->
            codecpar->codec_type != AVMEDIA_TYPE_VIDEO) {
        qWarning() << "x11grab stream[0] is not video";
        return false;
    }

    /*qDebug() << "x11grab video stream:";
    qDebug() << " id:" << in_video_format_ctx->streams[in_video_stream_idx]->id;
    qDebug() << " height:" << in_video_format_ctx->streams[in_video_stream_idx]->codecpar->height;
    qDebug() << " width:" << in_video_format_ctx->streams[in_video_stream_idx]->codecpar->width;
    qDebug() << " codec_id:" << in_video_format_ctx->streams[in_video_stream_idx]->codecpar->codec_id;
    qDebug() << " codec_type:" << in_video_format_ctx->streams[in_video_stream_idx]->codecpar->codec_type;*/

    auto in_video_codec = avcodec_find_decoder(in_video_format_ctx->
                                               streams[in_video_stream_idx]->
                                               codecpar->codec_id);
    if (!in_video_codec) {
        qWarning() << "Error in avcodec_find_decoder for video";
        return false;
    }

    in_video_codec_ctx = in_video_format_ctx->streams[in_video_stream_idx]->codec;
    if (!in_video_codec_ctx) {
        qWarning() << "Error: in_video_codec_ctx is null";
        return false;
    }
#endif

    in_video_codec_ctx->width = video_size.width();
    in_video_codec_ctx->height = video_size.height();
    in_video_codec_ctx->time_base.num = 1;
    in_video_codec_ctx->time_base.den = video_framerate;
    in_video_codec_ctx->pix_fmt = AV_PIX_FMT_RGB32;

    av_dict_set(&options, "tune", "zerolatency", 0);
    ret = avcodec_open2(in_video_codec_ctx, in_video_codec, &options);
    if (ret < 0) {
        qWarning() << "Error in avcodec_open2 for in_video_codec_ctx:" << av_make_error_string(errbuf, 50, ret);
        av_dict_free(&options);
        return false;
    }
    av_dict_free(&options);

    /*qDebug() << "In video codec params:";
    qDebug() << " time_base.num:" << in_video_codec_ctx->time_base.num;
    qDebug() << " time_base.den:" << in_video_codec_ctx->time_base.den;
    qDebug() << " pix_fmt:" << in_video_codec_ctx->pix_fmt;*/

    in_frame = av_frame_alloc();
    if(!in_frame) {
        qWarning() << "Error in av_frame_alloc";
        return false;
    }

    av_init_packet(&in_pkt);

    // in audio

    if (s->getScreenAudio()) {
        auto in_audio_codec = avcodec_find_decoder(AV_CODEC_ID_PCM_S16LE);
        if (!in_audio_codec) {
            qWarning() << "Error in avcodec_find_decoder for audio";
            return false;
        }

        in_audio_codec_ctx = avcodec_alloc_context3(in_audio_codec);
        if (!in_audio_codec_ctx) {
            qWarning() << "Error: avcodec_alloc_context3 is null";
            return false;
        }

        in_audio_codec_ctx->channels = PulseAudioSource::sampleSpec.channels;
        in_audio_codec_ctx->channel_layout = av_get_default_channel_layout(in_audio_codec_ctx->channels);
        in_audio_codec_ctx->sample_rate = PulseAudioSource::sampleSpec.rate;
        in_audio_codec_ctx->sample_fmt = AV_SAMPLE_FMT_S16;
        in_audio_codec_ctx->time_base.num = 1;
        in_audio_codec_ctx->time_base.den = in_audio_codec_ctx->sample_rate;

        av_dict_set(&options, "tune", "zerolatency", 0);
        ret = avcodec_open2(in_audio_codec_ctx, in_audio_codec, &options);
        if (ret < 0) {
            qWarning() << "Error in avcodec_open2 for in_audio_codec_ctx:" << av_make_error_string(errbuf, 50, ret);
            av_dict_free(&options);
            return false;
        }
        av_dict_free(&options);

        /*qDebug() << "In audio codec params:";
        qDebug() << " channels:" << in_audio_codec_ctx->channels;
        qDebug() << " channel_layout:" << in_audio_codec_ctx->channel_layout;
        qDebug() << " sample_rate:" << in_audio_codec_ctx->sample_rate;
        qDebug() << " codec_id:" << in_audio_codec_ctx->codec_id;
        qDebug() << " codec_type:" << in_audio_codec_ctx->codec_type;
        qDebug() << " sample_fmt:" << in_audio_codec_ctx->sample_fmt;
        qDebug() << " frame_size:" << in_audio_codec_ctx->frame_size;
        qDebug() << " time_base.num:" << in_audio_codec_ctx->time_base.num;
        qDebug() << " time_base.den:" << in_audio_codec_ctx->time_base.den;*/
    }

    // out

    if (avformat_alloc_output_context2(&out_format_ctx, nullptr, "mpegts", nullptr) < 0) {
    //if (avformat_alloc_output_context2(&out_format_ctx, nullptr, nullptr, "output.mp4") < 0) {
        qWarning() << "Error in avformat_alloc_output_context2";
        return false;
    }

    auto out_video_codec = avcodec_find_encoder(AV_CODEC_ID_H264);
    if (!out_video_codec) {
        qWarning() << "Error in avcodec_find_encoder for H264";
        return false;
    }

    AVStream *out_video_stream = avformat_new_stream(out_format_ctx, out_video_codec);
    if (!out_video_stream) {
        qWarning() << "Error in avformat_new_stream for video";
        return false;
    }
    out_video_stream->id = 0;
    out_video_codec_ctx = out_video_stream->codec;
    out_video_codec_ctx->pix_fmt  = AV_PIX_FMT_YUV420P;
    out_video_codec_ctx->width = video_size.width();
    out_video_codec_ctx->height = video_size.height();
    out_video_codec_ctx->flags = AVFMT_FLAG_NOBUFFER | AVFMT_FLAG_FLUSH_PACKETS;
    //out_video_codec_ctx->gop_size = 3;
    //out_video_codec_ctx->max_b_frames = 2;
    out_video_codec_ctx->time_base.num = 1;
    out_video_codec_ctx->time_base.den = video_framerate;

    auto passLogfile = QDir(QStandardPaths::writableLocation(QStandardPaths::CacheLocation)).filePath(
                QString("passlogfile-%1.log").arg(Utils::randString())).toLatin1();
    av_dict_set(&options, "preset", "fast", 0);
    av_dict_set(&options, "tune", "zerolatency", 0);
    av_dict_set(&options, "passlogfile", passLogfile.constData(), 0);
    ret = avcodec_open2(out_video_codec_ctx, out_video_codec, &options);
    if (ret < 0) {
        qWarning() << "Error in avcodec_open2 for out_video_codec_ctx:" << av_make_error_string(errbuf, 50, ret);
        av_dict_free(&options);
        return false;
    }
    av_dict_free(&options);

    qDebug() << "Out video codec params:" << out_video_codec_ctx->codec_id;
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

    int nbytes = av_image_get_buffer_size(out_video_codec_ctx->pix_fmt,
                                          out_video_codec_ctx->width,
                                          out_video_codec_ctx->height, 32);

    video_outbuf = (uint8_t*)av_malloc(nbytes);
    if (!video_outbuf) {
        qWarning() << "Unable to allocate memory";
        return false;
    }

    in_frame_s = av_frame_alloc();
    if(!in_frame_s) {
        qWarning() << "Error in av_frame_alloc";
        return false;
    }

    if (av_image_fill_arrays(in_frame_s->data, in_frame_s->linesize,
                                 video_outbuf, out_video_codec_ctx->pix_fmt,
                                 out_video_codec_ctx->width,
                                 out_video_codec_ctx->height, 1) < 0) {
        qWarning() << "Error in av_image_fill_arrays";
        return false;
    }

    video_sws_ctx = sws_getContext(in_video_codec_ctx->width, in_video_codec_ctx->height,
                        in_video_codec_ctx->pix_fmt, out_video_codec_ctx->width,
                        out_video_codec_ctx->height, out_video_codec_ctx->pix_fmt,
                        SWS_BICUBIC, nullptr, nullptr, nullptr);
    if (!video_sws_ctx) {
        qWarning() << "Error in sws_getContext";
        return false;
    }

    const size_t outbuf_size = 500000;
    uint8_t *outbuf = (uint8_t*)av_malloc(outbuf_size);
    if (!outbuf) {
        qWarning() << "Unable to allocate memory";
        return false;
    }

    out_format_ctx->pb = avio_alloc_context(outbuf, outbuf_size, 1, nullptr, nullptr,
                                            write_packet_callback, nullptr);
    if (s->getScreenAudio()) {
        auto out_audio_codec = avcodec_find_encoder(AV_CODEC_ID_AAC);
        if (!out_audio_codec) {
            qWarning() << "Error in avcodec_find_encoder for AAC";
            return false;
        }

        AVStream *out_audio_stream = avformat_new_stream(out_format_ctx, out_audio_codec);
        if (!out_audio_stream) {
            qWarning() << "Error in avformat_new_stream for audio";
            return false;
        }
        out_audio_stream->id = 1;
        out_audio_codec_ctx = out_audio_stream->codec;
        out_audio_codec_ctx->channels = in_audio_codec_ctx->channels;
        out_audio_codec_ctx->sample_rate = in_audio_codec_ctx->sample_rate;
        out_audio_codec_ctx->sample_fmt = AV_SAMPLE_FMT_FLTP;
        out_audio_codec_ctx->flags = AVFMT_FLAG_NOBUFFER | AVFMT_FLAG_FLUSH_PACKETS;
        out_audio_codec_ctx->channel_layout = in_audio_codec_ctx->channel_layout;
        out_audio_codec_ctx->time_base.num = 1;
        out_audio_codec_ctx->time_base.den = in_audio_codec_ctx->sample_rate;

        av_dict_set(&options, "b", "128k", 0);
        av_dict_set(&options, "tune", "zerolatency", 0);
        ret = avcodec_open2(out_audio_codec_ctx, out_audio_codec, &options);
        if (ret < 0) {
            qWarning() << "Error in avcodec_open2 for out_audio_codec_ctx:" << av_make_error_string(errbuf, 50, ret);
            av_dict_free(&options);
            return false;
        }
        av_dict_free(&options);

        in_audio_codec_ctx->frame_size = out_audio_codec_ctx->frame_size;
        audio_frame_size = av_samples_get_buffer_size(nullptr,
                                              in_audio_codec_ctx->channels,
                                              out_audio_codec_ctx->frame_size,
                                              in_audio_codec_ctx->sample_fmt, 0);

        qDebug() << "Out audio codec params:" << out_audio_codec_ctx->codec_id;
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

        audio_swr_ctx = swr_alloc();
        av_opt_set_int(audio_swr_ctx, "in_channel_layout", in_audio_codec_ctx->channel_layout, 0);
        av_opt_set_int(audio_swr_ctx, "out_channel_layout", out_audio_codec_ctx->channel_layout,  0);
        av_opt_set_int(audio_swr_ctx, "in_sample_rate", in_audio_codec_ctx->sample_rate, 0);
        av_opt_set_int(audio_swr_ctx, "out_sample_rate", out_audio_codec_ctx->sample_rate, 0);
        av_opt_set_sample_fmt(audio_swr_ctx, "in_sample_fmt",  in_audio_codec_ctx->sample_fmt, 0);
        av_opt_set_sample_fmt(audio_swr_ctx, "out_sample_fmt", out_audio_codec_ctx->sample_fmt,  0);
        swr_init(audio_swr_ctx);
    }

    ret = avformat_write_header(out_format_ctx, nullptr);
    if (ret == AVSTREAM_INIT_IN_WRITE_HEADER) {
        qWarning() << "avformat_write_header returned AVSTREAM_INIT_IN_WRITE_HEADER";
    } else if (ret == AVSTREAM_INIT_IN_INIT_OUTPUT) {
        qWarning() << "avformat_write_header returned AVSTREAM_INIT_IN_INIT_OUTPUT";
    } else {
        qWarning() << "Error in avformat_write_header:"
                   << av_make_error_string(errbuf, 50, ret);
        return false;
    }

    av_init_packet(&out_pkt);
    return true;
}

void ScreenCaster::start()
{
#ifdef SAILFISH
    recorder = std::unique_ptr<Recorder>(new Recorder(this,1));
    //recorder->recordFrame();
    frameTimer.start();
#else
    emit readNextVideoFrame();
#endif
}

bool ScreenCaster::audioEnabled()
{
    return audio_frame_size > 0;
}

int ScreenCaster::write_packet_callback(void *opaque, uint8_t *buf, int buf_size)
{
    Q_UNUSED(opaque);
    //qDebug() << "write_packet_callback buff_size:" << buf_size;

    auto worker = ContentServerWorker::instance();
    worker->sendScreenCaptureData(static_cast<void*>(buf), buf_size);

    return buf_size;
}

bool ScreenCaster::writeAudioData(const QByteArray& data, uint64_t latency)
{
    //qDebug() << "=== ScreenCaster::writeAudioData ===";
    audio_outbuf.append(data);
    while (audio_outbuf.size() >= audio_frame_size) {
        const char* d = audio_outbuf.data();
        bool error = false;
        int ret = -1;

        if (av_new_packet(&in_pkt, audio_frame_size) < 0) {
            qDebug() << "Error in av_new_packet";
            error = true; break;
        } else {
            //char errbuf[50];
            in_pkt.dts = av_gettime() - latency;
            in_pkt.pts = in_pkt.dts;
            memcpy(in_pkt.data, d, audio_frame_size);
            ret = avcodec_send_packet(in_audio_codec_ctx, &in_pkt);
            //qDebug() << "avcodec_send_packet:" << av_make_error_string(errbuf, 50, ret);
            if (ret == 0) {
                ret = avcodec_receive_frame(in_audio_codec_ctx, in_frame);
                //qDebug() << "avcodec_receive_frame:" << av_make_error_string(errbuf, 50, ret);
                if (ret == 0 || ret == AVERROR(EAGAIN)) {
                    //qDebug() << "Audio frame decoded";
                    out_pkt.data = nullptr; out_pkt.size = 0;
                    // resampling
                    AVFrame* frame = av_frame_alloc();
                    frame->channel_layout = out_audio_codec_ctx->channel_layout;
                    frame->format = out_audio_codec_ctx->sample_fmt;
                    frame->sample_rate = out_audio_codec_ctx->sample_rate;
                    swr_convert_frame(audio_swr_ctx, frame, in_frame);
                    frame->pts = in_frame->pts;
                    ret = avcodec_send_frame(out_audio_codec_ctx, frame);
                    av_frame_free(&frame);
                    //qDebug() << "avcodec_send_frame:" << av_make_error_string(errbuf, 50, ret);
                    if (ret == 0 || ret == AVERROR(EAGAIN)) {
                        ret = avcodec_receive_packet(out_audio_codec_ctx, &out_pkt);
                        //qDebug() << "avcodec_receive_packet:" << av_make_error_string(errbuf, 50, ret);
                        if (ret == 0) {
                            //qDebug() << "Audio packet encoded";
                            auto in_tb  = AVRational{1, 1000000};
                            auto out_tb = out_format_ctx->streams[1]->time_base;
                            out_pkt.stream_index = 1; // audio output stream
                            out_pkt.pts = av_rescale_q_rnd(out_pkt.pts, in_tb, out_tb,
                               static_cast<AVRounding>(AV_ROUND_NEAR_INF|AV_ROUND_PASS_MINMAX));
                            out_pkt.dts = av_rescale_q_rnd(out_pkt.dts, in_tb, out_tb,
                               static_cast<AVRounding>(AV_ROUND_NEAR_INF|AV_ROUND_PASS_MINMAX));
                            out_pkt.duration = av_rescale_q(out_pkt.duration, in_tb, out_tb);
                            out_pkt.pos = -1;
                            //ret = av_write_frame(out_format_ctx, &out_pkt);
                            ret = av_interleaved_write_frame(out_format_ctx, &out_pkt);
                            //qDebug() << "av_write_frame:" << av_make_error_string(errbuf, 50, ret);
                            if (ret < 0) {
                                qWarning() << "Error in av_write_frame for audio";
                            }
                        } else if (ret == AVERROR(EAGAIN)) {
                            //qDebug() << "Packet not ready";
                        } else {
                            qWarning() << "Error in avcodec_receive_packet for audio";
                            error = true; break;
                        }
                    } else {
                        qWarning() << "Error in avcodec_send_frame for audio";
                        error = true; break;
                    }
                } else if (ret == AVERROR(EAGAIN)) {
                    //qDebug() << "Audio frame not ready";
                } else {
                    qWarning() << "Error in avcodec_receive_frame";
                    error = true; break;
                }
            } else {
                qWarning() << "Error in avcodec_receive_frame";
                error = true; break;
            }

            av_packet_unref(&in_pkt);
            av_packet_unref(&out_pkt);
        }

        audio_outbuf.remove(0, audio_frame_size);

        if (error) {
            qWarning() << "Error writeAudioFrame";
            return false;  break;
        }
    }

    return true;
}

#ifdef SAILFISH
void ScreenCaster::repaint()
{
    //qDebug() << "Repaint request";
    if (recorder)
        recorder->repaint();
    repaintTimer.start();
}

void ScreenCaster::writeVideoData()
{
    bool error = false;

    auto worker = ContentServerWorker::instance();
    auto& img = worker->displayStatus ? currImg : bgImg;
    auto size = img.byteCount();
    auto data = img.constBits();

    if (av_new_packet(&in_pkt, size) < 0) {
        qDebug() << "Error in av_new_packet";
        error = true;
    } else {
        //char errbuf[50];
        in_pkt.dts = av_gettime();
        in_pkt.pts = in_pkt.dts;
        memcpy(in_pkt.data, data, size);
        int ret = avcodec_send_packet(in_video_codec_ctx, &in_pkt);
        //qDebug() << "avcodec_send_packet:" << av_make_error_string(errbuf, 50, ret);
        if (ret == 0 || ret == AVERROR(EAGAIN)) {
            ret = avcodec_receive_frame(in_video_codec_ctx, in_frame);
            //qDebug() << "avcodec_receive_frame:" << av_make_error_string(errbuf, 50, ret);
            if (ret == 0) {
                //qDebug() << "Video frame decoded";
                sws_scale(video_sws_ctx, in_frame->data, in_frame->linesize, 0,
                          out_video_codec_ctx->height, in_frame_s->data,
                          in_frame_s->linesize);
                in_frame_s->width = in_frame->width;
                in_frame_s->height = in_frame->width;
                in_frame_s->format = in_frame->format;
                av_frame_copy_props(in_frame_s, in_frame);
                out_pkt.data = nullptr;
                out_pkt.size = 0;
                ret = avcodec_send_frame(out_video_codec_ctx, in_frame_s);
                //qDebug() << "avcodec_send_frame:" << av_make_error_string(errbuf, 50, ret);
                if (ret == 0 || ret == AVERROR(EAGAIN)) {
                    ret = avcodec_receive_packet(out_video_codec_ctx, &out_pkt);
                    //qDebug() << "avcodec_receive_packet:" << av_make_error_string(errbuf, 50, ret);
                    if (ret == 0) {
                        //qDebug() << "Video packet encoded";
                        auto in_tb  = AVRational{1, 1000000};
                        auto out_tb = out_format_ctx->streams[0]->time_base;
                        out_pkt.stream_index = 0; // video output stream
                        out_pkt.pts = av_rescale_q_rnd(out_pkt.pts, in_tb, out_tb,
                           static_cast<AVRounding>(AV_ROUND_NEAR_INF|AV_ROUND_PASS_MINMAX));
                        out_pkt.dts = av_rescale_q_rnd(out_pkt.dts, in_tb, out_tb,
                           static_cast<AVRounding>(AV_ROUND_NEAR_INF|AV_ROUND_PASS_MINMAX));
                        out_pkt.duration = av_rescale_q(out_pkt.duration, in_tb, out_tb);
                        out_pkt.pos = -1;
                        //ret = av_write_frame(out_format_ctx, &out_pkt);
                        ret = av_interleaved_write_frame(out_format_ctx, &out_pkt);
                        //qDebug() << "av_write_frame:" << av_make_error_string(errbuf, 50, ret);
                        if (ret < 0) {
                            qWarning() << "Error in av_write_frame for video";
                        }
                        frameTimer.start();
                    } else if (ret == AVERROR(EAGAIN)) {
                        //qDebug() << "Packet not ready";
                        frameTimer.start();
                    } else {
                        qWarning() << "Error in avcodec_receive_packet for video";
                        error = true;
                    }
                } else {
                    qWarning() << "Error in avcodec_send_frame for video";
                    error = true;
                }
            } else if (ret == AVERROR(EAGAIN)) {
                //qDebug() << "Video frame not ready";
                frameTimer.start();
            } else {
                qWarning() << "Error in avcodec_receive_frame for video";
                error = true;
            }
        } else {
            qWarning() << "Error in avcodec_send_packet for video";
            error = true;
        }
    }

    av_packet_unref(&in_pkt);
    av_packet_unref(&out_pkt);

    if (error) {
        qWarning() << "Error readVideoFrame";
        emit frameError();
    }
}

void ScreenCaster::saveCurrImg(QEvent *e)
{
    if (e->type() == FrameEvent::FrameEventType) {
        //qDebug() << "New screen image";
        auto fe = static_cast<FrameEvent*>(e);
        auto screenBuf = fe->buffer;
        currImg = fe->transform == LIPSTICK_RECORDER_TRANSFORM_Y_INVERTED ?
                    screenBuf->image.mirrored(false, true) : screenBuf->image;

        if (currImg.width() < currImg.height()) {
            auto orientation = QGuiApplication::primaryScreen()->orientation();
            if (orientation == Qt::LandscapeOrientation) {
                QTransform t; t.rotate(-90);
                currImg = currImg.transformed(t);
            } else if (orientation == Qt::InvertedLandscapeOrientation) {
                QTransform t; t.rotate(-270);
                currImg = currImg.transformed(t);
            } else {
                if (orientation == Qt::InvertedPortraitOrientation) {
                    QTransform t; t.rotate(-180);
                    currImg = currImg.transformed(t);
                }
                QImage target = bgImg;
                QPainter p(&target);
                p.setCompositionMode(QPainter::CompositionMode_SourceOver);
                currImg = currImg.scaledToHeight(target.height(), Qt::SmoothTransformation);
                p.drawImage((target.width()-currImg.width())/2, 0, currImg);
                p.end();
                currImg = target;
            }
        }

        if (currImg.format() != QImage::Format_RGB32) {
            currImg = currImg.convertToFormat(QImage::Format_RGB32);
        }

        screenBuf->busy = false;
        if (recorder && recorder->m_starving)
            recorder->recordFrame();

        repaintTimer.start();
    }
}

bool ScreenCaster::event(QEvent *e)
{
    saveCurrImg(e);
    return true;
}
#endif

#ifdef DESKTOP
void ScreenCaster::readVideoFrame()
{
    //qDebug() << "========= ScreenCaster::readVideoFrame";
    //char errbuf[50];
    bool error = false;
    if (av_read_frame(in_video_format_ctx, &in_pkt) >= 0) {
        if (in_pkt.stream_index == 0) { // video in stream
            int ret = avcodec_send_packet(in_video_codec_ctx, &in_pkt);
            //qDebug() << "avcodec_send_packet:" << av_make_error_string(errbuf, 50, ret);
            if (ret == 0 || ret == AVERROR(EAGAIN)) {
                ret = avcodec_receive_frame(in_video_codec_ctx, in_frame);
                //qDebug() << "avcodec_receive_frame:" << av_make_error_string(errbuf, 50, ret);
                if (ret == 0) {
                    //qDebug() << "Video frame decoded";
                    sws_scale(video_sws_ctx, in_frame->data, in_frame->linesize, 0,
                              out_video_codec_ctx->height, in_frame_s->data,
                              in_frame_s->linesize);
                    in_frame_s->width = in_frame->width;
                    in_frame_s->height = in_frame->width;
                    in_frame_s->format = in_frame->format;
                    av_frame_copy_props(in_frame_s, in_frame);
                    out_pkt.data = nullptr;
                    out_pkt.size = 0;
                    ret = avcodec_send_frame(out_video_codec_ctx, in_frame_s);
                    //qDebug() << "avcodec_send_frame:" << av_make_error_string(errbuf, 50, ret);
                    if (ret == 0 || ret == AVERROR(EAGAIN)) {
                        ret = avcodec_receive_packet(out_video_codec_ctx, &out_pkt);
                        //qDebug() << "avcodec_receive_packet:" << av_make_error_string(errbuf, 50, ret);
                        if (ret == 0) {
                            //qDebug() << "Video packet encoded";
                            auto in_video_stream  = in_video_format_ctx->streams[0];
                            auto out_video_stream = out_format_ctx->streams[0];
                            out_pkt.stream_index = 0; // video out stream
                            out_pkt.pts = av_rescale_q_rnd(out_pkt.pts,
                               in_video_stream->time_base,
                               out_video_stream->time_base,
                               static_cast<AVRounding>(AV_ROUND_NEAR_INF|AV_ROUND_PASS_MINMAX));
                            out_pkt.dts = av_rescale_q_rnd(out_pkt.dts,
                               in_video_stream->time_base,
                               out_video_stream->time_base,
                               static_cast<AVRounding>(AV_ROUND_NEAR_INF|AV_ROUND_PASS_MINMAX));
                            out_pkt.duration = av_rescale_q(out_pkt.duration,
                                                            in_video_stream->time_base,
                                                            out_video_stream->time_base);
                            out_pkt.pos = -1;
                            //ret = av_write_frame(out_format_ctx, &out_pkt);
                            ret = av_interleaved_write_frame(out_format_ctx, &out_pkt);
                            //qDebug() << "av_write_frame:" << av_make_error_string(errbuf, 50, ret);
                            if (ret < 0) {
                                qWarning() << "Error in av_write_frame for video";
                            }
                            emit readNextVideoFrame();
                        } else if (ret == AVERROR(EAGAIN)) {
                            //qDebug() << "Packet not ready";
                            emit readNextVideoFrame();
                        } else {
                            qWarning() << "Error in avcodec_receive_packet for video";
                            error = true;
                        }
                    } else {
                        qWarning() << "Error in avcodec_send_frame for video";
                        error = true;
                    }
                } else if (ret == AVERROR(EAGAIN)) {
                    //qDebug() << "Video frame not ready";
                    emit readNextVideoFrame();
                } else {
                    qWarning() << "Error in avcodec_receive_frame for video";
                    error = true;
                }
            } else {
                qWarning() << "Error in avcodec_receive_frame for video";
                error = true;
            }
        } else {
            qDebug() << "stream_index != video_index for video";
            error = true;
        }
    } else {
        qWarning() << "Error in av_read_frame for video";
        error = true;
    }

    av_packet_unref(&in_pkt);
    av_packet_unref(&out_pkt);

    if (error) {
        qWarning() << "Error readVideoFrame";
        emit frameError();
    }
}
#endif
