/* Copyright (C) 2022 Michal Kosciesza <michal@mkiol.net>
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#ifndef TRANSCODER_H
#define TRANSCODER_H

#include <QString>
#include <optional>

extern "C" {
#include "libavcodec/avcodec.h"
}

struct AvData {
    QString path;
    QString mime;
    QString type;
    QString extension;
    int64_t bitrate = 0;
    int channels = 0;
    int64_t size = 0;
};

class Transcoder {
   public:
    static std::optional<AvData> extractAudio(const QString &file);

   private:
    static bool fillAvData(const AVCodecParameters *codec,
                           const QString &videoFile, AvData *data);
};

#endif  // TRANSCODER_H
