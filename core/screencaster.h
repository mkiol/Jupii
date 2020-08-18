/* Copyright (C) 2019 Michal Kosciesza <michal@mkiol.net>
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#ifndef SCREENCASTER_H
#define SCREENCASTER_H

#include <QObject>
#include <QByteArray>
#include <QTimer>
#include <QImage>
#include <QEvent>
#include <QSize>
#include <memory>

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libswscale/swscale.h>
#include <libswresample/swresample.h>
}

#ifdef SAILFISH
#include "recorder.h"
#endif

class ScreenCaster : public QObject
{
    Q_OBJECT
public:
    ScreenCaster(QObject *parent = nullptr);
    ~ScreenCaster();
    bool init();
    void start();
    bool audioEnabled();
    void writeAudioData(const QByteArray& data);

signals:
#ifdef DESKTOP
    void readNextVideoData();
#endif
    void error();

private slots:
    void writeVideoData();
#ifdef SAILFISH
    void repaint();
#endif

private:
    enum Encoder {
        ENC_UNKNOWN,
        ENC_H264_X264,
        ENC_H264_X264_RGB,
        ENC_H264_NVENC,
        ENC_H264_OMX
    };
#ifdef SAILFISH
    QTimer frameTimer;
    QTimer repaintTimer;
    std::unique_ptr<Recorder> recorder;
    QImage bgImg;
    Buffer* currImgBuff = nullptr;
    int currImgTransform = 0;
    int64_t currImgTimestamp = 0;
    QImage makeCurrImg();
    bool event(QEvent *e);
#endif
    bool havePrevVideoPkt = false;
    AVPacket video_out_pkt;
    AVPacket audio_out_pkt;
    AVFormatContext* in_video_format_ctx = nullptr;
    AVCodecContext* in_video_codec_ctx = nullptr;
    AVCodecContext* out_video_codec_ctx = nullptr;
    AVFormatContext* out_format_ctx = nullptr;
    AVFrame* in_frame = nullptr;
    AVFrame* in_frame_s = nullptr;
    SwsContext* video_sws_ctx = nullptr;
    uint8_t* video_outbuf = nullptr;
    uint64_t audio_frame_size = 0; // 0 => audio disabled for screen casting
    int video_framerate = 0;
    QSize video_size;
    int xoff;
    int yoff;
    int trans_type;
    int quality;
    int res_div = 1;
    QString x11_dpy;
    Qt::TransformationMode trans_mode = Qt::FastTransformation;
    int64_t video_pkt_time = 0;
    int64_t video_pkt_start_time = 0;
    int64_t video_pkt_duration = 0;
    int64_t audio_pkt_time = 0;
    int64_t audio_pkt_duration = 0;
    int video_audio_ratio = 0;
    AVCodecContext* in_audio_codec_ctx = nullptr;
    AVCodecContext* out_audio_codec_ctx = nullptr;
    SwrContext* audio_swr_ctx = nullptr;
    QByteArray audio_outbuf; // pulse audio data buf
    int skipped_frames_max = 0;
    int skipped_frames = 0;
    Encoder encoder;

    //static int read_packet_callback(void *opaque, uint8_t *buf, int buf_size);
    static int write_packet_callback(void *opaque, uint8_t *buf, int buf_size);
    void tuneQuality();
    void initVideoSize();
    bool initVideoEncoder(const char *name);
    bool initVideoEncoder();
    bool writeAudioData2();
    bool writeAudioData3();
    bool writeVideoData2();
    void errorHandler();
    void setPixFormats();
    bool initInAudio();
    bool initOutAudio();
    bool initScaler();
    bool scalingNeeded();
};

#endif // SCREENCASTER_H
