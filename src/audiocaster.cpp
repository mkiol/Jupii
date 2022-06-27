/* Copyright (C) 2019-2022 Michal Kosciesza <michal@mkiol.net>
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#include "audiocaster.h"

#include <QDebug>

extern "C" {
#include <libavdevice/avdevice.h>
#include <libavutil/dict.h>
#include <libavutil/imgutils.h>
#include <libavutil/mathematics.h>
#include <libavutil/time.h>
#include <libavutil/timestamp.h>
#include <pulse/sample.h>
}

#include "contentserver.h"
#include "contentserverworker.h"
#include "pulseaudiosource.h"
#include "settings.h"

AudioCaster::AudioCaster(QObject* parent) : QObject{parent} {
    connect(this, &AudioCaster::error, ContentServerWorker::instance(),
            &ContentServerWorker::audioErrorHandler);
}

AudioCaster::~AudioCaster() {
    if (out_format_ctx) {
        if (out_format_ctx->pb) {
            //            if (out_format_ctx->pb->buffer) {
            //                av_free(out_format_ctx->pb->buffer);
            //                out_format_ctx->pb->buffer = nullptr;
            //            }
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
    if (audio_swr_ctx) {
        swr_free(&audio_swr_ctx);
        audio_swr_ctx = nullptr;
    }
    if (in_audio_codec_ctx) {
        avcodec_free_context(&in_audio_codec_ctx);
        in_audio_codec_ctx = nullptr;
    }

    if (audio_out_pkt) {
        av_packet_unref(audio_out_pkt);
        audio_out_pkt = nullptr;
    }
}

bool AudioCaster::init() {
    qDebug() << "AudioCaster init";

    auto* in_audio_codec = avcodec_find_decoder(AV_CODEC_ID_PCM_S16LE);
    if (!in_audio_codec) {
        qWarning() << "error in avcodec_find_decoder for audio";
        return false;
    }

    in_frame = av_frame_alloc();
    if (!in_frame) {
        qWarning() << "error in av_frame_alloc";
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

    if (avcodec_open2(in_audio_codec_ctx, in_audio_codec, nullptr) < 0) {
        qWarning() << "error in avcodec_open2 for audio";
        return false;
    }

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

    // out

    if (avformat_alloc_output_context2(&out_format_ctx, nullptr, "mp3",
                                       nullptr) < 0) {
        qWarning() << "error in avformat_alloc_output_context2";
        return false;
    }

    const size_t outbuf_size = 500000;
    const auto outbuf = new uint8_t[outbuf_size];
    if (!outbuf) {
        qWarning() << "unable to allocate memory";
        return false;
    }

    out_format_ctx->pb =
        avio_alloc_context(outbuf, outbuf_size, 1, nullptr, nullptr,
                           write_packet_callback, nullptr);

    auto* out_audio_codec = avcodec_find_encoder(AV_CODEC_ID_MP3);
    if (!out_audio_codec) {
        qWarning() << "error in avcodec_find_encoder for MP3";
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
    out_audio_codec_ctx->channel_layout = in_audio_codec_ctx->channel_layout;
    out_audio_codec_ctx->time_base.num = 1;
    out_audio_codec_ctx->flags = AVFMT_FLAG_NOBUFFER | AVFMT_FLAG_FLUSH_PACKETS;
    out_audio_codec_ctx->thread_count = 1;
    out_audio_codec_ctx->time_base.den = in_audio_codec_ctx->sample_rate;

    auto* out_audio_stream = avformat_new_stream(out_format_ctx, nullptr);
    if (!out_audio_stream) {
        qWarning() << "error in avformat_new_stream for audio";
        return false;
    }
    out_audio_stream->id = 0;
    if (avcodec_parameters_from_context(out_audio_stream->codecpar,
                                        out_audio_codec_ctx) < 0) {
        qWarning() << "error in avcodec_parameters_from_context";
        return false;
    }

    AVDictionary* options = nullptr;
    av_dict_set(&options, "b", "128k", 0);
    av_dict_set(&options, "q", "9", 0);
    if (avcodec_open2(out_audio_codec_ctx, out_audio_codec, &options) < 0) {
        qWarning() << "error in avcodec_open2 for audio";
        av_dict_free(&options);
        return false;
    }
    av_dict_free(&options);

    in_audio_codec_ctx->frame_size = out_audio_codec_ctx->frame_size;
    audio_frame_size = uint64_t(av_samples_get_buffer_size(
        nullptr, in_audio_codec_ctx->channels, out_audio_codec_ctx->frame_size,
        in_audio_codec_ctx->sample_fmt, 0));
    audio_pkt_duration =
        out_audio_codec_ctx
            ->frame_size;  // time_base is 1/rate, so duration of 1 sample is 1

#ifdef QT_DEBUG
    qDebug() << "out audio codec params:" << out_audio_codec_ctx->codec_id;
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
#endif

    audio_swr_ctx = swr_alloc();
    av_opt_set_int(audio_swr_ctx, "in_channel_layout",
                   int64_t(in_audio_codec_ctx->channel_layout), 0);
    av_opt_set_int(audio_swr_ctx, "out_channel_layout",
                   int64_t(out_audio_codec_ctx->channel_layout), 0);
    av_opt_set_int(audio_swr_ctx, "in_sample_rate",
                   in_audio_codec_ctx->sample_rate, 0);
    av_opt_set_int(audio_swr_ctx, "out_sample_rate",
                   out_audio_codec_ctx->sample_rate, 0);
    av_opt_set_sample_fmt(audio_swr_ctx, "in_sample_fmt",
                          in_audio_codec_ctx->sample_fmt, 0);
    av_opt_set_sample_fmt(audio_swr_ctx, "out_sample_fmt",
                          out_audio_codec_ctx->sample_fmt, 0);
    swr_init(audio_swr_ctx);

    if (int ret = avformat_write_header(out_format_ctx, nullptr);
        ret != AVSTREAM_INIT_IN_WRITE_HEADER &&
        ret != AVSTREAM_INIT_IN_INIT_OUTPUT) {
        char errbuf[50];
        qWarning() << "error in avformat_write_header:"
                   << av_make_error_string(errbuf, 50, ret);
        return false;
    }

    return true;
}

int AudioCaster::write_packet_callback(void*, uint8_t* buf, int buf_size) {
    ContentServerWorker::instance()->sendAudioCaptureData(
        static_cast<void*>(buf), buf_size);
    return buf_size;
}

void AudioCaster::errorHandler() { emit error(); }

void AudioCaster::writeAudioData(const QByteArray& data) {
    audio_outbuf.append(data);

    if (!writeAudioData2()) {
        errorHandler();
    }
}

bool AudioCaster::writeAudioData2() {
    while (uint64_t(audio_outbuf.size()) >= audio_frame_size) {
        const char* d = audio_outbuf.data();
        int ret = -1;

        auto* audio_in_pkt = av_packet_alloc();
        if (!audio_in_pkt) {
            qDebug() << "audio_in_pkt is null";
            return false;
        }

        if (av_new_packet(audio_in_pkt,
                          static_cast<int32_t>(audio_frame_size)) < 0) {
            qWarning() << "error in av_new_packet";
            return false;
        }

        audio_in_pkt->dts = audio_pkt_time;
        audio_in_pkt->pts = audio_pkt_time;
        audio_in_pkt->duration = audio_pkt_duration;

        memcpy(audio_in_pkt->data, d, audio_frame_size);

        ret = avcodec_send_packet(in_audio_codec_ctx, audio_in_pkt);
        if (ret == 0) {
            ret = avcodec_receive_frame(in_audio_codec_ctx, in_frame);
            if (ret == 0 || ret == AVERROR(EAGAIN)) {
                // qDebug() << "audio frame decoded";
                //  resampling
                AVFrame* frame = av_frame_alloc();
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
                        if (!writeAudioData3()) return false;
                    } else if (ret == AVERROR(EAGAIN)) {
                        // qDebug() << "packet not ready";
                    } else {
                        qWarning()
                            << "error in avcodec_receive_packet for audio";
                        return false;
                    }
                } else {
                    qWarning() << "error in avcodec_send_frame";
                    return false;
                }
            } else if (ret == AVERROR(EAGAIN)) {
                // qDebug() << "audio frame not ready";
            } else {
                qWarning() << "error in avcodec_receive_frame";
                return false;
            }
        } else {
            qWarning() << "error in avcodec_send_packet";
            return false;
        }

        audio_outbuf.remove(0, int32_t(audio_frame_size));
        audio_pkt_time += audio_pkt_duration;

        av_packet_unref(audio_in_pkt);
    }

    return true;
}

bool AudioCaster::writeAudioData3() {
    audio_out_pkt->stream_index = 0;  // audio output stream
    audio_out_pkt->pos = -1;
    audio_out_pkt->pts = audio_pkt_time;
    audio_out_pkt->dts = audio_pkt_time;

    int ret = av_interleaved_write_frame(out_format_ctx, audio_out_pkt);
    if (ret < 0) {
        qWarning() << "error in av_write_frame";
        return false;
    }

    return true;
}
