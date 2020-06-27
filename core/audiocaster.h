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
    void writeAudioData(const QByteArray& data);

signals:
    void error();

private:
    AVPacket audio_out_pkt;
    AVCodecContext* out_video_codec_ctx = nullptr;
    AVFormatContext* out_format_ctx = nullptr;
    AVFrame* in_frame = nullptr;
    static int write_packet_callback(void *opaque, uint8_t *buf, int buf_size);
    uint64_t audio_frame_size = 0;
    AVCodecContext* in_audio_codec_ctx = nullptr;
    AVCodecContext* out_audio_codec_ctx = nullptr;
    SwrContext* audio_swr_ctx = nullptr;
    QByteArray audio_outbuf; // pulse audio data buf
    int64_t audio_pkt_time = 0;
    int64_t audio_pkt_duration = 0;
    bool writeAudioData2();
    bool writeAudioData3();
    void errorHandler();
};

#endif // AUDIOCASTER_H
