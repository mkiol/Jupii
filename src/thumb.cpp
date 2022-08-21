/* Copyright (C) 2022 Michal Kosciesza <michal@mkiol.net>
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#include "thumb.h"

#include <QBuffer>
#include <QDebug>
#include <QDir>
#include <QFileInfo>
#include <QIODevice>

#include "contentserver.h"
#include "downloader.h"
#include "exif.h"
#include "utils.h"

Thumb::ImageOrientation Thumb::imageOrientation(const QByteArray &data) {
    easyexif::EXIFInfo exif;
    if (exif.parseFrom(
            reinterpret_cast<const unsigned char *>(data.constData()),
            data.size())) {
        return ImageOrientation::R0;
    }

    switch (exif.Orientation) {
        case 3:
            return ImageOrientation::R180;
        case 6:
            return ImageOrientation::R90;
        case 8:
            return ImageOrientation::R270;
    }

    return ImageOrientation::R0;
}

bool Thumb::convert(const QByteArray &format, QByteArray &data,
                    ImageOrientation orientation) {
    QPixmap pixmap;
    if (!pixmap.loadFromData(data, format)) {
        qWarning() << "cannot load thumb";
        return false;
    }

    if (orientation == ImageOrientation::Unknown) {
        orientation = imageOrientation(data);
    }

    if (!convertPixmap(pixmap, orientation)) return true;

    data.clear();

    QBuffer b{&data};
    b.open(QIODevice::WriteOnly);
    if (!pixmap.save(&b, format)) {
        qWarning() << "cannot save thumb";
        return false;
    }

    return true;
}

bool Thumb::convertToJpeg(const QByteArray &format, QByteArray &data) {
    QPixmap p;
    if (!p.loadFromData(data, format)) {
        qWarning() << "cannot load format";
        return false;
    }

    data.clear();

    QBuffer b{&data};
    b.open(QIODevice::WriteOnly);

    if (!p.save(&b, "jpeg")) {
        qWarning() << "cannot save format";
        return false;
    }

    return true;
}

bool Thumb::convertPixmap(QPixmap &pixmap, ImageOrientation orientation) {
    bool modified = false;

    if (pixmap.width() != pixmap.height()) {
        if (pixmap.width() > pixmap.height()) {
            pixmap = pixmap.copy((pixmap.width() - pixmap.height()) / 2, 0,
                                 pixmap.height(), pixmap.height());
        } else {
            pixmap = pixmap.copy(0, (pixmap.height() - pixmap.width()) / 2,
                                 pixmap.width(), pixmap.width());
        }
        modified = true;
    }

    if (pixmap.width() > maxPixelSize) {
        pixmap = pixmap.scaled(maxPixelSize, maxPixelSize,
                               Qt::AspectRatioMode::IgnoreAspectRatio,
                               Qt::TransformationMode::SmoothTransformation);
        modified = true;
    }

    if (orientation != ImageOrientation::Unknown &&
        orientation != ImageOrientation::R0) {
        if (orientation == ImageOrientation::R90)
            pixmap = pixmap.transformed(QMatrix{}.rotate(90.0));
        else if (orientation == ImageOrientation::R180)
            pixmap = pixmap.transformed(QMatrix{}.rotate(180.0));
        else if (orientation == ImageOrientation::R270)
            pixmap = pixmap.transformed(QMatrix{}.rotate(270.0));
        modified = true;
    }

    return modified;
}

std::optional<QString> Thumb::path(const QUrl &url) {
    if (url.isEmpty()) return std::nullopt;

    auto pathForExt = [url = url.toString()](const QString &ext) {
        auto p = ContentServer::albumArtCachePath(url, ext);
        if (QFileInfo::exists(p)) return p;
        return QString{};
    };

    auto p = pathForExt(QStringLiteral("jpg"));
    if (p.isEmpty()) p = pathForExt(QStringLiteral("png"));

    if (p.isEmpty()) return std::nullopt;
    return p;
}

std::optional<QString> Thumb::download(const QUrl &url) {
    if (url.isEmpty()) return std::nullopt;
    if (auto p = path(url)) return p;

    auto data = Downloader{}.downloadData(url);
    return save(std::move(data.bytes), url,
                ContentServer::extFromMime(data.mime));
}

std::optional<QString> Thumb::save(QByteArray &&data, const QUrl &url,
                                   QString ext) {
    if (data.isEmpty()) {
        qWarning() << "thumb data is empty";
        return std::nullopt;
    }

    if (ext.isEmpty()) {
        qWarning() << "invalid thumb ext";
        return std::nullopt;
    }

    auto orientation = imageOrientation(data);

    if (ext != QStringLiteral("jpg") && ext != QStringLiteral("png")) {
        qDebug() << "converting thumb format";
        if (!convertToJpeg(ext.toLatin1(), data)) return std::nullopt;
        ext = QStringLiteral("jpg");
    }

    if (!convert(ext.toLatin1(), data, orientation)) return std::nullopt;

    return Utils::writeToCacheFile(
        ContentServer::albumArtCacheName(url.toString(), ext), data, true);
}
