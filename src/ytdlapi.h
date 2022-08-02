/* Copyright (C) 2022 Michal Kosciesza <michal@mkiol.net>
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#ifndef YTDLAPI_H
#define YTDLAPI_H

#include <ytmusic.h>

#include <QByteArray>
#include <QObject>
#include <QString>
#include <QUrl>
#include <memory>
#include <optional>

class YtdlApi : public QObject {
    Q_OBJECT
   public:
    struct Track {
        QString title;
        QUrl webUrl;
        QUrl streamUrl;
        QUrl streamAudioUrl;
        QUrl imageUrl;
    };

    YtdlApi(QObject* parent = nullptr);
    std::optional<Track> track(const QUrl& url) const;
    static bool init();

   private:
    enum class State { Unknown, Disabled, Enabled };
    static State state;
#ifdef SAILFISH
    const static QString pythonArchivePath;
#endif
    static bool unpack();
    static bool check();
    static QUrl bestAudioUrl(const std::vector<video_info::Format>& formats);
    static QUrl bestVideoUrl(const std::vector<video_info::Format>& formats);
};

#endif  // YTDLAPI_H
