/* Copyright (C) 2022 Michal Kosciesza <michal@mkiol.net>
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#include "transcoder.h"

#include <QDebug>
#include <QFile>
#include <QFileInfo>

extern "C" {
#include "libavcodec/avcodec.h"
#include "libavdevice/avdevice.h"
#include "libavformat/avformat.h"
#include "libavutil/dict.h"
#include "libavutil/imgutils.h"
#include "libavutil/log.h"
#include "libavutil/mathematics.h"
#include "libavutil/time.h"
#include "libavutil/timestamp.h"
#include "libswresample/swresample.h"
#include "libswscale/swscale.h"
}

bool Transcoder::fillAvData(const AVCodecParameters *codec,
                            const QString &videoFile, AvData *data) {
    switch (codec->codec_id) {
        case AV_CODEC_ID_MP2:
        case AV_CODEC_ID_MP3:
            data->mime = QStringLiteral("audio/mpeg");
            data->type = QStringLiteral("mp3");
            data->extension = QStringLiteral("mp3");
            break;
        case AV_CODEC_ID_VORBIS:
            data->mime = QStringLiteral("audio/ogg");
            data->type = QStringLiteral("oga");
            data->extension = QStringLiteral("oga");
            break;
        case AV_CODEC_ID_AAC:
        case AV_CODEC_ID_AAC_LATM:
            data->mime = QStringLiteral("audio/mp4");
            data->type = QStringLiteral("mp4");
            data->extension = QStringLiteral("m4a");
            break;
        default:
            qWarning() << "not supported codec:" << codec->codec_id;
            return false;
    }

    data->path = videoFile + ".audio-extracted." + data->extension;
    data->bitrate = codec->bit_rate;
    data->channels = codec->channels;

    return true;
}

template <typename Input, typename Output>
static bool copyAvMetadata(const Input ic, Output oc) {
    static_assert((std::is_same<Input, AVStream *>() &&
                   std::is_same<Output, AVStream *>()) ||
                      (std::is_same<Input, AVFormatContext *>() &&
                       std::is_same<Output, AVFormatContext *>()),
                  "Invalid type");
    if (ic->metadata) {
        AVDictionaryEntry *tag = nullptr;
        qDebug() << "stream metadata found:";
        while ((tag = av_dict_get(ic->metadata, "", tag, AV_DICT_IGNORE_SUFFIX))) {
            qDebug() << "  " << tag->key << "=" << tag->value;
        }

        if (av_dict_copy(&oc->metadata, ic->metadata, 0) < 0) {
            qWarning() << "oc->metadata av_dict_copy error";
            return false;
        }
    }
    return true;
}

std::optional<AvData> Transcoder::extractAudio(const QString &file) {
    const auto f = file.toUtf8();
    auto *cfile = f.constData();

    qDebug() << "extracting audio from file:" << cfile;

    AVFormatContext *ic = nullptr;
    if (avformat_open_input(&ic, cfile, nullptr, nullptr) < 0) {
        qWarning() << "avformat_open_input error";
        return std::nullopt;
    }

    if ((avformat_find_stream_info(ic, nullptr)) < 0) {
        qWarning() << "could not find stream info";
        avformat_close_input(&ic);
        return std::nullopt;
    }

    auto aidx = av_find_best_stream(ic, AVMEDIA_TYPE_AUDIO, -1, -1, nullptr, 0);
    if (aidx < 0) {
        qWarning() << "no audio stream found";
        avformat_close_input(&ic);
        return std::nullopt;
    }

#ifdef QT_DEBUG
    qDebug() << "nb_streams:" << ic->nb_streams;
    qDebug() << "audio stream index is:" << aidx;
    qDebug() << "audio stream nb_side_data:" << ic->streams[aidx]->nb_side_data;
    for (int i = 0; i < ic->streams[aidx]->nb_side_data; ++i) {
        qDebug() << "-- audio stream side data --";
        qDebug() << "type:" << ic->streams[aidx]->side_data[i].type;
        qDebug() << "size:" << ic->streams[aidx]->side_data[i].size;
        QByteArray data{reinterpret_cast<const char *>(
                            ic->streams[aidx]->side_data[i].data),
                        ic->streams[aidx]->side_data[i].size};
        qDebug() << "data:" << data;
    }
    qDebug() << "Audio codec:";
    qDebug() << "codec_id:" << ic->streams[aidx]->codecpar->codec_id;
    qDebug() << "codec_channels:" << ic->streams[aidx]->codecpar->channels;
    qDebug() << "codec_tag:" << ic->streams[aidx]->codecpar->codec_tag;
#endif

    AvData avData;
    if (!fillAvData(ic->streams[aidx]->codecpar, file, &avData)) {
        qWarning() << "unable to find correct mime for the codec:"
                   << ic->streams[aidx]->codecpar->codec_id;
        avformat_close_input(&ic);
        return std::nullopt;
    }

    qDebug() << "extracted audio stream:";
    qDebug() << "  content type:" << avData.mime;
    qDebug() << "  bitrate:" << avData.bitrate;
    qDebug() << "  channels:" << avData.channels;

    auto t = avData.type.toLatin1();
    auto *of = av_guess_format(t.constData(), nullptr, nullptr);
    if (!of) {
        qWarning() << "av_guess_format error";
        avformat_close_input(&ic);
        return std::nullopt;
    }

    auto *oc = avformat_alloc_context();
    if (!oc) {
        qWarning() << "avformat_alloc_context error";
        avformat_close_input(&ic);
        return std::nullopt;
    }

    if (!copyAvMetadata(ic, oc)) {
        avformat_close_input(&ic);
        avformat_free_context(oc);
        return std::nullopt;
    }

    oc->oformat = of;

    auto *in_codec =
        avcodec_find_decoder(ic->streams[aidx]->codecpar->codec_id);
    if (!in_codec) {
        qWarning() << "avcodec_find_decoder error";
        avformat_close_input(&ic);
        avformat_free_context(oc);
        return std::nullopt;
    }

    auto *out_codec = in_codec;  // no transcoding

    auto *ast = avformat_new_stream(oc, out_codec);
    if (!ast) {
        qWarning() << "avformat_new_stream error";
        avformat_close_input(&ic);
        avformat_free_context(oc);
        return std::nullopt;
    }

    ast->id = 0;

    if (!copyAvMetadata(ic->streams[aidx], ast)) {
        avformat_close_input(&ic);
        avformat_free_context(oc);
        return std::nullopt;
    }

    if (avcodec_parameters_copy(ast->codecpar, ic->streams[aidx]->codecpar) <
        0) {
        qWarning() << "avcodec_parameters_copy error";
        avformat_close_input(&ic);
        avformat_free_context(oc);
        return std::nullopt;
    }

    ast->codecpar->codec_tag = av_codec_get_tag(
        oc->oformat->codec_tag, ic->streams[aidx]->codecpar->codec_id);

    if (oc->oformat->flags & AVFMT_GLOBALHEADER)
        oc->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;

    qDebug() << "extracted audio file:" << avData.path;

    QFile audioFile{avData.path};
    if (audioFile.exists()) {
        qDebug() << "extracted audio stream already exists";
        avData.size = QFileInfo{avData.path}.size();
        avformat_close_input(&ic);
        avformat_free_context(oc);
        return avData;
    }

    auto bapath = avData.path.toUtf8();
    if (avio_open(&oc->pb, bapath.data(), AVIO_FLAG_WRITE) < 0) {
        qWarning() << "avio_open error";
        avformat_close_input(&ic);
        avformat_free_context(oc);
        return std::nullopt;
    }

    if (avformat_write_header(oc, nullptr) < 0) {
        qWarning() << "avformat_write_header error";
        avformat_close_input(&ic);
        avio_close(oc->pb);
        avformat_free_context(oc);
        audioFile.remove();
        return std::nullopt;
    }

    AVPacket pkt{};
    av_init_packet(&pkt);

    while (!av_read_frame(ic, &pkt)) {
        if (pkt.stream_index == aidx) {
#ifdef QT_DEBUG
            for (int i = 0; i < pkt.side_data_elems; ++i) {
                qDebug() << "audio stream packet side data:";
                qDebug() << "  type:" << pkt.side_data[i].type;
                qDebug() << "  size:" << pkt.side_data[i].size;
                QByteArray data{
                    reinterpret_cast<const char *>(pkt.side_data[i].data),
                    pkt.side_data[i].size};
                qDebug() << "  data:" << data;
            }
#endif

            /*qDebug() << "------ orig -----";
            qDebug() << "duration:" << pkt.duration;
            qDebug() << "dts:" << pkt.dts;
            qDebug() << "pts:" << pkt.pts;
            qDebug() << "pos:" << pkt.pos;

            qDebug() << "------ time base -----";
            qDebug() << "ast->codec->time_base:" <<
            ast->codec->time_base.num << ast->codec->time_base.den; qDebug()
            << "ast->time_base:" << ast->time_base.num <<
            ast->time_base.den; qDebug() <<
                "ic->streams[aidx]->codec->time_base:" <<
                ic->streams[aidx]->codec->time_base.num <<
                ic->streams[aidx]->codec->time_base.den; qDebug() <<
                "ic->streams[aidx]->time_base:" <<
            ic->streams[aidx]->time_base.num
                     << ic->streams[aidx]->time_base.den;*/

            av_packet_rescale_ts(&pkt, ic->streams[aidx]->time_base,
                                 ast->time_base);

            /*qDebug() << "------ after rescale -----";
            qDebug() << "duration:" << pkt.duration;
            qDebug() << "dts:" << pkt.dts;
            qDebug() << "pts:" << pkt.pts;
            qDebug() << "pos:" << pkt.pos;*/

            pkt.stream_index = ast->index;

            if (av_write_frame(oc, &pkt) != 0) {
                qWarning() << "error while writing audio frame";
                av_packet_unref(&pkt);
                avformat_close_input(&ic);
                avio_close(oc->pb);
                avformat_free_context(oc);
                audioFile.remove();
                return std::nullopt;
            }
        }

        av_packet_unref(&pkt);
    }

    if (av_write_trailer(oc) < 0) {
        qWarning() << "av_write_trailer error";
        avformat_close_input(&ic);
        avio_close(oc->pb);
        avformat_free_context(oc);
        audioFile.remove();
        return std::nullopt;
    }

    avformat_close_input(&ic);

    if (avio_close(oc->pb) < 0) {
        qWarning() << "avio_close error";
    }

    avformat_free_context(oc);

    avData.size = QFileInfo{avData.path}.size();

    qDebug() << "audio file successfully extracted:" << cfile;

    return avData;
}
