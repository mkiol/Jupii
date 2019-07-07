/* Copyright (C) 2019 Michal Kosciesza <michal@mkiol.net>
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#include "miccaster.h"

#include <QDebug>

extern "C" {
#include <libavutil/dict.h>
#include <libavutil/mathematics.h>
#include <libavdevice/avdevice.h>
#include <libavutil/imgutils.h>
#include <libavutil/timestamp.h>
#include <libavutil/time.h>
}

#include "contentserver.h"
#include "settings.h"

MicCaster::MicCaster(QObject *parent) : QIODevice(parent)
{
}

MicCaster::~MicCaster()
{
    qDebug() << "Stopping mic";
    close();
    input->stop();

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
    if (audio_swr_ctx) {
        swr_free(&audio_swr_ctx);
        audio_swr_ctx = nullptr;
    }
    if (in_audio_codec_ctx) {
        avcodec_free_context(&in_audio_codec_ctx);
        in_audio_codec_ctx = nullptr;
    }
}

bool MicCaster::init()
{
    qDebug() << "MicCaster init";

    volume = Settings::instance()->getMicVolume();

    auto in_audio_codec = avcodec_find_decoder(AV_CODEC_ID_PCM_S16BE);
    if (!in_audio_codec) {
        qWarning() << "Error in avcodec_find_decoder for audio";
        return false;
    }

    in_frame = av_frame_alloc();
    if(!in_frame) {
        qWarning() << "Error in av_frame_alloc";
        return false;
    }

    av_init_packet(&in_pkt);

    in_audio_codec_ctx = avcodec_alloc_context3(in_audio_codec);
    if (!in_audio_codec_ctx) {
        qWarning() << "Error: avcodec_alloc_context3 is null";
        return false;
    }

    in_audio_codec_ctx->channels = MicCaster::channelCount;
    in_audio_codec_ctx->channel_layout = av_get_default_channel_layout(in_audio_codec_ctx->channels);
    in_audio_codec_ctx->sample_rate = MicCaster::sampleRate;
    in_audio_codec_ctx->sample_fmt = AV_SAMPLE_FMT_S16;
    in_audio_codec_ctx->time_base.num = 1;
    in_audio_codec_ctx->time_base.den = in_audio_codec_ctx->sample_rate;

    if (avcodec_open2(in_audio_codec_ctx, in_audio_codec, nullptr) < 0) {
        qWarning() << "Error in avcodec_open2 for audio";
        return false;
    }

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

    // out

    // modes:
    // 0 - MP3 16-bit 44100 Hz stereo 128 kbps (default)
    // 1 - MPEG TS MP3 16-bit 44100 Hz stereo 128 kbps
    auto mode = Settings::instance()->getAudioCaptureMode();
    if (avformat_alloc_output_context2(&out_format_ctx, nullptr,
                                       mode > 0 ? "mpegts" : "mp3", nullptr) < 0) {
        qWarning() << "Error in avformat_alloc_output_context2";
        return false;
    }

    const size_t outbuf_size = 500000;
    uint8_t *outbuf = static_cast<uint8_t*>(av_malloc(outbuf_size));
    if (!outbuf) {
        qWarning() << "Unable to allocate memory";
        return false;
    }

    out_format_ctx->pb = avio_alloc_context(outbuf, outbuf_size, 1, nullptr, nullptr,
                                            write_packet_callback, nullptr);

    auto out_audio_codec = avcodec_find_encoder(AV_CODEC_ID_MP3);
    if (!out_audio_codec) {
        qWarning() << "Error in avcodec_find_encoder for MP3";
        return false;
    }

    AVStream *out_audio_stream = avformat_new_stream(out_format_ctx, out_audio_codec);
    if (!out_audio_stream) {
        qWarning() << "Error in avformat_new_stream for audio";
        return false;
    }
    out_audio_stream->id = 0;
    out_audio_codec_ctx = out_audio_stream->codec;
    out_audio_codec_ctx->channels = in_audio_codec_ctx->channels;
    out_audio_codec_ctx->sample_rate = in_audio_codec_ctx->sample_rate;
    out_audio_codec_ctx->sample_fmt = AV_SAMPLE_FMT_FLTP;
    out_audio_codec_ctx->channel_layout = in_audio_codec_ctx->channel_layout;
    out_audio_codec_ctx->time_base.num = 1;
    out_audio_codec_ctx->flags = AVFMT_FLAG_NOBUFFER | AVFMT_FLAG_FLUSH_PACKETS;
    out_audio_codec_ctx->thread_count = 1;
    out_audio_codec_ctx->time_base.den = in_audio_codec_ctx->sample_rate;

    AVDictionary* options = nullptr;
    av_dict_set(&options, "b", "128k", 0);
    av_dict_set(&options, "tune", "zerolatency", 0);
    if (avcodec_open2(out_audio_codec_ctx, out_audio_codec, &options) < 0) {
        qWarning() << "Error in avcodec_open2 for audio";
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

    int ret = avformat_write_header(out_format_ctx, nullptr);
    if (ret == AVSTREAM_INIT_IN_WRITE_HEADER) {
        qWarning() << "avformat_write_header returned AVSTREAM_INIT_IN_WRITE_HEADER";
    } else if (ret == AVSTREAM_INIT_IN_INIT_OUTPUT) {
        qWarning() << "avformat_write_header returned AVSTREAM_INIT_IN_INIT_OUTPUT";
    } else {
        char errbuf[50];
        qWarning() << "Error in avformat_write_header:"
                   << av_make_error_string(errbuf, 50, ret);
        return false;
    }

    av_init_packet(&out_pkt);
    return true;
}

void MicCaster::start()
{
    qDebug() << "Starting mic";

    QAudioFormat format;
    format.setSampleRate(sampleRate);
    format.setChannelCount(channelCount);
    format.setSampleSize(sampleSize);
    format.setCodec("audio/pcm");
    //format.setByteOrder(QAudioFormat::LittleEndian);
    format.setByteOrder(QAudioFormat::BigEndian);
    format.setSampleType(QAudioFormat::SignedInt);

    auto dev = QAudioDeviceInfo::defaultInputDevice();

    /*auto devName = QAudioDeviceInfo::defaultOutputDevice().deviceName() + ".monitor";
    auto devName = QString("sink.deep_buffer.monitor");
    auto devs = QAudioDeviceInfo::availableDevices(QAudio::AudioInput);
    for (auto& d : devs) {
        qDebug() << "input dev:" << d.deviceName();
        if (d.deviceName() == devName) {
            dev = d;
            qDebug() << "Got dev:" << dev.deviceName();
        }
    }*/

    qDebug() << "Available input devs:";
    auto idevs = QAudioDeviceInfo::availableDevices(QAudio::AudioInput);
    for (auto& d : idevs) {
        qDebug() << "  " << d.deviceName();
    }
    qDebug() << "Available output devs:";
    auto odevs = QAudioDeviceInfo::availableDevices(QAudio::AudioOutput);
    for (auto& d : odevs) {
        qDebug() << "  " << d.deviceName();
    }

    if (!dev.isFormatSupported(format)) {
        qWarning() << "Default audio format not supported, trying to use the nearest";
        format = dev.nearestFormat(format);
        qDebug() << "Nerest format:";
        qDebug() << " codec:" << format.codec();
        qDebug() << " sampleSize:" << format.sampleSize();
        qDebug() << " sampleRate:" << format.sampleRate();
        qDebug() << " channelCount:" << format.channelCount();
    }

    qDebug() << "Input dev:" << dev.deviceName();

    input = std::unique_ptr<QAudioInput>(new QAudioInput(dev, format));
    open(QIODevice::WriteOnly);
    input->start(this);
}


