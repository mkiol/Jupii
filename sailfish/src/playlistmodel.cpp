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

PlayListModel::PlayListModel(QObject *parent) :
    ListModel(new FileItem, parent)
{
}

PlayListModel::~PlayListModel()
{
}

int PlayListModel::getActiveItemIndex() const
{
    return m_activeItemIndex;
}

void PlayListModel::addItem(const QString& path)
{
    if (find(path) != 0) {
        qWarning() << "File" << path << "already added";
        emit error(E_FileExists);
        return;
    }

    QFileInfo file(path);

    if (!file.exists()) {
        qWarning() << "File" << path << "doesn't exist";
        return;
    }

    FileMetaData metaData;
    metaData.path = path;
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

    appendRow(new FileItem(metaData.path,
                           metaData.filename,
                           metaData.path,
                           metaData.date,
                           metaData.type,
                           metaData.size,
                           metaData.filename,
                           metaData.icon,
                           false,
                           this));
    emit itemAdded(path);
}


void PlayListModel::setActivePath(const QString &path)
{
    int len = m_list.length();

    for (int i = 0; i < len; ++i) {
        auto fi = static_cast<FileItem*>(m_list.at(i));

        bool active = fi->path() == path;

        fi->setActive(active);

        if(active)
            setActiveItemIndex(i);
    }
}

void PlayListModel::setActiveUrl(const QUrl &url)
{
    auto cs = ContentServer::instance();
    setActivePath(cs->pathFromUrl(url));
}

void PlayListModel::clear()
{
    if(rowCount() > 0) {
        removeRows(0, rowCount());
    }

    setActiveItemIndex(-1);
}

QString PlayListModel::activePath() const
{
    if (m_activeItemIndex > -1) {
        auto fi = static_cast<FileItem*>(m_list.at(m_activeItemIndex));
        return fi->path();
    }

    return QString();
}

QString PlayListModel::firstPath() const
{
    if (m_list.length() > 0)
        return static_cast<FileItem*>(m_list.first())->path();

    return QString();
}

void PlayListModel::setActiveItemIndex(int index)
{
    if (m_activeItemIndex != index) {
        m_activeItemIndex = index;
        emit activeItemChanged();
    }
}

bool PlayListModel::remove(const QString &path)
{
    int index = indexFromId(path);
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

QString PlayListModel::nextActivePath() const
{
    int nextIndex = m_activeItemIndex + 1;

    if (m_activeItemIndex > -1 && m_list.length() > nextIndex) {
        auto fi = static_cast<FileItem*>(m_list.at(nextIndex));
        return fi->path();
    }

    return QString();
}

QString PlayListModel::prevActivePath() const
{
    int prevIndex = m_activeItemIndex - 1;

    if (prevIndex > -1 && m_list.length() > m_activeItemIndex) {
        auto fi = static_cast<FileItem*>(m_list.at(prevIndex));
        return fi->path();
    }

    return QString();
}


QString PlayListModel::nextPath(const QString &path) const
{
    bool nextFound = false;

    for (auto li : m_list) {
        auto fi = static_cast<FileItem*>(li);
        if (nextFound)
            return fi->path();
        if(fi->path() == path)
            nextFound = true;
    }

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
