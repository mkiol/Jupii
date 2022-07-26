/* Copyright (C) 2022 Michal Kosciesza <michal@mkiol.net>
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#ifndef TRANSCODER_H
#define TRANSCODER_H

#include <QByteArray>
#include <QIODevice>
#include <QString>
#include <optional>

extern "C" {
#include "libavcodec/avcodec.h"
}

#include "contentserver.h"

struct AVFormatContext;
struct AVOutputFormat;
struct AVStream;

class Transcoder {
   public:
    static bool isMp4Isom(QIODevice &device);
    static bool isMp4Isom(const QString &file);
    static std::optional<AvMeta> extractAudio(const QString &inFile,
                                              const QString &outFile = {});

   private:
    struct InputAvData {
        AvMeta meta;
        AVFormatContext *ic = nullptr;
        const AVCodec *icodec = nullptr;
        int aidx = 0;
    };
    static bool writeOutput(const QString &path, AVFormatContext *ic,
                            AVFormatContext *oc, AVStream *ast, int aidx);
    static const AVOutputFormat *bestFormatForMime(const QString &mime);
    static std::optional<InputAvData> openAudioInput(const QString &file);
    static std::optional<AvMeta> makeAvMeta(const AVCodecParameters *codec);
};

#endif  // TRANSCODER_H
