/* Copyright (C) 2019 Michal Kosciesza <michal@mkiol.net>
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#ifndef MICCASTER_H
#define MICCASTER_H

#include <QObject>
#include <QByteArray>
#include <QIODevice>
#include <QAudioInput>
#include <memory>

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libswscale/swscale.h>
#include <libswresample/swresample.h>
}

class MicCaster : public QIODevice
{
    Q_OBJECT
public:
    const static int sampleRate = 22050;
    const static int channelCount = 1;
    const static int sampleSize = 16;

    MicCaster(QObject *parent = nullptr);
    ~MicCaster();
    bool init();
    void start();
    bool writeAudioData(const QByteArray& data);

protected:
    qint64 readData(char *data, qint64 maxSize);
    qint64 writeData(const char *data, qint64 maxSize);

private:
    std::unique_ptr<QAudioInput> input;
    bool active = false;
    int audio_frame_size = 0;
    float volume = 1.0f;
    AVPacket in_pkt;
    AVPacket out_pkt;
    AVFormatContext* out_format_ctx = nullptr;
    AVFrame* in_frame = nullptr;
    AVCodecContext* in_audio_codec_ctx = nullptr;
    AVCodecContext* out_audio_codec_ctx = nullptr;
    SwrContext* audio_swr_ctx = nullptr;
    QByteArray audio_outbuf; // audio data buf
    static int write_packet_callback(void *opaque, uint8_t *buf, int buf_size);
};

#endif // MICCASTER_H
