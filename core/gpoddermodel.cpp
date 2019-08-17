/* Copyright (C) 2018 Michal Kosciesza <michal@mkiol.net>
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#include <QDebug>
#include <QFile>
#include <QList>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QSqlError>
#include <QLatin1String>
#include <QStandardPaths>

#include "gpoddermodel.h"
#include "iconprovider.h"

QString Gpodder::conName = "qt_sql_jupii_connection";
QSqlDatabase Gpodder::m_db = QSqlDatabase();

bool Gpodder::enabled()
{
    return Gpodder::db().open();
}

QDir Gpodder::dataDir()
{
#ifdef SAILFISH
    QDir dir(QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation));
    dir.cd("harbour-org.gpodder.sailfish");
#else
    QDir dir(QStandardPaths::writableLocation(QStandardPaths::HomeLocation));
    dir.cd("gPodder");
#endif
    return dir;
}

QSqlDatabase Gpodder::db()
{
    if (m_db.open())
        return m_db;

    auto dir = dataDir();
#ifdef SAILFISH
    auto file = dir.filePath("Database.minidb");
#else
    auto file = dir.filePath("Database");
#endif

    if (QFile::exists(file)) {
        m_db = QSqlDatabase::addDatabase("QSQLITE", conName);
        m_db.setConnectOptions(QLatin1String("QSQLITE_OPEN_READONLY"));
        m_db.setDatabaseName(file);
        m_db.open();
        if (m_db.open()) {
            QSqlQuery query(m_db);
            query.exec("PRAGMA journal_mode = MEMORY");
            query.exec("PRAGMA synchronous = OFF");
            return m_db;
        }
    }

    qWarning() << "Cannot open gpodder db";

    return QSqlDatabase();
}

GpodderEpisodeModel::GpodderEpisodeModel(QObject *parent) :
    SelectableItemModel(new GpodderEpisodeItem, parent)
{
    m_dir = Gpodder::dataDir();
}

QList<ListItem*> GpodderEpisodeModel::makeItems()
{
    QList<ListItem*> items;

    auto db = Gpodder::db();

    if (!db.open()) {
        qWarning() << "Gpodder DB is not available";
        return items;
    }

    QSqlQuery query(db);
    query.prepare("SELECT PodcastEpisode.id, PodcastEpisode.title, "
                  "PodcastEpisode.subtitle, PodcastEpisode.published, "
                  "PodcastEpisode.download_filename, PodcastEpisode.total_time, "
                  "PodcastEpisode.current_position, PodcastEpisode.mime_type, "
                  "PodcastChannel.title, PodcastChannel.download_folder "
                  "FROM PodcastEpisode JOIN PodcastChannel "
                  "ON PodcastEpisode.podcast_id = PodcastChannel.id "
                  "WHERE PodcastEpisode.download_filename NOTNULL AND "
                  "PodcastEpisode.download_filename != '' AND "
                  "PodcastEpisode.state = 1 AND "
                  "(PodcastEpisode.title LIKE ? OR "
                  "PodcastChannel.title LIKE ?) "
                  "ORDER BY PodcastEpisode.published DESC "
                  "LIMIT 50");
    query.addBindValue("%" + getFilter() + "%");
    query.addBindValue("%" + getFilter() + "%");

    if (query.exec()) {
        while(query.next()) {
            auto dir(m_dir);
            auto folder = query.value(9).toString();
#ifdef SAILFISH
            dir.cd(folder);
            if (!dir.exists()) {
                qWarning() << "Dir" << m_dir.absolutePath() << "doesn't exist";
                continue;
            }
            QUrl icon;
            if (dir.exists("folder.jpg"))
                icon = QUrl::fromLocalFile(dir.absoluteFilePath("folder.jpg"));
            else if (dir.exists("folder.png"))
                icon = QUrl::fromLocalFile(dir.absoluteFilePath("folder.png"));
            else if (dir.exists("folder.gif"))
                icon = QUrl::fromLocalFile(dir.absoluteFilePath("folder.gif"));
            else
                icon = QUrl::fromLocalFile(IconProvider::pathToNoResId("icon-gpodder"));
#else
            dir.cd("Downloads");
            dir.cd(folder);
            dir.cd(folder);
            if (!dir.exists()) {
                qWarning() << "Dir" << dir.absolutePath() << "doesn't exist";
                continue;
            }
            QIcon icon;
            if (dir.exists("folder.jpg"))
                icon = QIcon(dir.absoluteFilePath("folder.jpg"));
            else if (dir.exists("folder.png"))
                icon = QIcon(dir.absoluteFilePath("folder.png"));
            else if (dir.exists("folder.gif"))
                icon = QIcon(dir.absoluteFilePath("folder.gif"));
#endif
            auto filename = query.value(4).toString();
            if (dir.exists(filename)) {
                auto type = ContentServer::typeFromMime(query.value(7).toString());
                items << new GpodderEpisodeItem(
                                query.value(0).toString(), // id
                                query.value(1).toString(), // title
                                QString(), // query.value(2).toString(), // desc
                                query.value(8).toString(), // podcast title
                                QUrl::fromLocalFile(dir.absoluteFilePath(filename)), // url
                                query.value(5).toInt(), // duration
                                query.value(6).toInt(), // position
                                type, // type
                                query.value(3).toUInt(), // published
                                icon // icon
                             );
            } else {
                qWarning() << "Episode file doesn't exist:" << dir.absoluteFilePath(filename);
            }
        }
    } else {
        auto err = query.lastError();
        qWarning() << "Gpodder DB query error:"
                   << err.number()
                   << err.type()
                   << err.text();
    }

    return items;
}

QVariantList GpodderEpisodeModel::selectedItems()
{
    QVariantList list;

    for (auto item : m_list) {
        auto episode = dynamic_cast<GpodderEpisodeItem*>(item);
        if (episode->selected()) {
            QVariantMap map;
            map.insert("url", QVariant(episode->url()));
            map.insert("name", QVariant(episode->title()));
            map.insert("author", QVariant(episode->podcastTitle()));
            //map.insert("desc", QVariant(episode->description()));
            map.insert("icon", QVariant(episode->icon()));
            list << map;
        }
    }

    return list;
}

GpodderEpisodeItem::GpodderEpisodeItem(const QString &id,
                   const QString &title,
                   const QString &description,
                   const QString &podcastTitle,
                   const QUrl &content,
                   int duration,
                   int position,
                   ContentServer::Type type,
                   uint published,
#ifdef SAILFISH
                   const QUrl &icon,
#else
                   const QIcon &icon,
#endif
                   QObject *parent) :
    SelectableItem(parent),
    m_id(id),
    m_title(title),
    m_description(description),
    m_ptitle(podcastTitle),
    m_url(content),
    m_duration(duration),
    m_position(position),
    m_type(type),
    m_published(published),
    m_icon(icon)
{
}

QHash<int, QByteArray> GpodderEpisodeItem::roleNames() const
{
    QHash<int, QByteArray> names;
    names[IdRole] = "id";
    names[TitleRole] = "title";
    names[DescriptionRole] = "description";
    names[IconRole] = "icon";
    names[UrlRole] = "url";
    names[DurationRole] = "duration";
    names[PositionRole] = "position";
    names[SelectedRole] = "selected";
    names[TypeRole] = "type";
    names[PublishedRole] = "published";
    names[PodcastTitleRole] = "podcastTitle";
    return names;
}

QVariant GpodderEpisodeItem::data(int role) const
{
    switch(role) {
    case IdRole:
        return id();
    case TitleRole:
        return title();
    case DescriptionRole:
        return description();
    case IconRole:
        return icon();
    case UrlRole:
        return url();
    case DurationRole:
        return duration();
    case PositionRole:
        return position();
    case SelectedRole:
        return selected();
    case TypeRole:
        return type();
    case PublishedRole:
        return published();
    case PodcastTitleRole:
        return podcastTitle();
    default:
        return QVariant();
    }
}
