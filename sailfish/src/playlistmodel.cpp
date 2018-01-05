/* Copyright (C) 2017 Michal Kosciesza <michal@mkiol.net>
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#include <QDebug>
#include <QHash>

#include "playlistmodel.h"
#include "utils.h"
#include "filemetadata.h"
#include "settings.h"

PlayListModel::PlayListModel(QObject *parent) :
    ListModel(new FileItem, parent)
{
}

void PlayListModel::save()
{
    QStringList ids;

    for (auto item : m_list) {
        ids << item->id();
    }

    Settings::instance()->setLastPlaylist(ids);
}

void PlayListModel::load()
{
    QStringList ids = Settings::instance()->getLastPlaylist();
    QStringList n_ids;

    for (auto& id : ids) {
        if (addId(id)) {
            n_ids << id;
        }
    }

    if (!n_ids.isEmpty())
        emit itemsLoaded(n_ids);
}

int PlayListModel::getPlayMode() const
{
    return m_playMode;
}

void PlayListModel::setPlayMode(int value)
{
    if (value != m_playMode) {
        m_playMode = value;
        emit playModeChanged();
    }
}

int PlayListModel::getActiveItemIndex() const
{
    return m_activeItemIndex;
}

void PlayListModel::addItems(const QStringList& paths)
{
    qDebug() << "addItems:" << paths;

    QStringList n_ids;

    for (auto& path : paths) {
        // id = path/cookie e.g. /home/nemo/Music/track01.mp3/y6Dgh
        QString id = path + "/" + Utils::randString();
        if (addId(id)) {
            n_ids << id;
        }
    }

    if (!n_ids.isEmpty()) {
        emit itemsAdded(n_ids);

        if (Settings::instance()->getRememberPlaylist())
            save();
    }
}

bool PlayListModel::addId(const QString& id)
{
    QString path = Utils::pathFromId(id);

    QFileInfo file(path);

    if (!file.exists()) {
        qWarning() << "File" << path << "doesn't exist";
        return false;
    }

    if (find(id) != 0) {
        qWarning() << "Id" << id << "already added";
        emit error(E_FileExists);
        return false;
    }

    FileMetaData metaData;
    metaData.filename = file.fileName();
    metaData.type = ContentServer::instance()->getContentType(metaData.filename);
    metaData.size = file.size();

    switch (metaData.type) {
    case ContentServer::TypeDir:
        metaData.icon = "image://theme/icon-m-file-folder";
        break;
    case ContentServer::TypeImage:
        metaData.icon = "image://theme/icon-m-file-image";
        break;
    case ContentServer::TypeMusic:
        metaData.icon = "image://theme/icon-m-file-audio";
        break;
    case ContentServer::TypeVideo:
        metaData.icon = "image://theme/icon-m-file-video";
        break;
    default:
        metaData.icon = "image://theme/icon-m-file-other";
    }

    appendRow(new FileItem(id,
                           metaData.filename,
                           path,
                           metaData.date,
                           metaData.type,
                           metaData.size,
                           metaData.filename,
                           metaData.icon,
                           false,
                           this));
    return true;
}

void PlayListModel::setActiveId(const QString &id)
{
    if (id == activeId())
        return;

    int len = m_list.length();

    for (int i = 0; i < len; ++i) {
        auto fi = static_cast<FileItem*>(m_list.at(i));

        bool active = fi->id() == id;

        fi->setActive(active);

        if(active)
            setActiveItemIndex(i);
    }
}

void PlayListModel::setActiveUrl(const QUrl &url)
{
    auto cs = ContentServer::instance();
    setActiveId(cs->idFromUrl(url));
}

void PlayListModel::clear()
{
    if(rowCount() > 0) {
        removeRows(0, rowCount());
    }

    setActiveItemIndex(-1);
}

QString PlayListModel::activeId() const
{
    if (m_activeItemIndex > -1) {
        auto fi = m_list.at(m_activeItemIndex);
        return fi->id();
    }

    return QString();
}

QString PlayListModel::firstId() const
{
    if (m_list.length() > 0)
        return m_list.first()->id();

    return QString();
}

QString PlayListModel::secondId() const
{
    if (m_list.length() > 1)
        return m_list.at(1)->id();

    return QString();
}

void PlayListModel::setActiveItemIndex(int index)
{
    if (m_activeItemIndex != index) {
        m_activeItemIndex = index;
        emit activeItemChanged();
    }
}

bool PlayListModel::remove(const QString &id)
{
    int index = indexFromId(id);
    if (index < 0)
        return false;

    bool ok = removeRow(index);

    if (ok && m_activeItemIndex > -1) {
        if (index == m_activeItemIndex) {
            setActiveItemIndex(-1);
        } else if (index < m_activeItemIndex) {
            setActiveItemIndex(m_activeItemIndex - 1);
        }
    }

    return ok;
}

QString PlayListModel::nextActiveId() const
{
    if (m_activeItemIndex < 0)
        return QString();

    int l = m_list.length();
    if (l == 0)
        return QString();

    if (m_playMode == PM_RepeatOne)
        return m_list.at(m_activeItemIndex)->id();

    int nextIndex = m_activeItemIndex + 1;

    if (nextIndex < l) {
        return m_list.at(nextIndex)->id();
    } else if (m_playMode == PM_RepeatAll)
        return m_list.first()->id();

    return QString();
}

QString PlayListModel::prevActiveId() const
{
    if (m_activeItemIndex < 0)
        return QString();

    int l = m_list.length();
    if (l == 0)
        return QString();

    if (m_playMode == PM_RepeatOne)
        return m_list.at(m_activeItemIndex)->id();

    int prevIndex = m_activeItemIndex - 1;

    if (prevIndex < 0) {
        if (m_playMode == PM_RepeatAll)
            prevIndex = l - 1;
        else
            return QString();
    }

    return m_list.at(prevIndex)->id();
}


QString PlayListModel::nextId(const QString &id) const
{
    int l = m_list.length();
    if (l == 0)
        return QString();

    if (m_playMode == PM_RepeatOne)
        return id;

    bool nextFound = false;

    for (auto li : m_list) {
        if (nextFound)
            return li->id();
        if(li->id() == id)
            nextFound = true;
    }

    if (nextFound && m_playMode == PM_RepeatAll)
        return m_list.first()->id();

    return QString();
}

FileItem::FileItem(const QString &id,
                   const QString &name,
                   const QString &path,
                   const QString &date,
                   ContentServer::Type type,
                   const qint64 size,
                   const QString &title,
                   const QString &icon,
                   bool active,
                   QObject *parent) :
    ListItem(parent),
    m_id(id),
    m_name(name),
    m_path(path),
    m_date(date),
    m_type(type),
    m_size(size),
    m_title(title),
    m_icon(icon),
    m_active(active)
{}

QHash<int, QByteArray> FileItem::roleNames() const
{
    QHash<int, QByteArray> names;
    names[IdRole] = "id";
    names[NameRole] = "name";
    names[PathRole] = "path";
    names[DateRole] = "date";
    names[TypeRole] = "type";
    names[SizeRole] = "size";
    names[TitleRole] = "title";
    names[IconRole] = "icon";
    names[ActiveRole] = "active";
    return names;
}

QVariant FileItem::data(int role) const
{
    switch(role) {
    case IdRole:
        return id();
    case NameRole:
        return name();
    case PathRole:
        return path();
    case DateRole:
        return date();
    case TypeRole:
        return type();
    case SizeRole:
        return size();
    case TitleRole:
        return title();
    case IconRole:
        return icon();
    case ActiveRole:
        return active();
    default:
        return QVariant();
    }
}

void FileItem::setActive(bool value)
{
    if (m_active != value) {
        m_active = value;
        emit dataChanged();
    }
}
