/* Copyright (C) 2021 Michal Kosciesza <michal@mkiol.net>
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#ifndef PLAYLISTPARSER_H
#define PLAYLISTPARSER_H

#include <QString>
#include <QUrl>
#include <QByteArray>
#include <QList>

struct PlaylistItemMeta {
    QUrl url;
    QString title;
    int length = 0;
};

QList<PlaylistItemMeta> parsePls(const QByteArray &data, const QString &context = {});
QList<PlaylistItemMeta> parseM3u(const QByteArray &data, const QString &context = {});
QList<PlaylistItemMeta> parseXspf(const QByteArray &data, const QString &context = {});
void resolveM3u(QByteArray &data, const QString &context);

#endif // PLAYLISTPARSER_H
