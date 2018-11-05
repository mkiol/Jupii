/* Copyright (C) 2018 Michal Kosciesza <michal@mkiol.net>
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#ifdef SAILFISH
#include <sailfishapp.h>
#include <mlite5/MGConfItem>
#endif

#include <QPainter>
#include <QDebug>
#include <QFile>
#include <QDir>
#include <QStandardPaths>

#include "iconprovider.h"

IconProvider::IconProvider() : QQuickImageProvider(QQuickImageProvider::Pixmap)
{
    this->themeDir = IconProvider::themeDirPath();
}

QString IconProvider::themeDirPath()
{
    QString themeDir;
#ifdef SAILFISH
    double ratio = MGConfItem("/desktop/sailfish/silica/theme_pixel_ratio").value().toDouble();
    if (ratio == 0) {
        qWarning() << "Pixel ratio is 0, defaulting to 1.0.";
        themeDir = SailfishApp::pathTo("images/z1.0").toString(QUrl::RemoveScheme);
    } else if (ratio == 1.0) {
        themeDir = SailfishApp::pathTo("images/z1.0").toString(QUrl::RemoveScheme);
    } else if (ratio == 1.25) {
        themeDir = SailfishApp::pathTo("images/z1.25").toString(QUrl::RemoveScheme);
    } else if (ratio == 1.5) {
        themeDir = SailfishApp::pathTo("images/z1.5").toString(QUrl::RemoveScheme);
    } else if (ratio == 1.75 || ratio == 1.8) {
        themeDir = SailfishApp::pathTo("images/z1.75").toString(QUrl::RemoveScheme);
    } else if (ratio == 2.0) {
        themeDir = SailfishApp::pathTo("images/z2.0").toString(QUrl::RemoveScheme);
    }

    if (!QDir(themeDir).exists()) {
        qWarning() << "Theme" << themeDir << "for ratio" << ratio << "doesn't exist!";
        themeDir = SailfishApp::pathTo("images/z1.0").toString(QUrl::RemoveScheme);
    }
#else
    //TODO theme dir for desktop
#endif
    return themeDir;
}

QString IconProvider::pathToId(const QString &id)
{
    QDir dir(IconProvider::themeDirPath());
    auto filepath = dir.absoluteFilePath(id.split('?').at(0) + ".png");

    if (!QFile::exists(filepath)) {
        // Icon file is not exist -> fallback to default icon
        filepath = dir.absoluteFilePath("icon-m-item.png");
    }

    return filepath;
}

QPixmap IconProvider::requestPixmap(const QString &id, QSize *size, const QSize &requestedSize)
{
    QStringList parts = id.split('?');

    QString filepath = themeDir + "/" + parts.at(0) + ".png";
    if (!QFile::exists(filepath)) {
        // Icon file is not exist -> fallback to default icon
        filepath = themeDir + "/icon-m-item.png";
    }

    QPixmap sourcePixmap(filepath);

    if (size)
        *size  = sourcePixmap.size();

    if (parts.length() > 1)
        if (QColor::isValidColor(parts.at(1))) {
            QPainter painter(&sourcePixmap);
            painter.setCompositionMode(QPainter::CompositionMode_SourceIn);
            painter.fillRect(sourcePixmap.rect(), parts.at(1));
            painter.end();
        }

    if (requestedSize.width() > 0 && requestedSize.height() > 0)
        return sourcePixmap.scaled(requestedSize.width(), requestedSize.height(), Qt::IgnoreAspectRatio);
    else
        return sourcePixmap;
}
