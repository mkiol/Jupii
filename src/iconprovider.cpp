/* Copyright (C) 2019-2022 Michal Kosciesza <michal@mkiol.net>
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#ifdef SAILFISH
#include <sailfishapp.h>
#include <mlite5/MGConfItem>
#endif

#include <QDebug>
#include <QDir>
#include <QFile>
#include <QPainter>
#include <QStandardPaths>

#include "iconprovider.h"

#ifdef SAILFISH
static inline QString pathTo(const QString &dir) {
    return SailfishApp::pathTo(dir).toString(QUrl::RemoveScheme);
}
#endif

IconProvider::IconProvider()
    : QQuickImageProvider(QQuickImageProvider::Pixmap),
      themeDir{themeDirPath()} {}

QString IconProvider::themeDirPath() {
    QString themeDir;
#ifdef SAILFISH
    auto ratio =
        MGConfItem(QStringLiteral("/desktop/sailfish/silica/theme_pixel_ratio"))
            .value()
            .toDouble();
    qDebug() << "device pixel ratio:" << ratio;
    if (ratio == 1.0) {
        themeDir = pathTo(QStringLiteral("images/z1.0"));
    } else if (ratio == 1.25) {
        themeDir = pathTo(QStringLiteral("images/z1.25"));
    } else if (ratio == 1.5) {
        themeDir = pathTo(QStringLiteral("images/z1.5"));
    } else if (ratio == 1.65 || ratio == 1.75 || ratio == 1.8) {
        themeDir = pathTo(QStringLiteral("images/z1.75"));
    } else if (ratio == 2.0) {
        themeDir = pathTo(QStringLiteral("images/z2.0"));
    } else {
        qWarning() << "unknown pixel ratio so, defaulting to 1.0";
        themeDir = pathTo(QStringLiteral("images/z1.0"));
    }
#endif  // SAILFISH
    return themeDir;
}

QString IconProvider::pathToId(const QString &id) {
    QDir dir{themeDirPath()};
    auto filepath = dir.absoluteFilePath(id.split('?').at(0) + ".png");
    if (!QFile::exists(filepath)) {
        // Icon file is not exist -> fallback to default icon
        filepath = dir.absoluteFilePath(QStringLiteral("icon-m-item.png"));
    }
    return filepath;
}

QString IconProvider::pathToNoResId(const QString &id) {
#ifdef SAILFISH
    return QDir{pathTo(QStringLiteral("images"))}.absoluteFilePath(id + ".png");
#else
    return "qrc:/images/" + id + ".png";
#endif
}

QUrl IconProvider::urlToNoResId(const QString &id) {
#ifdef SAILFISH
    return QUrl::fromLocalFile(
        QDir{pathTo(QStringLiteral("images"))}.absoluteFilePath(id + ".png"));
#else
    return QUrl{"qrc:/images/" + id + ".png"};
#endif
}

QUrl IconProvider::urlToImg(const QString &filename) {
#ifdef SAILFISH
    return QUrl::fromLocalFile(
        QDir{pathTo(QStringLiteral("images"))}.absoluteFilePath(filename));
#else
    return QUrl{"qrc:/images/" + filename};
#endif
}

QPixmap IconProvider::requestPixmap(const QString &id, QSize *size,
                                    const QSize &requestedSize) {
    auto parts = id.split('?');
    auto filepath = QStringLiteral("%1/%2.png").arg(themeDir, parts.at(0));
    if (!QFile::exists(filepath)) {
        filepath = themeDir + "/icon-m-item.png";
    }

    QPixmap pixmap{filepath};

    if (parts.size() > 1 && QColor::isValidColor(parts.at(1))) {
        QPainter painter{&pixmap};
        painter.setCompositionMode(QPainter::CompositionMode_SourceIn);
        painter.fillRect(pixmap.rect(), parts.at(1));
        painter.end();
    }

    if (size) *size = pixmap.size();

    if (requestedSize.width() > 0 && requestedSize.height() > 0) {
        return pixmap.scaled(requestedSize.width(), requestedSize.height());
    }

    return pixmap;
}
