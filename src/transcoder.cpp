/* Copyright (C) 2022 Michal Kosciesza <michal@mkiol.net>
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#include "transcoder.h"

#include <QDebug>
#include <QDir>
#include <QFile>
#include <QFileInfo>

extern "C" {
#include "libavcodec/avcodec.h"
#include "libavdevice/avdevice.h"
#include "libavformat/avformat.h"
#include "libavformat/avio.h"
#include "libavutil/dict.h"
#include "libavutil/imgutils.h"
#include "libavutil/log.h"
#include "libavutil/mathematics.h"
#include "libavutil/time.h"
#include "libavutil/timestamp.h"
#include "libswresample/swresample.h"
#include "libswscale/swscale.h"
}

static inline void logAvError(const char *message, int error) {
    char buff[AV_ERROR_MAX_STRING_SIZE];
    qWarning() << message
               << av_make_error_string(buff, AV_ERROR_MAX_STRING_SIZE, error);
}

static inline QString removeExtension(const QString &fileName) {
    auto idx = fileName.lastIndexOf('.');
    if (idx == -1) return fileName;
    return fileName.left(idx);
}

std::optional<AvMeta> Transcoder::makeAvMeta(const AVCodecParameters *codec) {
    AvMeta meta;

    switch (codec->codec_id) {
        case AV_CODEC_ID_MP2:
        case AV_CODEC_ID_MP3:
            meta.mime = QStringLiteral("audio/mpeg");
            meta.type = QStringLiteral("mp3");
            meta.extension = QStringLiteral("mp3");
            break;
        case AV_CODEC_ID_VORBIS:
            meta.mime = QStringLiteral("audio/ogg");
            meta.type = QStringLiteral("oga");
            meta.extension = QStringLiteral("oga");
            break;
        case AV_CODEC_ID_AAC:
        case AV_CODEC_ID_AAC_LATM:
            meta.mime = QStringLiteral("audio/mp4");
            meta.type = QStringLiteral("mp4");
            meta.extension = QStringLiteral("m4a");
            break;
        default:
            qWarning() << "not supported codec:" << codec->codec_id;
            return std::nullopt;
    }

    meta.bitrate = codec->bit_rate;
    meta.channels = codec->channels;

    return meta;
}

bool Transcoder::isMp4Isom(QIODevice &device) {
    return device.peek(100).indexOf(QByteArrayLiteral("ftypisom")) != -1;
}

bool Transcoder::isMp4Isom(const QString &file) {
    QFile f{file};
    if (!f.open(QIODevice::ReadOnly)) return false;
    return isMp4Isom(f);
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
        if (std::is_same<Input, AVStream *>())
            qDebug() << "stream metadata found:";
        else
            qDebug() << "format metadata found:";
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

template <typename Input>
static void logAvMetadata(const Input ic) {
    static_assert(std::is_same<Input, AVStream *>() ||
                      std::is_same<Input, AVFormatContext *>(),
                  "Invalid type");
    if (ic->metadata) {
        AVDictionaryEntry *tag = nullptr;
        if (std::is_same<Input, AVStream *>())
            qDebug() << "stream metadata found:";
        else
            qDebug() << "format metadata found:";
        while (
            (tag = av_dict_get(ic->metadata, "", tag, AV_DICT_IGNORE_SUFFIX))) {
            qDebug() << "  " << tag->key << "=" << tag->value;
        }
    }
}

std::optional<Transcoder::InputAvData> Transcoder::openAudioInput(
    const QString &file) {
    const auto f = file.toUtf8();
    auto *cfile = f.constData();

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
    qDebug() << "av input:";
    qDebug() << "  nb_streams:" << ic->nb_streams;
    qDebug() << "  audio stream index is:" << aidx;
    qDebug() << "  audio stream nb_side_data:"
             << ic->streams[aidx]->nb_side_data;
    for (int i = 0; i < ic->streams[aidx]->nb_side_data; ++i) {
        qDebug() << "  -- audio stream side data --";
        qDebug() << "  type:" << ic->streams[aidx]->side_data[i].type;
        qDebug() << "  size:" << ic->streams[aidx]->side_data[i].size;
    }
    qDebug() << "  audio codec:";
    qDebug() << "    codec_id:" << ic->streams[aidx]->codecpar->codec_id;
    qDebug() << "    codec_channels:" << ic->streams[aidx]->codecpar->channels;
    qDebug() << "    codec_tag:" << ic->streams[aidx]->codecpar->codec_tag;
#endif

    auto *icodec = avcodec_find_decoder(ic->streams[aidx]->codecpar->codec_id);
    if (!icodec) {
        qWarning() << "avcodec_find_decoder error";
        avformat_close_input(&ic);
        return std::nullopt;
    }

    auto meta = makeAvMeta(ic->streams[aidx]->codecpar);
    if (!meta) {
        avformat_close_input(&ic);
        return std::nullopt;
    }

    qDebug() << "extracted audio stream:";
    qDebug() << "  content type:" << meta->mime;
    qDebug() << "  bitrate:" << meta->bitrate;
    qDebug() << "  channels:" << meta->channels;

    return InputAvData{*meta, ic, icodec, aidx};
}

bool Transcoder::writeOutput(const QString &path, AVFormatContext *ic,
                             AVFormatContext *oc, AVStream *ast, int aidx) {
    auto bapath = path.toUtf8();
    if (avio_open(&oc->pb, bapath.data(), AVIO_FLAG_WRITE) < 0) {
        qWarning() << "avio_open error";
        return false;
    }

    if (avformat_write_header(oc, nullptr) < 0) {
        qWarning() << "avformat_write_header error";
        avio_close(oc->pb);
        return false;
    }

    auto *pkt = av_packet_alloc();

    while (!av_read_frame(ic, pkt)) {
        if (pkt->stream_index == aidx) {
#ifdef QT_DEBUG
            for (int i = 0; i < pkt->side_data_elems; ++i) {
                qDebug() << "audio stream packet side data:";
                qDebug() << "  type:" << pkt->side_data[i].type;
                qDebug() << "  size:" << pkt->side_data[i].size;
            }
#endif
            av_packet_rescale_ts(pkt, ic->streams[aidx]->time_base,
                                 ast->time_base);

            pkt->stream_index = ast->index;

            if (av_write_frame(oc, pkt) != 0) {
                qWarning() << "error while writing audio frame";
                av_packet_free(&pkt);
                avio_close(oc->pb);
                QFile::remove(path);
                return false;
            }
        }

        av_packet_unref(pkt);
    }

    av_packet_free(&pkt);

    if (av_write_trailer(oc) < 0) {
        qWarning() << "av_write_trailer error";
        avio_close(oc->pb);
        QFile::remove(path);
        return false;
    }

    avio_close(oc->pb);

    return true;
}

const AVOutputFormat *Transcoder::bestFormatForMime(const QString &mime) {
    auto t = mime.toLatin1();
    auto *format = av_guess_format(t.constData(), nullptr, nullptr);
    if (!format) {
        qWarning() << "av_guess_format error";
        return nullptr;
    }

    return format;
}

std::optional<AvMeta> Transcoder::extractAudio(const QString &inFile,
                                               const QString &outFile) {
    auto input = openAudioInput(inFile);
    if (!input) {
        return std::nullopt;
    }

    auto *oformat = bestFormatForMime(input->meta.type);
    if (!oformat) {
        avformat_close_input(&input->ic);
        return std::nullopt;
    }

    auto *oc = avformat_alloc_context();
    if (!oc) {
        qWarning() << "avformat_alloc_context error";
        avformat_close_input(&input->ic);
        return std::nullopt;
    }

    auto *ic = input->ic;

    logAvMetadata(ic);

    oc->oformat = oformat;

    auto *ocodec = input->icodec;  // no transcoding

    auto *ast = avformat_new_stream(oc, ocodec);
    if (!ast) {
        qWarning() << "avformat_new_stream error";
        avformat_free_context(oc);
        avformat_close_input(&input->ic);
        return std::nullopt;
    }
    ast->id = 0;
    if (!copyAvMetadata(ic->streams[input->aidx], ast)) {
        avformat_free_context(oc);
        avformat_close_input(&input->ic);
        return std::nullopt;
    }

    if (avcodec_parameters_copy(ast->codecpar,
                                ic->streams[input->aidx]->codecpar) < 0) {
        qWarning() << "avcodec_parameters_copy error";
        avformat_free_context(oc);
        avformat_close_input(&input->ic);
        return std::nullopt;
    }

    ast->codecpar->codec_tag = av_codec_get_tag(
        oc->oformat->codec_tag, ic->streams[input->aidx]->codecpar->codec_id);

    if (oc->oformat->flags & AVFMT_GLOBALHEADER)
        oc->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;

    if (outFile.isEmpty()) {
        input->meta.path = ContentServer::extractedAudioCachePath(
            inFile, input->meta.extension);
    } else {
        input->meta.path =
            removeExtension(outFile) + '.' + input->meta.extension;
    }

    qDebug() << "extracting audio to file:" << input->meta.path;

    if (QFile::exists(input->meta.path)) {
        qDebug() << "extracted audio stream already exists";
        input->meta.size = QFileInfo{input->meta.path}.size();
        avformat_free_context(oc);
        avformat_close_input(&input->ic);
        return input->meta;
    }

    if (!writeOutput(input->meta.path, ic, oc, ast, input->aidx)) {
        avformat_free_context(oc);
        avformat_close_input(&input->ic);
        return std::nullopt;
    }

    avformat_free_context(oc);

    input->meta.size = QFileInfo{input->meta.path}.size();

    qDebug() << "audio successfully extracted";

    return input->meta;
}
