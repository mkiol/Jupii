/* Copyright (C) 2019-2022 Michal Kosciesza <michal@mkiol.net>
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#ifndef ICONPROVIDER_H
#define ICONPROVIDER_H

#include <QPixmap>
#include <QQuickImageProvider>
#include <QSize>
#include <QString>
#include <QUrl>

class IconProvider : public QQuickImageProvider {
   public:
    static QString pathToId(const QString &id);
    static QString pathToNoResId(const QString &id);
    static QUrl urlToNoResId(const QString &id);
    static QUrl urlToImg(const QString &filename);
    static QString themeDirPath();

    IconProvider();
    QPixmap requestPixmap(const QString &id, QSize *size,
                          const QSize &requestedSize) override;

   private:
    QString themeDir;
    QString cacheDir;
};

#endif  // ICONPROVIDER_H
