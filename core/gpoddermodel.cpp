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

GpodderPodcastModel::GpodderPodcastModel(QObject *parent) :
    SelectableItemModel(new GpodderPodcastItem, parent)
{
}

QList<ListItem*> GpodderPodcastModel::makeItems()
{
    QList<ListItem*> items;

    QSqlDatabase db = Gpodder::db();

    if (!db.open()) {
        qWarning() << "Gpodder DB is not available";
        return items;
    }

    QSqlQuery query(db);
    /*query.prepare("SELECT id, title, description, download_folder "
                  "FROM PodcastChannel "
                  "WHERE title LIKE ? AND "
                  "(SELECT count(*) FROM PodcastEpisode "
                  "WHERE state = 1 AND "
                  "download_filename NOTNULL AND download_filename != '' AND "
                  "podcast_id = PodcastChannel.id) > 0 "
                  "ORDER BY title "
                  "LIMIT 50");*/
    query.prepare("SELECT PodcastChannel.id, PodcastChannel.title, "
                  "PodcastChannel.description, PodcastChannel.download_folder "
                  "FROM PodcastChannel JOIN PodcastEpisode "
                  "ON PodcastChannel.id = PodcastEpisode.podcast_id "
                  "WHERE PodcastEpisode.state = 1 AND "
                  "PodcastEpisode.download_filename NOTNULL AND "
                  "PodcastEpisode.download_filename != '' AND "
                  "PodcastChannel.title LIKE ? "
                  "GROUP BY PodcastEpisode.podcast_id "
                  "ORDER BY PodcastChannel.title LIMIT 50");
    query.addBindValue("%" + getFilter() + "%");

    if (query.exec()) {
        while(query.next()) {
            auto folder = query.value(3).toString();

            // Podcast icon
            auto dir = Gpodder::dataDir();
#ifdef SAILFISH
            QUrl icon;
            dir.cd(folder);
            if (dir.exists("folder.jpg"))
                icon = QUrl::fromLocalFile(dir.absoluteFilePath("folder.jpg"));
            else if (dir.exists("folder.png"))
                icon = QUrl::fromLocalFile(dir.absoluteFilePath("folder.png"));
            else if (dir.exists("folder.gif"))
                icon = QUrl::fromLocalFile(dir.absoluteFilePath("folder.gif"));
#else
            QIcon icon;
            dir.cd("Downloads");
            dir.cd(folder);
            if (dir.exists("folder.jpg"))
                icon = QIcon(dir.absoluteFilePath("folder.jpg"));
            else if (dir.exists("folder.png"))
                icon = QIcon(dir.absoluteFilePath("folder.png"));
            else if (dir.exists("folder.gif"))
                icon = QIcon(dir.absoluteFilePath("folder.gif"));
#endif
            items << new GpodderPodcastItem(
                         query.value(0).toString(), // id
                         query.value(1).toString(), // title
                         query.value(2).toString(), // desc
                         folder, // folder
                         icon // icon
                         );
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

GpodderPodcastItem::GpodderPodcastItem(const QString &id,
                   const QString &title,
                   const QString &description,
                   const QString &folder,
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
    m_folder(folder),
    m_icon(icon)
{
}

QHash<int, QByteArray> GpodderPodcastItem::roleNames() const
{
    QHash<int, QByteArray> names;
    names[IdRole] = "id";
    names[TitleRole] = "title";
    names[DescriptionRole] = "description";
    names[IconRole] = "icon";
    return names;
}

QVariant GpodderPodcastItem::data(int role) const
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
    default:
        return QVariant();
    }
}

GpodderEpisodeModel::GpodderEpisodeModel(QObject *parent) :
    SelectableItemModel(new GpodderEpisodeItem, parent)
{
}

void GpodderEpisodeModel::updateDir()
{
    if (m_podcastId.isEmpty()) {
        qWarning() << "Podcast ID is empty";
        return;
    }

    auto db = Gpodder::db();

    if (!db.open()) {
        qWarning() << "Gpodder DB is not available";
        return;
    }

    QSqlQuery query(db);
    query.prepare("SELECT download_folder FROM PodcastChannel WHERE id = ?");
    query.addBindValue(m_podcastId);

    if (query.exec()) {
        while(query.next()) {
            auto folder = query.value(0).toString();
            m_dir = Gpodder::dataDir();
#ifdef SAILFISH
            m_dir.cd(folder);
            if (m_dir.exists("folder.jpg"))
                m_icon = QUrl::fromLocalFile(m_dir.absoluteFilePath("folder.jpg"));
            else if (m_dir.exists("folder.png"))
                m_icon = QUrl::fromLocalFile(m_dir.absoluteFilePath("folder.png"));
            else if (m_dir.exists("folder.gif"))
                m_icon = QUrl::fromLocalFile(m_dir.absoluteFilePath("folder.gif"));
#else
            m_dir.cd("Downloads");
            m_dir.cd(folder);
            if (m_dir.exists("folder.jpg"))
                m_icon = QIcon(m_dir.absoluteFilePath("folder.jpg"));
            else if (m_dir.exists("folder.png"))
                m_icon = QIcon(m_dir.absoluteFilePath("folder.png"));
            else if (m_dir.exists("folder.gif"))
                m_icon = QIcon(m_dir.absoluteFilePath("folder.gif"));
#endif
        }
    } else {
        auto err = query.lastError();
        qWarning() << "Gpodder DB query error:"
                   << err.number()
                   << err.type()
                   << err.text();
    }
}

QList<ListItem*> GpodderEpisodeModel::makeItems()
{
    QList<ListItem*> items;

    if (m_podcastId.isEmpty()) {
        qWarning() << "Podcast ID is empty";
        return items;
    }

    if (!m_dir.exists()) {
        qWarning() << "Dir" << m_dir.absolutePath() << "doesn't exist";
        return items;
    }

    auto db = Gpodder::db();

    if (!db.open()) {
        qWarning() << "Gpodder DB is not available";
        return items;
    }

    QSqlQuery query(db);
    /*query.prepare("SELECT id, title, subtitle, published, "
                  "download_filename, total_time, current_position, mime_type "
                  "FROM PodcastEpisode "
                  "WHERE download_filename NOTNULL AND "
                  "download_filename != '' AND "
                  "podcast_id = ? AND "
                  "state = 1 AND "
                  "title LIKE ? "
                  "ORDER BY published DESC "
                  "LIMIT 50");*/
    query.prepare("SELECT PodcastEpisode.id, PodcastEpisode.title, "
                  "PodcastEpisode.subtitle, PodcastEpisode.published, "
                  "PodcastEpisode.download_filename, PodcastEpisode.total_time, "
                  "PodcastEpisode.current_position, PodcastEpisode.mime_type, "
                  "PodcastChannel.title "
                  "FROM PodcastEpisode JOIN PodcastChannel "
                  "ON PodcastEpisode.podcast_id = PodcastChannel.id "
                  "WHERE PodcastEpisode.download_filename NOTNULL AND "
                  "PodcastEpisode.download_filename != '' AND "
                  "PodcastEpisode.podcast_id = ? AND "
                  "PodcastEpisode.state = 1 AND "
                  "PodcastEpisode.title LIKE ? "
                  "ORDER BY PodcastEpisode.published DESC "
                  "LIMIT 50");
    query.addBindValue(m_podcastId);
    query.addBindValue("%" + getFilter() + "%");

    if (query.exec()) {
        while(query.next()) {
            auto filename = query.value(4).toString();
            if (m_dir.exists(filename)) {
                auto type = ContentServer::typeFromMime(query.value(7).toString());
                items << new GpodderEpisodeItem(
                                query.value(0).toString(), // id
                                query.value(1).toString(), // title
                                query.value(2).toString(), // desc
                                query.value(8).toString(), // podcast title
                                QUrl::fromLocalFile(m_dir.absoluteFilePath(filename)), // url
                                query.value(5).toInt(), // duration
                                query.value(6).toInt(), // position
                                type, // type
                                query.value(3).toUInt(), // published
                                m_icon // icon
                             );
            } else {
                qWarning() << "Episode file doesn't exist:" << m_dir.absoluteFilePath(filename);
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

void GpodderEpisodeModel::setPodcastId(const QString &id)
{
    if (!m_podcastId.isEmpty()) {
        m_podcastId.clear();
        emit podcastIdChanged();
    }

    if (m_podcastId != id) {
        m_podcastId = id;
        emit podcastIdChanged();
        updateDir();
        updateModel();
    }
}

QString GpodderEpisodeModel::getPodcastId()
{
    return m_podcastId;
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
