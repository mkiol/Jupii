/* Copyright (C) 2019-2022 Michal Kosciesza <michal@mkiol.net>
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#ifndef MICCASTER_H
#define MICCASTER_H

#include <QAudioInput>
#include <QByteArray>
#include <QIODevice>
#include <QObject>
#include <memory>

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libswresample/swresample.h>
#include <libswscale/swscale.h>
}

class MicCaster : public QIODevice {
    Q_OBJECT
   public:
    const static int sampleRate = 22050;
    const static int channelCount = 1;
    const static int sampleSize = 16;

    MicCaster(QObject* parent = nullptr);
    ~MicCaster();
    bool init();
    void start();
    void writeAudioData(const QByteArray& data);

   signals:
    void error();

   protected:
    qint64 readData(char* data, qint64 maxSize) override;
    qint64 writeData(const char* data, qint64 maxSize) override;

   private:
    std::unique_ptr<QAudioInput> input;
    bool active = false;
    float volume = 1.0F;
    AVPacket* audio_out_pkt = nullptr;
    AVFormatContext* out_format_ctx = nullptr;
    AVFrame* in_frame = nullptr;
    static int write_packet_callback(void* opaque, uint8_t* buf, int buf_size);
    int audio_frame_size = 0;
    AVCodecContext* in_audio_codec_ctx = nullptr;
    AVCodecContext* out_audio_codec_ctx = nullptr;
    SwrContext* audio_swr_ctx = nullptr;
    QByteArray audio_outbuf;  // audio data buf
    int64_t audio_pkt_time = 0;
    int64_t audio_pkt_duration = 0;
    bool writeAudioData2();
    bool writeAudioData3();
    void errorHandler();
};

#endif  // MICCASTER_H