int MicCaster::write_packet_callback(void *opaque, uint8_t *buf, int buf_size)
{
    Q_UNUSED(opaque);
    auto worker = ContentServerWorker::instance();
    worker->sendMicData(static_cast<void*>(buf), buf_size);
    return buf_size;
}

bool MicCaster::writeAudioData(const QByteArray& data)
{
    if (volume != 1.0f) {
        QByteArray d(data);
        ContentServerWorker::adjustVolume(&d, volume, false);
        audio_outbuf.append(d);
    } else {
        audio_outbuf.append(data);
    }
    while (audio_outbuf.size() >= audio_frame_size) {
        char* d = audio_outbuf.data();
        bool error = false;
        int ret = -1;
        if (av_new_packet(&in_pkt, audio_frame_size) < 0) {
            qDebug() << "Error in av_new_packet";
            error = true; break;
        } else {
            //char errbuf[50];
            in_pkt.dts = av_gettime();
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
                            auto out_tb = out_format_ctx->streams[0]->time_base;
                            out_pkt.stream_index = 0; // audio output stream
                            out_pkt.pts = av_rescale_q_rnd(out_pkt.pts, in_tb, out_tb,
                               static_cast<AVRounding>(AV_ROUND_NEAR_INF|AV_ROUND_PASS_MINMAX));
                            out_pkt.dts = av_rescale_q_rnd(out_pkt.dts, in_tb, out_tb,
                               static_cast<AVRounding>(AV_ROUND_NEAR_INF|AV_ROUND_PASS_MINMAX));
                            out_pkt.duration = av_rescale_q(out_pkt.duration, in_tb, out_tb);
                            out_pkt.pos = -1;
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
            return false;
        }
    }

    return true;
}

qint64 MicCaster::readData(char* data, qint64 maxSize)
{
    Q_UNUSED(data)
    Q_UNUSED(maxSize)
    return 0;
}

qint64 MicCaster::writeData(const char* data, qint64 maxSize)
{
    writeAudioData(QByteArray::fromRawData(data, maxSize));
    return maxSize;
}
