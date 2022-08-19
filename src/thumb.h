/* Copyright (C) 2022 Michal Kosciesza <michal@mkiol.net>
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#ifndef THUMB_H
#define THUMB_H

#include <QByteArray>
#include <QPixmap>
#include <QString>
#include <QUrl>
#include <optional>

class Thumb {
   public:
    static std::optional<QString> path(const QUrl &url);
    static std::optional<QString> download(const QUrl &url);
    static std::optional<QString> save(QByteArray &&data, const QUrl &url,
                                       QString ext);
    inline static QUrl makeCache(const QUrl &url) {
        if (download(url)) return url;
        return {};
    }

   private:
    enum class ImageOrientation { Unknown, R0, R90, R180, R270 };
    static const int maxPixelSize = 512;

    static ImageOrientation imageOrientation(const QByteArray &data);
    static bool convertToJpeg(const QByteArray &format, QByteArray &data);
    static bool convert(
        const QByteArray &format, QByteArray &data,
        ImageOrientation orientation = ImageOrientation::Unknown);
    static bool convertPixmap(QPixmap &pixmap, ImageOrientation orientation);
};

#endif // THUMB_H
