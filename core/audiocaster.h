/* Copyright (C) 2019 Michal Kosciesza <michal@mkiol.net>
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#ifndef AUDIOCASTER_H
#define AUDIOCASTER_H

#include <QObject>
#include <QByteArray>

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libswscale/swscale.h>
#include <libswresample/swresample.h>
}

class AudioCaster : public QObject
{
    Q_OBJECT
public:
    AudioCaster(QObject *parent = nullptr);
    ~AudioCaster();
    bool init();
    bool writeAudioData(const QByteArray& data, uint64_t latency = 0);

signals:
    void frameError();

private:
    AVPacket in_pkt;
    AVPacket out_pkt;
    AVCodecContext* out_video_codec_ctx = nullptr;
    AVFormatContext* out_format_ctx = nullptr;
    AVFrame* in_frame = nullptr;
    static int write_packet_callback(void *opaque, uint8_t *buf, int buf_size);
    int audio_frame_size = 0;
    AVCodecContext* in_audio_codec_ctx = nullptr;
    AVCodecContext* out_audio_codec_ctx = nullptr;
    SwrContext* audio_swr_ctx = nullptr;
    QByteArray audio_outbuf; // pulse audio data buf
};

#endif // AUDIOCASTER_H
