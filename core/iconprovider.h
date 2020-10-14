/* Copyright (C) 2019 Michal Kosciesza <michal@mkiol.net>
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#ifndef ICONPROVIDER_H
#define ICONPROVIDER_H

#include <QSize>
#include <QPixmap>
#include <QString>
#include <QUrl>

#if defined(SAILFISH) || defined(KIRIGAMI)
#include <QQuickImageProvider>
#endif // SAILFISH || KIRIGAMI

#if defined(SAILFISH) || defined(KIRIGAMI)
class IconProvider : public QQuickImageProvider
#else
class IconProvider
#endif // SAILFISH || KIRIGAMI
{
public:
    static QString pathToId(const QString &id);
    static QString pathToNoResId(const QString &id);
    static QUrl urlToNoResId(const QString &id);
    static QUrl urlToImg(const QString &filename);
    static QString themeDirPath();

    IconProvider();
    QPixmap requestPixmap(const QString &id, QSize *size, const QSize &requestedSize);

private:
    QString themeDir;
    QString cacheDir;
};

#endif // ICONPROVIDER_H
