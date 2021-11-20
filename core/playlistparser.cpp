/* Copyright (C) 2021 Michal Kosciesza <michal@mkiol.net>
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#include "playlistparser.h"

#include <QDebug>
#include <QDomDocument>
#include <QTextStream>
#include <QMap>
#include <QRegExp>

#include "utils.h"

QList<PlaylistItemMeta> parseM3u(const QByteArray &data, const QString &context)
{
    qDebug() << "Parsing M3U playlist";

    QList<PlaylistItemMeta> list;

    QTextStream s(data, QIODevice::ReadOnly);
    s.setAutoDetectUnicode(true);

    while (!s.atEnd()) {
        const auto line = s.readLine();
        if (line.startsWith("#")) {
            // TODO: read title from M3U playlist
        } else {
            auto url = Utils::urlFromText(line, context);
            if (!url.isEmpty()) {
                PlaylistItemMeta item;
                item.url = url;
                list.append(item);
            }
        }
    }

    return list;
}

QList<PlaylistItemMeta> parseXspf(const QByteArray &data, const QString &context)
{
    qDebug() << "Parsing XSPF playlist";
    QList<PlaylistItemMeta> list;

    QDomDocument doc; QString error;
    if (doc.setContent(data, false, &error)) {
        auto tracks = doc.elementsByTagName("track");
        const int l = tracks.length();
        for (int i = 0; i < l; ++i) {
            auto track = tracks.at(i).toElement();
            if (!track.isNull()) {
                const auto ls = track.elementsByTagName("location");
                if (!ls.isEmpty()) {
                    const auto l = ls.at(0).toElement();
                    if (!l.isNull()) {
                        //qDebug() << "location:" << l.text();
                        auto url = Utils::urlFromText(l.text(), context);
                        if (!url.isEmpty()) {
                            PlaylistItemMeta item;
                            item.url = url;

                            const auto ts = track.elementsByTagName("title");
                            if (!ts.isEmpty()) {
                                auto t = ts.at(0).toElement();
                                if (!t.isNull()) {
                                    //qDebug() << "title:" << t.text();
                                    item.title = t.text();
                                }
                            }

                            const auto ds = track.elementsByTagName("duration");
                            if (!ds.isEmpty()) {
                                auto d = ds.at(0).toElement();
                                if (!d.isNull()) {
                                    //qDebug() << "duration:" << d.text();
                                    item.length = d.text().toInt();
                                }
                            }

                            list.append(item);
                        }
                    }
                }
            }
        }
    } else {
        qWarning() << "Playlist parse error:" << error << data;
    }

    return list;
}

QList<PlaylistItemMeta> parsePls(const QByteArray &data, const QString &context)
{
    qDebug() << "Parsing PLS playlist";
    QMap<int, PlaylistItemMeta> map;
    int pos;

    // urls
    QRegExp rxFile("\\nFile(\\d\\d?)=([^\\n]*)", Qt::CaseInsensitive);
    pos = 0;
    while ((pos = rxFile.indexIn(data, pos)) != -1) {
        const auto cap1 = rxFile.cap(1);
        const auto cap2 = rxFile.cap(2);
        //qDebug() << "cap:" << cap1 << cap2;

        bool ok;
        int n = cap1.toInt(&ok);
        if (ok) {
            auto url = Utils::urlFromText(cap2, context);
            if (!url.isEmpty()) {
                auto &item = map[n];
                item.url = url;
            } else {
                qWarning() << "Playlist item url is invalid";
            }
        } else {
            qWarning() << "Playlist item no is invalid";
        }

        pos += rxFile.matchedLength();
    }

    if (!map.isEmpty()) {
        // titles
        QRegExp rxTitle("\\nTitle(\\d\\d?)=([^\\n]*)", Qt::CaseInsensitive);
        pos = 0;
        while ((pos = rxTitle.indexIn(data, pos)) != -1) {
            const auto cap1 = rxTitle.cap(1);
            const auto cap2 = rxTitle.cap(2);
            //qDebug() << "cap:" << cap1 << cap2;

            bool ok;
            int n = cap1.toInt(&ok);
            if (ok && map.contains(n)) {
                auto &item = map[n];
                item.title = cap2;
            }

            pos += rxTitle.matchedLength();
        }

        // length
        QRegExp rxLength("\\nLength(\\d\\d?)=([^\\n]*)", Qt::CaseInsensitive);
        pos = 0;
        while ((pos = rxLength.indexIn(data, pos)) != -1) {
            const auto cap1 = rxLength.cap(1);
            const auto cap2 = rxLength.cap(2);
            //qDebug() << "cap:" << cap1 << cap2;

            bool ok;
            int n = cap1.toInt(&ok);
            if (ok && map.contains(n)) {
                bool ok;
                int length = cap2.toInt(&ok);
                if (ok) {
                    auto &item = map[n];
                    item.length = length < 0 ? 0 : length;
                }
            }

            pos += rxLength.matchedLength();
        }
    } else {
        qWarning() << "Playlist doesn't contain any URLs";
    }

    return map.values();
}

void resolveM3u(QByteArray &data, const QString &context)
{
    QStringList lines;

    QTextStream s(data, QIODevice::ReadOnly);
    s.setAutoDetectUnicode(true);

    while (!s.atEnd()) {
        auto line = s.readLine();
        if (!line.startsWith("#"))
            lines << line;
    }

    foreach (const auto& line, lines) {
        auto url = Utils::urlFromText(line, context);
        if (!url.isEmpty())
            data.replace(line.toUtf8(), url.toString().toUtf8());
    }
}
